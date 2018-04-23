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

#include <SDL_cpuinfo.h>
#include <SDL_timer.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

#include "q_types.h"
#include "q_threads.h"

typedef struct thread_pool_s
{
	SDL_mutex *mutex;

	thread_t *threads;
	uint16_t num_threads;
} thread_pool_t;

static thread_pool_t thread_pool;

/*
===============
Thread_Run

We wrap the user's function with our own. The function pointer and data to
manipulate are set on the thread_t itself, allowing us to simply take the
thread as a parameter here.
===============
*/
static int Thread_Run(void *p)
{
	thread_t *t = (thread_t *)p;

	while(thread_pool.mutex)
	{
		SDL_mutexP (t->mutex);

		if(t->status == THREAD_RUNNING)
		{
			// invoke the user function
			t->Run(t->data);

			t->Run = NULL;
			t->data = NULL;

			t->status = THREAD_WAIT;
		}
		else
		{
			SDL_CondWait (t->cond, t->mutex);
		}

		SDL_mutexV (t->mutex);
	}

	return 0;
}

/*
===============
Thread_Init_

Initializes the threads backing the thread pool
===============
*/
static void Thread_Init_ (uint16_t num_threads)
{
	if (num_threads == 0)
		num_threads = SDL_GetCPUCount ();
	else if (num_threads == -1)
		num_threads = 0;
	else if (num_threads > MAX_THREADS)
		num_threads = MAX_THREADS;

	thread_pool.num_threads = num_threads;
	if (thread_pool.num_threads)
	{
		thread_pool.threads = malloc (sizeof(thread_t) * thread_pool.num_threads);

		thread_t *t = thread_pool.threads;
		uint16_t i = 0;

		for (i = 0; i < thread_pool.num_threads; i++, t++)
		{
			t->cond = SDL_CreateCond ();
			t->mutex = SDL_CreateMutex ();
			t->thread = SDL_CreateThread (Thread_Run, __func__, t);
		}
	}
}

/*
===============
Thread_Shutdown_
===============
*/
static void Thread_Shutdown_ (void)
{
	if (thread_pool.num_threads)
	{
		thread_t *t = thread_pool.threads;
		uint16_t i = 0;

		for (i = 0; i < thread_pool.num_threads; i++, t++)
		{
			Thread_Wait (t);
			SDL_CondSignal (t->cond);
			SDL_WaitThread (t->thread, NULL);
			SDL_DestroyCond (t->cond);
			SDL_DestroyMutex (t->mutex);
		}

		free (thread_pool.threads);
	}
}

/*
===============
Thread_Create_

Creates a new thread to run the specified function. Callers must use
Thread_Wait on the returned handle to release the thread when finished.
===============
*/
thread_t *Thread_Create_(char *name, ThreadRunFunc run, void *data)
{
	thread_t *t = thread_pool.threads;
	uint16_t i = 0;

	// if threads are available, find an idle one and dispatch it
	if(thread_pool.num_threads)
	{
		SDL_mutexP (thread_pool.mutex);

		for(i = 0; i < thread_pool.num_threads; i++, t++)
		{
			if(t->status == THREAD_IDLE)
			{
				SDL_mutexP (t->mutex);

				// if the thread is idle, dispatch it
				if (t->status == THREAD_IDLE)
				{
					strncpy (t->name, name, sizeof(t->name));

					t->Run = run;
					t->data = data;

					t->status = THREAD_RUNNING;

					SDL_mutexV (t->mutex);
					SDL_CondSignal (t->cond);

					break;
				}

				SDL_mutexV (t->mutex);
			}
		}

		SDL_mutexV (thread_pool.mutex);
	}

	// if we failed to allocate a thread, run the function in this thread
	if (i == thread_pool.num_threads)
	{
		if (thread_pool.num_threads)
		{
			t = NULL;
			run (data);
		}
	}

	return t;
}

/*
===============
Thread_Wait

Wait for the specified thread to complete.
===============
*/
void Thread_Wait(thread_t *t)
{
	if (!t || t->status == THREAD_IDLE)
		return;

	while(t->status != THREAD_WAIT)
		SDL_Delay (0);

	t->status = THREAD_IDLE;
}

/*
===============
Thread_Count

Returns the number of threads in the pool.
===============
*/
uint16_t Thread_Count(void)
{
	return thread_pool.num_threads;
}

/*
===============
Thread_Init

Initializes the thread pool.
===============
*/
void Thread_Init(uint16_t num_threads)
{
	memset (&thread_pool, 0, sizeof(thread_pool));

	thread_pool.mutex = SDL_CreateMutex ();

	Thread_Init_ (num_threads);
}

/*
===============
Thread_Shutdown

Shuts down the thread pool.
===============
*/
void Thread_Shutdown (void)
{
	if (thread_pool.mutex)
	{
		SDL_DestroyMutex (thread_pool.mutex);
		thread_pool.mutex = NULL;
	}

	Thread_Shutdown_ ();

	memset (&thread_pool, 0, sizeof(thread_pool));
}
