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

// vid_gl.c - This file contains ALL of the specific stuff having to do with the OpenGL refresh. 
#include <assert.h>
#include <SDL.h>

#include <GL/glew.h>
#include <GL/gl.h>
#include <math.h>

#include "ref_public.h"
#include "vid_public.h"

extern cvar_t *gl_swapinterval;

static SDL_Window* window = NULL;
static SDL_GLContext context = NULL;

static qboolean vsyncActive = false;
int vid_refreshRate = -1;

void VID_ShutdownWindow(void);

static int IsFullscreen()
{
	if (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) {
		return 1;
	} else if (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) {
		return 2;
	} else {
		return 0;
	}
}

static qboolean CreateSDLWindow(int flags, int w, int h)
{
	int windowPos = SDL_WINDOWPOS_UNDEFINED;
	// TODO: support fullscreen on different displays with SDL_WINDOWPOS_UNDEFINED_DISPLAY(displaynum)
	window = SDL_CreateWindow("Quake II", windowPos, windowPos, w, h, flags);

	return window != NULL;
}

static qboolean GetWindowSize(int* w, int* h)
{
	if (window == NULL || w == NULL || h == NULL)
		return false;

	SDL_DisplayMode m;
	if (SDL_GetWindowDisplayMode(window, &m) != 0) {
		Com_Printf("Can't get Displaymode: %s\n", SDL_GetError());
		return false;
	}
	*w = m.w;
	*h = m.h;

	return true;
}


// called by VID_InitWindow() before creating window,
// returns flags for SDL window creation, -1 on error
static int PrepareForWindow(void)
{
	unsigned int flags = 0;
	int msaa_samples = 0;

	 // Default OpenGL is fine.
	if (SDL_GL_LoadLibrary(NULL) < 0) {
		// TODO: is there a better way?
		Sys_Error("Couldn't load libGL: %s!", SDL_GetError());
		return -1;
	}

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	int contextFlags = SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG;
	if (contextFlags != 0) {
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, contextFlags);
	}

	// Initiate the flags
	flags = SDL_WINDOW_OPENGL;

	return flags;
}


static void SetSwapInterval(void)
{
	// Set vsync - TODO: -1 could be set for "late swap tearing"
	SDL_GL_SetSwapInterval(gl_swapinterval->value ? 1 : 0);
	vsyncActive = SDL_GL_GetSwapInterval() != 0;
}


static int Init_GL_Context(void* win)
{
	char title[40] = { 0 };

	if (win == NULL) {
		Sys_Error("Init_GL_Context() must not be called with NULL argument!");
		return false;
	}
	window = (SDL_Window*)win;

	context = SDL_GL_CreateContext(window);
	if (context == NULL) {
		VID_Printf(PRINT_ALL, "Init_GL_Context(): Creating OpenGL Context failed: %s\n", SDL_GetError());
		window = NULL;
		return false;
	}

	// this must be done after creating the window
	SetSwapInterval();

	// init glew
	glewExperimental = true;
	if (GLEW_OK != glewInit()) {
		VID_Printf(PRINT_ALL, "Init_GL_Context(): loading OpenGL function pointers failed!\n");
		return false;
	} else {
		VID_Printf(PRINT_ALL, "Successfully loaded OpenGL function pointers using glew!\n");
	}

	// set the window title
	SDL_SetWindowTitle(window, va("Quake II %d", VERSION));

	return true;
}

/*
* Shuts the OpenGL backend down
*/
void VID_Shutdown_GL(qboolean contextOnly)
{
	// Clear the backbuffer and make it current. 
	// Only do this if we have a context, though.
	if (window) {
		if (context) {
			glClearColor(0.0, 0.0, 0.0, 0.0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			SDL_GL_SwapWindow(window);

			SDL_GL_DeleteContext(context);
			context = NULL;
		}
	}

	window = NULL;

	if (!contextOnly) {
		VID_ShutdownWindow();
	}
}


/*
* Initializes the OpenGL window
*/
qboolean VID_InitWindow(int fullscreen, int *pwidth, int *pheight)
{
	int flags;
	int curWidth, curHeight;
	int width = *pwidth;
	int height = *pheight;
	unsigned int fs_flag = 0;

	// reset fresh rate
	vid_refreshRate = -1;

	if (fullscreen == 1) {
		fs_flag = SDL_WINDOW_FULLSCREEN_DESKTOP;
	} else if (fullscreen == 2) {
		fs_flag = SDL_WINDOW_FULLSCREEN;
	}

	if (GetWindowSize(&curWidth, &curHeight) && (curWidth == width) && (curHeight == height)) {
		// If we want fullscreen, but aren't
		if (fullscreen != IsFullscreen()) {
			SDL_SetWindowFullscreen(window, fs_flag);
			Cvar_SetValue("vid_fullscreen", fullscreen);
		}

		// Are we now?
		if (fullscreen == IsFullscreen()) {
			return true;
		}
	}

	// Is the window surface used?
	if (window) {
		VID_Shutdown_GL(true);
		SDL_DestroyWindow(window);
		window = NULL;
	}

	// Create the window
	VID_NewWindow(width, height);

	// let renderer prepare things (set OpenGL attributes)
	flags = PrepareForWindow();
	if (flags == -1) {
		// hopefully PrepareForWindow() logged an error
		return false;
	}

	if (fs_flag) {
		flags |= fs_flag;
	}

	while (1) {
		if (!CreateSDLWindow(flags, width, height)) {
			if (width != 640 || height != 480 || (flags & fs_flag)) {
				Com_Printf("SDL SetVideoMode failed: %s\n", SDL_GetError());
				Com_Printf("Reverting to windowed gl_mode 5 (640x480).\n");

				// Try to recover
				Cvar_SetValue("gl_mode", 5);
				Cvar_SetValue("vid_fullscreen", 0);
				VID_NewWindow(width, height);
				*pwidth = width = 640;
				*pheight = height = 480;
				flags &= ~fs_flag;
			} else {
				Com_Error(ERR_FATAL, "Failed to revert to gl_mode 5. Exiting...\n");
				return false;
			}
		} else {
			break;
		}
	}

	if (!Init_GL_Context(window)) {
		// Init_GL_Context() should have logged an error
		return false;
	}

	// Set the window icon, this must be done after creating the window
	Sys_SetIcon();

	// No cursor
	SDL_ShowCursor(0);

	return true;
}


/*
* (Un)grab Input
*/
void VID_GrabInput(qboolean grab)
{
	if (window != NULL) {
		SDL_SetWindowGrab(window, grab ? SDL_TRUE : SDL_FALSE);
	}

	if (SDL_SetRelativeMouseMode(grab ? SDL_TRUE : SDL_FALSE) < 0) {
		Com_Printf("WARNING: Setting Relative Mousemode failed, reason: %s\n", SDL_GetError());
		Com_Printf("         You should probably update to SDL 2.0.3 or newer!\n");
	}
}


/*
VID_ShutdownWindow

This routine does all OS specific shutdown procedures for the OpenGL subsystem.
*/
void VID_ShutdownWindow(void)
{
	if (window) {
		// cleanly ungrab input (needs window)
		VID_GrabInput(false);
		SDL_DestroyWindow(window);
	}

	window = NULL;

	if (SDL_WasInit(SDL_INIT_EVERYTHING) == SDL_INIT_VIDEO)	{
		SDL_Quit();
	} else {
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
	}
}


/*
VID_Init_GL

This routine is responsible for initializing the OS specific portions of OpenGL.
*/
qboolean VID_Init_GL (void)
{
	if (!SDL_WasInit(SDL_INIT_VIDEO)) {
		if (SDL_Init(SDL_INIT_VIDEO) == -1) {
			Com_Printf("Couldn't init SDL video: %s.\n", SDL_GetError());
			return false;
		}

		const char* driverName = SDL_GetCurrentVideoDriver();
		Com_Printf("SDL video driver is \"%s\".\n", driverName);
	}

	return true;
}


/*
VID_IsVSyncActive
*/
qboolean VID_IsVSyncActive(void)
{
	return vsyncActive;
}

/*
VID_GetRefreshRate

Returns the current display refresh rate.
*/
int VID_GetRefreshRate(void)
{
	if (vid_refreshRate == -1)
	{
		SDL_DisplayMode mode;

		int i = SDL_GetWindowDisplayIndex(window);
		if (i >= 0 && SDL_GetCurrentDisplayMode(i, &mode) == 0)
		{
			vid_refreshRate = mode.refresh_rate;
		}

		if (vid_refreshRate <= 0)
		{
			// apparently the stuff above failed, use default
			vid_refreshRate = 60;
		}
	}

	return vid_refreshRate;
}


/*
VID_BeginFrame
*/
void VID_BeginFrame (float camera_separation)
{
}


/*
VID_EndFrame

Responsible for doing a swapbuffer.
*/
extern void Draw_End2D(void);
extern void GL_UseProgram(GLuint progid);

void VID_EndFrame (void)
{
	Draw_End2D ();

	GL_UseProgram (0);

	SDL_GL_SwapWindow(window);
}
