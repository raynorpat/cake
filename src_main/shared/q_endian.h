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

// q_endian.h -- endian header

#pragma once

// endianness
void CopyShortSwap( void *dest, void *src );
void CopyLongSwap( void *dest, void *src );
short ShortSwap( short l );
int LongSwap( int l );
float FloatSwap( const float *f );

#if defined(Q_BIG_ENDIAN)
	#define CopyLittleShort( dest, src )	CopyShortSwap( dest, src )
	#define CopyLittleLong( dest, src )		CopyLongSwap( dest, src )
	#define LittleShort( x )				ShortSwap( x )
	#define LittleLong( x )					LongSwap( x )
	#define LittleFloat( x )				FloatSwap( &x )
	#define BigShort
	#define BigLong
	#define BigFloat
#elif defined(Q_LITTLE_ENDIAN)
	#define CopyLittleShort( dest, src )	memcpy( dest, src, 2 )
	#define CopyLittleLong( dest, src )		memcpy( dest, src, 4 )
	#define LittleShort
	#define LittleLong
	#define LittleFloat
	#define BigShort( x )					ShortSwap( x )
	#define BigLong( x )					LongSwap( x )
	#define BigFloat( x )					FloatSwap( &x )
#endif
