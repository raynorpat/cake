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

#include "q_types.h"
#include "q_threads.h"

#include "q2map.h"
#include "threads.h"
extern qboolean verbose;

thread_work_t thread_work;

/*
=============
GetThreadWork

return an iteration of work, updating progress when appropriate.
=============
*/
static int GetThreadWork (void)
{
	int	r;
	int	f;

	ThreadLock ();

	if (thread_work.index == thread_work.count)
	{
		// done
		ThreadUnlock ();
		return -1;
	}

	// update work fraction and output progress if desired
	f = 10 * thread_work.index / thread_work.count;
	if (f != thread_work.fraction)
	{
		thread_work.fraction = f;
		if (thread_work.progress && !verbose)
		{
			Con_Print("%i...", f);
			fflush(stdout);
		}
	}

	// assign the next work iteration
	r = thread_work.index;
	thread_work.index++;

	ThreadUnlock ();

	return r;
}

// generic function pointer to actual work to be done
static void(*WorkFunction)(int);

/*
=============
ThreadWork

Shared work entry point by all threads.
Retrieve and perform chunks of work iteratively until work is finished.
=============
*/
static void ThreadWork(void *p)
{
	int		work;

	while (1)
	{
		work = GetThreadWork ();
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
	int i;

	if (!thread_pool.num_threads)
	{
		ThreadWork(0);
		return;
	}

	lock = SDL_CreateMutex();

	for (i = 0; i < thread_pool.num_threads; i++)
		Thread_Create(ThreadWork, NULL);

	for (i = 0; i < thread_pool.num_threads; i++)
		Thread_Wait(&thread_pool.threads[i]);

	SDL_DestroyMutex(lock);
	lock = NULL;
}

/*
=============
RunThreadsOn

Entry point for all thread work requests.
=============
*/
void RunThreadsOn(int workcount, qboolean progress, void(*func)(int))
{
	time_t start, end;

	thread_work.index = 0;
	thread_work.count = workcount;
	thread_work.fraction = -1;
	thread_work.progress = progress;

	WorkFunction = func;

	start = time(NULL);

	RunThreads();

	end = time(NULL);

	if (thread_work.progress)
		Con_Print(" (%i seconds)\n", (int)(end - start));
}
