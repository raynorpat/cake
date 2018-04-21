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

#include "q_types.h"
#include "q_endian.h"

/*
============================================================================

					BYTE ORDER FUNCTIONS

============================================================================
*/

void CopyShortSwap (void *dest, void *src)
{
	byte *to = (byte *)dest, *from = (byte *)src;

	to[0] = from[1];
	to[1] = from[0];
}

void CopyLongSwap (void *dest, void *src)
{
	byte *to = (byte *)dest, *from = (byte *)src;

	to[0] = from[3];
	to[1] = from[2];
	to[2] = from[1];
	to[3] = from[0];
}

short ShortSwap (short l)
{
	byte b1,b2;

	b1 = l & 255;
	b2 = (l >> 8 ) & 255;

	return (b1 << 8) + b2;
}

int LongSwap (int l)
{
	byte b1,b2,b3,b4;

	b1 = l & 255;
	b2 = (l >> 8) & 255;
	b3 = (l >> 16) & 255;
	b4 = (l >> 24) & 255;

	return ((int)b1 << 24) + ((int)b2 << 16) + ((int)b3 << 8) + b4;
}

float FloatSwap (const float *f)
{
	byteAlias_t out;

	out.f = *f;
	out.ui = LongSwap(out.ui);

	return out.f;
}

