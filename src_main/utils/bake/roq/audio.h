/*
------------------------------------------------------------------------------
RoQ Video Encoder

Copyright (C) 2007 Vitor <vitor1001@gmail.com>
Copyright (C) 2004-2007 Eric Lasota.
Based on RoQ specs (C) 2001 Tim Ferguson.

This file is part of FFmpeg.

FFmpeg is free software; you can redistribute it and/or modify it under the
terms of the GNU Lesser General Public License as published by the Free
Software Foundation; either version 2.1 of the License, or (at your option)
any later version.

FFmpeg is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
details.

You should have received a copy of the GNU Lesser General Public License along
with FFmpeg; if not, write to the Free Software Foundation, Inc., 51 Franklin
Street, Fifth Floor, Boston, MA 02110-1301 USA.
------------------------------------------------------------------------------
*/

#ifndef __ROQ_AUDIO_H__
#define __ROQ_AUDIO_H__

#define MAX_DPCM						(127 * 127)

typedef struct {
	char			name[1024];
	FILE *			file;

	byte *			buffer;
	byte *			ptr;

	bool			stereo;
	int				rate;
	int				samples;
	int				samplesEncoded;
	const short *	data;

	int				framerate;

	bool			firstFrame;

	short			previousValues[2];

	short			dpcmTable[MAX_DPCM];
} audioEnc_t;

void		EncodeAudioFrame (audioEnc_t *enc);

void		InitAudioEncoder (audioEnc_t *enc, const char *name, FILE *file, bool stereo, int rate, int samples, const short *data, int framerate);
void		ShutdownAudioEncoder (audioEnc_t *enc);

#endif	// __ROQ_AUDIO_H__
