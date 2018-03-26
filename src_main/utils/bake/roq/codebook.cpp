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
CompareInt6
==================
*/
static int CompareInt6 (const void *a, const void *b){

	const int	*ia = (const int *)a;
	const int	*ib = (const int *)b;
	int			at = 0, bt = 0;
	int			i;

	for (i = 0; i < 4; i++){
		at += *ia++;
		bt += *ib++;
	}

	return at - bt;
}

/*
==================
CompareInt24
==================
*/
static int CompareInt24 (const void *a, const void *b){

	const int	*ia = (const int *)a;
	const int	*ib = (const int *)b;
	int			at = 0, bt = 0;
	int			i, j;

	for (i = 0; i < 24; i += 6){
		for (j = 0; j < 4; j++){
			at += *ia++;
			bt += *ib++;
		}

		ia += 2;
		ib += 2;
	}

	return at - bt;
}

/*
==================
MakeCodebooks
==================
*/
void MakeCodebooks (randomState_t *randomState, const Image *image, byte cb2[1536], byte cb4[1024]){

	Block <2>	cb2Unpacked[256];
	Block <4>	cb4Unpacked[256];
	Block <2>	block;
	int			*cb2Int;
	int			*cb4Int;
	int			*points;
	int			*nearest;
	int			*ptr;
	int			numClusters;
	int			blockX, blockY;
	int			subX, subY;
	int			x, y, w, h;
	int			i;

	printf("Making codebooks...\n");

	w = image->Width();
	h = image->Height();

	numClusters = (w >> 1) * (h >> 1);

	qprintf("%i clusters\n", numClusters);

	cb2Int = (int *)malloc(256 * 6 * sizeof(int));
	cb4Int = (int *)malloc(256 * 24 * sizeof(int));

	points = (int *)malloc(numClusters * 6 * sizeof(int));
	ptr = points;

	// Map in CB4 cluster order to allow the same inputs to be reused
	for (blockY = 0; blockY < h; blockY += 4){
		for (blockX = 0; blockX < w; blockX += 4){
			for (subY = 0; subY < 4; subY += 2){
				for (subX = 0; subX < 4; subX += 2){
					x = blockX + subX;
					y = blockY + subY;

					block = Block <2> (image, x, y);

					*ptr++ = block.Row(0, 0)[0];
					*ptr++ = block.Row(0, 0)[1];
					*ptr++ = block.Row(0, 1)[0];
					*ptr++ = block.Row(0, 1)[1];
					*ptr++ = block.Row(1, 0)[0] + block.Row(1, 0)[1] + block.Row(1, 1)[0] + block.Row(1, 1)[1];
					*ptr++ = block.Row(2, 0)[0] + block.Row(2, 0)[1] + block.Row(2, 1)[0] + block.Row(2, 1)[1];
				}
			}
		}
	}

	// Generate 2x2 codebook
	nearest = (int *)malloc(numClusters * sizeof(int));
	InitELBG(points, 6, numClusters, cb2Int, 256, 1, nearest, randomState);
	DoELBG(points, 6, numClusters, cb2Int, 256, 1, nearest, randomState);
	free(nearest);

	qprintf("%i entries in 2x2 codebook\n", 256);

	qsort(cb2Int, 256, 6 * sizeof(int), CompareInt6);

	// Generate 4x4 codebook
	nearest = (int *)malloc(numClusters / 4 * sizeof(int));
	InitELBG(points, 24, numClusters / 4, cb4Int, 256, 1, nearest, randomState);
	DoELBG(points, 24, numClusters / 4, cb4Int, 256, 1, nearest, randomState);
	free(nearest);

	qprintf("%i entries in 4x4 codebook\n", 256);

	qsort(cb4Int, 256, 24 * sizeof(int), CompareInt24);

	// Store and unpack CB2
	for (i = 0; i < 1536; i += 6){
		cb2[i+0] = cb2Int[i+0];
		cb2[i+1] = cb2Int[i+1];
		cb2[i+2] = cb2Int[i+2];
		cb2[i+3] = cb2Int[i+3];
		cb2[i+4] = (cb2Int[i+4] + 2) >> 2;
		cb2[i+5] = (cb2Int[i+5] + 2) >> 2;

		UnpackCB2(&cb2Int[i], &cb2Unpacked[i/6]);
	}

	// Index CB4 to CB2 and store
	for (i = 0; i < 6144; i += 6){
		UnpackCB2(&cb4Int[i+0], &block);

		cb4[i/6] = block.IndexList(cb2Unpacked, 256, 0);
	}

	// Unpack all CB4 entries
	UnpackAllCB4(cb2Unpacked, cb4, cb4Unpacked);

	free(points);

	free(cb4Int);
	free(cb2Int);
}
