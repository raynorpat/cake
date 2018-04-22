/*
===========================================================================
Copyright (C) 1997-2006 Id Software, Inc.
Copyright (C) 2017-2018 Pat 'raynorpat' Raynor <raynorpat@gmail.com>

This file is part of Quake 2 Tools source code.

Quake 2 Tools source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake 2 Tools source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake 2 Tools source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

int BSP_Main (int argc, char **argv);
int Vis_Main (int argc, char **argv);
int Light_Main (int argc, char **argv);

#if !defined(__GNUC__)
#define	__attribute__(x)
#endif

#define MAX_PRINT_MSG 4096

void Con_Open(void);
void Con_Close(void);

void Con_Error(const char *fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));
void Con_Print(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void Con_Verbose(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
