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

#include "q_types.h"
#include "q_threads.h"

thread_pool_t thread_pool;
int numthreads;

#ifdef _WIN32
#include <Windows.h>
static void usleep(__int64 usec)
{
	HANDLE timer;
	LARGE_INTEGER ft;

	ft.QuadPart = -(10 * usec); // Convert to 100 nanosecond interval, negative value indicates relative time

	timer = CreateWaitableTimer(NULL, TRUE, NULL);
	SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
	WaitForSingleObject(timer, INFINITE);
	CloseHandle(timer);
}
#endif

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

	while(!thread_pool.shutdown)
	{
		if(t->state == THREAD_RUN)
		{
			// invoke the user function
			t->function(t->data);
			t->state = THREAD_DONE;
		}

		usleep(0);
	}

	return 0;
}


/*
===============
Thread_Create

Creates a new thread to run the specified function. Callers must use
Thread_Wait on the returned handle to release the thread when it finishes.
===============
*/
thread_t *Thread_Create(void (function)(void *data), void *data)
{
	if(thread_pool.num_threads)
	{
		thread_t *t;
		int i;

		SDL_mutexP(thread_pool.mutex);

		for(i = 0, t = thread_pool.threads; i < thread_pool.num_threads; i++, t++)
		{
			if(t->state == THREAD_IDLE)
			{
				t->function = function;
				t->data = data;
				t->state = THREAD_RUN;
				break;
			}
		}

		SDL_mutexV(thread_pool.mutex);

		if(i < thread_pool.num_threads)
			return t;
	}

	function(data);  // call the function in this thread

	return NULL;
}

/*
===============
Thread_Wait

Wait for the specified thread to complete.
===============
*/
void Thread_Wait(thread_t *t)
{
	if(!t)
		return;

	while(t->state == THREAD_RUN)
		usleep(0);

	t->state = THREAD_IDLE;
}

/*
===============
Thread_Shutdown

Terminates any running threads, resetting the thread pool.
===============
*/
void Thread_Shutdown(void)
{
	thread_t *t;
	int i;

	if(!thread_pool.num_threads)
		return;

	thread_pool.shutdown = true;  // inform threads to quit

	for(i = 0, t = thread_pool.threads; i < thread_pool.num_threads; i++, t++)
		SDL_WaitThread(t->thread, NULL);

	free(thread_pool.threads);

	SDL_DestroyMutex(thread_pool.mutex);
}

/*
===============
Thread_Init

Initializes the thread pool.
===============
*/
void Thread_Init(void)
{
	thread_t *t;
	int i;

	memset(&thread_pool, 0, sizeof(thread_pool));

	numthreads = 2; // the default number of threads (cores) to utilize
	if(numthreads > MAX_THREADS)
		numthreads = MAX_THREADS;
	else if(numthreads < 0)
		numthreads = 0;

	thread_pool.num_threads = numthreads;

	if(thread_pool.num_threads)
	{
		thread_pool.threads = malloc(sizeof(thread_t) * thread_pool.num_threads);

		for(i = 0, t = thread_pool.threads; i < thread_pool.num_threads; i++, t++)
			t->thread = SDL_CreateThread(Thread_Run, "", t);

		thread_pool.mutex = SDL_CreateMutex();
	}
}
