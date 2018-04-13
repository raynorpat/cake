/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*/

#include "qcommon.h"
#include <errno.h>

#include "unzip.h"

/*
=============================================================================

QUAKE FILESYSTEM

All of Quake's data access is through a hierchal file system, but the contents of the file system can be transparently merged from several sources.

The "base directory" is the path to the directory holding the quake.exe and all game directories. The base directory is only used during filesystem
initialization.

The "game directory" is the first tree on the search path and directory that all generated files (savegames, screenshots, demos, config files) will be
saved to. This can be overridden with the "-game" command line parameter. The game directory can never be changed while quake is executing.
This is a precacution against having a malicious server instruct clients to write files over areas they shouldn't.

=============================================================================
*/

#define MAX_HANDLES		1024
#define MAX_READ		0x10000
#define MAX_WRITE		0x10000
#define MAX_FIND_FILES	0x04000
#define MAX_PAKS		100

typedef struct
{
	char		name[MAX_QPATH];
	fsMode_t	mode;
	FILE        *file;	// Only one will be used.
	unzFile     *zip;	// (file or zip)
} fsHandle_t;

typedef struct fsLink_s
{
	char        *from;
	char        *to;
	int			length;
	struct fsLink_s *next;
} fsLink_t;

typedef struct
{
	char		name[MAX_QPATH];
	int			size;
	int			offset;		// Ignored in PKZ files.
} fsPackFile_t;

typedef struct
{
	char		name[MAX_OSPATH];
	int			numFiles;
	FILE        *pak;
	unzFile     *pkz;
	fsPackFile_t *files;
} fsPack_t;

typedef struct fsSearchPath_s
{
	char		path[MAX_OSPATH]; // Only one used.
	fsPack_t	*pack;			// (path or pack)
	struct fsSearchPath_s *next;
} fsSearchPath_t;

typedef enum
{
	PAK,
	PKZ
} fsPackFormat_t;

typedef struct
{
	char		*suffix;
	fsPackFormat_t format;
} fsPackTypes_t;

fsHandle_t		fs_handles[MAX_HANDLES];
fsLink_t       *fs_links;
fsSearchPath_t *fs_searchPaths;
fsSearchPath_t *fs_baseSearchPaths;

// Pack formats / suffixes
fsPackTypes_t	fs_packtypes[] = {	
	{ "pak", PAK },
	{ "pkz", PKZ }
};

char			fs_gamedir[MAX_OSPATH];
qboolean		file_from_pak;

cvar_t         *fs_basedir;
cvar_t         *fs_cddir;
cvar_t         *fs_gamedirvar;
cvar_t         *fs_debug;

// raw search path, the actual search path is built from this one
typedef struct fsRawPath_s
{
	char		path[MAX_OSPATH];
	qboolean	create;
	qboolean	screenshot;
	struct fsRawPath_s *next;
} fsRawPath_t;
fsRawPath_t		*fs_rawPath;

fsHandle_t     *FS_GetFileByHandle(fileHandle_t f);

/*
============
COM_FilePath

Returns the path up to, but not including the last /
============
*/
void COM_FilePath(const char* in, char* out, size_t size)
{
	const char* s = in + strlen(in) - 1;

	while (s != in && *s != '/')
		s--;

	const size_t pathLength = s - in + 1;
	if (pathLength <= size)
		Q_strlcpy(out, in, pathLength);
	else if (size >= 1)
		out[0] = '\0';
}

/*
================
FS_FileLength
================
*/
int FS_FileLength (FILE *f)
{
	int		pos;
	int		end;

	pos = ftell (f);
	fseek (f, 0, SEEK_END);
	end = ftell (f);
	fseek (f, pos, SEEK_SET);

	return end;
}

/*
============
FS_CreatePath

Creates any directories needed to store the given filename
============
*/
void FS_CreatePath (char *path)
{
	char *cur;
	char *old;

	if (strstr(path, "..") != NULL)
	{
		Com_Printf(S_COLOR_YELLOW "WARNING: refusing to create relative path '%s'.\n", path);
		return;
	}

	cur = old = path;
	while (cur != NULL)
	{
		if ((cur - old) > 1)
		{
			// create the directory
			*cur = '\0';
			Sys_Mkdir (path);
			*cur = '/';
		}
		old = cur;
		cur = strchr(old + 1, '/');
	}
}

/*
============
FS_Gamedir

Called to find where to write a file (demos, savegames, etc)
============
*/
char *FS_Gamedir(void)
{
	if (*fs_gamedir)
		return fs_gamedir;
	else
		return BASEDIRNAME;
}

/*
=================
FS_HandleForFile

Finds a free fileHandle_t.
=================
*/
fsHandle_t *FS_HandleForFile(const char *path, fileHandle_t * f)
{
	int		i;
	fsHandle_t     *handle;

	handle = fs_handles;
	for (i = 0; i < MAX_HANDLES; i++, handle++)
	{
		if (handle->file == NULL && handle->zip == NULL)
		{
			strncpy(handle->name, path, sizeof(handle->name));
			*f = i + 1;
			return handle;
		}
	}

	// failed
	Com_Error(ERR_DROP, "FS_HandleForFile: none free");

	return NULL;
}

/*
=================
FS_GetFileByHandle

Returns a fsHandle_t * for the given fileHandle_t.
=================
*/
fsHandle_t *FS_GetFileByHandle(fileHandle_t f)
{
	if (f < 0 || f > MAX_HANDLES)
		Com_Error(ERR_DROP, "FS_GetFileByHandle: out of range");

	if (f == 0)
		f++;

	return &fs_handles[f - 1];
}

/*
=================
FS_FOpenFileAppend

Returns file size or -1 on error.
=================
*/
int FS_FOpenFileAppend(fsHandle_t * handle)
{
	char path[MAX_OSPATH];

	Com_sprintf(path, sizeof(path), "%s/%s", fs_gamedir, handle->name);
	FS_CreatePath(path);

	handle->file = fopen(path, "ab");
	if (handle->file)
	{
		if (fs_debug->value)
			Com_Printf("FS_FOpenFileAppend: '%s'.\n", path);
		return FS_FileLength(handle->file);
	}
	if (fs_debug->value)
		Com_Printf(S_COLOR_RED "FS_FOpenFileAppend: couldn't open '%s'.\n", path);

	return -1;
}

/*
=================
FS_FOpenFileWrite

Always returns 0 or -1 on error.
=================
*/
int FS_FOpenFileWrite(fsHandle_t * handle)
{
	char path[MAX_OSPATH];

	Com_sprintf(path, sizeof(path), "%s/%s", fs_gamedir, handle->name);
	FS_CreatePath(path);

	if ((handle->file = fopen(path, "wb")) == NULL)
	{
		if (fs_debug->value)
			Com_Printf("FS_FOpenFileWrite: '%s'.\n", path);
		return 0;
	}
	else
	{
		return 1;
	}
	if (fs_debug->value)
		Com_Printf(S_COLOR_RED "FS_FOpenFileWrite: couldn't open '%s'.\n", path);

	return -1;
}

/*
=================
FS_FOpenFileRead

Returns file size or -1 if not found. Can open separate files as well as
files inside pack files (both PAK and PKZ).
=================
*/
int FS_FOpenFileRead(fsHandle_t * handle, qboolean gamedirOnly)
{
	char		path[MAX_OSPATH];
	int		i;
	fsSearchPath_t *search;
	fsPack_t       *pack;

	file_from_pak = false;

	// search through the path, one element at a time.
	for (search = fs_searchPaths; search; search = search->next)
	{
		if (gamedirOnly)
		{
			if (strstr(search->path, FS_Gamedir()) == NULL)
				continue;
		}

		// HACK: for maps.lst and players/*
		// TODO: A flag to ignore paks would be better
		if ((strcmp(fs_gamedirvar->string, "") == 0) && search->pack)
		{
			if ((strcmp(handle->name, "maps.lst") == 0) || (strncmp(handle->name, "players/", 8) == 0))
				continue;
		}

		// search inside a pack file.
		if (search->pack)
		{
			pack = search->pack;
			for (i = 0; i < pack->numFiles; i++)
			{
				if (Q_stricmp(pack->files[i].name, handle->name) == 0)
				{
					// found it
					if (fs_debug->value)
						Com_Printf("FS_FOpenFileRead: '%s' (found in '%s').\n",	handle->name, pack->name);

					if (pack->pak)
					{
						// PAK
						file_from_pak = true;
						handle->file = fopen(pack->name, "rb");
						if (handle->file)
						{
							fseek(handle->file, pack->files[i].offset, SEEK_SET);
							return pack->files[i].size;
						}
					}
					else if (pack->pkz)
					{
						// PKZ
						file_from_pak = true;
						handle->zip = unzOpen(pack->name);
						if (handle->zip)
						{
							if (unzLocateFile(handle->zip, handle->name, 2) == UNZ_OK)
							{
								if (unzOpenCurrentFile(handle->zip) == UNZ_OK)
									return pack->files[i].size;
							}
							unzClose(handle->zip);
						}
					}

					Com_Error(ERR_FATAL, "Couldn't reopen '%s'", pack->name);
				}
			}
		}
		else
		{
			// search in a directory tree.
			Com_sprintf(path, sizeof(path), "%s/%s", search->path, handle->name);

			handle->file = fopen(path, "rb");
			if (!handle->file)
			{
				strlwr(path);
				handle->file = fopen(path, "rb");
			}

			if (!handle->file)
				continue;

			if (handle->file)
			{
				// found it
				if (fs_debug->value)
					Com_Printf("FS_FOpenFileRead: '%s' (found in '%s').\n",	handle->name, search->path);

				return FS_FileLength(handle->file);
			}
		}
	}

	// not found
	if (fs_debug->value)
		Com_Printf(S_COLOR_RED "FS_FOpenFileRead: couldn't find '%s'.\n", handle->name);

	return -1;
}

/*
==============
FS_FCloseFile

Other dll's can't just call fclose() on files returned by FS_FOpenFile.
==============
*/
void FS_FCloseFile(fileHandle_t f)
{
	fsHandle_t *handle;

	handle = FS_GetFileByHandle(f);

	if (handle->file)
	{
		fclose(handle->file);
	}
	else if (handle->zip)
	{
		unzCloseCurrentFile(handle->zip);
		unzClose(handle->zip);
	}

	memset(handle, 0, sizeof(*handle));
}

/*
===========
FS_FOpenFile

Finds the file in the search path. Returns filesize and an open FILE *. Used
for streaming data out of either a pak file or a seperate file.
===========
*/
int FS_FOpenFile(const char *name, fileHandle_t *f, fsMode_t mode, qboolean gamedirOnly)
{
	int	size = 0;
	fsHandle_t *handle;

	handle = FS_HandleForFile(name, f);

	strncpy(handle->name, name, sizeof(handle->name));
	handle->mode = mode;

	switch (mode)
	{
		case FS_READ:
			size = FS_FOpenFileRead(handle, gamedirOnly);
			break;
		case FS_WRITE:
			size = FS_FOpenFileWrite(handle);
			break;
		case FS_APPEND:
			size = FS_FOpenFileAppend(handle);
			break;
		default:
			Com_Error(ERR_FATAL, "FS_FOpenFile: bad mode (%i)", mode);
			break;
	}

	if (size != -1)
		return size;

	// couldn't open, so free the handle.
	memset(handle, 0, sizeof(*handle));
	*f = 0;

	return -1;
}

/*
=================
FS_ReadFile

Properly handles partial reads.
=================
*/
int FS_Read(void *buffer, int size, fileHandle_t f)
{
	qboolean tried = false;
	byte	*buf;
	int		r;
	int		remaining;
	fsHandle_t *handle;

	handle = FS_GetFileByHandle(f);

	// read
	remaining = size;
	buf = (byte *)buffer;

	while (remaining)
	{
		if (handle->file)
			r = fread(buf, 1, remaining, handle->file);
		else if (handle->zip)
			r = unzReadCurrentFile(handle->zip, buf, remaining);
		else
			return 0;

		if (r == 0)
		{
			if (!tried)
			{
				// might tried to read from a CD
				tried = true;
			}
			else
			{
				// already tried once
				Com_Error(ERR_FATAL, va("FS_Read: 0 bytes read from '%s'", handle->name));
				return size - remaining;
			}
		}
		else if (r == -1)
		{
			Com_Error(ERR_FATAL, "FS_Read: -1 bytes read from '%s'", handle->name);
		}

		remaining -= r;
		buf += r;
	}

	return size;
}

/*
=================
FS_FRead

Properly handles partial reads of size up to count times. No error if it
can't read.
=================
*/
int FS_FRead(void *buffer, int size, int count, fileHandle_t f)
{
	qboolean tried = false;
	byte	*buf;
	int		loops;
	int		r;
	int		remaining;
	fsHandle_t *handle;

	handle = FS_GetFileByHandle(f);

	// read
	loops = count;
	buf = (byte *)buffer;

	while (loops)
	{
		// read in chunks.
		remaining = size;
		while (remaining)
		{
			if (handle->file)
				r = fread(buf, 1, remaining, handle->file);
			else if (handle->zip)
				r = unzReadCurrentFile(handle->zip, buf, remaining);
			else
				return 0;

			if (r == 0)
			{
				if (!tried)
				{
					// might tried to read from a CD
					tried = true;
				}
				else
				{
					// already tried once
					return size - remaining;
				}
			}
			else if (r == -1)
			{
				Com_Error(ERR_FATAL, "FS_FRead: -1 bytes read from '%s'", handle->name);
			}

			remaining -= r;
			buf += r;
		}
		loops--;
	}

	return size;
}

/*
=================
FS_Write

Properly handles partial writes.
=================
*/
int FS_Write(const void *buffer, int size, fileHandle_t f)
{
	byte	*buf;
	int		remaining;
	int		w = 0;
	fsHandle_t *handle;

	handle = FS_GetFileByHandle(f);

	// write
	remaining = size;
	buf = (byte *)buffer;

	while (remaining)
	{
		if (handle->file)
			w = fwrite(buf, 1, remaining, handle->file);
		else if (handle->zip)
			Com_Error(ERR_FATAL, "FS_Write: can't write to zip file '%s'", handle->name);
		else
			return 0;

		if (w == 0)
			return size - remaining;
		else if (w == -1)
			Com_Error(ERR_FATAL, "FS_Write: -1 bytes written to '%s'", handle->name);

		remaining -= w;
		buf += w;
	}

	return size;
}

/*
=================
FS_FTell
=================
*/
int FS_FTell(fileHandle_t f)
{
	fsHandle_t *handle;

	handle = FS_GetFileByHandle(f);

	if (handle->file)
		return ftell(handle->file);
	else if (handle->zip)
		return unztell(handle->zip);

	return 0;
}

/*
=================
FS_ListPak

Generates a listing of the contents of a pak file.
=================
*/
char **FS_ListPak(char *find, int *num)
{
	char	**list = 0;
	int		nfiles = 0;
	int		nfound = 0;
	int		i;
	fsPack_t  *pak;
	fsSearchPath_t *search;

	// check pak files
	for (search = fs_searchPaths; search != NULL; search = search->next)
	{
		if (search->pack == NULL)
			continue;

		pak = search->pack;

		// find and build list.
		for (i = 0; i < pak->numFiles; i++)
			nfiles++;
	}

	list = calloc(nfiles, sizeof(char *));

	for (search = fs_searchPaths; search != NULL; search = search->next)
	{
		if (search->pack == NULL)
			continue;

		pak = search->pack;

		// find and build list.
		for (i = 0; i < pak->numFiles; i++)
		{
			if (strstr(pak->files[i].name, find))
			{
				list[nfound] = strdup(pak->files[i].name);
				nfound++;
			}
		}
	}

	*num = nfound;
	list = realloc(list, sizeof(char *) * nfound);

	return list;
}

/*
=================
FS_Seek
=================
*/
void FS_Seek(fileHandle_t f, int offset, fsOrigin_t origin)
{
	byte		dummy[0x8000];
	int			len;
	int			r;
	int			remaining = 0;
	fsHandle_t  *handle;
	unz_file_info info;

	handle = FS_GetFileByHandle(f);

	if (handle->file)
	{
		switch (origin)
		{
			case FS_SEEK_SET:
				fseek(handle->file, offset, SEEK_SET);
				break;
			case FS_SEEK_CUR:
				fseek(handle->file, offset, SEEK_CUR);
				break;
			case FS_SEEK_END:
				fseek(handle->file, offset, SEEK_END);
				break;
			default:
				Com_Error(ERR_FATAL, "FS_Seek: bad origin (%i)", origin);
				break;
		}
	}
	else if (handle->zip)
	{
		switch (origin)
		{
			case FS_SEEK_SET:
				remaining = offset;
				break;
			case FS_SEEK_CUR:
				remaining = offset + unztell(handle->zip);
				break;
			case FS_SEEK_END:
				unzGetCurrentFileInfo(handle->zip, &info, NULL, 0, NULL, 0, NULL, 0);
				remaining = offset + info.uncompressed_size;
				break;
			default:
				Com_Error(ERR_FATAL, "FS_Seek: bad origin (%i)", origin);
				break;
		}

		// reopen the file.
		unzCloseCurrentFile(handle->zip);
		unzOpenCurrentFile(handle->zip);

		// skip until the desired offset is reached.
		while (remaining)
		{
			len = remaining;
			if (len > sizeof(dummy))
				len = sizeof(dummy);

			r = unzReadCurrentFile(handle->zip, dummy, len);
			if (r <= 0)
				break;
			
			remaining -= r;
		}
	}
}

/*
=================
FS_FileExists
================
*/
qboolean FS_FileExists(char *path)
{
	fileHandle_t	f;

	FS_FOpenFile(path, &f, FS_READ, false);

	if (f != 0)
	{
		FS_FCloseFile(f);
		return (true);
	}
	return false;
}

/*
=================
FS_RenameFile
=================
*/
void FS_RenameFile(const char *oldPath, const char *newPath)
{
	Com_DPrintf("FS_RenameFile(%s, %s)\n", oldPath, newPath);

	if (rename(oldPath, newPath))
		Com_DPrintf("FS_RenameFile: failed to rename '%s' to '%s'.\n", oldPath, newPath);
}

/*
=================
FS_DeleteFile
=================
*/
void FS_DeleteFile(const char *path)
{
	Com_DPrintf("FS_DeleteFile(%s)\n", path);

	if (remove(path))
		Com_DPrintf("FS_DeleteFile: failed to delete '%s'.\n", path);
}

/*
=============
FS_LoadFile

Filename are reletive to the quake search path. A null buffer will just
return the file length without loading.
=============
*/
int FS_LoadFile(char *path, void **buffer)
{
	byte		*buf;
	int			size;
	fileHandle_t f;

	buf = NULL;
	size = FS_FOpenFile(path, &f, FS_READ, false);

	if (size <= 0)
	{
		if (buffer)
			*buffer = NULL;
		return size;
	}
	if (buffer == NULL)
	{
		FS_FCloseFile(f);
		return size;
	}
	buf = malloc(size);
	*buffer = buf;

	FS_Read(buf, size, f);
	FS_FCloseFile(f);

	return size;
}

/*
=============
FS_FreeFile
=============
*/
void FS_FreeFile(void *buffer)
{
	if (buffer == NULL)
	{
		Com_DPrintf("FS_FreeFile: NULL buffer.\n");
		return;
	}
	free(buffer);
}

/*
=============
FS_LoadPAK

Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning of the
list so they override previous pack files.
=============
*/
fsPack_t *FS_LoadPAK(const char *packPath)
{
	int				i;
	int				numFiles;
	FILE           *handle;
	fsPackFile_t   *files;
	fsPack_t       *pack;
	dpackheader_t	header;
	dpackfile_t		info[MAX_FILES_IN_PACK];

	handle = fopen(packPath, "rb");
	if (handle == NULL)
		return (NULL);

	fread(&header, 1, sizeof(dpackheader_t), handle);

	if (LittleLong(header.ident) != IDPAKHEADER)
	{
		fclose(handle);
		Com_Error(ERR_FATAL, "FS_LoadPAK: '%s' is not a pack file", packPath);
	}
	header.dirofs = LittleLong(header.dirofs);
	header.dirlen = LittleLong(header.dirlen);

	numFiles = header.dirlen / sizeof(dpackfile_t);
	if (numFiles > MAX_FILES_IN_PACK || numFiles == 0)
	{
		fclose(handle);
		Com_Error(ERR_FATAL, "FS_LoadPAK: '%s' has %i files", packPath, numFiles);
	}
	files = Z_Malloc(numFiles * sizeof(fsPackFile_t));

	fseek(handle, header.dirofs, SEEK_SET);
	fread(info, 1, header.dirlen, handle);

	// parse the directory
	for (i = 0; i < numFiles; i++)
	{
		strncpy(files[i].name, info[i].name, sizeof(files[i].name));
		files[i].offset = LittleLong(info[i].filepos);
		files[i].size = LittleLong(info[i].filelen);
	}

	pack = Z_Malloc(sizeof(fsPack_t));
	strncpy(pack->name, packPath, sizeof(pack->name));
	pack->pak = handle;
	pack->pkz = NULL;
	pack->numFiles = numFiles;
	pack->files = files;

	Com_Printf("Added packfile '%s' (%i files).\n", pack, numFiles);

	return pack;
}

/*
=================
FS_LoadPKZ

Takes an explicit (not game tree related) path to a pack file.

Loads the header and directory, adding the files at the beginning of the list
so they override previous pack files.
=================
*/
fsPack_t *FS_LoadPKZ(const char *packPath)
{
	char			fileName[MAX_QPATH];
	int				i = 0;
	int				numFiles;
	int				status;
	fsPackFile_t   *files;
	fsPack_t       *pack;
	unzFile			*handle;
	unz_file_info	info;
	unz_global_info	global;

	handle = unzOpen(packPath);
	if (handle == NULL)
		return NULL;

	if (unzGetGlobalInfo(handle, &global) != UNZ_OK)
	{
		unzClose(handle);
		Com_Error(ERR_FATAL, "FS_LoadPKZ: '%s' is not a pack file", packPath);
	}
	numFiles = global.number_entry;

	if (numFiles > MAX_FILES_IN_PACK || numFiles == 0)
	{
		unzClose(handle);
		Com_Error(ERR_FATAL, "FS_LoadPKZ: '%s' has %i files", packPath, numFiles);
	}
	files = Z_Malloc(numFiles * sizeof(fsPackFile_t));

	// parse the directory
	status = unzGoToFirstFile(handle);
	while (status == UNZ_OK)
	{
		fileName[0] = '\0';
		unzGetCurrentFileInfo(handle, &info, fileName, MAX_QPATH, NULL, 0, NULL, 0);
		strncpy(files[i].name, fileName, sizeof(files[i].name));
		files[i].offset = -1; // offset is not used in ZIP files
		files[i].size = info.uncompressed_size;
		i++;
		status = unzGoToNextFile(handle);
	}

	pack = Z_Malloc(sizeof(fsPack_t));
	strncpy(pack->name, packPath, sizeof(pack->name));
	pack->pak = NULL;
	pack->pkz = handle;
	pack->numFiles = numFiles;
	pack->files = files;

	Com_Printf("Added packfile '%s' (%i files).\n", pack, numFiles);

	return pack;
}

/*
================
FS_NextPath

Allows enumerating all of the directories in the search path
================
*/
char *FS_NextPath(char *prevPath)
{
	char           *prev;
	fsSearchPath_t *search;

	if (prevPath == NULL)
		return (fs_gamedir);

	prev = fs_gamedir;
	for (search = fs_searchPaths; search; search = search->next)
	{
		if (search->pack != NULL)
			continue;

		if (prevPath == prev)
			return (search->path);

		prev = search->path;
	}

	return NULL;
}

/*
============
FS_Path_f
============
*/
void FS_Path_f(void)
{
	int				i;
	int				totalFiles = 0;
	fsSearchPath_t	*search;
	fsHandle_t		*handle;
	fsLink_t		*link;

	Com_Printf("Current search path:\n");

	for (search = fs_searchPaths; search; search = search->next)
	{
		if (search->pack != NULL)
		{
			Com_Printf("%s (%i files)\n", search->pack->name, search->pack->numFiles);
			totalFiles += search->pack->numFiles;
		}
		else
		{
			Com_Printf("%s\n", search->path);
		}
	}

	Com_Printf("\n");

	for (i = 0, handle = fs_handles; i < MAX_HANDLES; i++, handle++)
	{
		if (handle->file != NULL || handle->zip != NULL)
			Com_Printf("Handle %i: '%s'.\n", i + 1, handle->name);
	}

	for (i = 0, link = fs_links; link; i++, link = link->next)
		Com_Printf("Link %i: '%s' -> '%s'.\n", i, link->from, link->to);

	Com_Printf("----------------------\n");
	Com_Printf("%i files in PAK/PKZ files.\n", totalFiles);
}

/*
================
FS_Link_f

Creates a filelink_t.
================
*/
void FS_Link_f(void)
{
	fsLink_t *l, **prev;

	if (Cmd_Argc() != 3)
	{
		Com_Printf("USAGE: link <from> <to>\n");
		return;
	}

	// see if the link already exists
	prev = &fs_links;
	for (l = fs_links; l != NULL; l = l->next)
	{
		if (strcmp(l->from, Cmd_Argv(1)) == 0)
		{
			Z_Free(l->to);
			if (strlen(Cmd_Argv(2)) == 0)
			{
				// delete it
				*prev = l->next;
				Z_Free(l->from);
				Z_Free(l);
				return;
			}
			l->to = CopyString(Cmd_Argv(2));
			return;
		}
		prev = &l->next;
	}

	// create a new link
	l = Z_Malloc(sizeof(*l));
	l->next = fs_links;
	fs_links = l;
	l->from = CopyString(Cmd_Argv(1));
	l->length = strlen(l->from);
	l->to = CopyString(Cmd_Argv(2));
}

/*
===========
FS_ListFiles

Create a list of files that match a criteria.
===========
*/
char **FS_ListFiles(char *findname, int *numfiles)
{
	char *s;
	int nfiles = 0;
	char **list = 0;

	// count the number of matches
	s = Sys_FindFirst(findname);
	while (s != NULL)
	{
		if (s[strlen(s) - 1] != '.')
			nfiles++;

		s = Sys_FindNext();
	}
	Sys_FindClose();

	// check if there are matches
	if (nfiles == 0)
		return NULL;

	nfiles++; // add space for a guard
	*numfiles = nfiles;

	// allocate the list
	list = calloc(nfiles, sizeof(char *));

	// fill the list
	s = Sys_FindFirst(findname);
	nfiles = 0;
	while (s)
	{
		if (s[strlen(s) - 1] != '.')
		{
			list[nfiles] = strdup(s);
#ifdef _WIN32
			Q_strlwr(list[nfiles]);
#endif
			nfiles++;
		}
		s = Sys_FindNext();
	}
	Sys_FindClose();

	return list;
}

/*
===========
CompareAttributesPack

Compare file attributes in packed files.
Returns a boolean value, true if the attributes match the file.
===========
*/
static qboolean ComparePackFiles(const char *findname, const char *name, char *output, int size)
{
	qboolean	 retval;
	char		 buffer[MAX_OSPATH];

	strncpy(buffer, name, sizeof(buffer));

	retval = glob_match((char *)findname, buffer);
	if (retval && output != NULL)
		strncpy(output, buffer, size);

	return retval;
}

/*
===========
FS_ListFiles2

Create a list of files that match a criteria.
Searchs are relative to the game directory and use all the search paths
including .pak files.
===========
*/
char **FS_ListFiles2 (char *findname, int *numfiles)
{
	fsSearchPath_t	*search;
	int				i, j;
	int				nfiles;
	int				tmpnfiles;
	char			**tmplist;
	char			**list;
	char			path[MAX_OSPATH];

	nfiles = 0;
	list = malloc(sizeof(char *));

	for (search = fs_searchPaths; search != NULL; search = search->next)
	{
		if (search->pack != NULL)
		{
			// search pak files
			for (i = 0, j = 0; i < search->pack->numFiles; i++)
			{
				if (ComparePackFiles(findname, search->pack->files[i].name, NULL, 0))
					j++;
			}
			if (j == 0)
				continue;
			nfiles += j;
			list = realloc(list, nfiles * sizeof(char *));
			for (i = 0, j = nfiles - j; i < search->pack->numFiles; i++)
			{
				if (ComparePackFiles(findname, search->pack->files[i].name,	path, sizeof(path)))
					list[j++] = strdup(path);
			}
		}
		else if (search->path != NULL)
		{
			// search base filesystem
			Com_sprintf(path, sizeof(path), "%s/%s", search->path, findname);
			tmplist = FS_ListFiles(path, &tmpnfiles);
			if (tmplist != NULL)
			{
				tmpnfiles--;
				nfiles += tmpnfiles;
				list = realloc(list, nfiles * sizeof(char *));
				for (i = 0, j = nfiles - tmpnfiles;	i < tmpnfiles; i++, j++)
					list[j] = strdup(tmplist[i] + strlen(search->path) + 1);
				FS_FreeFileList(tmplist, tmpnfiles + 1);
			}
		}
	}

	// delete any duplicates
	tmpnfiles = 0;
	for (i = 0; i < nfiles; i++)
	{
		if (list[i] == NULL)
			continue;

		for (j = i + 1; j < nfiles; j++)
		{
			if (list[j] != NULL && strcmp(list[i], list[j]) == 0)
			{
				free(list[j]);
				list[j] = NULL;
				tmpnfiles++;
			}
		}
	}

	if (tmpnfiles > 0)
	{
		nfiles -= tmpnfiles;
		tmplist = malloc(nfiles * sizeof(char *));
		for (i = 0, j = 0; i < nfiles + tmpnfiles; i++)
		{
			if (list[i] != NULL)
				tmplist[j++] = list[i];
		}
		free(list);
		list = tmplist;
	}

	// add a guard
	if (nfiles > 0)
	{
		nfiles++;
		list = realloc(list, nfiles * sizeof(char *));
		list[nfiles - 1] = NULL;
	}
	else
	{
		free(list);
		list = NULL;
	}

	*numfiles = nfiles;

	return (list);
}

/*
===========
FS_FreeFileList

Free list of files created by FS_ListFiles()
===========
*/
void FS_FreeFileList(char **list, int n)
{
	int i;

	for (i = 0; i < n; i++)
	{
		if (list[i])
		{
			free(list[i]);
			list[i] = 0;
		}
	}

	free(list);
}

/*
===========
FS_ConvertPath
===========
*/
void FS_ConvertPath(char *s)
{
	while (*s)
	{
		if (*s == '\\' || *s == ':')
			*s = '/';
		s++;
	}
}

/*
===========
FS_FilenameCompletion
===========
*/
void FS_FilenameCompletion(char *dir, char *ext, qboolean stripExt, void(*callback)(char *s))
{
	char	**filenames = NULL;
	int		nfiles;
	int		i;
	char	path[MAX_STRING_CHARS];
	char	filename[MAX_STRING_CHARS];

	strcpy(path, va("%s/*.%s", dir, ext));
	filenames = FS_ListFiles2(path, &nfiles);

	for (i = 0; i < nfiles; i++)
	{
		if (filenames[i] == 0)
			continue;

		FS_ConvertPath(filenames[i]);
		Q_strlcpy(filename, filenames[i], MAX_STRING_CHARS);

		strcpy(filename, COM_StripPathFromFilename(filename));

		if (stripExt)
			COM_StripExtensionSafe(filename, filename, sizeof(filename));

		callback(filename);
	}

	FS_FreeFileList(filenames, nfiles);
}

/*
===========
FS_Dir_f

Directory listing
===========
*/
void FS_Dir_f(void)
{
	char **dirnames;
	char findname[1024];
	char *path = NULL;
	char wildcard[1024] = "*.*";
	int	i;
	int	ndirs;

	// check for pattern in arguments
	if (Cmd_Argc() != 1)
		strncpy(wildcard, Cmd_Argv(1), sizeof(wildcard));

	// scan search paths and list files
	while ((path = FS_NextPath(path)) != NULL)
	{
		Com_sprintf(findname, sizeof(findname), "%s/%s", path, wildcard);
		Com_Printf("Directory of '%s'.\n", findname);
		Com_Printf("----\n");

		if ((dirnames = FS_ListFiles(findname, &ndirs)) != 0)
		{
			for (i = 0; i < ndirs - 1; i++)
			{
				if (strrchr(dirnames[i], '/'))
					Com_Printf("%s\n", strrchr(dirnames[i], '/') + 1);
				else
					Com_Printf("%s\n", dirnames[i]);
			}
			FS_FreeFileList(dirnames, ndirs);
		}
		Com_Printf("\n");
	}
}

/*
================
FS_AddDirToSearchPath

adds the directory to the head of the path,
then loads and adds pak1.pak pak2.pak ...
================
*/
void FS_AddDirToSearchPath(char *dir, qboolean create)
{
	char **list;
	char path[MAX_OSPATH];
	int i, j;
	int nfiles;
	fsPack_t *pack = NULL;
	fsSearchPath_t *search;
	size_t len = strlen(dir);

	// the directory must not end with an /
	// it would f*ck up the logic in other parts of the game...
	if (dir[len - 1] == '/')
		dir[len - 1] = '\0';
	else if (dir[len - 1] == '\\')
		dir[len - 1] = '\0';

	// set the current directory as game directory.
	// this is somewhat fragile since the game directory MUST be the last directory added to the search path
	Q_strlcpy(fs_gamedir, dir, sizeof(fs_gamedir));
	if (create)
		FS_CreatePath(fs_gamedir);

	// add the directory itself
	search = Z_Malloc (sizeof(fsSearchPath_t));
	Q_strlcpy (search->path, dir, sizeof(search->path));
	search->next = fs_searchPaths;
	fs_searchPaths = search;

	// we need to add numbered paks in te directory in sequence and all other paks after them.
	// otherwise the gamedata may break.
	for (i = 0; i < sizeof(fs_packtypes) / sizeof(fs_packtypes[0]); i++)
	{
		for (j = 0; j < MAX_PAKS; j++)
		{
			Com_sprintf (path, sizeof(path), "%s/pak%d.%s", dir, j, fs_packtypes[i].suffix);

			switch (fs_packtypes[i].format)
			{
				case PAK:
					pack = FS_LoadPAK(path);
					break;
				case PKZ:
					pack = FS_LoadPKZ(path);
					break;
			}

			if (pack == NULL)
				continue;

			search = Z_Malloc (sizeof(fsSearchPath_t));
			search->pack = pack;
			search->next = fs_searchPaths;
			fs_searchPaths = search;
		}
	}

	// and as said above all other pak files
	for (i = 0; i < sizeof(fs_packtypes) / sizeof(fs_packtypes[0]); i++)
	{
		Com_sprintf (path, sizeof(path), "%s/*.%s", dir, fs_packtypes[i].suffix);

		// nothing here, next pak type please
		if ((list = FS_ListFiles(path, &nfiles)) == NULL)
			continue;

		Com_sprintf (path, sizeof(path), "%s/pak*.%s", dir, fs_packtypes[i].suffix);

		for (j = 0; j < nfiles - 1; j++)
		{
			// if the pak starts with the string 'pak' it's ignored
			// this is somewhat stupid, it would be better to ignore just pak%d...
			if (glob_match(path, list[j]))
				continue;

			switch (fs_packtypes[i].format)
			{
				case PAK:
					pack = FS_LoadPAK (list[j]);
					break;
				case PKZ:
					pack = FS_LoadPKZ (list[j]);
					break;
			}

			if (pack == NULL)
				continue;

			search = Z_Malloc (sizeof(fsSearchPath_t));
			search->pack = pack;
			search->next = fs_searchPaths;
			fs_searchPaths = search;
		}

		FS_FreeFileList (list, nfiles);
	}
}

/*
================
FS_BuildGenericSearchPath

builds all of the search paths
for FS_AddDirToSearchPath()
================
*/
void FS_BuildGenericSearchPath (void)
{
	// we may not use the va() function from q_shared.c
	// since it's buffersize is 1024 while most OS have a maximum path size of 4096...
	char path[MAX_OSPATH];
	fsRawPath_t *search = fs_rawPath;

	while (search != NULL)
	{
		Com_sprintf (path, sizeof(path), "%s/%s", search->path, BASEDIRNAME);
		FS_AddDirToSearchPath (path, search->create);

		search = search->next;
	}

	// until here we've added the generic directories to the search path
	// save the current head node so we can distinguish generic and specialized directories
	fs_baseSearchPaths = fs_searchPaths;

	// we need to create the game directory
	Sys_Mkdir (fs_gamedir);

	// we need to create the screenshot directory since the refresh doesn't link the filesystem stuff
	Com_sprintf (path, sizeof(path), "%s/scrnshot", fs_gamedir);
	Sys_Mkdir (path);
}

/*
================
FS_BuildGameSpecificSearchPath

builds all of the game specific search paths
for FS_AddDirToSearchPath()
================
*/
void FS_BuildGameSpecificSearchPath (char *dir)
{
	// we may not use the va() function from q_shared.c
	// since it's buffersize is 1024 while most OS have a maximum path size of 4096...
	char path[MAX_OSPATH];
	int i;
	fsRawPath_t *search;
	fsSearchPath_t *next;

	// empty string means base game/mod
	if (dir[0] == '\0')
		dir = BASEDIRNAME;

	// the user may give us paths like xatrix/ or even /home/stupid/quake2/xatrix
	if (!strcmp(dir, ".") || strstr(dir, "..") || strstr(dir, "/") || strstr(dir, "\\"))
	{
		Com_Printf (S_COLOR_RED "Gamedir should be a single filename, not a path.\n");
		return;
	}

	// BASEDIR is already added as a generic directory
	// adding it again as a specialised directory breaks the logic in other parts of the code
	// this may happen if the user does something like ./quake2 +set game baseq2
	if (!Q_stricmp(dir, BASEDIRNAME))
		return;

	// we may already have specialised directories in our search path
	// this can happen if the server changes the mod. Let's remove them
	while (fs_searchPaths != fs_baseSearchPaths)
	{
		if (fs_searchPaths->pack)
		{
			if (fs_searchPaths->pack->pak)
				fclose (fs_searchPaths->pack->pak);

			if (fs_searchPaths->pack->pkz)
				unzClose (fs_searchPaths->pack->pkz);

			Z_Free (fs_searchPaths->pack->files);
			Z_Free (fs_searchPaths->pack);
		}

		next = fs_searchPaths->next;
		Z_Free (fs_searchPaths);
		fs_searchPaths = next;
	}

	// close open files for game dir
	for (i = 0; i < MAX_HANDLES; i++)
	{
		if (strstr(fs_handles[i].name, dir) && ((fs_handles[i].file != NULL) || (fs_handles[i].zip != NULL)))
			FS_FCloseFile(i);
	}

	// enforce a renderer and sound backend restart to purge all internal caches
	// this is rather hacky but Quake II doesn't have a better mechanism...
	if ((dedicated != NULL) && (dedicated->value != 1))
		Cbuf_AddText("vid_restart\nsnd_restart\n");

	// the game was reset to baseq2
	if ((Q_stricmp(dir, BASEDIRNAME) == 0) || (*dir == 0))
	{
		Cvar_FullSet("gamedir", "", CVAR_SERVERINFO | CVAR_NOSET);
		Cvar_FullSet("game", "", CVAR_LATCH | CVAR_SERVERINFO);

		// fs_gamedir must be reset to the last dir of the generic search path.
		Q_strlcpy (fs_gamedir, fs_baseSearchPaths->path, sizeof(fs_gamedir));
	}
	else
	{
		Cvar_FullSet("gamedir", dir, CVAR_SERVERINFO | CVAR_NOSET);
		search = fs_rawPath;
		while (search != NULL)
		{
			Com_sprintf (path, sizeof(path), "%s/%s", search->path, dir);
			FS_AddDirToSearchPath (path, search->create);

			search = search->next;
		}
	}

	// create the game directory
	Sys_Mkdir (fs_gamedir);

	// we need to create the screenshot directory since the refresh doesn't link the filesystem stuff
	Com_sprintf (path, sizeof(path), "%s/scrnshot", fs_gamedir);
	Sys_Mkdir (path);

	// the gamedir has changed, so read in the corresponding configs
	Qcommon_ExecConfigs (false);
}

void FS_AddDirToRawPath(const char *dir, qboolean create)
{
	fsRawPath_t *search;

	// Add the directory
	search = Z_Malloc (sizeof(fsRawPath_t));
	Q_strlcpy (search->path, dir, sizeof(search->path));
	search->create = create;
	search->next = fs_rawPath;
	fs_rawPath = search;
}

void FS_BuildRawPath(void)
{
	// add $HOME/.xxx (MUST be the last dir!)
	char *homedir = Sys_GetHomeDir ();
	if (homedir != NULL)
		FS_AddDirToRawPath (homedir, true);

	// add $binarydir
	char *binarydir = Sys_GetBinaryDir ();
	if (binarydir[0] != '\0')
		FS_AddDirToRawPath (binarydir, false);

	// add $basedir/
	FS_AddDirToRawPath (fs_basedir->string, false);

	// the CD must be the last directory of the path, otherwise we cannot be sure that the game won't stream the videos from the CD.
	if (fs_cddir->string[0] != '\0')
		FS_AddDirToRawPath (fs_cddir->string, false);
}

void FS_InitFilesystem (void)
{
	// Register FS commands.
	Cmd_AddCommand ("path", FS_Path_f);
	Cmd_AddCommand ("link", FS_Link_f);
	Cmd_AddCommand ("dir", FS_Dir_f);

	// Register cvars
	fs_basedir = Cvar_Get ("basedir", ".", CVAR_NOSET);
	fs_cddir = Cvar_Get ("cddir", "", CVAR_NOSET);
	fs_gamedirvar = Cvar_Get ("game", "", CVAR_LATCH | CVAR_SERVERINFO);
	fs_debug = Cvar_Get ("fs_debug", "0", 0);

	// Build search path
	FS_BuildRawPath ();
	FS_BuildGenericSearchPath ();

	if (fs_gamedirvar->string[0] != '\0')
		FS_BuildGameSpecificSearchPath (fs_gamedirvar->string);

	Com_Printf ("Using '%s' for writing.\n", fs_gamedir);
}

/*
=================
FS_Shutdown
=================
*/
void FS_Shutdown (void)
{
	int				i;
	fsHandle_t		*handle;
	fsSearchPath_t	*next;
	fsPack_t		*pack;

	// Unregister commands
	Cmd_RemoveCommand("dir");
	Cmd_RemoveCommand("link");
	Cmd_RemoveCommand("path");

	// close all files
	for (i = 0, handle = fs_handles; i < MAX_HANDLES; i++, handle++)
	{
		if (handle->file != NULL)
			fclose(handle->file);

		if (handle->zip != NULL)
		{
			unzCloseCurrentFile(handle->zip);
			unzClose(handle->zip);
		}
	}

	// free the search paths
	while (fs_searchPaths != NULL)
	{
		if (fs_searchPaths->pack != NULL)
		{
			pack = fs_searchPaths->pack;
			if (pack->pak != NULL)
				fclose(pack->pak);
			if (pack->pkz != NULL)
				unzClose(pack->pkz);
			Z_Free(pack->files);
			Z_Free(pack);
		}
		next = fs_searchPaths->next;
		Z_Free(fs_searchPaths);
		fs_searchPaths = next;
	}
}
