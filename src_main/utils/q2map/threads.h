/*
===========================================================================
Copyright (C) 1997-2006 Id Software, Inc.

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

#include "q_threads.h"

typedef struct semaphores_s
{
	SDL_sem *active_portals;
	SDL_sem *active_nodes;
	SDL_sem *vis_nodes;
	SDL_sem *nonvis_nodes;
	SDL_sem *active_brushes;
} semaphores_t;

extern semaphores_t semaphores;

void Sem_Init (void);
void Sem_Shutdown (void);

typedef struct thread_work_s
{
	int index;				// current work cycle
	int count;				// total work cycles
	int fraction;			// last fraction of work completed (tenths)
	qboolean progress;		// are we reporting progress
} thread_work_t;

extern thread_work_t thread_work;

typedef void(*ThreadWorkFunc)(int32_t);

void ThreadLock (void);
void ThreadUnlock (void);
void RunThreadsOn (int32_t workcount, qboolean progress, ThreadWorkFunc func);
