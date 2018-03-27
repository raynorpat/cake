/*
===========================================================================
Copyright (C) 1997-2006 Id Software, Inc.

This file is part of Quake 2 Tools source code.

Quake 2 Tools source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake 2 Tools source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake 2 Tools source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "bake.h"
#include "roq_encoder.h"

char	base[32];

typedef struct {
	const char *		name;
	int					framerate;
} framerate_t;

typedef struct {
	const char *		name;
	int					bitrate;
} bitrate_t;

static framerate_t		framerateList[] = {
	{ "15 FPS",				15 },
	{ "20 FPS",				20 },
	{ "25 FPS",				25 },
	{ "30 FPS",				30 }
};
#define NUM_FRAMERATES (sizeof(framerateList) / sizeof(framerate_t))

static bitrate_t		bitrateList[] = {
	{ "1 Mbit/sec",			1000000 },
	{ "2 Mbit/sec",			2000000 },
	{ "3 Mbit/sec",			3000000 },
	{ "4 Mbit/sec",			4000000 },
	{ "5 Mbit/sec",			5000000 },
	{ "6 Mbit/sec",			6000000 },
	{ "7 Mbit/sec",			7000000 },
	{ "8 Mbit/sec",			8000000 },
	{ "9 Mbit/sec",			9000000 },
	{ "10 Mbit/sec",		10000000 },
	{ "11 Mbit/sec",		11000000 },
	{ "12 Mbit/sec",		12000000 },
	{ "13 Mbit/sec",		13000000 },
	{ "14 Mbit/sec",		14000000 },
	{ "15 Mbit/sec",		15000000 },
	{ "16 Mbit/sec",		16000000 },
	{ "17 Mbit/sec",		17000000 },
	{ "18 Mbit/sec",		18000000 },
	{ "19 Mbit/sec",		19000000 },
	{ "20 Mbit/sec",		20000000 }
};
#define NUM_BITRATES (sizeof(bitrateList) / sizeof(bitrate_t))

//==========================================================================

/*
===============
Cmd_Video

video <directory> <startframe> <padding> <endframe> <framerate> <bitrate>
===============
*/
void Cmd_Video (void)
{
	encodingParms_t	parms;
	int length = 0, index = 0;
	
	// get name
	GetToken (false);
	strcpy (base, token);
	
	// if building release, copy over the video and get out
	if (g_release)
	{
		sprintf (base, "video/%s.roq", token);
		ReleaseFile (base);
		return;
	}

	// set the names in the encoding parameters
	strcpy (parms.videoName, base);
	strcpy (parms.baseName, base);
	strcpy (parms.soundName, base);

	// set the image directory
	sprintf (parms.imageDir, "%svideo/%s", gamedir, base);

	// set start frame
	GetToken (false);
	parms.from = atoi(token);
	
	// set frame padding
	GetToken(false);
	parms.padding = atoi(token);

	// set end frame
	GetToken (false);
	parms.to = atoi(token);
	
	// do a check here on padding, compared to end frame
	length = strlen(token);
	if (length > parms.padding)
		parms.padding = length;

	// set framerate
	GetToken(false);
	index = atoi(token);
	if (index < 0 || index >= NUM_FRAMERATES)
	{
		Error("Invalid framerate specified.\n");
	}
	parms.framerate = framerateList[index].framerate;

	// set bitrate
	GetToken(false);
	index = atoi(token);
	if (index < 0 || index >= NUM_BITRATES)
	{
		Error("Invalid bitrate specified.\n");
	}
	parms.bitrate = bitrateList[index].bitrate;

	// issue the actual encode
	EncodeVideo(&parms);
}
