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
#include "snd_local.h"
#include "snd_ogg.h"

#define OV_EXCLUDE_STATIC_CALLBACKS

#ifndef _WIN32
#include <sys/time.h>
#endif
#include <errno.h>
#include <vorbis/vorbisfile.h>

qboolean ogg_first_init = true;
qboolean ogg_started = false;

int ogg_bigendian = 0;

byte *ogg_buffer;			// File buffer
char ovBuf[4096];           // Buffer for sound
OggVorbis_File ovFile;		// Ogg Vorbis file
vorbis_info *ogg_info;		// Ogg Vorbis file information
int ogg_numbufs;			// Number of buffers for OpenAL
int ovSection;				// Position in Ogg Vorbis file
ogg_status_t ogg_status;	// Status indicator

int ogg_curfile;			// Index of currently played file
char **ogg_filelist;		// List of Ogg Vorbis files
int ogg_numfiles;			// Number of Ogg Vorbis files

cvar_t *ogg_autoplay;		// Play this song when started
cvar_t *ogg_sequence;		// Sequence play indicator

void OGG_LoadFileList (void);

// console commands
void OGG_ListCmd (void);
void OGG_PauseCmd (void);
void OGG_PlayCmd (void);
void OGG_ResumeCmd (void);
void OGG_StatusCmd (void);

/*
===========
OGG_Init

Initialize the Ogg Vorbis subsystem
===========
*/
void OGG_Init(void)
{
	cvar_t *cv;

	if (ogg_started)
		return;

	Com_Printf("Starting Ogg Vorbis.\n");

	// Skip initialization if disabled
	cv = Cvar_Get("ogg_enable", "1", CVAR_ARCHIVE);
	if (cv->value != 1)
	{
		Com_Printf("Ogg Vorbis not initializing.\n");
		return;
	}

	// for ovread()
	if (bigendien == true)
		ogg_bigendian = 1;

	// Cvars
	ogg_autoplay = Cvar_Get("ogg_autoplay", "?", CVAR_ARCHIVE);
	ogg_sequence = Cvar_Get("ogg_sequence", "loop", CVAR_ARCHIVE);

	// Console commands
	Cmd_AddCommand("ogg_list", OGG_ListCmd);
	Cmd_AddCommand("ogg_pause", OGG_PauseCmd);
	Cmd_AddCommand("ogg_play", OGG_PlayCmd);
	Cmd_AddCommand("ogg_resume", OGG_ResumeCmd);
	Cmd_AddCommand("ogg_status", OGG_StatusCmd);
	Cmd_AddCommand("ogg_stop", OGG_Stop);

	// Build list of files
	ogg_numfiles = 0;

	if (ogg_numfiles == 0)
		OGG_LoadFileList();

	// Check if we have Ogg Vorbis files
	if (ogg_numfiles <= 0)
	{
		Com_Printf("No Ogg Vorbis files found.\n");
		ogg_started = true;
		OGG_Shutdown();
		return;
	}

	// Initialize variables
	if (ogg_first_init)
	{
		ogg_buffer = NULL;
		ogg_curfile = -1;
		ogg_info = NULL;
		ogg_status = STOP;
		ogg_first_init = false;
	}

	ogg_started = true;

	Com_Printf(S_COLOR_GREEN "%d Ogg Vorbis files found.\n", ogg_numfiles);

	// Autoplay support
	if (ogg_autoplay->string[0] != '\0')
		OGG_ParseCmd(ogg_autoplay->string);
}

/*
===========
OGG_Init

Shutdown the Ogg Vorbis subsystem
===========
*/
void OGG_Shutdown(void)
{
	if (!ogg_started)
		return;

	Com_Printf("Shutting down Ogg Vorbis.\n");

	OGG_Stop();

	// Free the list of files
	FS_FreeFileList(ogg_filelist, ogg_numfiles);

	// Remove console commands
	Cmd_RemoveCommand("ogg_list");
	Cmd_RemoveCommand("ogg_pause");
	Cmd_RemoveCommand("ogg_play");
	Cmd_RemoveCommand("ogg_resume");
	Cmd_RemoveCommand("ogg_status");
	Cmd_RemoveCommand("ogg_stop");

	ogg_started = false;
}

/*
===========
OGG_Check

Check if the file is a valid Ogg Vorbis file
===========
*/
static qboolean OGG_Check(char *name)
{
	qboolean res = false;
	byte *buffer;
	int size;
	OggVorbis_File ovf;

	if ((size = FS_LoadFile(name, (void **)&buffer)) > 0)
	{
		if (ov_test(NULL, &ovf, (char *)buffer, size) == 0)
		{
			res = true;
			ov_clear(&ovf);
		}

		FS_FreeFile(buffer);
	}

	return res;
}

/*
===========
OGG_Check

Load a list of Ogg Vorbis files in "music" directory
===========
*/
void OGG_LoadFileList(void)
{
	char **list;
	int i;
	int j;

	// Get file list
	list = FS_ListFiles2(va("%s/*.ogg", OGG_DIR), &ogg_numfiles);
	ogg_numfiles--;

	// Check if there are posible Ogg files
	if (list == NULL)
		return;

	// Allocate list of files
	ogg_filelist = malloc(sizeof(char *) * ogg_numfiles);

	// Add valid Ogg Vorbis file to the list
	for (i = 0, j = 0; i < ogg_numfiles; i++)
	{
		if (!OGG_Check(list[i]))
		{
			free(list[i]);
			continue;
		}

		ogg_filelist[j++] = list[i];
	}

	// Free the file list
	free(list);

	// Adjust the list size (remove space for invalid music files)
	ogg_numfiles = j;
	ogg_filelist = realloc(ogg_filelist, sizeof(char *) * ogg_numfiles);
}

/*
===========
OGG_Open

Play Ogg Vorbis file (with absolute or relative index)
===========
*/
qboolean OGG_Open(ogg_seek_t type, int offset)
{
	int size; 
	int pos = -1;
	int res;

	switch (type)
	{
		case ABS:
			// absolute index
			if ((offset < 0) || (offset >= ogg_numfiles))
			{
				Com_Printf(S_COLOR_RED "OGG_Open: %d out of range.\n", offset + 1);
				return false;
			}
			else
			{
				pos = offset;
			}
			break;
		case REL:
			// simulate a loopback
			if ((ogg_curfile == -1) && (offset < 0))
				offset++;

			while (ogg_curfile + offset < 0)
				offset += ogg_numfiles;
			while (ogg_curfile + offset >= ogg_numfiles)
				offset -= ogg_numfiles;

			pos = ogg_curfile + offset;
			break;
	}

	// check running music
	if (ogg_status == PLAY)
	{
		if (ogg_curfile == pos)
		{
			return true;
		}

		else
		{
			OGG_Stop();
		}
	}

	// find file
	if ((size = FS_LoadFile(ogg_filelist[pos], (void **)&ogg_buffer)) == -1)
	{
		Com_Printf(S_COLOR_RED "OGG_Open: could not open %d (%s): %s.\n", pos, ogg_filelist[pos], strerror(errno));
		return false;
	}

	// open ogg vorbis file
	if ((res = ov_open(NULL, &ovFile, (char *)ogg_buffer, size)) < 0)
	{
		Com_Printf(S_COLOR_RED "OGG_Open: '%s' is not a valid Ogg Vorbis file (error %i).\n", ogg_filelist[pos], res);
		FS_FreeFile(ogg_buffer);
		ogg_buffer = NULL;
		return false;
	}

	// grab info on the ogg
	ogg_info = ov_info(&ovFile, 0);
	if (!ogg_info)
	{
		Com_Printf(S_COLOR_RED "OGG_Open: Unable to get stream information for %s.\n", ogg_filelist[pos]);
		ov_clear(&ovFile);
		FS_FreeFile(ogg_buffer);
		ogg_buffer = NULL;
		return false;
	}

	// play file
	ovSection = 0;
	ogg_curfile = pos;
	ogg_status = PLAY;

	return true;
}

/*
===========
OGG_OpenName

Play Ogg Vorbis file (with name only)
===========
*/
qboolean OGG_OpenName(char *filename)
{
	int i;
	char *name;

	name = va("%s/%s.ogg", OGG_DIR, filename);

	for (i = 0; i < ogg_numfiles; i++)
	{
		if (strcmp(name, ogg_filelist[i]) == 0)
			break;
	}

	if (i < ogg_numfiles)
	{
		return OGG_Open(ABS, i);
	}
	else
	{
		Com_Printf("OGG_OpenName: '%s' not in the list.\n", filename);
		return false;
	}
}

/*
===========
OGG_Read

Play a portion of the currently opened file
===========
*/
static int OGG_Read(void)
{
	int res;

	// read and resample
	res = ov_read(&ovFile, ovBuf, sizeof(ovBuf), ogg_bigendian, OGG_SAMPLEWIDTH, 1,	&ovSection);
	S_RawSamples(res / (OGG_SAMPLEWIDTH * ogg_info->channels), ogg_info->rate, OGG_SAMPLEWIDTH, ogg_info->channels,	(byte *)ovBuf);

	// check for end of file
	if (res == 0)
	{
		OGG_Stop();
		OGG_Sequence();
	}

	return res;
}

/*
===========
OGG_Sequence

Play files in sequence
===========
*/
void OGG_Sequence(void)
{
	if (strcmp(ogg_sequence->string, "next") == 0)
		OGG_Open(REL, 1);
	else if (strcmp(ogg_sequence->string, "prev") == 0)
		OGG_Open(REL, -1);
	else if (strcmp(ogg_sequence->string, "random") == 0)
		OGG_Open(ABS, rand() % ogg_numfiles);
	else if (strcmp(ogg_sequence->string, "loop") == 0)
		OGG_Open(REL, 0);
	else if (strcmp(ogg_sequence->string, "none") != 0)
	{
		Com_Printf(S_COLOR_RED "Invalid value of ogg_sequence: %s\n", ogg_sequence->string);
		Cvar_Set("ogg_sequence", "none");
	}
}

/*
===========
OGG_Stop

Stop playing the current file
===========
*/
void OGG_Stop(void)
{
	if (ogg_status == STOP)
		return;

#ifdef USE_OPENAL
	if (sound_started == SS_OAL)
		AL_UnqueueRawSamples();
#endif

	ov_clear(&ovFile);
	ogg_status = STOP;
	ogg_info = NULL;
	ogg_numbufs = 0;

	if (ogg_buffer != NULL)
	{
		FS_FreeFile(ogg_buffer);
		ogg_buffer = NULL;
	}
}

/*
===========
OGG_Stream

Streams out the music
===========
*/
void OGG_Stream(void)
{
	if (!ogg_started)
		return;

	if (ogg_status == PLAY)
	{
#ifdef USE_OPENAL
		if (sound_started == SS_OAL)
		{
			// activeStreamBuffers is the count of stream buffers, maxStreamBuffers is the total available stream buffers
			while (activeStreamBuffers < maxStreamBuffers)
				OGG_Read ();
		}
		else
#endif
		{
			if (sound_started == SS_DMA)
			{
				// read that number samples into the buffer, that were played since the last call to this function
				// this keeps the buffer at all times at an "optimal" fill level
				while (paintedtime + MAX_RAW_SAMPLES - 2048 > s_rawend)
					OGG_Read ();
			}
		}
	}
}

/*
===========
OGG_ListCmd

Lists out Ogg Vorbis files in the "music" directory
===========
*/
void OGG_ListCmd(void)
{
	int i;

	for (i = 0; i < ogg_numfiles; i++)
		Com_Printf("%d %s\n", i + 1, ogg_filelist[i]);

	Com_Printf("%d Ogg Vorbis files.\n", ogg_numfiles);
}

/*
===========
OGG_ParseCmd

Parse play controls
===========
*/
void OGG_ParseCmd(char *arg)
{
	int n;

	if (!ogg_started)
		return;

	switch (arg[0])
	{
		case '#':
			n = (int)strtol(arg + 1, (char **)NULL, 10) - 1;
			OGG_Open(ABS, n);
			break;
		case '?':
			OGG_Open(ABS, rand() % ogg_numfiles);
			break;
		case '>':
			if (strlen(arg) > 1)
				OGG_Open(REL, (int)strtol(arg + 1, (char **)NULL, 10));
			else
				OGG_Open(REL, 1);
			break;
		case '<':
			if (strlen(arg) > 1)
				OGG_Open(REL, -(int)strtol(arg + 1, (char **)NULL, 10));
			else
				OGG_Open(REL, -1);
			break;
		default:
			OGG_OpenName(arg);
			break;
	}
}

/*
===========
OGG_PauseCmd

Pause current song
===========
*/
void OGG_PauseCmd(void)
{
	if (ogg_status == PLAY)
	{
		ogg_status = PAUSE;
		ogg_numbufs = 0;
	}

#ifdef USE_OPENAL
	if (sound_started == SS_OAL)
		AL_UnqueueRawSamples();
#endif
}

/*
===========
OGG_PlayCmd

Play song
===========
*/
void OGG_PlayCmd(void)
{
	if (Cmd_Argc() < 2)
	{
		Com_Printf("Usage: ogg_play {filename | #n | ? | >n | <n}\n");
		return;
	}

	OGG_ParseCmd(Cmd_Argv(1));
}

/*
===========
OGG_ResumeCmd

Resume current song
===========
*/
void OGG_ResumeCmd(void)
{
	if (ogg_status == PAUSE)
		ogg_status = PLAY;
}

/*
===========
OGG_StatusCmd

Display status on current song
===========
*/
void OGG_StatusCmd(void)
{
	switch (ogg_status)
	{
		case PLAY:
			Com_Printf("Playing file %d (%s) at %0.2f seconds.\n", ogg_curfile + 1, ogg_filelist[ogg_curfile], ov_time_tell(&ovFile));
			break;
		case PAUSE:
			Com_Printf("Paused file %d (%s) at %0.2f seconds.\n",
				ogg_curfile + 1, ogg_filelist[ogg_curfile],	ov_time_tell(&ovFile));
				break;
		case STOP:
			if (ogg_curfile == -1)
				Com_Printf("Stopped.\n");
			else
				Com_Printf("Stopped file %d (%s).\n", ogg_curfile + 1, ogg_filelist[ogg_curfile]);
			break;
	}
}
