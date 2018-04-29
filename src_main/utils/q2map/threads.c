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

#include <SDL_thread.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

#include "q_types.h"
#include "q_threads.h"

#include "q2map.h"
#include "threads.h"
extern qboolean verbose;

semaphores_t semaphores;
thread_work_t thread_work;

/*
=============
Sem_Init

Initializes the shared semaphores that threads will touch.
=============
*/
void Sem_Init (void)
{
	memset (&semaphores, 0, sizeof(semaphores));

	semaphores.active_portals = SDL_CreateSemaphore (0);
	semaphores.active_nodes = SDL_CreateSemaphore (0);
	semaphores.vis_nodes = SDL_CreateSemaphore (0);
	semaphores.nonvis_nodes = SDL_CreateSemaphore (0);
	semaphores.active_brushes = SDL_CreateSemaphore (0);
}

/*
=============
Sem_Shutdown

Shuts down shared semaphores, releasing any resources they hold.
=============
*/
void Sem_Shutdown (void)
{
	SDL_DestroySemaphore (semaphores.active_portals);
	SDL_DestroySemaphore (semaphores.active_nodes);
	SDL_DestroySemaphore (semaphores.vis_nodes);
	SDL_DestroySemaphore (semaphores.nonvis_nodes);
	SDL_DestroySemaphore (semaphores.active_brushes);
}

/*
=============
GetThreadWork

return an iteration of work, updating progress when appropriate.
=============
*/
static int GetThreadWork (void)
{
	int32_t r, f;

	ThreadLock ();

	if (thread_work.index == thread_work.count || !was_init)
	{
		// done or killed
		ThreadUnlock ();
		return -1;
	}

	// update work fraction and output progress if desired
	f = 50 * thread_work.index / thread_work.count;
	if (f != thread_work.fraction)
	{
		if (thread_work.progress && !verbose)
		{
			for (int32_t i = thread_work.fraction; i < f; i++)
			{
				if (i % 5 == 0)
					Con_Print ("%i", i / 5);
				else
					Con_Print (".");
				fflush (stdout);
			}
		}
		thread_work.fraction = f;
	}

	// assign the next work iteration
	r = thread_work.index;
	thread_work.index++;

	ThreadUnlock ();

	return r;
}

// generic function pointer to actual work to be done
static ThreadWorkFunc WorkFunction;

/*
=============
ThreadWork

Shared work entry point by all threads.
Retrieve and perform chunks of work iteratively until work is finished.
=============
*/
static void ThreadWork(void *p __attribute__((unused)))
{
	while (1)
	{
		int32_t work = GetThreadWork ();
		if (work == -1)
			break;
		WorkFunction(work);
	}
}

SDL_mutex *lock = NULL;

/*
=============
ThreadLock
=============
*/
void ThreadLock(void)
{
	if (!lock)
		return;

	SDL_mutexP(lock);
}

/*
=============
ThreadUnlock
=============
*/
void ThreadUnlock(void)
{
	if (!lock)
		return;

	SDL_mutexV(lock);
}

/*
=============
RunThreads
=============
*/
static void RunThreads(void)
{
	const size_t thread_count = Thread_Count();
	thread_t *threads[MAX_THREADS];

	if (thread_count == 0)
	{
		ThreadWork (0);
		return;
	}

	lock = SDL_CreateMutex();

	for (size_t i = 0; i < thread_count; i++)
		threads[i] = Thread_Create (ThreadWork, NULL);

	for (size_t i = 0; i < thread_count; i++)
		Thread_Wait (threads[i]);

	SDL_DestroyMutex(lock);
	lock = NULL;
}

/*
=============
RunThreadsOn

Entry point for all thread work requests.
=============
*/
void RunThreadsOn(int32_t workcount, qboolean progress, ThreadWorkFunc func)
{
	time_t start, end;

	thread_work.index = 0;
	thread_work.count = workcount;
	thread_work.fraction = 0;
	thread_work.progress = progress;

	WorkFunction = func;

	start = time (NULL);

	RunThreads ();

	end = time (NULL);

	if (thread_work.progress)
		Con_Print (" (%i seconds)\n", (int)(end - start));
}
