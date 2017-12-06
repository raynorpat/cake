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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
/*
** GLW_IMP.C
**
** This file contains ALL Win32 specific stuff having to do with the
** OpenGL refresh.  When a port is being made the following functions
** must be implemented by the port:
**
** GLimp_EndFrame
** GLimp_Init
** GLimp_Shutdown
**
*/
#include <assert.h>
#include <windows.h>
#include <SDL.h>
#include <SDL_syswm.h>

#include "../refresh/d3d9/d3d_local.h"

IDirect3D9 *d3d_Object = NULL;
IDirect3DDevice9 *d3d_Device = NULL;

static SDL_Window* window = NULL;
static SDL_SysWMinfo systemInfo;

D3DCAPS9 d3d_DeviceCaps;
qboolean d3d_DeviceLost = false;
D3DPRESENT_PARAMETERS d3d_PresentParameters;
D3DDISPLAYMODE d3d_DesktopMode;

CRenderToTexture rtt_Brightpass;

cvar_t *vid_lastwindowedmode;
cvar_t *vid_lastfullscreenmode;

struct vidhandler_t
{
	d3d9func_t LoadObjects;
	d3d9func_t ShutDown;
	d3d9func_t LostDevice;
	d3d9func_t ResetDevice;
};


#define MAX_VID_HANDLERS 128

static vidhandler_t vid_handlers[MAX_VID_HANDLERS];
static int numvidhandlers = 0;

void	VID_NewWindow(int width, int height);


CD3DHandler::CD3DHandler (d3d9func_t loadobjects, d3d9func_t shutdown, d3d9func_t lostdevice, d3d9func_t resetdevice)
{
	if (numvidhandlers >= MAX_VID_HANDLERS)
	{
		// this can't throw a regular error as it happens during static initialization time
		return;
	}

	vid_handlers[numvidhandlers].LoadObjects = loadobjects;
	vid_handlers[numvidhandlers].ShutDown = shutdown;
	vid_handlers[numvidhandlers].LostDevice = lostdevice;
	vid_handlers[numvidhandlers].ResetDevice = resetdevice;

	numvidhandlers++;
}


void VID_RunHandlers (int handlerflags)
{
	if (!d3d_Device) return;

	for (int i = 0; i < numvidhandlers; i++)
	{
		// destroy handlers are run before create handlers (in case these are ever used to destroy then immediately recreate objects)
		if ((handlerflags & VH_SHUTDOWN) && vid_handlers[i].ShutDown) vid_handlers[i].ShutDown ();
		if ((handlerflags & VH_LOSTDEVICE) && vid_handlers[i].LostDevice) vid_handlers[i].LostDevice ();
		if ((handlerflags & VH_LOADOBJECTS) && vid_handlers[i].LoadObjects) vid_handlers[i].LoadObjects ();
		if ((handlerflags & VH_RESETDEVICE) && vid_handlers[i].ResetDevice) vid_handlers[i].ResetDevice ();
	}
}


void D3D_GetPresentParameters (void)
{
	IDirect3DSwapChain9 *sc = NULL;

	if (SUCCEEDED (d3d_Device->GetSwapChain (0, &sc)))
	{
		sc->GetPresentParameters (&d3d_PresentParameters);
		sc->Release ();
	}
	else Com_Error (ERR_FATAL, "D3D_GetPresentParameters: could not get present parameters for the swapchain!");
}


static D3DFORMAT PixelFormatToD3DFMT(Uint32 format)
{
	switch (format) {
		case SDL_PIXELFORMAT_RGB565:
			return D3DFMT_R5G6B5;
		case SDL_PIXELFORMAT_RGB888:
			return D3DFMT_X8R8G8B8;
		case SDL_PIXELFORMAT_ARGB8888:
			return D3DFMT_A8R8G8B8;
		default:
			return D3DFMT_UNKNOWN;
	}
}

D3DPRESENT_PARAMETERS *D3D_GetPresentParameters (int fullscreen, int width, int height, HWND windowHandle)
{
	static D3DPRESENT_PARAMETERS d3d_PP;

	memset (&d3d_PP, 0, sizeof (D3DPRESENT_PARAMETERS));

	if (fullscreen)
	{
		d3d_PP.Windowed = FALSE;
		d3d_PP.BackBufferFormat = PixelFormatToD3DFMT(SDL_GetWindowPixelFormat(window));
		d3d_PP.FullScreen_RefreshRateInHz = 60;
	}
	else
	{
		d3d_PP.Windowed = TRUE;
		d3d_PP.BackBufferFormat = D3DFMT_UNKNOWN;
		d3d_PP.FullScreen_RefreshRateInHz = 0;
	}

	d3d_PP.EnableAutoDepthStencil = TRUE;
	d3d_PP.AutoDepthStencilFormat = D3DFMT_D24S8;
	d3d_PP.Flags = D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL;
	d3d_PP.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3d_PP.BackBufferCount = 3;
	d3d_PP.BackBufferWidth = width;
	d3d_PP.BackBufferHeight = height;
	d3d_PP.hDeviceWindow = windowHandle;
	d3d_PP.MultiSampleQuality = 0;
	d3d_PP.MultiSampleType = D3DMULTISAMPLE_NONE;

	//if (gl_swapinterval->value)
		d3d_PP.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
	//else d3d_PP.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

	return &d3d_PP;
}

static qboolean VerifyDriver (void)
{
	// see if the main object is available and if not create a new it
	if (!d3d_Object)
	{
		if (!(d3d_Object = Direct3DCreate9 (D3D_SDK_VERSION)))
		{
			Com_Error (ERR_FATAL, "VerifyDriver: Direct3DCreate9 failed\n");
			return false;
		}
	}

	// get caps
	d3d_Object->GetDeviceCaps (0, D3DDEVTYPE_HAL, &d3d_DeviceCaps);

	int vsvermaj = D3DSHADER_VERSION_MAJOR (d3d_DeviceCaps.VertexShaderVersion);
	int psvermaj = D3DSHADER_VERSION_MAJOR (d3d_DeviceCaps.PixelShaderVersion);

	// D3D9 will never report > 3 but just in case a future version (if there ever is one) does...
	if (vsvermaj < 3 || psvermaj < 3)
	{
		Com_Error (ERR_FATAL, "VerifyDriver : unsupported VS version (%i) or PS version (%i).\nCake needs Shader Model 3.0 minimum.\n", vsvermaj, psvermaj);
		return false;
	}

	// check other selected caps will go here

	// a bunch of check device format calls will go here
	return true;
}



static int IsFullscreen()
{
	if (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) {
		return 1;
	}
	else if (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) {
		return 2;
	}
	else {
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

/*
VID_Init_D3D9
*/
qboolean VID_Init_D3D9(void)
{
	if (!SDL_WasInit(SDL_INIT_VIDEO)) {
		if (SDL_Init(SDL_INIT_VIDEO) == -1) {
			Com_Printf("Couldn't init SDL video: %s.\n", SDL_GetError());
			return false;
		}
	}

	return true;
}


/*
** GLimp_SetMode
*/
int GLimp_SetMode (int fullscreen, int *pwidth, int *pheight)
{
	int flags;
	int curWidth, curHeight;
	int width = *pwidth;
	int height = *pheight;
	unsigned int fs_flag = 0;

	SDL_VERSION(&systemInfo.version);

	Com_Printf ("Initializing Direct3D display\n");

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
	if (window)
	{
		GLimp_Shutdown();
		SDL_DestroyWindow(window);
		window = NULL;
	}

	// Create the window
	VID_NewWindow(width, height);

	// let renderer prepare things
	flags = 0;
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
			}
			else {
				Com_Error(ERR_FATAL, "Failed to revert to gl_mode 5. Exiting...\n");
				return false;
			}
		}
		else {
			break;
		}
	}

	// Set the window icon, this must be done after creating the window
	Sys_SetIcon();

	// No cursor
	SDL_ShowCursor(0);

	// get window handle
	SDL_GetWindowWMInfo(window, &systemInfo);

	// verify
	if (!VerifyDriver ())
	{
		Com_Error (ERR_FATAL, "GLimp_SetMode: VerifyDriver failed\n");
		goto fail;
	}

	// see if the main object is available and if not create a new it
	if (!d3d_Object)
	{
		if (!(d3d_Object = Direct3DCreate9 (D3D_SDK_VERSION)))
		{
			Com_Error (ERR_FATAL, "GLimp_SetMode : Direct3DCreate9 failed\n");
			goto fail;
		}
	}

	// prefer a hardware VP device
	HRESULT hr;
	DWORD d3d_VPFlag[] = {D3DCREATE_HARDWARE_VERTEXPROCESSING, D3DCREATE_SOFTWARE_VERTEXPROCESSING, 0};

	for (int i = 0; ; i++)
	{
		if (d3d_VPFlag[i] == 0) goto fail;

		hr = d3d_Object->CreateDevice (
			D3DADAPTER_DEFAULT,
			D3DDEVTYPE_HAL,
			systemInfo.info.win.window,
			d3d_VPFlag[i] | D3DCREATE_FPU_PRESERVE,
			D3D_GetPresentParameters (fullscreen, width, height, systemInfo.info.win.window),
			&d3d_Device
		);

		if (SUCCEEDED (hr)) break;
	}

	if (FAILED (hr)) goto fail;

	// store the present parameters it was created with
	D3D_GetPresentParameters ();

	// run a device reset here to bring on the proper mode
	VID_ResetDevice ();

	// clear to black
	d3d_Device->Clear (0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB (0, 0, 0), 1.0f, 1);
	d3d_Device->Present (NULL, NULL, NULL, NULL);

	// and bring on everything else
	VID_RunHandlers (VH_LOADOBJECTS);

	// ensure that the initial mode set doesn't trigger a mode change in the first frame
	gl_mode->modified = false;
	vid_fullscreen->modified = false;

	// done
	return true;

fail:;
	SAFE_RELEASE (d3d_Device);
	SAFE_RELEASE (d3d_Object);

	return false;
}

// HACK! needs to be moved to vid.c
extern "C" void VID_GrabInput(qboolean grab);

/*
** GLimp_Shutdown
**
** This routine does all OS specific shutdown procedures for the OpenGL
** subsystem.  Under OpenGL this means NULLing out the current DC and
** HGLRC, deleting the rendering context, and releasing the DC acquired
** for the window.  The state structure is also nulled out.
**
*/
void GLimp_Shutdown (void)
{
	if (d3d_Device)
	{
		// go back to windowed mode for a clean shutdown
		if (vid_fullscreen->value)
		{
			// hack the windowed mode into the cvars so that they won't write out invalid values to the configs
			gl_mode->value = 0;
			vid_fullscreen->value = 0;
			VID_ResetDevice ();
		}

		// clear to black for going down
		d3d_Device->Clear (0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB (0, 0, 0), 1.0f, 1);
		d3d_Device->Present (NULL, NULL, NULL, NULL);
	}

	// D3D HERE
	// also release all textures and other gubbins
	// these are NOT automatically destroyed with the "rendering context"
	GL_ShutdownImages ();
	VID_RunHandlers (VH_SHUTDOWN | VH_LOSTDEVICE);

	SAFE_RELEASE (d3d_Device);
	SAFE_RELEASE (d3d_Object);

	// shut down SDL window
	if (window) {
		// cleanly ungrab input (needs window)
		VID_GrabInput(false);
		SDL_DestroyWindow(window);
	}

	window = NULL;

	if (SDL_WasInit(SDL_INIT_EVERYTHING) == SDL_INIT_VIDEO) {
		SDL_Quit();
	} else {
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
	}
}


/*
** GLimp_BeginFrame
*/
qboolean GLimp_TestDevice (void)
{
	if (d3d_DeviceLost)
	{
		// yield some CPU time
		Sleep (5);

		// see if the device can be recovered
		HRESULT hr = d3d_Device->TestCooperativeLevel ();

		switch (hr)
		{
		case D3D_OK:
			// the device is no longer lost
			VID_RunHandlers (VH_RESETDEVICE);
			d3d_DeviceLost = false;
			Com_Printf ("Lost device recovery complete\n");
			break;

		case D3DERR_DEVICELOST:
			// the device cannot be recovered at this time
			break;

		case D3DERR_DEVICENOTRESET:
			// the device is ready to be reset
			// HOWEVER, instead of resetting it, we'll reload the full refresh.  this is a bit cheesy but it
			// ensures that we'll get everything back the way it should be, provides a single code path for
			// it all, and handles resolution changing properly.
			VID_RunHandlers (VH_LOSTDEVICE);
			d3d_Device->Reset (&d3d_PresentParameters);
			break;

		case D3DERR_DRIVERINTERNALERROR:
		default:
			// something bad happened
			// note: this isn't really a proper clean-up path as it doesn't do input/sound/etc cleanup either...
			Com_Error (ERR_FATAL, "GLimp_BeginFrame: D3DERR_DRIVERINTERNALERROR");
			break;
		}

		// no rendering this frame
		return false;
	}

	// device is not lost so render this frame
	return true;
}


void GLimp_BeginFrame (float camera_separation)
{
	d3d_Device->BeginScene ();

	if (vid_gamma->value != 1.0f)
		rtt_Brightpass.Begin (d3d_BrightpassTexture);
}


/*
** GLimp_EndFrame
**
** Responsible for doing a swapbuffers and possibly for other stuff
** as yet to be determined.  Probably better not to make this a GLimp
** function and instead do a call to GLimp_SwapBuffers.
*/
void GLimp_EndFrame (void)
{
	if (d3d_DeviceLost) return;

	// finish any pending 2D drawing
	D3D_End2D ();

	// end our rtt passes
	rtt_Brightpass.End (RTTPASS_BRIGHTPASS);

	d3d_Device->EndScene ();

	HRESULT hr = d3d_Device->Present (NULL, NULL, NULL, NULL);

	// check for a lost device
	if (hr == D3DERR_DEVICELOST) d3d_DeviceLost = true;
}


void VID_ResetDevice (void)
{
	HRESULT hr;

	// this is the standard device reset that comes from a mode change
	if (!d3d_Device) return;
	
	// make sure that we're ready to reset
	while ((hr = d3d_Device->TestCooperativeLevel ()) != D3D_OK)
	{
		if (FAILED (hr))
		{
			// at this stage nothing has been destroyed so we can safely error out
			Com_Printf ("VID_ResetDevice: IDirect3DDevice9::TestCooperativeLevel failed\n");
			return;
		}

		Sys_SendKeyEvents ();
		Sleep (1);
	}

	// release default resources
	VID_RunHandlers (VH_LOSTDEVICE);
	
	// and reset the device
	if (FAILED (d3d_Device->Reset (D3D_GetPresentParameters (vid_fullscreen->value, viddef.width, viddef.height, systemInfo.info.win.window))))
	{
		VID_RunHandlers (VH_RESETDEVICE);
		Com_Printf ("VID_ResetDevice: IDirect3DDevice9::Reset failed\n");
		return;
	}

	// make sure that the reset has completed
	while ((hr = d3d_Device->TestCooperativeLevel ()) != D3D_OK)
	{
		if (FAILED (hr))
		{
			// at this stage stuff has been destroyed so we must crash out
			Com_Error (ERR_FATAL, "VID_ResetDevice: IDirect3DDevice9::TestCooperativeLevel failed\n");
			return;
		}

		Sys_SendKeyEvents ();
		Sleep (1);
	}

	VID_RunHandlers (VH_RESETDEVICE);

	// store the present parameters it was reset to
	D3D_GetPresentParameters ();

	// print it out	
	Com_Printf ("...setting mode %ix%i %s\n", viddef.width, viddef.height, (vid_fullscreen->value ? "fullscreen" : "windowed"));

	// let the sound and input subsystems know about the new window
	VID_NewWindow (viddef.width, viddef.height);

	// default states are lost so restore them
	D3D9_SetDefaultState ();
}

