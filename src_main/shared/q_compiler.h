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

// disable some annoying MSVC warnings
#ifdef _MSC_VER
#pragma warning(disable : 4244) // conversion from 'x' to 'y', possible loss of data
#pragma warning(disable : 4136) // conversion between different floating-point types
#pragma warning(disable : 4051) // type conversion; possible loss of data
#pragma warning(disable : 4018) // signed/unsigned mismatch
#pragma warning(disable : 4305) // truncation from 'x' to 'y'
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
#if defined _WIN32 || defined _WIN64 || defined __CYGWIN__
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

#ifdef _MSC_VER
#define Q_alloca _alloca
#else
#define Q_alloca alloca
#endif
