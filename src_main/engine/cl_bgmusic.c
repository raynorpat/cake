/*
 * Background music handling for Hexen II: Hammer of Thyrion (uHexen2)
 * Handles streaming music as raw sound samples
 *
 * Copyright (C) 1999-2005 Id Software, Inc.
 * Copyright (C) 2010 O.Sezer <sezero@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "client.h"
#include "snd_loc.h"
#include "snd_codec.h"
#include "cl_bgmusic.h"

#define MUSIC_DIRNAME "music"

qboolean bgmloop;

typedef struct music_handler_s
{
	unsigned int	type;	// power of two (snd_codec.h)
	int	is_available;		// -1 means not present
	char *ext;				// Expected file extension
	char *dir;				// Where to look for music file
	struct music_handler_s	*next;
} music_handler_t;
static music_handler_t *music_handlers = NULL;

static music_handler_t wanted_handlers[] =
{
	{ CODECTYPE_MP3,  -1,  ".mp3", MUSIC_DIRNAME, NULL },
	{ CODECTYPE_MOD,  -1,  ".s3m", MUSIC_DIRNAME, NULL },
	{ CODECTYPE_MOD,  -1,  ".xm",  MUSIC_DIRNAME, NULL },
	{ CODECTYPE_MOD,  -1,  ".it",  MUSIC_DIRNAME, NULL },
	{ CODECTYPE_MOD,  -1,  ".mod", MUSIC_DIRNAME, NULL },
	{ CODECTYPE_WAV,  -1,  ".wav", MUSIC_DIRNAME, NULL },
	{ CODECTYPE_NONE, -1,  NULL,   NULL,  NULL }
};
#define ANY_CODECTYPE 0xFFFFFFFF

static snd_stream_t *bgmstream = NULL;

static void BGM_Play_f (void)
{
	if (Cmd_Argc() == 2)
	{
		BGM_Play (Cmd_Argv(1));
	}
	else
	{
		Com_Printf ("music <musicfile>\n");
		return;
	}
}

static void BGM_Pause_f (void)
{
	BGM_Pause ();
}

static void BGM_Resume_f (void)
{
	BGM_Resume ();
}

static void BGM_Loop_f (void)
{
	if (Cmd_Argc() == 2)
	{
		if (Q_strcasecmp(Cmd_Argv(1),  "0") == 0 ||
		    Q_strcasecmp(Cmd_Argv(1),"off") == 0)
			bgmloop = false;
		else if (Q_strcasecmp(Cmd_Argv(1), "1") == 0 ||
		         Q_strcasecmp(Cmd_Argv(1),"on") == 0)
			bgmloop = true;
		else if (Q_strcasecmp(Cmd_Argv(1),"toggle") == 0)
			bgmloop = !bgmloop;
	}

	if (bgmloop)
		Com_Printf("Music will be looped\n");
	else
		Com_Printf("Music will not be looped\n");
}

static void BGM_Stop_f (void)
{
	BGM_Stop();
}

qboolean BGM_Init (void)
{
	music_handler_t *handlers = NULL;
	int i;

	Cmd_AddCommand("music", BGM_Play_f);
	Cmd_AddCommand("music_pause", BGM_Pause_f);
	Cmd_AddCommand("music_resume", BGM_Resume_f);
	Cmd_AddCommand("music_loop", BGM_Loop_f);
	Cmd_AddCommand("music_stop", BGM_Stop_f);

	bgmloop = true;

	for (i = 0; wanted_handlers[i].type != CODECTYPE_NONE; i++)
	{
		wanted_handlers[i].is_available = S_CodecIsAvailable(wanted_handlers[i].type);
		if (wanted_handlers[i].is_available != -1)
		{
			if (handlers)
			{
				handlers->next = &wanted_handlers[i];
				handlers = handlers->next;
			}
			else
			{
				music_handlers = &wanted_handlers[i];
				handlers = music_handlers;
			}
		}
	}

	return true;
}

void BGM_Shutdown (void)
{
	BGM_Stop();

	// sever our connections to snd_codec
	music_handlers = NULL;
}

static void BGM_Play_noext (char *filename, unsigned int allowed_types)
{
	char tmp[MAX_QPATH];
	music_handler_t *handler;

	handler = music_handlers;
	while (handler)
	{
		if (! (handler->type & allowed_types))
		{
			handler = handler->next;
			continue;
		}
		if (!handler->is_available)
		{
			// skip handlers which failed to initialize
			// TODO: implement re-init, make BGM aware of it
			handler = handler->next;
			continue;
		}
		snprintf(tmp, sizeof(tmp), "%s/%s%s", handler->dir, filename, handler->ext);

		bgmstream = S_CodecOpenStreamType(tmp, handler->type);
		if (bgmstream)
			return;	// success

		handler = handler->next;
	}

	Com_Printf("Couldn't handle music file %s\n", filename);
}

void BGM_Play (char *filename)
{
	char tmp[MAX_QPATH];
	char *ext;
	music_handler_t *handler;

	if (!filename || !*filename)
	{
		Com_DPrintf("null music file name\n");
		return;
	}

	BGM_Stop();

	ext = COM_FileExtension(filename);
	if (!ext)	// try all things
	{
		BGM_Play_noext(filename, ANY_CODECTYPE);
		return;
	}

	handler = music_handlers;
	while (handler)
	{
		// skip handlers which failed to initialize
		// TODO: implement re-init, make BGM aware of it
		if (handler->is_available && !Q_strcasecmp(ext, handler->ext))
			break;
		handler = handler->next;
	}
	if (!handler)
	{
		Com_Printf("Unhandled extension for %s\n", filename);
		return;
	}
	snprintf(tmp, sizeof(tmp), "%s/%s", handler->dir, filename);

	bgmstream = S_CodecOpenStreamType(tmp, handler->type);
	if (bgmstream)
		return;	// success

	Com_Printf("Couldn't handle music file %s\n", filename);
}

void BGM_PlayCDtrack (int track, qboolean looping)
{
	char tmp[MAX_QPATH];
	const char *ext;
	unsigned int type;
	music_handler_t *handler;
	FILE *h;

	BGM_Stop();

	type = 0;
	ext  = NULL;
	handler = music_handlers;
	while (handler)
	{
		if (! handler->is_available)
			goto _next;
		snprintf(tmp, sizeof(tmp), "%s/track%02d%s", MUSIC_DIRNAME, (int)track, handler->ext);
		if (FS_FOpenFile(tmp, &h) == -1)
		{
			goto _next;
		}
		else
		{
			type = handler->type;
			ext = handler->ext;
		}
	_next:
		handler = handler->next;
	}
	if (ext == NULL)
	{
		Com_Printf("Couldn't find bgm for track %d\n", (int)track);
	}
	else
	{
		snprintf(tmp, sizeof(tmp), "%s/track%02d%s", MUSIC_DIRNAME, (int)track, ext);
		bgmstream = S_CodecOpenStreamType(tmp, type);
		if (!bgmstream)
			Com_Printf("Couldn't handle music file %s\n", tmp);
	}
}

void BGM_Stop (void)
{
	if (bgmstream)
	{
		bgmstream->status = STREAM_NONE;
		S_CodecCloseStream(bgmstream);
		bgmstream = NULL;
		s_rawend = 0;
	}
}

void BGM_Pause (void)
{
	if (bgmstream)
	{
		if (bgmstream->status == STREAM_PLAY)
			bgmstream->status = STREAM_PAUSE;
	}
}

void BGM_Resume (void)
{
	if (bgmstream)
	{
		if (bgmstream->status == STREAM_PAUSE)
			bgmstream->status = STREAM_PLAY;
	}
}

static void BGM_UpdateStream (void)
{
	int	res; // Number of bytes read.
	int	bufferSamples;
	int	fileSamples;
	int	fileBytes;
	byte raw[16384];
	qboolean did_rewind = false;

	// don't bother playing anything if disabled
	if (s_nobgm->value)
		return;

	if (bgmstream->status != STREAM_PLAY)
		return;

	// don't bother playing anything if volume is 0
	if (s_volume->value <= 0)
		return;

	// see how many samples should be copied into the raw buffer
	if (s_rawend < paintedtime)
		s_rawend = paintedtime;

	while (s_rawend < paintedtime + MAX_RAW_SAMPLES)
	{
		bufferSamples = MAX_RAW_SAMPLES - (s_rawend - paintedtime);

		// decide how much data needs to be read from the file
		fileSamples = bufferSamples * bgmstream->info.rate / dma.speed;
		if (!fileSamples)
			return;

		// our max buffer size
		fileBytes = fileSamples * (bgmstream->info.width * bgmstream->info.channels);
		if (fileBytes > sizeof(raw))
		{
			fileBytes = sizeof(raw);
			fileSamples = fileBytes / (bgmstream->info.width * bgmstream->info.channels);
		}

		// Read
		res = S_CodecReadStream(bgmstream, fileBytes, raw);
		if (res < fileBytes)
		{
			fileBytes = res;
			fileSamples = res / (bgmstream->info.width * bgmstream->info.channels);
		}

		if (res > 0) // data: add to raw buffer
		{
			S_RawSamples(fileSamples, bgmstream->info.rate,	bgmstream->info.width, bgmstream->info.channels, raw);
			did_rewind = false;
		}
		else if (res == 0) // EOF
		{
			if (bgmloop)
			{
				if (did_rewind)
				{
					Com_Printf("Stream keeps returning EOF.\n");
					BGM_Stop();
					return;
				}

				res = S_CodecRewindStream(bgmstream);
				if (res != 0)
				{
					Com_Printf("Stream seek error (%i), stopping.\n", res);
					BGM_Stop();
					return;
				}

				did_rewind = true;
			}
			else
			{
				BGM_Stop();
				return;
			}
		}
		else // res < 0: some read error
		{
			Com_Printf("Stream read error (%i), stopping.\n", res);
			BGM_Stop();
			return;
		}
	}
}

void BGM_Update (void)
{
	if (bgmstream)
		BGM_UpdateStream ();
}
