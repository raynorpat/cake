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

/*
GL_IMP.C

This file contains ALL of the specific stuff having to do with the
OpenGL refresh. When a port is being made the following functions
must be implemented by the port:

GLimp_EndFrame
GLimp_Init
GLimp_Shutdown
GLimp_SwitchFullscreen
*/

#include <assert.h>
#include <SDL.h>
#include "gl_local.h"

#if SDL_VERSION_ATLEAST(2, 0, 0)
static SDL_Window* window = NULL;
static SDL_GLContext context = NULL;
#else
static SDL_Surface* window = NULL;
#endif

#if SDL_VERSION_ATLEAST(2, 0, 0)
// some compatibility defines
#define SDL_SRCCOLORKEY SDL_TRUE
#define SDL_OPENGL SDL_WINDOW_OPENGL
#endif

void GLimp_ShutdownWindow(void);

#if SDL_VERSION_ATLEAST(2, 0, 0)
/* The 64x64 32bit window icon */
#include "q2icon64.h"

static void SetSDLIcon()
{
	/* these masks are needed to tell SDL_CreateRGBSurface(From)
	to assume the data it gets is byte-wise RGB(A) data */
	Uint32 rmask, gmask, bmask, amask;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	int shift = (q2icon64.bytes_per_pixel == 3) ? 8 : 0;
	rmask = 0xff000000 >> shift;
	gmask = 0x00ff0000 >> shift;
	bmask = 0x0000ff00 >> shift;
	amask = 0x000000ff >> shift;
#else /* little endian, like x86 */
	rmask = 0x000000ff;
	gmask = 0x0000ff00;
	bmask = 0x00ff0000;
	amask = (q2icon64.bytes_per_pixel == 3) ? 0 : 0xff000000;
#endif

	SDL_Surface* icon = SDL_CreateRGBSurfaceFrom((void*)q2icon64.pixel_data, q2icon64.width,
		q2icon64.height, q2icon64.bytes_per_pixel * 8, q2icon64.bytes_per_pixel*q2icon64.width,
		rmask, gmask, bmask, amask);

	SDL_SetWindowIcon(window, icon);

	SDL_FreeSurface(icon);
}

#else /* SDL 1.2 */

/* The window icon */
#include "q2icon.xbm"

static void SetSDLIcon()
{
	SDL_Surface *icon;
	SDL_Color transColor, solidColor;
	Uint8 *ptr;
	int i;
	int mask;

	icon = SDL_CreateRGBSurface(SDL_SWSURFACE,
		q2icon_width, q2icon_height, 8,
		0, 0, 0, 0);

	if (icon == NULL)
	{
		return;
	}

	SDL_SetColorKey(icon, SDL_SRCCOLORKEY, 0);

	transColor.r = 255;
	transColor.g = 255;
	transColor.b = 255;

	solidColor.r = 0;
	solidColor.g = 0;
	solidColor.b = 0;

	SDL_SetColors(icon, &transColor, 0, 1);
	SDL_SetColors(icon, &solidColor, 1, 1);

	ptr = (Uint8 *)icon->pixels;

	for (i = 0; i < sizeof(q2icon_bits); i++)
	{
		for (mask = 1; mask != 0x100; mask <<= 1)
		{
			*ptr = (q2icon_bits[i] & mask) ? 1 : 0;
			ptr++;
		}
	}

	SDL_WM_SetIcon(icon, NULL);

	SDL_FreeSurface(icon);
}
#endif /* SDL 1.2 */

static int IsFullscreen()
{
#if SDL_VERSION_ATLEAST(2, 0, 0)
	if (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) {
		return 1;
	}
	else if (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) {
		return 2;
	}
	else {
		return 0;
	}
#else
	return !!(window->flags & SDL_FULLSCREEN);
#endif
}

static qboolean CreateSDLWindow(int flags, int w, int h)
{
#if SDL_VERSION_ATLEAST(2, 0, 0)
	int windowPos = SDL_WINDOWPOS_UNDEFINED;
	// TODO: support fullscreen on different displays with SDL_WINDOWPOS_UNDEFINED_DISPLAY(displaynum)
	window = SDL_CreateWindow("Quake II", windowPos, windowPos, w, h, flags);

	return window != NULL;
#else
	window = SDL_SetVideoMode(w, h, 0, flags);
	SDL_EnableUNICODE(SDL_TRUE);
	return window != NULL;
#endif
}

static qboolean GetWindowSize(int* w, int* h)
{
	if (window == NULL || w == NULL || h == NULL)
		return false;

#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_DisplayMode m;
	if (SDL_GetWindowDisplayMode(window, &m) != 0)
	{
		Com_Printf("Can't get Displaymode: %s\n", SDL_GetError());
		return false;
	}
	*w = m.w;
	*h = m.h;
#else
	*w = window->w;
	*h = window->h;
#endif

	return true;
}


// called by GLimp_InitGraphics() before creating window,
// returns flags for SDL window creation, -1 on error
static int PrepareForWindow(void)
{
	unsigned int flags = 0;
	int msaa_samples = 0;

	if (SDL_GL_LoadLibrary(NULL) < 0) // Default OpenGL is fine.
	{
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

#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	int contextFlags = SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG;
	if (contextFlags != 0)
	{
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, contextFlags);
	}
#else 
	// SDL1.2 doesn't have all this, so we'll have some kind of compatibility profile
#endif

#if !SDL_VERSION_ATLEAST(2, 0, 0)
	// Set vsync - For SDL1.2, this must be done before creating the window
	SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, gl_swapinterval->value ? 1 : 0);
#endif

	// Initiate the flags
#if SDL_VERSION_ATLEAST(2, 0, 0)
	flags = SDL_WINDOW_OPENGL;
#else // SDL 1.2
	flags = SDL_OPENGL;
#endif

	return flags;
}


void GLimp_SetSwapInterval(void)
{
#if SDL_VERSION_ATLEAST(2, 0, 0)
	// Set vsync - TODO: -1 could be set for "late swap tearing"
	SDL_GL_SetSwapInterval(gl_swapinterval->value ? 1 : 0);
#else
	VID_Printf(PRINT_ALL, "SDL1.2 requires a vid_restart to apply changes to gl_swapinterval (vsync)!\n");
#endif
}


int GLimp_InitContext(void* win)
{
	char title[40] = { 0 };

	if (win == NULL)
	{
		Sys_Error("R_InitContext() must not be called with NULL argument!");
		return false;
	}
#if SDL_VERSION_ATLEAST(2, 0, 0)
	window = (SDL_Window*)win;

	context = SDL_GL_CreateContext(window);
	if (context == NULL)
	{
		VID_Printf(PRINT_ALL, "GLimp_InitContext(): Creating OpenGL Context failed: %s\n", SDL_GetError());
		window = NULL;
		return false;
	}
#else // SDL 1.2
	window = (SDL_Surface*)win;
	// context is created implicitly with window, nothing to do here
#endif

#if SDL_VERSION_ATLEAST(2, 0, 0)
	// For SDL2, this must be done after creating the window
	GLimp_SetSwapInterval();
#endif

	// init glew
	glewExperimental = true;
	if (GLEW_OK != glewInit())
	{
		VID_Printf(PRINT_ALL, "R_InitContext(): loading OpenGL function pointers failed!\n");
		return false;
	}
	else
	{
		VID_Printf(PRINT_ALL, "Successfully loaded OpenGL function pointers using glew!\n");
	}

	/* Window title */
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_SetWindowTitle(window, va("Quake II %d", VERSION));
#else
	SDL_WM_SetCaption(title, title);
#endif

	return true;
}

/*
* Shuts the SDL render backend down
*/
void GLimp_Shutdown(qboolean contextOnly)
{
	// Clear the backbuffer and make it current. 
	// Only do this if we have a context, though.
	if (window)
	{
#if SDL_VERSION_ATLEAST(2, 0, 0)
		if (context)
		{
			glClearColor(0.0, 0.0, 0.0, 0.0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			SDL_GL_SwapWindow(window);

			SDL_GL_DeleteContext(context);
			context = NULL;
		}
#else // SDL1.2
		glClearColor(0.0, 0.0, 0.0, 0.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		SDL_GL_SwapBuffers();
#endif
	}

	window = NULL;

	if (!contextOnly)
	{
		GLimp_ShutdownWindow();
	}
}


/*
* Initializes the OpenGL window
*/
qboolean GLimp_InitGraphics(int fullscreen, int *pwidth, int *pheight)
{
	int flags;
	int curWidth, curHeight;
	int width = *pwidth;
	int height = *pheight;
	unsigned int fs_flag = 0;

#if SDL_VERSION_ATLEAST(2, 0, 0)
	if (fullscreen == 1) {
		fs_flag = SDL_WINDOW_FULLSCREEN_DESKTOP;
	} else if (fullscreen == 2) {
		fs_flag = SDL_WINDOW_FULLSCREEN;
	}
#else
	if (fullscreen) {
		fs_flag = SDL_FULLSCREEN;
	}
#endif

	if (GetWindowSize(&curWidth, &curHeight) && (curWidth == width) && (curHeight == height))
	{
		// If we want fullscreen, but aren't
		if (fullscreen != IsFullscreen())
		{
#if SDL_VERSION_ATLEAST(2, 0, 0)
			SDL_SetWindowFullscreen(window, fs_flag);
#else
			SDL_WM_ToggleFullScreen(window);
#endif

			Cvar_SetValue("vid_fullscreen", fullscreen);
		}

		// Are we now?
		if (fullscreen == IsFullscreen())
		{
			return true;
		}
	}

	// Is the window surface used?
	if (window)
	{
#if SDL_VERSION_ATLEAST(2, 0, 0)
		GLimp_Shutdown(true);

		SDL_DestroyWindow(window);
#else
		SDL_FreeSurface(window);
#endif
		window = NULL;
	}

	// Create the window
	VID_NewWindow(width, height);

	// let renderer prepare things (set OpenGL attributes)
	flags = PrepareForWindow();
	if (flags == -1)
	{
		// hopefully PrepareForWindow() logged an error
		return false;
	}

	if (fs_flag)
	{
		flags |= fs_flag;
	}

#if !SDL_VERSION_ATLEAST(2, 0, 0)
	// Set window icon - For SDL1.2, this must be done before creating the window
	SetSDLIcon();
#endif

	while (1)
	{
		if (!CreateSDLWindow(flags, width, height))
		{
			if (width != 640 || height != 480 || (flags & fs_flag))
			{
				Com_Printf("SDL SetVideoMode failed: %s\n", SDL_GetError());
				Com_Printf("Reverting to windowed gl_mode 5 (640x480).\n");

				// Try to recover
				Cvar_SetValue("gl_mode", 5);
				Cvar_SetValue("vid_fullscreen", 0);
				VID_NewWindow(width, height);
				*pwidth = width = 640;
				*pheight = height = 480;
				flags &= ~fs_flag;
			}
			else
			{
				Com_Error(ERR_FATAL, "Failed to revert to gl_mode 5. Exiting...\n");
				return false;
			}
		}
		else
		{
			break;
		}
	}

	if (!GLimp_InitContext(window))
	{
		// InitContext() should have logged an error
		return false;
	}

#if SDL_VERSION_ATLEAST(2, 0, 0)
	// Set the window icon - For SDL2, this must be done after creating the window
	SetSDLIcon();
#endif

	// No cursor
	SDL_ShowCursor(0);

	return true;
}


/*
* (Un)grab Input
*/
void GLimp_GrabInput(qboolean grab)
{
#if SDL_VERSION_ATLEAST(2, 0, 0)
	if (window != NULL)
	{
		SDL_SetWindowGrab(window, grab ? SDL_TRUE : SDL_FALSE);
	}
	if (SDL_SetRelativeMouseMode(grab ? SDL_TRUE : SDL_FALSE) < 0)
	{
		Com_Printf("WARNING: Setting Relative Mousemode failed, reason: %s\n", SDL_GetError());
		Com_Printf("         You should probably update to SDL 2.0.3 or newer!\n");
	}
#else
	SDL_WM_GrabInput(grab ? SDL_GRAB_ON : SDL_GRAB_OFF);
#endif
}


/*
GLimp_ShutdownWindow

This routine does all OS specific shutdown procedures for the OpenGL
subsystem.
*/
void GLimp_ShutdownWindow (void)
{
	if (window)
	{
		/* cleanly ungrab input (needs window) */
		GLimp_GrabInput(false);

#if SDL_VERSION_ATLEAST(2, 0, 0)
		SDL_DestroyWindow(window);
#else
		SDL_FreeSurface(window);
#endif
	}

	window = NULL;

	if (SDL_WasInit(SDL_INIT_EVERYTHING) == SDL_INIT_VIDEO)
	{
		SDL_Quit();
	}
	else
	{
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
	}
}


/*
GLimp_Init

This routine is responsible for initializing the OS specific portions
of OpenGL. Under Win32 this means dealing with the pixelformats and
doing the wgl interface stuff.
*/
qboolean GLimp_Init (void)
{
	if (!SDL_WasInit(SDL_INIT_VIDEO))
	{
		if (SDL_Init(SDL_INIT_VIDEO) == -1)
		{
			Com_Printf("Couldn't init SDL video: %s.\n", SDL_GetError());
			return false;
		}
#if SDL_VERSION_ATLEAST(2, 0, 0)
		const char* driverName = SDL_GetCurrentVideoDriver();
#else
		char driverName[64];
		SDL_VideoDriverName(driverName, sizeof(driverName));
#endif
		Com_Printf("SDL video driver is \"%s\".\n", driverName);
	}

	return true;
}


/*
GLimp_BeginFrame
*/
void GLimp_BeginFrame (float camera_separation)
{
}


/*
GLimp_EndFrame

Responsible for doing a swapbuffer.
*/
void Draw_FPS (void);

void GLimp_EndFrame (void)
{
	//Draw_FPS ();
	Draw_End2D ();

	GL_UseProgram (0);

	SDL_GL_SwapWindow(window);
}
