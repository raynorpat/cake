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

/*
=============================================================================

QUAKE FILESYSTEM

=============================================================================
*/


//
// in memory
//

typedef struct
{
	char	name[MAX_QPATH];
	int		filepos, filelen;
} packfile_t;

typedef struct pack_s
{
	char	filename[MAX_OSPATH];
	FILE	*handle;
	int		numfiles;
	packfile_t	*files;
} pack_t;

char	fs_gamedir[MAX_OSPATH];
cvar_t	*fs_basedir;
cvar_t	*fs_gamedirvar;

typedef struct searchpath_s
{
	char	filename[MAX_OSPATH];
	pack_t	*pack;		// only one of filename / pack will be used
	struct searchpath_s *next;
} searchpath_t;

searchpath_t	*fs_searchpaths;
searchpath_t	*fs_base_searchpaths;	// without gamedirs

/*

All of Quake's data access is through a hierchal file system, but the contents of the file system can be transparently merged from several sources.

The "base directory" is the path to the directory holding the quake.exe and all game directories. The sys_* files pass this to host_init in quakeparms_t->basedir.
This can be overridden with the "-basedir" command line parm to allow code debugging in a different directory. The base directory is only used during filesystem
initialization.

The "game directory" is the first tree on the search path and directory that all generated files (savegames, screenshots, demos, config files) will be saved to.
This can be overridden with the "-game" command line parameter. The game directory can never be changed while quake is executing. This is a precacution against
having a malicious server instruct clients to write files over areas they shouldn't.

*/

/*
============
COM_FilePath

Returns the path up to, but not including the last /
============
*/
void COM_FilePath(const char *path, char *dst, int dstSize)
{
	char *pos;

	if ((pos = strrchr(path, '/')) != NULL)
	{
		pos--;
		if ((pos - path) < dstSize)
		{
			memcpy(dst, path, pos - path);
			dst[pos - path] = '\0';
		}
		else
		{
			Com_Printf("Com_FilePath: not enough space.\n");
			return;
		}
	}
	else
	{
		strncpy(dst, path, dstSize);
	}
}

/*
================
FS_filelength
================
*/
int FS_filelength (FILE *f)
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
		Com_Printf("WARNING: refusing to create relative path '%s'.\n", path);
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
==============
FS_FCloseFile

For some reason, other dll's can't just cal fclose()
on files returned by FS_FOpenFile...
==============
*/
void FS_FCloseFile(FILE *f)
{
	fclose(f);
}

/*
===========
FS_FOpenFile

Finds the file in the search path.
returns filesize and an open FILE *
Used for streaming data out of either a pak file or
a seperate file.
===========
*/
int file_from_pak = 0;

int pakfilecmpfnc (packfile_t *a, packfile_t *b)
{
	return Q_stricmp (a->name, b->name);
}

packfile_t *FS_FindFileInPak (packfile_t *files, int numfiles, char *filename)
{
	int imid, comp;
	int imin = 0;
	int imax = numfiles - 1;

	while (1)
	{
		if (imax < imin) break;

		imid = (imax + imin) >> 1;
		comp = Q_stricmp (files[imid].name, filename);

		if (comp < 0)
			imin = imid + 1;
		else if (comp > 0)
			imax = imid - 1;
		else return &files[imid];
	}

	// not found
	return NULL;
}


int FS_FOpenFile (char *filename, FILE **file)
{
	searchpath_t	*search;
	char			netpath[MAX_OSPATH];
	pack_t			*pak;

	file_from_pak = 0;

	// search through the path, one element at a time
	for (search = fs_searchpaths; search; search = search->next)
	{
		// is the element a pak file?
		if (search->pack)
		{
			packfile_t *found = NULL;

			// HACK: for maps.lst and players/, we do not want to search the pak, as most player
			// models are stored outside the pak and id put an updated maps.lst outside the paks in the 3.14 update
			if ((strcmp(filename, "maps.lst") == 0) || (strncmp(filename, "players/", 8) == 0))
				continue;

			// look through all the pak file elements
			pak = search->pack;

			// this works because the name of the file is the first element in the packfile_t struct
			if ((found = FS_FindFileInPak (pak->files, pak->numfiles, filename)) != NULL)
			{
				// found it!
				file_from_pak = 1;
				Com_DPrintf ("PackFile: %s : %s for %s\n", pak->filename, found->name, filename);

				// open a new file on the pakfile
				*file = fopen (pak->filename, "rb");

				if (!*file)
					Com_Error (ERR_FATAL, "Couldn't reopen %s", pak->filename);

				fseek (*file, found->filepos, SEEK_SET);
				return found->filelen;
			}
		}
		else
		{
			// check a file in the directory tree
			Com_sprintf (netpath, sizeof (netpath), "%s/%s", search->filename, filename);

			*file = fopen (netpath, "rb");
			if (!*file)
				continue;

			Com_DPrintf ("FindFile: %s\n", netpath);

			return FS_filelength (*file);
		}
	}

	Com_DPrintf ("FindFile: can't find %s\n", filename);

	*file = NULL;
	return -1;
}

/*
=================
FS_ReadFile

Properly handles partial reads
=================
*/
#define	MAX_READ	0x10000		// read in blocks of 64k
int FS_Read (void *buffer, int len, FILE *f)
{
	size_t	block, total;
	int		read;
	byte	*buf;

	if (!len || !buffer)
		return 0;

	buf = (byte *) buffer;

	// read in chunks
	total = 0;
	do
	{
		block = min (len, MAX_READ);

		read = fread (buf, 1, block, f);
		if (read == 0)
		{
			// try again
			read = fread (buf, 1, block, f);
			if (read == 0)
				read = -1;
		}

//		if (read == -1)
//			Com_Error (ERR_FATAL, "FS_Read: could not read %i bytes", block);

		len -= block;
		buf += block;
		total += block;
	} while (len > 0);

	return total;
}

/*
============
FS_LoadFile

Filename are reletive to the quake search path
a null buffer will just return the file length without loading
============
*/
int FS_LoadFile (char *path, void **buffer)
{
	FILE	*h;
	byte	*buf;
	int		len;

	buf = NULL;	// quiet compiler warning

	// look for it in the filesystem or pack files
	len = FS_FOpenFile (path, &h);

	if (!h)
	{
		if (buffer)
			*buffer = NULL;

		return -1;
	}

	if (!buffer)
	{
		fclose (h);
		return len;
	}

	buf = malloc (len);
	*buffer = buf;

	FS_Read (buf, len, h);

	fclose (h);

	return len;
}

/*
=============
FS_FreeFile
=============
*/
void FS_FreeFile (void *buffer)
{
	if (buffer == NULL)
	{
		Com_DPrintf("FS_FreeFile: NULL buffer.\n");
		return;
	}
	free (buffer);
}

/*
=================
FS_LoadPackFile

Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
=================
*/
pack_t *FS_LoadPackFile (char *packfile)
{
	dpackheader_t	header;
	int				i;
	packfile_t		*newfiles;
	int				numpackfiles;
	pack_t			*pack;
	FILE			*packhandle;
	dpackfile_t		info[MAX_FILES_IN_PACK];
	unsigned		checksum;

	packhandle = fopen (packfile, "rb");

	if (!packhandle)
		return NULL;

	fread (&header, 1, sizeof (header), packhandle);

	if (LittleLong (header.ident) != IDPAKHEADER)
		Com_Error (ERR_FATAL, "%s is not a packfile", packfile);

	header.dirofs = LittleLong (header.dirofs);
	header.dirlen = LittleLong (header.dirlen);

	numpackfiles = header.dirlen / sizeof (dpackfile_t);

	if (numpackfiles > MAX_FILES_IN_PACK)
		Com_Error (ERR_FATAL, "%s has %i files", packfile, numpackfiles);

	newfiles = Z_Malloc (numpackfiles * sizeof (packfile_t));

	fseek (packhandle, header.dirofs, SEEK_SET);
	fread (info, 1, header.dirlen, packhandle);

	// crc the directory to check for modifications
	checksum = Com_BlockChecksum ((void *) info, header.dirlen);

	// parse the directory
	for (i = 0; i < numpackfiles; i++)
	{
		strcpy (newfiles[i].name, info[i].name);
		Q_strlwr (newfiles[i].name);
		newfiles[i].filepos = LittleLong (info[i].filepos);
		newfiles[i].filelen = LittleLong (info[i].filelen);
	}

	pack = Z_Malloc (sizeof (pack_t));
	strcpy (pack->filename, packfile);
	pack->handle = packhandle;
	pack->numfiles = numpackfiles;
	pack->files = newfiles;

	// sort the pack files so that we can search in them faster
	qsort (pack->files, pack->numfiles, sizeof (packfile_t), (int (*) (const void *, const void *)) pakfilecmpfnc);

	Com_Printf ("Added packfile %s (%i files)\n", packfile, numpackfiles);
	return pack;
}


/*
================
FS_AddGameDirectory

Sets fs_gamedir, adds the directory to the head of the path,
then loads and adds pak1.pak pak2.pak ...
================
*/
void FS_AddGameDirectory (char *dir)
{
	int				i;
	searchpath_t	*search;
	pack_t			*pak;
	char			pakfile[MAX_OSPATH];

	strcpy (fs_gamedir, dir);

	//
	// add the directory to the search path
	//
	search = Z_Malloc (sizeof (searchpath_t));
	strcpy (search->filename, dir);
	search->next = fs_searchpaths;
	fs_searchpaths = search;

	//
	// add any pak files in the format pak0.pak pak1.pak, ...
	//
	for (i = 0; i < 10; i++)
	{
		Com_sprintf (pakfile, sizeof (pakfile), "%s/pak%i.pak", dir, i);
		pak = FS_LoadPackFile (pakfile);

		if (!pak)
			continue;

		search = Z_Malloc (sizeof (searchpath_t));
		search->pack = pak;
		search->next = fs_searchpaths;
		fs_searchpaths = search;
	}
}

/*
============
FS_ExistsInGameDir

See if a file exists in the mod directory/paks (ignores baseq2)
============
*/
qboolean FS_ExistsInGameDir(char *filename)
{
	const searchpath_t	*search, *end;
	char				netpath[MAX_OSPATH];
	const pack_t		*pak;
	const packfile_t	*pakfile;
	FILE				*file;

	if (fs_searchpaths != fs_base_searchpaths)
	{
		end = fs_base_searchpaths;
	}
	else
	{
		end = NULL;
	}
	
	for (search = fs_searchpaths; search != end; search = search->next)
	{
		// is the element a pak file?
		if (search->pack)
		{
			pak = search->pack;

			if ((pakfile = FS_FindFileInPak(pak->files, pak->numfiles, filename)) != NULL)
			{
				// found it!
				Com_DPrintf("PackFile: %s : %s for %s\n", pak->filename, pakfile->name, filename);
				return true;
			}
		}
		else
		{
			// check a file in the directory tree
			Com_sprintf(netpath, sizeof(netpath), "%s/%s", search->filename, filename);

			file = fopen(netpath, "rb");
#ifndef _WIN32
			if (!file)
			{
				Q_strlwr(netpath);
				file = fopen(netpath, "rb");
			}
#endif
			if (file)
			{
				fclose(file);
				return true;
			}
		}
	}

	return false;
}

/*
================
FS_NextPath

Allows enumerating all of the directories in the search path
================
*/
char *FS_NextPath(char *prevpath)
{
	searchpath_t	*s;
	char			*prev;

	if (!prevpath)
		return fs_gamedir;

	prev = fs_gamedir;

	for (s = fs_searchpaths; s; s = s->next)
	{
		if (s->pack)
			continue;

		if (prevpath == prev)
			return s->filename;

		prev = s->filename;
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
	searchpath_t *s;

	Com_Printf("Current search path:\n");

	for (s = fs_searchpaths; s; s = s->next)
	{
		if (s == fs_base_searchpaths)
			Com_Printf("----------\n");

		if (s->pack)
			Com_Printf("%s (%i files)\n", s->pack->filename, s->pack->numfiles);
		else
			Com_Printf("%s\n", s->filename);
	}
}

/*
================
FS_SetGamedir

Sets the gamedir and path to a different directory.
================
*/
void FS_SetGamedir(char *dir)
{
	searchpath_t	*next;

	if (strstr(dir, "..") || strstr(dir, "/") || strstr(dir, "\\") || strstr(dir, ":"))
	{
		Com_Printf("Gamedir should be a single filename, not a path\n");
		return;
	}

	//
	// free up any current game dir info
	//
	while (fs_searchpaths != fs_base_searchpaths)
	{
		if (fs_searchpaths->pack)
		{
			fclose(fs_searchpaths->pack->handle);
			Z_Free(fs_searchpaths->pack->files);
			Z_Free(fs_searchpaths->pack);
		}

		next = fs_searchpaths->next;
		Z_Free(fs_searchpaths);
		fs_searchpaths = next;
	}

	//
	// flush all data, so it will be forced to reload
	//
	if (dedicated && !dedicated->value)
		Cbuf_AddText("vid_restart\nsnd_restart\n");

	Com_sprintf(fs_gamedir, sizeof(fs_gamedir), "%s/%s", fs_basedir->string, dir);

	if (!strcmp(dir, BASEDIRNAME) || (*dir == 0))
	{
		Cvar_FullSet("gamedir", "", CVAR_SERVERINFO | CVAR_NOSET);
		Cvar_FullSet("game", "", CVAR_LATCH | CVAR_SERVERINFO);
	}
	else
	{
		Cvar_FullSet("gamedir", dir, CVAR_SERVERINFO | CVAR_NOSET);

		FS_AddGameDirectory(va("%s/%s", fs_basedir->string, dir));
	}
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

	s = Sys_FindFirst(findname);

	while (s)
	{
		if (s[strlen(s) - 1] != '.')
			nfiles++;

		s = Sys_FindNext();
	}

	Sys_FindClose();

	if (!nfiles)
		return NULL;

	nfiles++; // add space for a guard
	*numfiles = nfiles;

	list = malloc(sizeof(char *) * nfiles);
	memset(list, 0, sizeof(char *) * nfiles);

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

	return (retval);
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
	searchpath_t	*search;
	int				i, j;
	int				nfiles;
	int				tmpnfiles;
	char			**tmplist;
	char			**list;
	char			path[MAX_OSPATH];

	nfiles = 0;
	list = malloc(sizeof(char *));

	for (search = fs_searchpaths; search != NULL; search = search->next)
	{
		if (search->pack != NULL)
		{
			// search pak files
			for (i = 0, j = 0; i < search->pack->numfiles; i++)
			{
				if (ComparePackFiles(findname, search->pack->files[i].name, NULL, 0))
					j++;
			}
			if (j == 0)
				continue;
			nfiles += j;
			list = realloc(list, nfiles * sizeof(char *));
			for (i = 0, j = nfiles - j; i < search->pack->numfiles; i++)
			{
				if (ComparePackFiles(findname, search->pack->files[i].name,	path, sizeof(path)))
					list[j++] = strdup(path);
			}
		}
		else if (search->filename != NULL)
		{
			// search base filesystem
			Com_sprintf(path, sizeof(path), "%s/%s", search->filename, findname);
			tmplist = FS_ListFiles(path, &tmpnfiles);
			if (tmplist != NULL)
			{
				tmpnfiles--;
				nfiles += tmpnfiles;
				list = realloc(list, nfiles * sizeof(char *));
				for (i = 0, j = nfiles - tmpnfiles;	i < tmpnfiles; i++, j++)
					list[j] = strdup(tmplist[i] + strlen(search->filename) + 1);
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
FS_FreeFileList
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
FS_Dir_f
===========
*/
void FS_Dir_f(void)
{
	char	*path = NULL;
	char	findname[1024];
	char	wildcard[1024] = "*.*";
	char	**dirnames;
	int		ndirs;

	if (Cmd_Argc() != 1)
	{
		strcpy(wildcard, Cmd_Argv(1));
	}

	while ((path = FS_NextPath(path)) != NULL)
	{
		char *tmp = findname;

		Com_sprintf(findname, sizeof(findname), "%s/%s", path, wildcard);

		while (*tmp != 0)
		{
			if (*tmp == '\\')
				*tmp = '/';

			tmp++;
		}

		Com_Printf("Directory of %s\n", findname);
		Com_Printf("----\n");

		if ((dirnames = FS_ListFiles(findname, &ndirs)) != 0)
		{
			int i;

			for (i = 0; i < ndirs - 1; i++)
			{
				if (strrchr(dirnames[i], '/'))
					Com_Printf("%s\n", strrchr(dirnames[i], '/') + 1);
				else
					Com_Printf("%s\n", dirnames[i]);

				free(dirnames[i]);
			}

			free(dirnames);
		}

		Com_Printf("\n");
	};
}

/*
================
FS_InitFilesystem
================
*/
void FS_InitFilesystem(void)
{
	Cmd_AddCommand("path", FS_Path_f);
	Cmd_AddCommand("dir", FS_Dir_f);

	//
	// basedir <path>
	// allows the game to run from outside the data tree
	//
	fs_basedir = Cvar_Get("basedir", ".", CVAR_NOSET);

	//
	// start up with baseq2 by default
	//
	FS_AddGameDirectory(va("%s/"BASEDIRNAME, fs_basedir->string));

	// any set gamedirs will be freed up to here
	fs_base_searchpaths = fs_searchpaths;

	// check for game override
	fs_gamedirvar = Cvar_Get("game", "", CVAR_LATCH | CVAR_SERVERINFO);

	if (fs_gamedirvar->string[0])
		FS_SetGamedir(fs_gamedirvar->string);
}


/* The following FS_*() stdio replacements are necessary if one is
* to perform non-sequential reads on files reopened on pak files
* because we need the bookkeeping about file start/end positions.
* Allocating and filling in the fshandle_t structure is the users'
* responsibility when the file is initially opened. */

size_t FS_fread(void *ptr, size_t size, size_t nmemb, fshandle_t *fh)
{
	long byte_size;
	long bytes_read;
	size_t nmemb_read;

	if (!fh) {
		errno = EBADF;
		return 0;
	}
	if (!ptr) {
		errno = EFAULT;
		return 0;
	}
	if (!size || !nmemb) {	/* no error, just zero bytes wanted */
		errno = 0;
		return 0;
	}

	byte_size = nmemb * size;
	if (byte_size > fh->length - fh->pos)	/* just read to end */
		byte_size = fh->length - fh->pos;
	bytes_read = fread(ptr, 1, byte_size, fh->file);
	fh->pos += bytes_read;

	/* fread() must return the number of elements read,
	* not the total number of bytes. */
	nmemb_read = bytes_read / size;
	/* even if the last member is only read partially
	* it is counted as a whole in the return value. */
	if (bytes_read % size)
		nmemb_read++;

	return nmemb_read;
}

int FS_fseek(fshandle_t *fh, long offset, int whence)
{
	/* I don't care about 64 bit off_t or fseeko() here.
	* the quake/hexen2 file system is 32 bits, anyway. */
	int ret;

	if (!fh) {
		errno = EBADF;
		return -1;
	}

	/* the relative file position shouldn't be smaller
	* than zero or bigger than the filesize. */
	switch (whence)
	{
	case SEEK_SET:
		break;
	case SEEK_CUR:
		offset += fh->pos;
		break;
	case SEEK_END:
		offset = fh->length + offset;
		break;
	default:
		errno = EINVAL;
		return -1;
	}

	if (offset < 0) {
		errno = EINVAL;
		return -1;
	}

	if (offset > fh->length)	/* just seek to end */
		offset = fh->length;

	ret = fseek(fh->file, fh->start + offset, SEEK_SET);
	if (ret < 0)
		return ret;

	fh->pos = offset;
	return 0;
}

int FS_fclose(fshandle_t *fh)
{
	if (!fh) {
		errno = EBADF;
		return -1;
	}
	return fclose(fh->file);
}

long FS_ftell(fshandle_t *fh)
{
	if (!fh) {
		errno = EBADF;
		return -1;
	}
	return fh->pos;
}

void FS_rewind(fshandle_t *fh)
{
	if (!fh) return;
	clearerr(fh->file);
	fseek(fh->file, fh->start, SEEK_SET);
	fh->pos = 0;
}

int FS_feof(fshandle_t *fh)
{
	if (!fh) {
		errno = EBADF;
		return -1;
	}
	if (fh->pos >= fh->length)
		return -1;
	return 0;
}

int FS_ferror(fshandle_t *fh)
{
	if (!fh) {
		errno = EBADF;
		return -1;
	}
	return ferror(fh->file);
}

int FS_fgetc(fshandle_t *fh)
{
	if (!fh) {
		errno = EBADF;
		return EOF;
	}
	if (fh->pos >= fh->length)
		return EOF;
	fh->pos += 1;
	return fgetc(fh->file);
}

char *FS_fgets(char *s, int size, fshandle_t *fh)
{
	char *ret;

	if (FS_feof(fh))
		return NULL;

	if (size > (fh->length - fh->pos) + 1)
		size = (fh->length - fh->pos) + 1;

	ret = fgets(s, size, fh->file);
	fh->pos = ftell(fh->file) - fh->start;

	return ret;
}

long FS_ffilelength(fshandle_t *fh)
{
	if (!fh) {
		errno = EBADF;
		return -1;
	}
	return fh->length;
}
