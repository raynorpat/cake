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

#define MAX_THREADS 16

typedef enum thread_state_s
{
	THREAD_IDLE,
	THREAD_RUN,
	THREAD_DONE
} thread_state_t;

typedef struct thread_s
{
	thread_state_t state;
	SDL_Thread *thread;
	void (*function)(void *data);
	void *data;
} thread_t;

typedef struct thread_pool_s
{
	thread_t *threads;
	int num_threads;
	SDL_mutex *mutex;
	qboolean shutdown;
} thread_pool_t;

extern thread_pool_t thread_pool;
extern int numthreads;

thread_t *Thread_Create (void (function)(void *data), void *data);
void Thread_Wait (thread_t *t);
void Thread_Shutdown (void);
void Thread_Init (void);

#endif