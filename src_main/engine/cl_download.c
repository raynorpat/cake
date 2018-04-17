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

extern cvar_t *allow_download;
extern cvar_t *allow_download_players;
extern cvar_t *allow_download_models;
extern cvar_t *allow_download_sounds;
extern cvar_t *allow_download_maps;

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
CL_RequestNextDownload

Runs precache check and dispatches downloads
=================
*/
int precache_check; // for autodownload of precache items
int precache_tex;
int precache_model_skin;

byte *precache_model; // used for skin checking in alias models

#define PLAYER_MULT 5

// ENV_CNT is map load, ENV_CNT+1 is first env map
#define ENV_CNT (CS_PLAYERSKINS + MAX_CLIENTS * PLAYER_MULT)
#define TEXTURE_CNT (ENV_CNT + 13)

static const char *env_suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};

void CL_RequestNextDownload (void)
{
	unsigned map_checksum; // for detecting cheater maps
	char fn[MAX_OSPATH];

	if (cls.state != ca_connected)
		return;

	if (!allow_download->value && precache_check < ENV_CNT)
		precache_check = ENV_CNT;

	if (precache_check == CS_MODELS)  // confirm map
	{
		precache_check = CS_MODELS + 2; // 0 isn't used

		if (allow_download_maps->value)
			if (!CL_CheckOrDownloadFile (cl.configstrings[CS_MODELS+1], DL_MAP))
				return; // started a download
	}

	if (CL_DownloadsPending(DL_MAP))
	{
		// map might still be downloading?
		Com_DPrintf("%s: waiting for maps...\n", __func__);
		return;
	}

	if (precache_check >= CS_MODELS && precache_check < CS_MODELS + MAX_MODELS)
	{
		if (allow_download_models->value)
		{
			if (precache_model_skin == -1)
			{
				// checking for models
				while (precache_check < CS_MODELS + MAX_MODELS && cl.configstrings[precache_check][0])
				{
					if (cl.configstrings[precache_check][0] == '*' || cl.configstrings[precache_check][0] == '#')
					{
						precache_check++;
						continue;
					}

					if (!CL_CheckOrDownloadFile(cl.configstrings[precache_check], DL_MODEL))
					{
						precache_check++;
						return; // started a download
					}
					precache_check++;
				}

				precache_model_skin = 0;
				precache_check = CS_MODELS + 2; // 0 isn't used
												 
				if (CL_DownloadsPending(DL_MODEL))
				{
					// pending downloads (models), let's wait here before we can check skins.
					Com_DPrintf("%s: waiting for models...\n", __func__);
					return;
				}
			}

			// checking for skins
			while (precache_check < CS_MODELS + MAX_MODELS && cl.configstrings[precache_check][0])
			{
				size_t num_skins, ofs_skins, end_skins;
				dmdl_t *md2header;
				dsprite_t *sp2header;
				dsprframe_t *sp2frame;
				uint32_t ident;
				size_t length;

				if (cl.configstrings[precache_check][0] == '*' || cl.configstrings[precache_check][0] == '#')
				{
					precache_check++;
					continue;
				}

				// checking for skins in the model
				if (!precache_model)
				{
					length = FS_LoadFile (cl.configstrings[precache_check], (void **) &precache_model);
					if (!precache_model)
					{
						precache_model_skin = 0;
						precache_check++;
						continue; // couldn't load it
					}
					if (length < sizeof(ident))
					{
						// file too small
						goto done;
					}

					// check ident
					ident = LittleLong(*(uint32_t *)precache_model);
					switch (ident)
					{
						case IDALIASHEADER:
							// alias model
							md2header = (dmdl_t *)precache_model;
							if (LittleLong(md2header->version) != ALIAS_VERSION)
							{
								// not an alias model
								goto done;
							}

							num_skins = LittleLong(md2header->num_skins);
							ofs_skins = LittleLong(md2header->ofs_skins);
							end_skins = ofs_skins + num_skins * MAX_SKINNAME;
							if (num_skins > MAX_MD2SKINS || end_skins < ofs_skins || end_skins > length)
							{
								// bad alias model
								goto done;
							}
							break;

						case IDSPRITEHEADER:
							// sprite model
							sp2header = (dsprite_t *)precache_model;
							if (LittleLong(sp2header->version) != SPRITE_VERSION)
							{
								// not a sprite model
								goto done;
							}

							num_skins = LittleLong(sp2header->numframes);
							ofs_skins = sizeof(*sp2header);
							end_skins = ofs_skins + num_skins * sizeof(dsprframe_t);
							if (num_skins > SP2_MAX_FRAMES || end_skins < ofs_skins || end_skins > length)
							{
								// bad sprite model
								goto done;
							}
							break;

						default:
							// unknown file format
							goto done;
					}
				}

				// check ident
				ident = LittleLong(*(uint32_t *)precache_model);
				switch (ident)
				{
					case IDALIASHEADER:
						// alias model
						md2header = (dmdl_t *)precache_model;
						num_skins = LittleLong(md2header->num_skins);
						ofs_skins = LittleLong(md2header->ofs_skins);

						while (precache_model_skin < num_skins)
						{
							if (!Q_memccpy(fn, (char *)precache_model + ofs_skins +	precache_model_skin * MAX_SKINNAME, 0, sizeof(fn)))
							{
								// bad alias model
								goto done;
							}

							if (!CL_CheckOrDownloadFile (fn, DL_MODEL))
							{
								precache_model_skin++;
								return; // started a download
							}

							precache_model_skin++;
						}
						break;

					case IDSPRITEHEADER:
						// sprite model
						sp2header = (dsprite_t *)precache_model;
						num_skins = LittleLong(sp2header->numframes);
						ofs_skins = sizeof(*sp2header);

						while (precache_model_skin < num_skins)
						{
							sp2frame = (dsprframe_t *)((byte *)precache_model + ofs_skins) + precache_model_skin;
							if (!Q_memccpy(fn, sp2frame->name, 0, sizeof(fn)))
							{
								// bad sprite model
								goto done;
							}

							if (!CL_CheckOrDownloadFile(fn, DL_MODEL))
							{
								precache_model_skin++;
								return; // started a download
							}

							precache_model_skin++;
						}
						break;

					default:
						// unknown file format
						break;
				}

done:
				FS_FreeFile (precache_model);
				precache_model = 0;
				precache_model_skin = 0;
				precache_check++;
			}
		}

		if (CL_DownloadsPending(DL_MODEL))
		{
			// pending downloads (models), let's wait here before we can check skins.
			Com_DPrintf("%s: waiting for models...\n", __func__);
			return;
		}

		precache_check = CS_SOUNDS;
	}

	if (precache_check >= CS_SOUNDS && precache_check < CS_SOUNDS + MAX_SOUNDS)
	{
		if (allow_download_sounds->value)
		{
			if (precache_check == CS_SOUNDS)
				precache_check++; // zero is blank

			while (precache_check < CS_SOUNDS + MAX_SOUNDS && cl.configstrings[precache_check][0])
			{
				if (cl.configstrings[precache_check][0] == '*')
				{
					precache_check++;
					continue;
				}

				Com_sprintf (fn, sizeof (fn), "sound/%s", cl.configstrings[precache_check++]);

				if (!CL_CheckOrDownloadFile (fn, DL_OTHER))
					return; // started a download
			}
		}

		precache_check = CS_IMAGES;
	}

	if (precache_check >= CS_IMAGES && precache_check < CS_IMAGES + MAX_IMAGES)
	{
		if (precache_check == CS_IMAGES)
			precache_check++; // zero is blank

		while (precache_check < CS_IMAGES + MAX_IMAGES && cl.configstrings[precache_check][0])
		{
			char *picname = cl.configstrings[precache_check++];

			if (*picname == '/' || *picname == '\\')
				Q_strlcpy (fn, picname + 1, sizeof(fn));
			else
				Q_concat (fn, sizeof(fn), "pics/", picname, ".pcx", NULL);

			if (!CL_CheckOrDownloadFile (fn, DL_OTHER))
				return; // started a download
		}

		precache_check = CS_PLAYERSKINS;
	}

	// skins are special, since a player has three things to download:
	// model, weapon model and skin
	// so precache_check is now *3
	if (precache_check >= CS_PLAYERSKINS && precache_check < CS_PLAYERSKINS + MAX_CLIENTS * PLAYER_MULT)
	{
		if (allow_download_players->value)
		{
			while (precache_check < CS_PLAYERSKINS + MAX_CLIENTS * PLAYER_MULT)
			{
				int i, n;
				char model[MAX_QPATH], skin[MAX_QPATH], *p;

				i = (precache_check - CS_PLAYERSKINS) / PLAYER_MULT;
				n = (precache_check - CS_PLAYERSKINS) % PLAYER_MULT;

				if (!cl.configstrings[CS_PLAYERSKINS+i][0])
				{
					precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
					continue;
				}

				if ((p = strchr (cl.configstrings[CS_PLAYERSKINS + i], '\\')) != NULL)
					p++;
				else p = cl.configstrings[CS_PLAYERSKINS + i];

				strcpy (model, p);
				p = strchr (model, '/');

				if (!p)
					p = strchr (model, '\\');

				if (p)
				{
					*p++ = 0;
					strcpy (skin, p);
				}
				else *skin = 0;

				switch (n)
				{
				case 0: // model
					Com_sprintf (fn, sizeof (fn), "players/%s/tris.md2", model);

					if (!CL_CheckOrDownloadFile (fn, DL_MODEL))
					{
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 1;
						return; // started a download
					}

					n++;
					/*FALL THROUGH*/

				case 1: // weapon model
					Com_sprintf (fn, sizeof (fn), "players/%s/weapon.md2", model);

					if (!CL_CheckOrDownloadFile (fn, DL_MODEL))
					{
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 2;
						return; // started a download
					}

					n++;
					/*FALL THROUGH*/

				case 2: // weapon skin
					Com_sprintf (fn, sizeof (fn), "players/%s/weapon.pcx", model);

					if (!CL_CheckOrDownloadFile (fn, DL_OTHER))
					{
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 3;
						return; // started a download
					}

					n++;
					/*FALL THROUGH*/

				case 3: // skin
					Com_sprintf (fn, sizeof (fn), "players/%s/%s.pcx", model, skin);

					if (!CL_CheckOrDownloadFile (fn, DL_OTHER))
					{
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 4;
						return; // started a download
					}

					n++;
					/*FALL THROUGH*/

				case 4: // skin_i
					Com_sprintf (fn, sizeof (fn), "players/%s/%s_i.pcx", model, skin);

					if (!CL_CheckOrDownloadFile (fn, DL_OTHER))
					{
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 5;
						return; // started a download
					}

					// move on to next model
					precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
				}
			}
		}

		// precache phase completed
		precache_check = ENV_CNT;
	}

	if (precache_check == ENV_CNT)
	{
		precache_check = ENV_CNT + 1;

		CM_LoadMap (cl.configstrings[CS_MODELS+1], true, &map_checksum);
		if (map_checksum != atoi (cl.configstrings[CS_MAPCHECKSUM]))
		{
			Com_Error (ERR_DROP, "Local map version differs from server: %i != '%s'\n", map_checksum, cl.configstrings[CS_MAPCHECKSUM]);
			return;
		}
	}

	if (CL_DownloadsPending(DL_MAP))
	{
		// map might still be downloading?
		Com_DPrintf("%s: waiting for maps...\n", __func__);
		return;
	}

	if (precache_check > ENV_CNT && precache_check < TEXTURE_CNT)
	{
		if (allow_download->value && allow_download_maps->value)
		{
			while (precache_check < TEXTURE_CNT)
			{
				int n = precache_check++ - ENV_CNT - 1;

				if (n & 1)
					Com_sprintf (fn, sizeof (fn), "env/%s%s.pcx", cl.configstrings[CS_SKY], env_suf[n / 2]);
				else Com_sprintf (fn, sizeof (fn), "env/%s%s.tga", cl.configstrings[CS_SKY], env_suf[n / 2]);

				if (!CL_CheckOrDownloadFile (fn, DL_OTHER))
					return; // started a download
			}
		}

		precache_check = TEXTURE_CNT;
	}

	if (precache_check == TEXTURE_CNT)
	{
		precache_check = TEXTURE_CNT + 1;
		precache_tex = 0;
	}

	// confirm existance of textures, download any that don't exist
	if (precache_check == TEXTURE_CNT + 1)
	{
		// from server/sv_cmodel.c
		extern int			numtexinfo;
		extern mapsurface_t	map_surfaces[];

		if (allow_download->value && allow_download_maps->value)
		{
			while (precache_tex < numtexinfo)
			{
				char *texname = map_surfaces[precache_tex++].rname;

				Q_concat (fn, sizeof(fn), "textures/", texname, ".wal", NULL);
				if (!CL_CheckOrDownloadFile (fn, DL_OTHER))
					return; // started a download
			}
		}

		precache_check = TEXTURE_CNT + 999;
	}

	if (CL_DownloadsPending(DL_OTHER))
	{
		// pending downloads (models), let's wait here before we can check skins.
		Com_DPrintf("%s: waiting for others...\n", __func__);
		return;
	}

	// all done, tell server we are ready
	CL_Begin();
}

/*
=================
CL_ResetPrecacheCheck
=================
*/
void CL_ResetPrecacheCheck(void)
{
	precache_check = CS_MODELS;
	if (precache_model)
	{
		FS_FreeFile (precache_model);
		precache_model = NULL;
	}
	precache_model_skin = -1;
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

	CL_CheckOrDownloadFile(filename, DL_OTHER);
}
