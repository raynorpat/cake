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

#include "client.h"

typedef enum {
	PRECACHE_MODELS,
	PRECACHE_OTHER,
	PRECACHE_MAP,
	PRECACHE_FINAL
} precache_t;

static precache_t precache_check;
static int precache_sexed_sounds[MAX_SOUNDS];
static int precache_sexed_total;

extern cvar_t *allow_download;
extern cvar_t *allow_download_players;
extern cvar_t *allow_download_models;
extern cvar_t *allow_download_sounds;
extern cvar_t *allow_download_maps;
extern cvar_t *allow_download_pics;
extern cvar_t *allow_download_textures;

/*
===============
CL_QueueDownload

Called from the precache check to queue a download
===============
*/
qboolean CL_QueueDownload (char *quakePath, dltype_t type)
{
	size_t		len;
	dlqueue_t	*q;

	q = &cls.download.queue;
	while (q->next)
	{
		q = q->next;

		// avoid sending duplicate requests
		if (!strcmp(quakePath, q->path))
			return true;
	}

	len = strlen(quakePath);
	if (len >= MAX_QPATH)
		Com_Error(ERR_DROP, "%s: oversize quake path", __func__);

	q->next = Z_TagMalloc(sizeof(*q) + len, 0);
	q = q->next;

	q->next = NULL;
	memcpy(q->path, quakePath, len + 1);
	q->type = type;
	q->state = DL_PENDING;

	// if a download entry has made it this far, CL_FinishDownload is guaranteed to be called.
	cls.download.pending++;

	return true;
}

/*
===============
CL_FinishDownload

Mark the queue entry as done, decrementing pending count.
===============
*/
void CL_FinishDownload (dlqueue_t *q)
{
	q->state = DL_DONE;
	cls.download.pending--;
}

/*
===============
CL_CleanupDownloads

Disconnected from server, clean up.
===============
*/
void CL_CleanupDownloads (void)
{
//	dlqueue_t *q, *last;

	// cleanup any http downloads
	CL_CleanupHTTPDownloads ();

#if 0
	last = NULL;
	q = &cls.download.queue;
	while (q->next)
	{
		q = q->next;
		if (last)
			Z_Free (last);
		last = q;
	}

	if (last)
		Z_Free (last);
#endif

	if (cls.download.file)
	{
		fclose(cls.download.file);
		cls.download.file = 0;
	}

	memset (&cls.download, 0, sizeof(cls.download));

	cls.download.pending = 0;

	cls.download.current = NULL;
	cls.download.percent = 0;
	cls.download.position = 0;

	cls.download.name[0] = 0;
	cls.download.tempname[0] = 0;
}

static qboolean CL_StartUDPDownload (dlqueue_t *q)
{
	char name[MAX_OSPATH];
	size_t len;
	FILE *fp;

	len = strlen(q->path);
	if (len >= MAX_QPATH)
		Com_Error(ERR_DROP, "%s: oversize quake path", __func__);

	// download to a temp name, and only rename
	// to the real name when done, so if interrupted a runt file wont be left
	memcpy(cls.download.tempname, q->path, len);
	memcpy(cls.download.tempname + len, ".tmp", 5);

	strcpy(cls.download.name, q->path);

	// check to see if we already have a tmp for this file, if so, try to resume
	// open the file if not opened yet
	Com_sprintf(name, sizeof(name), "%s/%s", FS_Gamedir(), cls.download.tempname);
	fp = fopen(name, "r+b");
	if (fp) // it exists
	{
		int len;
		fseek (fp, 0, SEEK_END);
		len = ftell (fp);

		cls.download.file = fp;
		cls.download.position = len;

		// give the server an offset to start the download
		Com_Printf (S_COLOR_GREEN "[UDP]" S_COLOR_WHITE " Resuming %s\n", q->path);
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, va("download %s %i", q->path, len));
	}
	else
	{
		Com_Printf (S_COLOR_GREEN "[UDP]" S_COLOR_WHITE " Downloading %s\n", q->path);
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, va("download %s", q->path));
	}

	cls.forcePacket = true;

	q->state = DL_RUNNING;
	cls.download.current = q;
	return true;
}

/*
===============
CL_StartNextDownload

Start another UDP download if possible
===============
*/
void CL_StartNextDownload (void)
{
	dlqueue_t *q;

	if (!cls.download.pending || cls.download.current)
		return;

	q = &cls.download.queue;
	while (q->next)
	{
		q = q->next;
		if (q->state == DL_PENDING)
		{
			if (CL_StartUDPDownload(q))
				break;
		}
	}
}

/*
=====================
CL_FinishUDPDownload

Cleans up current download and asks for more
=====================
*/
static void CL_FinishUDPDownload (char *msg)
{
	dlqueue_t *q = cls.download.current;

	// finished with current path
	CL_FinishDownload (q);

	cls.download.current = NULL;
	cls.download.percent = 0;
	cls.download.position = 0;

	if (cls.download.file)
	{
		fclose (cls.download.file);
		cls.download.file = 0;
	}

	cls.download.name[0] = 0;
	cls.download.tempname[0] = 0;

	if (msg)
		Com_Printf(S_COLOR_GREEN "[UDP]" S_COLOR_WHITE " [%s] %s [%d remaining file%s]\n", q->path, msg, cls.download.pending,	cls.download.pending == 1 ? "" : "s");

	// get another file if needed
	CL_RequestNextDownload ();
	CL_StartNextDownload ();
}

/*
=====================
CL_WriteUDPDownload

Writes the UDP download out via fwrite
=====================
*/
static int CL_WriteUDPDownload (byte *data, int size)
{
	size_t ret;

	ret = fwrite(data, 1, size, cls.download.file);
	if (ret != size)
	{
		Com_Printf(S_COLOR_GREEN "[UDP]" S_COLOR_RED " Couldn't write %s.\n", cls.download.tempname);
		CL_FinishUDPDownload (NULL);
		return -1;
	}

	return 0;
}

/*
=====================
CL_HandleDownload

A download data packet has been received from the server
=====================
*/
void CL_HandleDownload (byte *data, int size, int percent)
{
	char name[MAX_OSPATH];
	dlqueue_t *q = cls.download.current;

	if (!q)
		Com_Error(ERR_DROP, "%s: no download requested", __func__);

	if (size == -1)
	{
		if (!percent)
			CL_FinishUDPDownload ("FAIL");
		else
			CL_FinishUDPDownload ("STOP");
		return;
	}
	
	// open the file if not opened yet
	if (!cls.download.file)
	{
		Com_sprintf (name, sizeof(name), "%s/%s", FS_Gamedir(), cls.download.tempname);

		FS_CreatePath (name);

		cls.download.file = fopen(name, "wb");
		if (!cls.download.file)
		{
			CL_FinishUDPDownload (NULL);
			return;
		}
	}

	if (CL_WriteUDPDownload(data, size))
		return;
	
	if (percent != 100)
	{
		// request next block
		cls.download.percent = percent;
		cls.download.position += size;

		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		SZ_Print (&cls.netchan.message, "nextdl");
		cls.forcePacket = true;
	}
	else
	{
		char oldn[MAX_OSPATH];
		char newn[MAX_OSPATH];

		// close the file before renaming
		fclose (cls.download.file);
		cls.download.file = 0;

		Com_sprintf(oldn, sizeof(oldn), "%s/%s", FS_Gamedir(), cls.download.tempname);
		Com_sprintf(newn, sizeof(newn), "%s/%s", FS_Gamedir(), q->path);

		// rename the temp file to it's final name
		FS_RenameFile (oldn, newn);

		CL_FinishUDPDownload ("DONE");
	}
}

/*
===============
CL_CheckDownloadExtension

Only predefined set of filename extensions is allowed,
to prevent the server from uploading arbitrary files.
===============
*/
qboolean CL_CheckDownloadExtension (char *ext)
{
	static char allowed[][4] = {
		"pcx", "wal", "wav", "md2", "sp2", "tga", "png",
		"jpg", "bsp", "ent", "txt", "dm2", "loc"
	};
	static int total = sizeof(allowed) / sizeof(allowed[0]);
	int i;

	for (i = 0; i < total; i++)
	{
		if (!Q_stricmp(ext, allowed[i]))
			return true;
	}

	return false;
}

/*
===============
CL_CheckOrDownloadFile

Returns true if the file exists, otherwise it attempts
to start a download from the server.
===============
*/
static qboolean CL_CheckOrDownloadFile (char *filename, dltype_t type)
{
	char	*p;
	char	*ext;

	// fix backslashes
	while ((p = strchr(filename, '\\')))
		*p = '/';

	if (FS_LoadFile(filename, NULL) != -1)
	{
		// it exists, no need to download
		return true;
	}

	// make sure path is valid
	if (strstr(filename, "..") || strstr(filename, ":") || (*filename == '.') || (*filename == '/'))
	{
		Com_Printf(S_COLOR_RED "Refusing to download a path with ..: %s\n", filename);
		return true;
	}

	// make sure extension is valid
	ext = COM_FileExtension(filename);
	if (!CL_CheckDownloadExtension(ext))
	{
		Com_Printf(S_COLOR_RED "Refusing to download file with invalid extension.\n");
		return true;
	}

	// queue and start HTTP download
	if (CL_QueueHTTPDownload(filename, type))
		return true;

	// queue and start UDP download
	if (CL_QueueDownload(filename, type))
		CL_StartNextDownload();

	return false;
}

/*
=================
CL_DownloadsPending

for precaching dependencies
=================
*/
static qboolean CL_DownloadsPending (dltype_t type)
{
	dlqueue_t *q;

	// DL_OTHER just checks for any download
	if (type == DL_OTHER)
		return !!cls.download.pending;

	// see if there are pending downloads of the given type
	q = &cls.download.queue;
	while (q->next)
	{
		q = q->next;
		if (q->state != DL_DONE && q->type == type)
			return true;
	}

	return false;
}

/*
=================
CL_Download_CheckSkins

Checks models and model skins
=================
*/
static void CL_Download_CheckSkins (char *name)
{
	size_t i, num_skins, ofs_skins, end_skins;
	dmdl_t *md2header;
	char *md2skin;
	dsprite_t *sp2header;
	dsprframe_t *sp2frame;
	byte *model;
	uint32_t ident;
	size_t length;
	char fn[MAX_QPATH];

	length = FS_LoadFile (name, (void **)&model);
	if (!model)
	{
		// couldn't load it
		return;
	}
	
	if (length < sizeof(ident))
	{
		// file too small
		goto done;
	}

	// check ident
	ident = LittleLong(*(uint32_t *)model);
	switch (ident)
	{
		case IDALIASHEADER:
			// alias model
			md2header = (dmdl_t *)model;
			if (LittleLong(md2header->version) != ALIAS_VERSION)
			{
				// not an alias model
				goto done;
			}

			num_skins = LittleLong (md2header->num_skins);
			ofs_skins = LittleLong (md2header->ofs_skins);
			end_skins = ofs_skins + num_skins * MAX_SKINNAME;
			if (num_skins > MAX_MD2SKINS || end_skins < ofs_skins || end_skins > length)
			{
				// bad alias model
				goto done;
			}

			md2skin = (char *)model + ofs_skins;
			for (i = 0; i < num_skins; i++)
			{
				if (!Q_memccpy(fn, md2skin, 0, sizeof(fn)))
				{
					// bad alias model
					goto done;
				}

				CL_CheckOrDownloadFile (fn, DL_OTHER);
				md2skin += MAX_SKINNAME;
			}
			break;

		case IDSPRITEHEADER:
			// sprite model
			sp2header = (dsprite_t *)model;
			if (LittleLong(sp2header->version) != SPRITE_VERSION)
			{
				// not a sprite model
				goto done;
			}

			num_skins = LittleLong (sp2header->numframes);
			ofs_skins = sizeof(*sp2header);
			end_skins = ofs_skins + num_skins * sizeof(dsprframe_t);
			if (num_skins > SP2_MAX_FRAMES || end_skins < ofs_skins || end_skins > length)
			{
				// bad sprite model
				goto done;
			}

			sp2frame = (dsprframe_t *)(model + ofs_skins);
			for (i = 0; i < num_skins; i++)
			{
				if (!Q_memccpy(fn, sp2frame->name, 0, sizeof(fn)))
				{
					// bad sprite model
					goto done;
				}
				CL_CheckOrDownloadFile (fn, DL_OTHER);
				sp2frame++;
			}
			break;

		default:
			// unknown file format
			goto done;
	}

done:
	FS_FreeFile (model);
}

/*
=================
CL_Download_CheckPlayer

Checks player model, weapon models, skins, and sexed soudns
=================
*/
static void CL_Download_CheckPlayer (char *name)
{
	char fn[MAX_QPATH], model[MAX_QPATH], skin[MAX_QPATH], *p;
	size_t len;
	int i, j;

	CL_ParsePlayerSkin(NULL, model, skin, name);

	// model
	len = Q_concat (fn, sizeof(fn), "players/", model, "/tris.md2", NULL);
	CL_CheckOrDownloadFile (fn, DL_OTHER);

	// weapon models
	for (i = 0; i < num_cl_weaponmodels; i++)
	{
		len = Q_concat (fn, sizeof(fn), "players/", model, "/", cl_weaponmodels[i], NULL);
		CL_CheckOrDownloadFile (fn, DL_OTHER);
	}

	// default weapon skin
	len = Q_concat (fn, sizeof(fn), "players/", model, "/weapon.pcx", NULL);
	CL_CheckOrDownloadFile (fn, DL_OTHER);

	// skin
	len = Q_concat (fn, sizeof(fn), "players/", model, "/", skin, ".pcx", NULL);
	CL_CheckOrDownloadFile (fn, DL_OTHER);

	// skin_i
	len = Q_concat (fn, sizeof(fn), "players/", model, "/", skin, "_i.pcx", NULL);
	CL_CheckOrDownloadFile (fn, DL_OTHER);

	// sexed sounds
	for (i = 0; i < precache_sexed_total; i++)
	{
		j = precache_sexed_sounds[i];
		p = cl.configstrings[CS_SOUNDS + j];
		if (*p == '*')
		{
			len = Q_concat (fn, sizeof(fn), "players/", model, "/", p + 1, NULL);
			CL_CheckOrDownloadFile (fn, DL_OTHER);
		}
	}
}

/*
=================
CL_RequestNextDownload

Runs precache check and dispatches downloads
=================
*/
void CL_RequestNextDownload (void)
{
	unsigned map_checksum; // for detecting cheater maps
	char fn[MAX_OSPATH], *name;
	size_t len;
	int i;

	if (cls.state != ca_connected)
		return;

	// if downloads are disabled or running locally, skip downloading
	if (!allow_download->integer || NET_IsLocalAddress(cls.netchan.remote_address))
	{
		if (precache_check <= PRECACHE_MAP)
		{
			CM_LoadMap(cl.configstrings[CS_MODELS + 1], true, &map_checksum);
			if (map_checksum != atoi(cl.configstrings[CS_MAPCHECKSUM]))
			{
				Com_Error (ERR_DROP, "Local map version differs from server: %i != '%s'\n", map_checksum, cl.configstrings[CS_MAPCHECKSUM]);
				return;
			}
		}

		CL_Begin ();
		return;
	}

	switch (precache_check)
	{
		case PRECACHE_MODELS:
			// confirm map
			if (allow_download_maps->value)
				CL_CheckOrDownloadFile (cl.configstrings[CS_MODELS + 1], DL_MAP);
	
			// checking for models
			if (allow_download_models->integer)
			{
				for (i = 2; i < MAX_MODELS; i++)
				{
					name = cl.configstrings[CS_MODELS + i];
					if (!name[0])
						break;
					if (name[0] == '*' || name[0] == '#')
						continue;
					CL_CheckOrDownloadFile (name, DL_MODEL);
				}
			}

			precache_check = PRECACHE_OTHER;
			// fall through

		case PRECACHE_OTHER:
			if (allow_download_models->integer)
			{
				if (CL_DownloadsPending(DL_MODEL))
				{
					// map might still be downloading?
					Com_DPrintf ("%s: waiting for models...\n", __func__);
					return;
				}

				for (i = 2; i < MAX_MODELS; i++)
				{
					name = cl.configstrings[CS_MODELS + i];
					if (!name[0])
						break;
					if (name[0] == '*' || name[0] == '#')
						continue;
					CL_Download_CheckSkins (name);
				}
			}

			if (allow_download_sounds->integer)
			{
				for (i = 1; i < MAX_SOUNDS; i++)
				{
					name = cl.configstrings[CS_SOUNDS + i];
					if (!name[0])
						break;

					if (name[0] == '*')
						continue;

					if (name[0] == '#')
						len = Q_strlcpy(fn, name + 1, sizeof(fn));
					else
						len = Q_concat(fn, sizeof(fn), "sound/", name, NULL);
					CL_CheckOrDownloadFile (fn, DL_OTHER);
				}
			}

			if (allow_download_pics->integer)
			{
				for (i = 1; i < MAX_IMAGES; i++)
				{
					name = cl.configstrings[CS_IMAGES + i];
					if (!name[0])
						break;

					if (name[0] == '/' || name[0] == '\\')
						len = Q_strlcpy (fn, name + 1, sizeof(fn));
					else
						len = Q_concat (fn, sizeof(fn), "pics/", name, ".pcx", NULL);
					CL_CheckOrDownloadFile (fn, DL_OTHER);
				}
			}

			if (allow_download_players->integer)
			{
				// find sexed sounds
				precache_sexed_total = 0;
				for (i = 1; i < MAX_SOUNDS; i++)
				{
					if (cl.configstrings[CS_SOUNDS + i][0] == '*')
						precache_sexed_sounds[precache_sexed_total++] = i;
				}

				for (i = 0; i < MAX_CLIENTS; i++)
				{
					name = cl.configstrings[CS_PLAYERSKINS + i];
					if (!name[0])
						continue;
					CL_Download_CheckPlayer (name);
				}
			}

			if (allow_download_textures->integer)
			{
				static const char env_suf[6][3] = { "rt", "bk", "lf", "ft", "up", "dn" };

				for (i = 0; i < 6; i++)
				{
					len = Q_concat (fn, sizeof(fn), "env/", cl.configstrings[CS_SKY], env_suf[i], ".tga", NULL);
					CL_CheckOrDownloadFile (fn, DL_OTHER);
				}
			}

			precache_check = PRECACHE_MAP;
			// fall through

		case PRECACHE_MAP:
			if (CL_DownloadsPending(DL_MAP))
			{
				// map might still be downloading?
				Com_DPrintf ("%s: waiting for map...\n", __func__);
				return;
			}

			// load the map file before checking textures
			CM_LoadMap(cl.configstrings[CS_MODELS + 1], true, &map_checksum);
			if (map_checksum != atoi(cl.configstrings[CS_MAPCHECKSUM]))
			{
				Com_Error (ERR_DROP, "Local map version differs from server: %i != '%s'\n", map_checksum, cl.configstrings[CS_MAPCHECKSUM]);
				return;
			}

			if (allow_download_textures->integer)
			{
				// from server/sv_cmodel.c
				extern int			numtexinfo;
				extern mapsurface_t	map_surfaces[];

				for (i = 0; i < numtexinfo; i++)
				{
					len = Q_concat (fn, sizeof(fn), "textures/", map_surfaces[i].rname, ".wal", NULL);
					CL_CheckOrDownloadFile (fn, DL_OTHER);
				}
			}

			precache_check = PRECACHE_FINAL;
			// fall through

		case PRECACHE_FINAL:
			if (CL_DownloadsPending(DL_OTHER))
			{
				// pending downloads (possibly textures), let's wait here.
				Com_DPrintf ("%s: waiting for others...\n", __func__);
				return;
			}

			// all done, tell server we are ready
			CL_Begin ();
			break;

		default:
			Com_Error (ERR_DROP, "%s: bad precache_check\n", __func__);
	}
}

/*
=================
CL_ResetPrecacheCheck
=================
*/
void CL_ResetPrecacheCheck(void)
{
	precache_check = PRECACHE_MODELS;
}

/*
===============
CL_Download_f

Request a download from the server
===============
*/
void CL_Download_f(void)
{
	char filename[MAX_OSPATH];

	if (cls.state <= ca_connecting)
	{
		Com_Printf("Not connected.\n");
		return;
	}

	if (Cmd_Argc() != 2)
	{
		Com_Printf("Usage: download <filename>\n");
		return;
	}

	Com_sprintf(filename, sizeof(filename), "%s", Cmd_Argv(1));

	if (CL_CheckOrDownloadFile(filename, DL_OTHER))
		Com_Printf(S_COLOR_RED "Couldn't download %s.\n");
}
