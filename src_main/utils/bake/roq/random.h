/*
 ------------------------------------------------------------------------------
 Mersenne Twister Random Algorithm

 Copyright (C) 2006 Ryan Martell.
 Based on A C-program for MT19937, with initialization improved 2002/1/26.
 Coded by Takuji Nishimura and Makoto Matsumoto.

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

#ifndef __ROQ_RANDOM_H__
#define __ROQ_RANDOM_H__

#define RANDOM_N	624

typedef struct {
	unsigned int	mt[RANDOM_N];			// The array for the state vector
	int				index;					// Current untempered value we use as the base
} randomState_t;

void			InitRandom (unsigned int seed, randomState_t *state);
unsigned int	Random (randomState_t *state);

#endif	// __ROQ_RANDOM_H__
