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

#ifndef __ROQ_ENCODER_H__
#define __ROQ_ENCODER_H__

typedef struct
{
	char		imageDir[1024];
	char		baseName[1024];

	int			padding;
	int			from;
	int			to;

	qboolean	noSound;
	char		soundName[1024];
	qboolean	soundNormalize;

	char		videoName[1024];

	int			framerate;
	int			bitrate;

	qboolean	hqAudio;
} encodingParms_t;

void EncodeVideo (encodingParms_t *parms);

#endif	// __ROQ_ENCODER_H__
