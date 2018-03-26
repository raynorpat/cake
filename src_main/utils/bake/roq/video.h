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

#ifndef __ROQ_VIDEO_H__
#define __ROQ_VIDEO_H__

typedef struct {
	int				x;
	int				y;
} motionVec_t;

typedef struct {
	char			name[1024];
	FILE *			file;

	byte *			buffer;

	Image			frames[2];

	Image *			lastFrame;
	Image *			currentFrame;

	const Image *	frameToEnc;
	int				frameCount;

	byte			cb2[1536];
	byte			cb4[1024];

	motionVec_t *	motion4[2];
	motionVec_t *	motion8[2];

	motionVec_t *	thisMotion4;
	motionVec_t *	lastMotion4;

	motionVec_t *	thisMotion8;
	motionVec_t *	lastMotion8;

	int				width;
	int				height;

	int				framerate;
	int				bitrate;

	int				minBytes;
	int				maxBytes;

	int				finalSize;

	int				slip;

	unsigned long long dist;
	unsigned long long targetDist;
} videoEnc_t;

void		EncodeVideoFrame (videoEnc_t *enc, const Image *image);

void		InitVideoEncoder (videoEnc_t *enc, const char *name, FILE *file, int width, int height, int framerate, int bitrate);
void		ShutdownVideoEncoder (videoEnc_t *enc);

#endif	// __ROQ_VIDEO_H__
