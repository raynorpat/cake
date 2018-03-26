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

#ifndef __ROQ_ROQ_H__
#define __ROQ_ROQ_H__

/*
==============================================================================

ROQ files are used for cinematics and in-game videos

==============================================================================
*/

#define ROQ_ID							0x1084

#define ROQ_CHUNK_HEADER_SIZE			8
#define ROQ_CHUNK_MAX_SIZE				65536

#define ROQ_QUAD_INFO					0x1001
#define ROQ_QUAD_CODEBOOK				0x1002
#define ROQ_QUAD_VQ						0x1011
#define ROQ_QUAD_JPEG					0x1012
#define ROQ_QUAD_HANG					0x1013

#define ROQ_SOUND_MONO					0x1020
#define ROQ_SOUND_STEREO				0x1021

#define ROQ_VQ_MOT						0x0000
#define ROQ_VQ_FCC						0x4000
#define ROQ_VQ_SLD						0x8000
#define ROQ_VQ_CCC						0xC000

typedef struct {
	unsigned short		id;
	unsigned long		size;
	unsigned short		flags;
} roqChunk_t;

typedef struct {
	byte				pixel[4][4];
} roqQuadVector_t;

typedef struct {
	byte				index[4];
} roqQuadCell_t;

#include "common.h"
#include "random.h"
#include "elbg.h"
#include "codebook.h"
#include "audio.h"
#include "video.h"

#endif	// __ROQ_ROQ_H__
