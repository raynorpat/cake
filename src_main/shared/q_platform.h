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

// q_platform.h -- platform header

#pragma once

// for windows fastcall option
#define QDECL
#define QCALL

// Win64
#if defined(WIN64) || defined(_WIN64) || defined(__WIN64__)

	#define idx64

	#undef QDECL
	#define QDECL __cdecl

	#undef QCALL
	#define QCALL __stdcall

	#define QINLINE __inline

	#define PATH_SEP '\\'

	#if defined(_MSC_VER)
		#define OS_STRING "win_msvc"
	#elif defined(__MINGW64__)
		#define OS_STRING "win_mingw"
	#endif

	#define ARCH_STRING "x86_64"

	#define Q_LITTLE_ENDIAN

	#define LIB_PREFIX ""
	#define LIB_SUFFIX ".dll"

// Win32
#elif defined(_WIN32) || defined(__WIN32__)

	#undef QDECL
	#define	QDECL __cdecl

	#undef QCALL
	#define QCALL __stdcall

	#define QINLINE __inline
	
	#define PATH_SEP '\\'

	#if defined(_MSC_VER)
		#define OS_STRING "win_msvc"
	#elif defined(__MINGW32__)
		#define OS_STRING "win_mingw"
	#endif

	#define ARCH_STRING "x86"

	#define Q_LITTLE_ENDIAN
	
	#define LIB_PREFIX ""
	#define LIB_SUFFIX ".dll"

// MAC OS X
#elif defined(MACOS_X) || defined(__APPLE_CC__)

	// make sure this is defined, just for sanity's sake...
	#ifndef MACOS_X
		#define MACOS_X
	#endif

	#define QINLINE /*inline*/

	#define OS_STRING "macosx"

	#define	PATH_SEP '/'

	#if defined(__ppc__)
		#define ARCH_STRING "ppc"
		#define Q_BIG_ENDIAN
	#elif defined(__i386__)
		#define ARCH_STRING "x86"
		#define Q_LITTLE_ENDIAN
	#elif defined(__x86_64__)
		#define idx64
		#define ARCH_STRING "x86_64"
		#define Q_LITTLE_ENDIAN
	#endif

	#define LIB_PREFIX ""
	#define LIB_SUFFIX ".dylib"

// Linux
#elif defined __linux__ || defined ( __FreeBSD__ )

	#include <endian.h>

	#ifdef __clang__
		#define QINLINE static inline
	#else
		#define QINLINE inline
	#endif

	#define PATH_SEP '/'

	#if defined(__linux__)
		#define OS_STRING "linux"
	#else
		#define OS_STRING "kFreeBSD"
	#endif

	#ifdef __i386__
		#define ARCH_STRING	"i386"
	#elif defined ( __x86_64__ )
		#define ARCH_STRING	"x86_64"
	#elif defined ( __powerpc__ )
		#define ARCH_STRING	"ppc"
	#elif defined __alpha__
		#define ARCH_STRING	"axp"
	#elif defined ( __arm__ )
		#define ARCH_STRING	"arm"
	#elif defined ( _MIPS_ARCH )
		#define ARCH_STRING	"mips"
	#else
		#define ARCH_STRING	"Unknown"
	#endif

	#if defined(__x86_64__)
		#define idx64
	#endif

	#if __FLOAT_WORD_ORDER == __BIG_ENDIAN
		#define Q_BIG_ENDIAN
	#else
		#define Q_LITTLE_ENDIAN
	#endif

	#define LIB_PREFIX "lib"
	#define LIB_SUFFIX ".so"

// BSD
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)

	#include <sys/types.h>
	#include <machine/endian.h>

	#ifndef __BSD__
		#define __BSD__
	#endif

	#define QINLINE inline

	#define PATH_SEP '/'

	#if defined(__FreeBSD__)
		#define OS_STRING "freebsd"
	#elif defined(__OpenBSD__)
		#define OS_STRING "openbsd"
	#elif defined(__NetBSD__)
		#define OS_STRING "netbsd"
	#endif

	#ifdef __i386__
		#define ARCH_STRING	"i386"
	#elif defined ( __amd64__ )
		#define ARCH_STRING	"x86_64"
	#elif defined ( __powerpc__ )
	#define ARCH_STRING	"ppc"
		#elif defined __alpha__
	#define ARCH_STRING	"axp"
		#elif defined ( __arm__ )
		#define ARCH_STRING	"arm"
	#elif defined ( _MIPS_ARCH )
		#define ARCH_STRING	"mips"
	#else
		#define ARCH_STRING	"Unknown"
	#endif

	#if defined(__amd64__)
		#define idx64
	#endif

	#if BYTE_ORDER == BIG_ENDIAN
		#define Q_BIG_ENDIAN
	#else
		#define Q_LITTLE_ENDIAN
	#endif

	#define LIB_PREFIX "lib"
	#define LIB_SUFFIX ".so"

#else

	#define OS_STRING "NON-WIN32"
	#define ARCH_STRING	"NON-WIN32"

	#define LIB_PREFIX	""
	#define LIB_SUFFIX	".so"

#endif

#if (defined _M_IX86 || defined __i386__) && !defined(C_ONLY) && !defined (__sun__)
#define id386	1
#else
#define id386	0
#endif

#if (defined(powerc) || defined(powerpc) || defined(ppc) || defined(__ppc) || defined(__ppc__)) && !defined(C_ONLY)
#define idppc	1
#else
#define idppc	0
#endif

#if defined _M_ALPHA && !defined(C_ONLY)
#define idaxp	1
#else
#define idaxp	0
#endif

// catch missing defines in above blocks
#if !defined(OS_STRING)
	#error "Operating system not supported"
#endif
#if !defined(ARCH_STRING)
	#error "Architecture not supported"
#endif
#if !defined(LIB_PREFIX)
	#error "LIB_PREFIX not defined"
#endif
#if !defined(LIB_SUFFIX)
	#error "LIB_SUFFIX not defined"
#endif
#if !defined(QINLINE)
	#error "QINLINE not defined"
#endif
#if !defined(PATH_SEP)
	#error "PATH_SEP not defined"
#endif

#if defined(Q_BIG_ENDIAN) && defined(Q_LITTLE_ENDIAN)
	#error "Endianness defined as both big and little"
#elif !defined(Q_BIG_ENDIAN) && !defined(Q_LITTLE_ENDIAN)
	#error "Endianness not defined"
#endif

// platform string
#if defined(NDEBUG)
	#define PLATFORM_STRING OS_STRING "-" ARCH_STRING
#else
	#define PLATFORM_STRING OS_STRING "-" ARCH_STRING "-debug"
#endif
