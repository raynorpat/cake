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

// q_types.h -- platform header

#pragma once

#include <assert.h>
#include <time.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h> // needed for size_t and NULL
#include <stdarg.h> // needed for va_list
#include <ctype.h>
#ifdef _WIN32
#include <direct.h>
#endif

#ifndef NULL
#ifdef __cplusplus
#define NULL 0
#else
#define NULL ((void *)0)
#endif
#endif

#ifndef __cplusplus
#define Q_NULL NULL
#else
#include <cstddef>
#define Q_NULL nullptr
#endif

#if defined (_MSC_VER) && (_MSC_VER < 1600)
	#include <io.h>

	typedef signed __int64 int64_t;
	typedef signed __int32 int32_t;
	typedef signed __int16 int16_t;
	typedef signed __int8  int8_t;
	typedef unsigned __int64 uint64_t;
	typedef unsigned __int32 uint32_t;
	typedef unsigned __int16 uint16_t;
	typedef unsigned __int8  uint8_t;
#else
	#include <stdint.h>
#endif

#if defined(__cplusplus) || defined(__bool_true_false_are_defined)
typedef int					qboolean;
#else
typedef enum {false, true}	qboolean;
#endif

typedef uint8_t byte;
typedef uint16_t word;
typedef unsigned long ulong;

// 32 bit field aliasing
typedef union byteAlias_u
{
	float f;
	int32_t i;
	uint32_t ui;
	qboolean qb;
	byte b[4];
	char c[4];
} byteAlias_t;


#define PAD(x,y)			(((x)+(y)-1) & ~((y)-1))
#define PADLEN(x, y)		(PAD((x), (y)) - (x))

#define ARRAY_LEN(x)		(sizeof(x) / sizeof(*(x)))

