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
// sys.c

#include "qcommon.h"
#include "input.h"

#include "SDL.h"
#ifdef _WIN32
#include "winsock.h"
#endif
#include <errno.h>
#include <float.h>
#include <fcntl.h>
#include <stdio.h>
#include <direct.h>
#include <io.h>
#include <conio.h>

unsigned	sys_frame_time;

qboolean	stdin_active = true;
cvar_t		*nostdout;

/*
===============================================================================

SYSTEM IO

===============================================================================
*/

void Sys_Error (char *error, ...)
{
	va_list		argptr;
	char		text[1024];

	CL_Shutdown ();
	Qcommon_Shutdown ();

	va_start (argptr, error);
	vsprintf (text, error, argptr);
	va_end (argptr);

	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", text, NULL);

	// shut down QHOST hooks if necessary
	//DeinitConProc ();

	exit (1);
}

void Sys_Quit (void)
{
	CL_Shutdown ();
	Qcommon_Shutdown ();

	/*
	if (dedicated && dedicated->value)
		FreeConsole ();

	// shut down QHOST hooks if necessary
	DeinitConProc ();
	*/

	exit (0);
}


//================================================================


/*
================
Sys_Init
================
*/
void Sys_Init (void)
{
}


/*
================
Sys_ConsoleInput
================
*/
char *Sys_ConsoleInput (void)
{
	static char text[256];
	int len;
	fd_set fdset;
	struct timeval timeout;

	if (!dedicated || !dedicated->value)
	{
		return (NULL);
	}

	if (!stdin_active)
	{
		return (NULL);
	}

	FD_ZERO(&fdset);
	FD_SET(0, &fdset); /* stdin */
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	if ((select(1, &fdset, NULL, NULL, &timeout) == -1) || !FD_ISSET(0, &fdset))
	{
		return (NULL);
	}

	len = read(0, text, sizeof(text));

	if (len == 0) /* eof! */
	{
		stdin_active = false;
		return (NULL);
	}

	if (len < 1)
	{
		return (NULL);
	}

	text[len - 1] = 0; /* rip off the /n and terminate */

	return (text);
}


/*
================
Sys_ConsoleOutput

Print text to the dedicated console
================
*/
void Sys_ConsoleOutput (char *string)
{
	if (nostdout && nostdout->value)
	{
		return;
	}

	fputs(string, stdout);
}


/*
================
Sys_SendKeyEvents

Send Key_Event calls
================
*/
void Sys_SendKeyEvents (void)
{
#ifndef DEDICATED_ONLY
	IN_Update();
#endif

	// grab frame time
	sys_frame_time = Sys_Milliseconds ();	// FIXME: should this be at start?
}


/*
================
Sys_GetClipboardData
================
*/
char *Sys_GetClipboardData (void)
{
	char *data = NULL;
	char *cliptext;

	if ((cliptext = SDL_GetClipboardText()) != NULL) {
		if (cliptext[0] != '\0') {
			size_t bufsize = strlen(cliptext) + 1;

			data = Z_Malloc(bufsize);
			strncpy(data, cliptext, bufsize);

			// find first listed char and set to '\0'
			strtok(data, "\n\r\b");
		}

		SDL_free(cliptext);
	}

	return data;
}


/*
========================================================================

GAME DLL

========================================================================
*/

static void	*game_library;

/*
=================
Sys_UnloadGame
=================
*/
void Sys_UnloadGame (void)
{
	SDL_UnloadObject(game_library);

	if (!game_library)
		Com_Error (ERR_FATAL, "FreeLibrary failed for game library");

	game_library = NULL;
}

/*
=================
Sys_GetGameAPI

Loads the game dll
=================
*/
void *Sys_GetGameAPI (void *parms)
{
	void	* (*GetGameAPI) (void *);
	char	name[MAX_OSPATH];
	char	*path;
	char	cwd[MAX_OSPATH];
	const char *gamename = "gamex86.dll";

	if (game_library)
		Com_Error(ERR_FATAL, "Sys_GetGameAPI without Sys_UnloadingGame");

	// check the current directory for other development purposes
	_getcwd(cwd, sizeof(cwd));
	Com_sprintf(name, sizeof(name), "%s/%s", cwd, gamename);
	game_library = SDL_LoadObject(name);
	if (!game_library)
	{
		// now run through the search paths
		path = NULL;

		while (1)
		{
			path = FS_NextPath(path);

			if (!path)
				return NULL; // couldn't find one anywhere

			Com_sprintf(name, sizeof(name), "%s/%s", path, gamename);
			game_library = SDL_LoadObject(name);
			if (game_library)
			{
				break;
			}
		}
	}

	GetGameAPI = (void *)SDL_LoadFunction(game_library, "GetGameAPI");
	if (!GetGameAPI)
	{
		Sys_UnloadGame();
		return NULL;
	}

	return GetGameAPI(parms);
}

//=======================================================================

/*
==================
main
==================
*/
int main(int argc, char **argv)
{
	int time, oldtime, newtime;

	printf("\n Quake II v%4.2f\n", VERSION);
	printf("=====================\n\n");

	Qcommon_Init(argc, argv);

	nostdout = Cvar_Get("nostdout", "0", 0);

	oldtime = Sys_Milliseconds();

	// The legendary mainloop
	while (1)
	{
		// find time spent rendering last frame
		do
		{
			newtime = Sys_Milliseconds();
			time = newtime - oldtime;
		} while (time < 1);

		Qcommon_Frame(time);

		oldtime = newtime;
	}
}
