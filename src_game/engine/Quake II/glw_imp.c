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
GLW_IMP.C

This file contains ALL Win32 specific stuff having to do with the
OpenGL refresh. When a port is being made the following functions
must be implemented by the port:

GLimp_EndFrame
GLimp_Init
GLimp_Shutdown
GLimp_SwitchFullscreen
*/

#include <assert.h>
#include <windows.h>
#include "gl_local.h"
#include "glw_win.h"
#include "winquake.h"

#pragma comment (lib, "glew32s.lib")
#pragma comment (lib, "opengl32.lib")
#pragma comment (lib, "glu32.lib")

qboolean GLimp_InitGL (void);

glwstate_t glw_state;

extern cvar_t *vid_fullscreen;

/*
VID_CreateWindow
*/
#define	WINDOW_CLASS_NAME	"Quake II"

qboolean VID_CreateWindow (int width, int height, qboolean fullscreen)
{
	WNDCLASS		wc;
	RECT			windowrect;
	int				stylebits;
	int				x, y, w, h;
	int				exstyle;
	HDC				hdc;

	/* Register the frame class */
	wc.style     = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc  = (WNDPROC) glw_state.wndproc;
	wc.cbClsExtra  = 0;
	wc.cbWndExtra  = 0;
	wc.hInstance   = glw_state.hInstance;
	wc.hIcon     = 0;
	wc.hCursor    = LoadCursor (NULL, IDC_ARROW);
	wc.hbrBackground = (void *) COLOR_GRAYTEXT;
	wc.lpszMenuName = 0;
	wc.lpszClassName = WINDOW_CLASS_NAME;

	if (!RegisterClass (&wc))
		VID_Error (ERR_FATAL, "Couldn't register window class");

	if (fullscreen)
	{
		exstyle = WS_EX_TOPMOST;
		stylebits = WS_POPUP | WS_VISIBLE;
	}
	else
	{
		// these are the same styles as DXGI enforces for a windowed mode
		exstyle = 256;
		stylebits = 348127232;
	}

	windowrect.left = 0;
	windowrect.top = 0;
	windowrect.right = width;
	windowrect.bottom = height;

	AdjustWindowRect (&windowrect, stylebits, FALSE);

	w = windowrect.right - windowrect.left;
	h = windowrect.bottom - windowrect.top;

	if (fullscreen)
	{
		x = 0;
		y = 0;
	}
	else
	{
		RECT workarea;

		SystemParametersInfo (SPI_GETWORKAREA, 0, &workarea, 0);
		x = workarea.left + ((workarea.right - workarea.left) - (windowrect.right - windowrect.left)) / 2;
		y = workarea.top + ((workarea.bottom - workarea.top) - (windowrect.bottom - windowrect.top)) / 2;
	}

	glw_state.hWnd = CreateWindowEx (
						 exstyle,
						 WINDOW_CLASS_NAME,
						 "Quake II",
						 stylebits,
						 x, y, w, h,
						 NULL,
						 NULL,
						 glw_state.hInstance,
						 NULL);

	if (!glw_state.hWnd)
		VID_Error (ERR_FATAL, "Couldn't create window");

	ShowWindow (glw_state.hWnd, SW_SHOW);
	UpdateWindow (glw_state.hWnd);

	hdc = GetDC (glw_state.hWnd);
	PatBlt (hdc, 0, 0, w, h, BLACKNESS);
	ReleaseDC (glw_state.hWnd, hdc);

	// init all the gl stuff for the window
	if (!GLimp_InitGL ())
	{
		VID_Printf (PRINT_ALL, "VID_CreateWindow() - GLimp_InitGL failed\n");
		return false;
	}

	SetForegroundWindow (glw_state.hWnd);
	SetFocus (glw_state.hWnd);

	// let the sound and input subsystems know about the new window
	VID_NewWindow (width, height);

	return true;
}


/*
GLimp_SetMode
*/
rserr_t GLimp_SetMode (int *pwidth, int *pheight, int mode, qboolean fullscreen)
{
	int width, height;
	const char *win_fs[] = { "W", "FS" };

	VID_Printf (PRINT_ALL, "Initializing OpenGL display\n");
	VID_Printf (PRINT_ALL, "...setting mode %d:", mode);

	if (!VID_GetModeInfo (&width, &height, mode))
	{
		VID_Printf (PRINT_ALL, " invalid mode\n");
		return rserr_invalid_mode;
	}

	VID_Printf (PRINT_ALL, " %d %d %s\n", width, height, win_fs[fullscreen]);

	// destroy the existing window
	if (glw_state.hWnd)
	{
		GLimp_Shutdown ();
	}

	// do a CDS if needed
	if (fullscreen)
	{
		DEVMODE dm;

		VID_Printf (PRINT_ALL, "...attempting fullscreen\n");

		memset (&dm, 0, sizeof (dm));

		dm.dmSize = sizeof (dm);

		dm.dmPelsWidth = width;
		dm.dmPelsHeight = height;
		dm.dmFields   = DM_PELSWIDTH | DM_PELSHEIGHT;

		VID_Printf (PRINT_ALL, "...calling CDS: ");

		if (ChangeDisplaySettings (&dm, CDS_FULLSCREEN) == DISP_CHANGE_SUCCESSFUL)
		{
			*pwidth = width;
			*pheight = height;

			gl_state.fullscreen = true;

			VID_Printf (PRINT_ALL, "ok\n");

			if (!VID_CreateWindow (width, height, true))
				return rserr_invalid_mode;

			return rserr_ok;
		}
		else
		{
			*pwidth = width;
			*pheight = height;

			VID_Printf (PRINT_ALL, "failed\n");
			VID_Printf (PRINT_ALL, "...calling CDS assuming dual monitors:");

			dm.dmPelsWidth = width * 2;
			dm.dmPelsHeight = height;
			dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;

			// our first CDS failed, so maybe we're running on some weird dual monitor
			// system
			if (ChangeDisplaySettings (&dm, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
			{
				VID_Printf (PRINT_ALL, " failed\n");
				VID_Printf (PRINT_ALL, "...setting windowed mode\n");

				ChangeDisplaySettings (0, 0);

				*pwidth = width;
				*pheight = height;
				gl_state.fullscreen = false;

				if (!VID_CreateWindow (width, height, false))
					return rserr_invalid_mode;

				return rserr_invalid_fullscreen;
			}
			else
			{
				VID_Printf (PRINT_ALL, " ok\n");

				if (!VID_CreateWindow (width, height, true))
					return rserr_invalid_mode;

				gl_state.fullscreen = true;
				return rserr_ok;
			}
		}
	}
	else
	{
		VID_Printf (PRINT_ALL, "...setting windowed mode\n");

		ChangeDisplaySettings (0, 0);

		*pwidth = width;
		*pheight = height;
		gl_state.fullscreen = false;

		if (!VID_CreateWindow (width, height, false))
			return rserr_invalid_mode;
	}

	return rserr_ok;
}

/*
GLimp_Shutdown

This routine does all OS specific shutdown procedures for the OpenGL
subsystem. Under OpenGL this means NULLing out the current DC and
HGLRC, deleting the rendering context, and releasing the DC acquired
for the window. The state structure is also nulled out.
*/
void GLimp_Shutdown (void)
{
	if (!wglMakeCurrent (NULL, NULL))
		VID_Printf (PRINT_ALL, "ref_gl::R_Shutdown() - wglMakeCurrent failed\n");

	if (glw_state.hGLRC)
	{
		if (!wglDeleteContext (glw_state.hGLRC))
			VID_Printf (PRINT_ALL, "ref_gl::R_Shutdown() - wglDeleteContext failed\n");

		glw_state.hGLRC = NULL;
	}

	if (glw_state.hDC)
	{
		if (!ReleaseDC (glw_state.hWnd, glw_state.hDC))
			VID_Printf (PRINT_ALL, "ref_gl::R_Shutdown() - ReleaseDC failed\n");

		glw_state.hDC  = NULL;
	}

	if (glw_state.hWnd)
	{
		ShowWindow (glw_state.hWnd, SW_HIDE);
		DestroyWindow (glw_state.hWnd);
		glw_state.hWnd = NULL;
	}

	UnregisterClass (WINDOW_CLASS_NAME, glw_state.hInstance);

	if (gl_state.fullscreen)
	{
		ChangeDisplaySettings (0, 0);
		gl_state.fullscreen = false;
	}
}


/*
GLimp_Init

This routine is responsible for initializing the OS specific portions
of OpenGL. Under Win32 this means dealing with the pixelformats and
doing the wgl interface stuff.
*/
qboolean GLimp_Init (void *hinstance, void *wndproc)
{
	glw_state.hInstance = (HINSTANCE) hinstance;
	glw_state.wndproc = wndproc;

	return true;
}

qboolean GLimp_CreateNewContext (void)
{
	// startup the OpenGL subsystem by creating a context and making it current
	if ((glw_state.hGLRC = wglCreateContext (glw_state.hDC)) != NULL)
	{
		if (wglMakeCurrent (glw_state.hDC, glw_state.hGLRC))
		{
			glewInit ();
			return true;
		}
		else
			VID_Printf (PRINT_ALL, "GLimp_Init() - wglMakeCurrent failed\n");
	}
	else
		VID_Printf (PRINT_ALL, "GLimp_Init() - wglCreateContext failed\n");

	if (glw_state.hGLRC)
	{
		wglDeleteContext (glw_state.hGLRC);
		glw_state.hGLRC = NULL;
	}

	if (glw_state.hDC)
	{
		ReleaseDC (glw_state.hWnd, glw_state.hDC);
		glw_state.hDC = NULL;
	}

	return false;
}


#ifndef PFD_SUPPORT_COMPOSITION
#define PFD_SUPPORT_COMPOSITION 0
#endif

qboolean GLimp_InitGL (void)
{
	int pixelformat = 0;

	memset (&glw_state.pfd, 0, sizeof (PIXELFORMATDESCRIPTOR));

	glw_state.pfd.nSize = sizeof (PIXELFORMATDESCRIPTOR);
	glw_state.pfd.nVersion = 1;
	glw_state.pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER | PFD_SUPPORT_COMPOSITION;
	glw_state.pfd.iPixelType = PFD_TYPE_RGBA;
	glw_state.pfd.cColorBits = 32;
	glw_state.pfd.cDepthBits = 24;
	glw_state.pfd.cStencilBits = 8;
	glw_state.pfd.iLayerType = PFD_MAIN_PLANE;

	// Get a DC for the specified window
	if (glw_state.hDC != NULL)
		VID_Printf (PRINT_ALL, "GLimp_Init() - non-NULL DC exists\n");

	if ((glw_state.hDC = GetDC (glw_state.hWnd)) == NULL)
	{
		VID_Printf (PRINT_ALL, "GLimp_Init() - GetDC failed\n");
		return false;
	}

	if ((pixelformat = ChoosePixelFormat (glw_state.hDC, &glw_state.pfd)) == 0)
	{
		VID_Printf (PRINT_ALL, "GLimp_Init() - ChoosePixelFormat failed\n");
		return false;
	}

	if (SetPixelFormat (glw_state.hDC, pixelformat, &glw_state.pfd) == FALSE)
	{
		VID_Printf (PRINT_ALL, "GLimp_Init() - SetPixelFormat failed\n");
		return false;
	}

	DescribePixelFormat (glw_state.hDC, pixelformat, sizeof (PIXELFORMATDESCRIPTOR), &glw_state.pfd);

	if (!GLimp_CreateNewContext ()) return false;

	// print out PFD specifics
	VID_Printf (PRINT_ALL, "GL PFD: color (%d-bits)  Z (%d-bit)  stencil (%d-bits)\n",
		(int) glw_state.pfd.cColorBits,
		(int) glw_state.pfd.cDepthBits,
		(int) glw_state.pfd.cStencilBits);

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

Responsible for doing a swapbuffers and possibly for other stuff
as yet to be determined. Probably better not to make this a GLimp
function and instead do a call to GLimp_SwapBuffers.
*/
void Draw_FPS (void);

void GLimp_EndFrame (void)
{
	int		err;

	Draw_FPS ();
	Draw_End2D ();
	GL_UseProgram (0);

	if (!SwapBuffers (glw_state.hDC))
		VID_Error (ERR_FATAL, "GLimp_EndFrame() - SwapBuffers() failed!\n");
}


void GL_Clear (GLbitfield mask)
{
	// because depth/stencil are interleaved, if we're clearing depth we must also clear stencil
	if ((mask & GL_DEPTH_BUFFER_BIT) && glw_state.pfd.cStencilBits)
		mask |= GL_STENCIL_BUFFER_BIT;

	glClear (mask);
}


/*
GLimp_AppActivate
*/
void GLimp_AppActivate (qboolean active)
{
	if (active)
	{
		SetForegroundWindow (glw_state.hWnd);
		ShowWindow (glw_state.hWnd, SW_RESTORE);
	}
	else
	{
		if (vid_fullscreen->value)
			ShowWindow (glw_state.hWnd, SW_MINIMIZE);
	}
}
