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

void VID_ShutdownWindow(void);

// used for grabbing the OpenGL mode info
extern qboolean R_GetModeInfo(int *width, int *height, float *windowAspect, int mode);

// used for buffer swap
extern void Draw_End2D(void);
extern void GL_UseProgram(GLuint progid);

// used for video export
extern void CL_WriteAVIVideoFrame(const byte *imageBuffer, int size);

/*
===============
VID_CompareModes
===============
*/
static int VID_CompareModes(const void *a, const void *b)
{
	const float ASPECT_EPSILON = 0.001f;
	SDL_Rect *modeA = (SDL_Rect *)a;
	SDL_Rect *modeB = (SDL_Rect *)b;
	float aspectA = (float)modeA->w / (float)modeA->h;
	float aspectB = (float)modeB->w / (float)modeB->h;
	int areaA = modeA->w * modeA->h;
	int areaB = modeB->w * modeB->h;
	float aspectDiffA = fabs(aspectA - viddef.displayAspect);
	float aspectDiffB = fabs(aspectB - viddef.displayAspect);
	float aspectDiffsDiff = aspectDiffA - aspectDiffB;

	if (aspectDiffsDiff > ASPECT_EPSILON)
		return 1;
	else if (aspectDiffsDiff < -ASPECT_EPSILON)
		return -1;
	else
		return areaA - areaB;
}

/*
===============
VID_DetectAvailableModes
===============
*/
static void VID_DetectAvailableModes(void)
{
	int i, j;
	char buf[MAX_STRING_CHARS] = { 0 };
	int numSDLModes;
	SDL_Rect *modes;
	int numModes = 0;

	SDL_DisplayMode windowMode;
	int display = SDL_GetWindowDisplayIndex(window);
	if (display < 0)
	{
		VID_Printf(PRINT_DEVELOPER, S_COLOR_RED "Couldn't get window display index, no resolutions detected: %s\n", SDL_GetError());
		return;
	}
	numSDLModes = SDL_GetNumDisplayModes(display);

	if (SDL_GetWindowDisplayMode(window, &windowMode) < 0 || numSDLModes <= 0)
	{
		VID_Printf(PRINT_DEVELOPER, S_COLOR_RED "Couldn't get window display mode, no resolutions detected: %s\n", SDL_GetError());
		return;
	}

	modes = SDL_calloc((size_t)numSDLModes, sizeof(SDL_Rect));
	if (!modes)
	{
		VID_Error(ERR_FATAL, S_COLOR_RED "Out of memory");
	}

	for (i = 0; i < numSDLModes; i++)
	{
		SDL_DisplayMode mode;

		if (SDL_GetDisplayMode(display, i, &mode) < 0)
			continue;

		if (!mode.w || !mode.h)
		{
			VID_Printf(PRINT_ALL, S_COLOR_GREEN "Display supports any resolution\n");
			SDL_free(modes);
			return;
		}

		if (windowMode.format != mode.format)
			continue;

		// SDL can give the same resolution with different refresh rates.
		// Only list resolution once.
		for (j = 0; j < numModes; j++)
		{
			if (mode.w == modes[j].w && mode.h == modes[j].h)
				break;
		}

		if (j != numModes)
			continue;

		modes[numModes].w = mode.w;
		modes[numModes].h = mode.h;
		numModes++;
	}

	if (numModes > 1)
		qsort(modes, numModes, sizeof(SDL_Rect), VID_CompareModes);

	for (i = 0; i < numModes; i++)
	{
		const char *newModeString = va("%ux%u ", modes[i].w, modes[i].h);

		if (strlen(newModeString) < (int)sizeof(buf) - strlen(buf))
			strcat(buf, newModeString);
		else
			VID_Printf(PRINT_DEVELOPER, "Skipping mode %ux%u, buffer too small\n", modes[i].w, modes[i].h);
	}

	if (*buf)
	{
		buf[strlen(buf) - 1] = 0;
		VID_Printf(PRINT_ALL, "Available modes: '%s'\n", buf);
		Cvar_ForceSet ("r_availableModes", buf);
	}
	SDL_free(modes);
}

/*
==============
VID_GL_GetRefreshRate

Returns the current display refresh rate.
==============
*/
int VID_GL_GetRefreshRate(void)
{
	if (vid_refreshrate->value > -1)
	{
		viddef.refreshRate = ceil(vid_refreshrate->value);
	}

	if (viddef.refreshRate == -1)
	{
		SDL_DisplayMode mode;

		int i = SDL_GetWindowDisplayIndex(window);
		if (i >= 0 && SDL_GetCurrentDisplayMode(i, &mode) == 0)
		{
			viddef.refreshRate = mode.refresh_rate;
		}

		if (viddef.refreshRate <= 0)
		{
			// apparently the stuff above failed, use default
			viddef.refreshRate = 60;
		}
	}

	// HACK: The refresh rate returned from SDL may be a frame too low.
	// Since it doesn't really hurt if we are a frame ahead, add an extra frame here.
	viddef.refreshRate++;

	return viddef.refreshRate;
}


/*
* Shuts the OpenGL backend down
*/
void VID_Shutdown_GL(qboolean destroyWindow)
{
	if (destroyWindow) {
		VID_ShutdownWindow();
	}
}


/*
* Initializes the OpenGL window
*/
vidrserr_t VID_InitWindow(int mode, int fullscreen)
{
	Uint32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
	SDL_DisplayMode desktopMode;
	int i = 0;
	int display = 0;
	int x = SDL_WINDOWPOS_UNDEFINED, y = SDL_WINDOWPOS_UNDEFINED;

	VID_Printf(PRINT_ALL, "Initializing OpenGL display\n");

	// if a window exists, note its display index
	if (window != NULL)
	{
		display = SDL_GetWindowDisplayIndex(window);
		if (display < 0)
			VID_Printf(PRINT_DEVELOPER, S_COLOR_RED "SDL_GetWindowDisplayIndex() failed: %s\n", SDL_GetError());
	}

	if (display >= 0 && SDL_GetDesktopDisplayMode(display, &desktopMode) == 0)
	{
		viddef.displayAspect = (float)desktopMode.w / (float)desktopMode.h;
		VID_Printf(PRINT_DEVELOPER, "Display aspect: %.3f\n", viddef.displayAspect);
	}
	else
	{
		memset(&desktopMode, 0, sizeof(SDL_DisplayMode));
		viddef.displayAspect = 1.333f;
		VID_Printf(PRINT_DEVELOPER, S_COLOR_RED "Cannot determine display aspect, assuming 1.333\n");
	}

	VID_Printf(PRINT_ALL, "...setting mode %d:", mode);

	if (mode == -2)
	{
		// use desktop video resolution
		if (desktopMode.h > 0)
		{
			viddef.width = desktopMode.w;
			viddef.height = desktopMode.h;
		}
		else
		{
			viddef.width = 1024;
			viddef.height = 768;
			VID_Printf(PRINT_ALL, S_COLOR_RED "Cannot determine display resolution, assuming 1024x768\n");
		}

		viddef.displayAspect = (float)viddef.width / (float)viddef.height;
	}
	else if (!R_GetModeInfo(&viddef.width, &viddef.height, &viddef.displayAspect, mode))
	{
		VID_Printf(PRINT_ALL, " invalid mode\n");
		return RSERR_INVALID_MODE;
	}
	VID_Printf(PRINT_ALL, " %d %d\n", viddef.width, viddef.height);

	viddef.refreshRate = -1;

#if 0
	// center window if not fullscreen
	if (!fullscreen)
	{
		x = (desktopMode.w / 2) - (viddef.width / 2);
		y = (desktopMode.h / 2) - (viddef.height / 2);
	}
#endif

	// destroy existing state if it exists
	if (context)
	{
		SDL_GL_DeleteContext(context);
		context = NULL;
	}

	if (window != NULL)
	{
		SDL_GetWindowPosition(window, &x, &y);
		VID_Printf(PRINT_DEVELOPER, "Existing window at %dx%d before being destroyed\n", x, y);
		SDL_DestroyWindow(window);
		window = NULL;
	}

	// set fullscreen flag
	if (fullscreen)
	{
		flags |= SDL_WINDOW_FULLSCREEN;
	}

	// create the window
	VID_NewWindow(viddef.width, viddef.height);

	int perChannelColorBits;
	int colorBits = 24;
	int depthBits = 24;
	int stencilBits = 8;
	int samples = 0;

	for (i = 0; i < 16; i++)
	{
		int testColorBits, testDepthBits, testStencilBits;
		int realColorBits[3];

		// 0 - default
		// 1 - minus colorBits
		// 2 - minus depthBits
		// 3 - minus stencil
		if ((i % 4) == 0 && i)
		{
			// one pass, reduce
			switch (i / 4)
			{
			case 2:
				if (colorBits == 24)
					colorBits = 16;
				break;
			case 1:
				if (depthBits == 24)
					depthBits = 16;
				else if (depthBits == 16)
					depthBits = 8;
			case 3:
				if (stencilBits == 24)
					stencilBits = 16;
				else if (stencilBits == 16)
					stencilBits = 8;
			}
		}

		testColorBits = colorBits;
		testDepthBits = depthBits;
		testStencilBits = stencilBits;

		if ((i % 4) == 3)
		{
			// reduce colorBits
			if (testColorBits == 24)
				testColorBits = 16;
		}

		if ((i % 4) == 2)
		{
			// reduce depthBits
			if (testDepthBits == 24)
				testDepthBits = 16;
			else if (testDepthBits == 16)
				testDepthBits = 8;
		}

		if ((i % 4) == 1)
		{
			// reduce stencilBits
			if (testStencilBits == 24)
				testStencilBits = 16;
			else if (testStencilBits == 16)
				testStencilBits = 8;
			else
				testStencilBits = 0;
		}

		if (testColorBits == 24)
			perChannelColorBits = 8;
		else
			perChannelColorBits = 4;

#ifdef __sgi // Fix for SGIs grabbing too many bits of color
		if (perChannelColorBits == 4)
			perChannelColorBits = 0; // Use minimum size for 16-bit color
		// Need alpha or else SGIs choose 36+ bit RGB mode
		SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 1);
#endif

		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, perChannelColorBits);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, perChannelColorBits);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, perChannelColorBits);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, testDepthBits);
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, testStencilBits);

		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, samples ? 1 : 0);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, samples);

		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		
		if ((window = SDL_CreateWindow("Quake II", x, y, viddef.width, viddef.height, flags)) == NULL)
		{
			VID_Printf(PRINT_DEVELOPER, "SDL_CreateWindow failed: %s\n", SDL_GetError());
			continue;
		}

		if (fullscreen)
		{
			SDL_DisplayMode mode;

			switch (testColorBits)
			{
			case 16: mode.format = SDL_PIXELFORMAT_RGB565; break;
			case 24: mode.format = SDL_PIXELFORMAT_RGB24;  break;
			default: VID_Printf(PRINT_DEVELOPER, "testColorBits is %d, can't fullscreen\n", testColorBits); continue;
			}

			mode.w = viddef.width;
			mode.h = viddef.height;
			mode.refresh_rate = VID_GL_GetRefreshRate();
			mode.driverdata = NULL;

			if (SDL_SetWindowDisplayMode(window, &mode) < 0)
			{
				VID_Printf(PRINT_DEVELOPER, S_COLOR_RED "SDL_SetWindowDisplayMode failed: %s\n", SDL_GetError());
				continue;
			}
		}

		// try for OpenGL core context vs legacy OpenGL context
		GLenum err = 0;
		if (1)
		{
			int profileMask, majorVersion, minorVersion;
			SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &profileMask);
			SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &majorVersion);
			SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minorVersion);

			VID_Printf(PRINT_DEVELOPER, "Trying to get an OpenGL 3.3 core context\n");
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
			int contextFlags = SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG;
			if (contextFlags != 0) {
				SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, contextFlags);
			}

			if ((context = SDL_GL_CreateContext(window)) == NULL)
			{
				VID_Printf(PRINT_DEVELOPER, S_COLOR_RED "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
				VID_Printf(PRINT_DEVELOPER, S_COLOR_RED "Reverting to default context\n");
				SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, profileMask);
				SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, majorVersion);
				SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minorVersion);
			}
			else
			{
				const char *renderer;

				VID_Printf(PRINT_DEVELOPER, S_COLOR_GREEN " ...SDL_GL_CreateContext succeeded.\n");

				err = glewInit();
				if (GLEW_OK == err) {
					renderer = (const char *)glGetString(GL_RENDERER);
				} else {
					VID_Printf(PRINT_DEVELOPER, S_COLOR_RED "glewInit() core context failed: %s\n", glewGetErrorString(err));
					renderer = NULL;
				}
								
				if (!renderer || (strstr(renderer, "Software Renderer") || strstr(renderer, "Software Rasterizer")))
				{
					if (renderer)
						VID_Printf(PRINT_DEVELOPER, S_COLOR_RED "GL_RENDERER is %s, rejecting context\n", renderer);

					SDL_GL_DeleteContext(context);
					context = NULL;

					SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, profileMask);
					SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, majorVersion);
					SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minorVersion);
				}
			}
		}
		else
		{
			context = NULL;
		}

		if (!context)
		{
			if ((context = SDL_GL_CreateContext(window)) == NULL)
			{
				VID_Printf(PRINT_DEVELOPER, S_COLOR_RED "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
				continue;
			}

			err = glewInit();
			if (GLEW_OK != err)
			{
				VID_Printf(PRINT_ALL, S_COLOR_RED "glewInit() failed: %s\n", glewGetErrorString(err));

				SDL_GL_DeleteContext(context);
				context = NULL;

				SDL_DestroyWindow(window);
				window = NULL;
				continue;
			}
		}

		// clear to black
		glClearColor(0, 0, 0, 1);
		glClear(GL_COLOR_BUFFER_BIT);
		SDL_GL_SwapWindow(window);

		if (SDL_GL_SetSwapInterval(gl_swapinterval->integer) == -1)
		{
			VID_Printf(PRINT_DEVELOPER, S_COLOR_RED "SDL_GL_SetSwapInterval failed: %s\n", SDL_GetError());
			viddef.vsyncActive = false;
		}
		else
		{
			viddef.vsyncActive = SDL_GL_GetSwapInterval() != 0;
		}

		SDL_GL_GetAttribute (SDL_GL_RED_SIZE, &realColorBits[0]);
		SDL_GL_GetAttribute (SDL_GL_GREEN_SIZE, &realColorBits[1]);
		SDL_GL_GetAttribute (SDL_GL_BLUE_SIZE, &realColorBits[2]);
		SDL_GL_GetAttribute (SDL_GL_DEPTH_SIZE, &depthBits);
		SDL_GL_GetAttribute (SDL_GL_STENCIL_SIZE, &stencilBits);
		
		colorBits = realColorBits[0] + realColorBits[1] + realColorBits[2];

		VID_Printf(PRINT_ALL, "Using %d color bits, %d depth, %d stencil display.\n", colorBits, depthBits, stencilBits);
		break;
	}

	if (!window)
	{
		VID_Printf(PRINT_ALL, S_COLOR_RED "Couldn't get a visual\n");
		return RSERR_INVALID_MODE;
	}

	// detect available modes and throw them into a CVar for the video menu
	VID_DetectAvailableModes();

	// set the window icon, this must be done after creating the window
	Sys_SetIcon();

	// no cursor
	SDL_ShowCursor(0);

	return RSERR_OK;
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
		Com_Printf(S_COLOR_YELLOW "WARNING: Setting Relative Mousemode failed, reason: %s\n", SDL_GetError());
		Com_Printf(S_COLOR_YELLOW "         You should probably update to SDL 2.0.3 or newer!\n");
	}
}


/*
VID_ShutdownWindow

This routine does all OS specific shutdown procedures for the OpenGL subsystem.
*/
void VID_ShutdownWindow(void)
{
	// cleanly ungrab input (needs window)
	VID_GrabInput(false);

	// make sure that after vid_restart the refresh rate will be queried from SDL again.
	viddef.refreshRate = -1;

	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}


/*
VID_Init_GL

This routine is responsible for initializing the OS specific portions of OpenGL.
*/
qboolean VID_Init_GL (void)
{
	if (SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		Com_Printf("Couldn't init SDL video: %s.\n", SDL_GetError());
		return false;
	}

	const char* driverName = SDL_GetCurrentVideoDriver();
	Com_DPrintf("SDL video driver is \"%s\".\n", driverName);

	return true;
}


/*
VID_GL_BeginFrame
*/
void VID_GL_BeginFrame(float camera_separation)
{
}


/*
VID_GL_EndFrame

Responsible for doing a swapbuffer.
*/
void VID_GL_EndFrame (void)
{
	Draw_End2D ();

	GL_UseProgram (0);

	SDL_GL_SwapWindow(window);
}

/*
VID_GL_TakeVideoFrame
*/
void VID_GL_TakeVideoFrame(int width, int height, byte *captureBuffer, byte *encodeBuffer)
{
	int frameSize = 0;

	if (!captureBuffer)
		return;

	glReadPixels(0, 0, width, height, GL_BGR, GL_UNSIGNED_BYTE, captureBuffer);

	frameSize = width * height * 3;
	CL_WriteAVIVideoFrame(captureBuffer, frameSize);
}
