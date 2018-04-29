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

#include <SDL.h>

#include "cmdlib.h"
#include "q2map.h"
#include "threads.h"

qboolean was_init = false;

/*
============
main
============
*/
int main (int argc, char **argv)
{
	int32_t num_threads = 0;

	Con_Open ();

	Con_Print ("q2map v1.0 (c) 1996-2006 Id Software, Inc. and cake contributors\n");
	Con_Print ("q2map v1.1 (c) 2017-2018 Pat 'raynorpat' Raynor <raynorpat@gmail.com>\n\n");
	
	if(argc < 2)
		goto showUsage;

	SDL_Init (SDL_INIT_TIMER);

	Thread_Init (num_threads);
	Sem_Init ();

	Con_Print ("Using %u threads\n", Thread_Count());

	was_init = true;

	// check for general program options
	if(!strcmp(argv[1], "-bsp"))
	{
		BSP_Main(argc - 1, argv + 1);
		goto exit;
	}
	if(!strcmp(argv[1], "-vis"))
	{
		Vis_Main(argc - 1, argv + 1);
		goto exit;
	}
	if(!strcmp(argv[1], "-light"))
	{
		Light_Main(argc - 1, argv + 1);
		goto exit;
	}

showUsage:
	Con_Error("usage: q2map [-<switch> [-<switch> ...]] <mapname>\n"
		  "\n"
		  "Switches:\n"
		  "   bsp        	 = compile MAP to BSP\n"
		  "   vis            = compute visibility\n"
		  "   light          = compute lighting\n");

exit:
	was_init = false;

	Thread_Shutdown ();
	Sem_Shutdown ();

	SDL_Quit ();

	puts ("\nPress any key to close..\n");
	getchar ();

	exit (0);
	return 0;
}
