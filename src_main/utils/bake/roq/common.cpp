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

extern "C"
{
#include "cmdlib.h"
}

#include "roq.h"

/*
==================
UnpackCB2
==================
*/
void UnpackCB2 (const int *cb2, Block <2> *block){

	block->Row(0, 0)[0] = cb2[0];
	block->Row(0, 0)[1] = cb2[1];
	block->Row(0, 1)[0] = cb2[2];
	block->Row(0, 1)[1] = cb2[3];

	block->Row(1, 0)[0] = block->Row(1, 0)[1] = block->Row(1, 1)[0] = block->Row(1, 1)[1] = (cb2[4] + 2) >> 2;
	block->Row(2, 0)[0] = block->Row(2, 0)[1] = block->Row(2, 1)[0] = block->Row(2, 1)[1] = (cb2[5] + 2) >> 2;
}

/*
==================
UnpackAllCB2
==================
*/
void UnpackAllCB2 (const byte cb2[1536], Block <2> *cb2Unpacked){

	Block <2>	*block;
	const byte	*cb2Ptr;
	int			i;

	block = cb2Unpacked;

	cb2Ptr = cb2;

	for (i = 0; i < 256; i++){
		block->Row(0, 0)[0] = *cb2Ptr++;
		block->Row(0, 0)[1] = *cb2Ptr++;
		block->Row(0, 1)[0] = *cb2Ptr++;
		block->Row(0, 1)[1] = *cb2Ptr++;

		block->Row(1, 0)[0] = block->Row(1, 0)[1] = block->Row(1, 1)[0] = block->Row(1, 1)[1] = *cb2Ptr++;
		block->Row(2, 0)[0] = block->Row(2, 0)[1] = block->Row(2, 1)[0] = block->Row(2, 1)[1] = *cb2Ptr++;

		block++;
	}
}

/*
==================
UnpackSingleCB4
==================
*/
static void UnpackSingleCB4 (const Block <2> *cb2Unpacked, const byte *cb4, Block <4> *block){

	static int		offsets[4][2]= {{0, 0}, {2, 0}, {0, 2}, {2, 2}};
	const Block <2>	*ptr;
	int				p;
	int				i;

	for (p = 0; p < 3; p++){
		for (i = 0; i < 4; i++){
			ptr = cb2Unpacked + cb4[i];

			memcpy(block->Row(p, offsets[i][1] + 0) + offsets[i][0], ptr->Row(p, 0), 2);
			memcpy(block->Row(p, offsets[i][1] + 1) + offsets[i][0], ptr->Row(p, 1), 2);
		}
	}
}

/*
==================
UnpackAllCB4
==================
*/
void UnpackAllCB4 (const Block <2> cb2Unpacked[256], const byte cb4[1024], Block <4> *cb4Unpacked){

	int		i;

	for (i = 0; i < 256; i++)
		UnpackSingleCB4(cb2Unpacked, cb4 + i * 4, cb4Unpacked + i);
}

/*
==================
Enlarge4To8
==================
*/
Block <8> Enlarge4To8 (const Block <4> &b4){

	Block <8>	b8;
	int			p;
	int			x, y;

	for (p = 0; p < 3; p++){
		for (y = 0; y < 8; y++){
			for (x = 0; x < 8; x++)
				b8.Row(p, y)[x] = b4.Row(p, y >> 1)[x >> 1];
		}
	}

	return b8;
}
