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

// q_platform.h -- platform and compiler dependent header

#ifdef WIN32

#define BUILDSTRING "Win32"

#ifdef _M_IX86
#define	CPUSTRING	"x86"
#define ARCH		"x86"
#elif defined ( __x86_64__ ) || defined( _M_AMD64 )
#define CPUSTRING	"x64"
#define ARCH		"x64"
#endif

#define LIB_PREFIX ""
#define LIB_SUFFIX ".dll"

#elif defined __linux__ || defined ( __FreeBSD__ )

#if defined ( __FreeBSD__ )
#define BUILDSTRING "FreeBSD"
#else
#define BUILDSTRING "Linux"
#endif

#ifdef __i386__
#define ARCH		"i386"
#define CPUSTRING	"i386"
#elif defined ( __x86_64__ )
#define ARCH		"x86_64"
#define CPUSTRING	"x86_64"
#elif defined ( __powerpc__ )
#define ARCH		"ppc"
#define CPUSTRING	"ppc"
#elif defined __alpha__
#define ARCH		"alpha"
#define CPUSTRING	"axp"
#elif defined ( __arm__ )
#define ARCH		 "arm"
#define CPUSTRING	"arm"
#elif defined ( _MIPS_ARCH )
#define ARCH		"mips"
#define CPUSTRING	"mips"
#else
#define ARCH		""
#define CPUSTRING	"Unknown"
#endif

#define LIB_PREFIX "lib"
#define LIB_SUFFIX ".so"

#if defined ( __APPLE__ ) && defined ( __MACH__ )

#ifndef __MACOSX__
#define __MACOSX__
#endif

// macOS has universal binaries, so no need for cpu dependency
#define BUILDSTRING "macOS"
#define CPUSTRING	"universal"
#define ARCH		"mac"

#define LIB_PREFIX "lib"
#define LIB_SUFFIX ".dylib"

#endif

#elif defined __sun__

#define BUILDSTRING "Solaris"

#ifdef __i386__
#define CPUSTRING	"i386"
#define ARCH		"i386"
#else
#define CPUSTRING	"sparc"
#define ARCH		"sparc"
#endif

#define LIB_PREFIX	"lib"
#define LIB_SUFFIX	".so"

#else	// !WIN32

#define BUILDSTRING "NON-WIN32"
#define	CPUSTRING	"NON-WIN32"
#define ARCH		""

#define LIB_PREFIX	"lib"
#define LIB_SUFFIX	".so"

#endif

// disable some annoying MSVC warnings
#ifdef _MSC_VER
#pragma warning(disable : 4244) // conversion from 'x' to 'y', possible loss of data
#pragma warning(disable : 4136) // conversion between different floating-point types
#pragma warning(disable : 4051) // type conversion; possible loss of data
#pragma warning(disable : 4018) // signed/unsigned mismatch
#pragma warning(disable : 4305) // truncation from 'x' to 'y'
#endif

#if (defined _M_IX86 || defined __i386__) && !defined C_ONLY && !defined __sun__
#define id386	1
#else
#define id386	0
#endif

#if defined _M_ALPHA && !defined C_ONLY
#define idaxp	1
#else
#define idaxp	0
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

#if !defined(__GNUC__)
#define	__attribute__(x)
#endif

// argument format attributes for function pointers are supported for gcc >= 3.1
#if defined(__GNUC__) && (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ > 0))
#define	__fp_attribute__ __attribute__
#else
#define	__fp_attribute__(x)
#endif

// Generic helper definitions for shared library support
#if defined _WIN32 || defined __CYGWIN__
# define Q_DLL_IMPORT __declspec( dllimport )
# define Q_DLL_EXPORT __declspec( dllexport )
# define Q_DLL_LOCAL
#else
# if __GNUC__ >= 4
#  define Q_DLL_IMPORT __attribute__ ( ( visibility( "default" ) ) )
#  define Q_DLL_EXPORT __attribute__ ( ( visibility( "default" ) ) )
#  define Q_DLL_LOCAL  __attribute__ ( ( visibility( "hidden" ) ) )
# else
#  define Q_DLL_IMPORT
#  define Q_DLL_EXPORT
#  define Q_DLL_LOCAL
# endif
#endif

#ifdef _WIN32
#define Q_alloca _alloca
#else
#define Q_alloca alloca
#endif