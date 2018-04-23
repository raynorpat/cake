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

#ifndef __THREAD_H__
#define __THREAD_H__

#include <SDL_thread.h>

#define MAX_THREADS 128

typedef enum thread_status_e
{
	THREAD_IDLE,
	THREAD_RUNNING,
	THREAD_WAIT
} thread_status_t;

typedef void(*ThreadRunFunc)(void *data);

typedef struct thread_s
{
	SDL_Thread *thread;
	SDL_cond *cond;
	SDL_mutex *mutex;
	char name[64];
	volatile thread_status_t status;
	ThreadRunFunc Run;
	void *data;
} thread_t;

thread_t *Thread_Create_ (char *name, ThreadRunFunc Run, void *data);
#define Thread_Create(f, d) Thread_Create_(#f, f, d)
void Thread_Wait (thread_t *t);
uint16_t Thread_Count (void);
void Thread_Init (uint16_t num_threads);
void Thread_Shutdown (void);

#endif
