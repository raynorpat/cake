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

// See http://en.wikipedia.org/wiki/Mersenne_twister for an explanation of this algorithm
extern "C"
{
#include "cmdlib.h"
}

#include "roq.h"

// Period parameters
#define M							397
#define A							0x9908B0DF
#define UPPER_MASK					0x80000000
#define LOWER_MASK					0x7FFFFFFF

/*
==================
InitRandom

Initializes mt[RANDOM_N] with a seed
==================
*/
void InitRandom (unsigned int seed, randomState_t *state){

	unsigned int prev;
	int		index;

	// This differs from the wikipedia article. Source is from the Makoto
	// Matsumoto and Takuji Nishimura code, with the following comment:
	// See Knuth TAOCP Vol2. 3rd Ed. P.106 for multiplier.
	// In the previous versions, MSBs of the seed affect only MSBs of the array
	// mt[].
	state->mt[0] = seed & 0xFFFFFFFF;

	for (index = 1; index < RANDOM_N; index++){
		prev = state->mt[index - 1];

		state->mt[index] = (1812433253UL * (prev ^ (prev >> 30)) + index) & 0xFFFFFFFF;
	}

	state->index = index;	// Will cause it to generate untempered numbers the first iteration
}

/*
==================
RandomGenerateUntemperedNumbers

Generate RANDOM_N words at one time (which will then be tempered later)
==================
*/
static void RandomGenerateUntemperedNumbers (randomState_t *state){

	unsigned int y;
	int		kk;

	for (kk = 0; kk < RANDOM_N - M; kk++){
		y = (state->mt[kk] & UPPER_MASK) | (state->mt[kk + 1] & LOWER_MASK);
		state->mt[kk] = state->mt[kk + M] ^ (y >> 1) ^ ((y & 1) * A);
	}

	for ( ; kk < RANDOM_N - 1; kk++){
		y = (state->mt[kk] & UPPER_MASK) | (state->mt[kk + 1] & LOWER_MASK);
		state->mt[kk] = state->mt[kk + (M - RANDOM_N)] ^ (y >> 1) ^ ((y & 1) * A);
	}

	y = (state->mt[RANDOM_N - 1] & UPPER_MASK) | (state->mt[0] & LOWER_MASK);
	state->mt[RANDOM_N - 1] = state->mt[M - 1] ^ (y >> 1) ^ ((y & 1) * A);

	state->index = 0;
}

/*
==================
Random

Generates a random number on [0,0xFFFFFFFF]-interval
==================
*/
unsigned int Random (randomState_t *state){

	unsigned int y;

	// Regenerate the untempered numbers if we should...
	if (state->index >= RANDOM_N)
		RandomGenerateUntemperedNumbers(state);

	// Grab one...
	y = state->mt[state->index++];

	// Now temper (Mersenne Twister coefficients).
	// The coefficients for MT19937 are...
	y ^= (y >> 11);
	y ^= (y << 7) & 0x9D2C5680;
	y ^= (y << 15) & 0xEFC60000;
	y ^= (y >> 18);
	
	return y;
}
