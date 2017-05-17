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
// Main windowed and fullscreen graphics interface module. This module
// is used for both the software and OpenGL rendering versions of the
// Quake refresh engine.
#include <assert.h>
#include <float.h>

#include "client.h"
#include "winquake.h"
#include "vid_common.h"

cvar_t *win_noalttab;

// Console variables that we need to access from this module
cvar_t		*vid_gamma;
cvar_t		*vid_fullscreen;

// Global variables used internally by this module
viddef_t	viddef;				// global video state; used by other modules
qboolean	reflib_active = 0;

HWND    cl_hwnd;      // Main window handle for life of program

LONG WINAPI MainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

static qboolean s_alttab_disabled;

extern	unsigned	sys_msg_time;

/*
WIN32 helper functions
*/
extern qboolean s_win95;

static void WIN_DisableAltTab (void)
{
	if (s_alttab_disabled)
		return;

	if (s_win95)
	{
		BOOL old;

		SystemParametersInfo (SPI_SCREENSAVERRUNNING, 1, &old, 0);
	}
	else
	{
		RegisterHotKey (0, 0, MOD_ALT, VK_TAB);
		RegisterHotKey (0, 1, MOD_ALT, VK_RETURN);
	}

	s_alttab_disabled = true;
}

static void WIN_EnableAltTab (void)
{
	if (s_alttab_disabled)
	{
		if (s_win95)
		{
			BOOL old;

			SystemParametersInfo (SPI_SCREENSAVERRUNNING, 0, &old, 0);
		}
		else
		{
			UnregisterHotKey (0, 0);
			UnregisterHotKey (0, 1);
		}

		s_alttab_disabled = false;
	}
}

/*
==========================================================================

DLL GLUE

==========================================================================
*/

#define	MAXPRINTMSG	4096

void VID_Printf (int print_level, char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];
	static qboolean	inupdate;

	va_start (argptr, fmt);
	vsprintf (msg, fmt, argptr);
	va_end (argptr);

	if (print_level == PRINT_ALL)
	{
		Com_Printf ("%s", msg);
	}
	else if (print_level == PRINT_DEVELOPER)
	{
		Com_DPrintf ("%s", msg);
	}
	else if (print_level == PRINT_ALERT)
	{
		MessageBox (0, msg, "PRINT_ALERT", MB_ICONWARNING);
		OutputDebugString (msg);
	}
}

void VID_Error (int err_level, char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];
	static qboolean	inupdate;

	va_start (argptr, fmt);
	vsprintf (msg, fmt, argptr);
	va_end (argptr);

	Com_Error (err_level, "%s", msg);
}

//==========================================================================

byte scantokey[128] =
{
	0x00, 0x1b, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x2d, 0x3d, 0x7f, 0x09,
	0x71, 0x77, 0x65, 0x72, 0x74, 0x79, 0x75, 0x69, 0x6f, 0x70, 0x5b, 0x5d, 0x0d, 0x85, 0x61, 0x73,
	0x64, 0x66, 0x67, 0x68, 0x6a, 0x6b, 0x6c, 0x3b, 0x27, 0x60, 0x86, 0x5c, 0x7a, 0x78, 0x63, 0x76,
	0x62, 0x6e, 0x6d, 0x2c, 0x2e, 0x2f, 0x86, 0x2a, 0x84, 0x20, 0x00, 0x87, 0x88, 0x89, 0x8a, 0x8b,
	0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0xff, 0x00, 0x97, 0x80, 0x96, 0xad, 0x82, 0xa4, 0x83, 0xae, 0x98,
	0x81, 0x95, 0x93, 0x94, 0x00, 0x00, 0x00, 0x91, 0x92, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/*
=======
MapKey

Map from windows to quake keynums
=======
*/
int MapKey (int key)
{
	int result;
	int modified = (key >> 16) & 255;
	qboolean is_extended = false;

	if (modified > 127)
		return 0;

	if (key & (1 << 24))
		is_extended = true;

	result = scantokey[modified];

	if (!is_extended)
	{
		switch (result)
		{
		case K_HOME:
			return K_KP_HOME;
		case K_UPARROW:
			return K_KP_UPARROW;
		case K_PGUP:
			return K_KP_PGUP;
		case K_LEFTARROW:
			return K_KP_LEFTARROW;
		case K_RIGHTARROW:
			return K_KP_RIGHTARROW;
		case K_END:
			return K_KP_END;
		case K_DOWNARROW:
			return K_KP_DOWNARROW;
		case K_PGDN:
			return K_KP_PGDN;
		case K_INS:
			return K_KP_INS;
		case K_DEL:
			return K_KP_DEL;
		default:
			return result;
		}
	}
	else
	{
		switch (result)
		{
		case 0x0D:
			return K_KP_ENTER;
		case 0x2F:
			return K_KP_SLASH;
		case 0xAF:
			return K_KP_PLUS;
		}

		return result;
	}
}

void AppActivate (BOOL fActive, BOOL minimize)
{
	Minimized = minimize;

	Key_ClearStates ();

	// we don't want to act like we're active if we're minimized
	if (fActive && !Minimized)
		ActiveApp = true;
	else ActiveApp = false;

	// minimize/restore mouse-capture on demand
	if (!ActiveApp)
	{
		IN_Activate (false);
		CDAudio_Activate (false);
		S_Activate (false);

		if (win_noalttab->value)
		{
			WIN_EnableAltTab ();
		}
	}
	else
	{
		IN_Activate (true);
		CDAudio_Activate (true);
		S_Activate (true);

		if (win_noalttab->value)
		{
			WIN_DisableAltTab ();
		}
	}
}

/*
====================
MainWndProc

main window procedure
====================
*/
LONG WINAPI MainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_ERASEBKGND: return 1; // treachery!!! see your MSDN!

	case WM_MOUSEWHEEL:
		// this chunk of code theoretically only works under NT4 and Win98
		// since this message doesn't exist under Win95
		if ((short) HIWORD (wParam) > 0)
		{
			Key_Event (K_MWHEELUP, true, sys_msg_time);
			Key_Event (K_MWHEELUP, false, sys_msg_time);
		}
		else
		{
			Key_Event (K_MWHEELDOWN, true, sys_msg_time);
			Key_Event (K_MWHEELDOWN, false, sys_msg_time);
		}

		return 0;

	case WM_HOTKEY:
		return 0;

	case WM_CREATE:
		cl_hwnd = hWnd;
		break;

	case WM_DESTROY:
		// let sound and input know about this?
		cl_hwnd = NULL;
		break;

	case WM_ACTIVATE:
		{
			int	fActive, fMinimized;

			// KJB: Watch this for problems in fullscreen modes with Alt-tabbing.
			fActive = LOWORD (wParam);
			fMinimized = (BOOL) HIWORD (wParam);

			AppActivate (fActive != WA_INACTIVE, fMinimized);

			if (reflib_active)
				GLimp_AppActivate (!(fActive == WA_INACTIVE));

			return 0;
		}

	// this is complicated because Win32 seems to pack multiple mouse events into
	// one update sometimes, so we always check all states and look for events
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MOUSEMOVE:
		{
			int	temp = 0;

			if (wParam & MK_LBUTTON) temp |= 1;
			if (wParam & MK_RBUTTON) temp |= 2;
			if (wParam & MK_MBUTTON) temp |= 4;

			IN_MouseEvent (temp);

			return 0;
		}

	case WM_SYSCOMMAND:
		if (wParam == SC_SCREENSAVE)
			return 0;

		break;

	case WM_SYSKEYDOWN:
		if ((int) wParam == 13)
		{
			if (vid_fullscreen)
				Cvar_SetValue ("vid_fullscreen", !vid_fullscreen->value);

			return 0;
		}

		// fall through
	case WM_KEYDOWN:
		Key_Event (MapKey (lParam), true, sys_msg_time);
		return 0;

	case WM_SYSKEYUP:
	case WM_KEYUP:
		Key_Event (MapKey (lParam), false, sys_msg_time);
		return 0;

	default:
		// pass all unhandled messages to DefWindowProc
		break;
	}

	// return 0 if handled message, 1 if not
	return DefWindowProc (hWnd, uMsg, wParam, lParam);
}

/*
============
VID_Restart_f

Console command to re-start the video mode and refresh DLL.
============
*/
void VID_Restart_f (void)
{
	extern qboolean ref_modified;
	ref_modified = true;
}

void VID_Front_f (void)
{
	SetWindowLong (cl_hwnd, GWL_EXSTYLE, WS_EX_TOPMOST);
	SetForegroundWindow (cl_hwnd);
}

/*
VID_GetModeInfo
*/
#define MAX_MODE_LIST	99

vidmode_t vid_modelist[MAX_MODE_LIST];
char *vid_resolutions[MAX_MODE_LIST + 1];
int vid_nummodes = 0;

void VID_EnumResolutions (void)
{
	DEVMODE devmode;
	int modenum;
	BOOL stat;

	vid_nummodes = 0;
	modenum = 0;

	do
	{
		memset (&devmode, 0, sizeof (DEVMODE));
		stat = EnumDisplaySettings (NULL, modenum, &devmode);

		// only bother with 32-bit modes where width > height
		if ((devmode.dmBitsPerPel == 32) &&
			(devmode.dmPelsWidth >= devmode.dmPelsHeight) &&
			(vid_nummodes < MAX_MODE_LIST))
		{
			devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

			if (ChangeDisplaySettings (&devmode, CDS_TEST | CDS_FULLSCREEN) == DISP_CHANGE_SUCCESSFUL)
			{
				int i;
				qboolean existingmode = false;

				for (i = 0; i < vid_nummodes; i++)
				{
					if (vid_modelist[i].width != devmode.dmPelsWidth) continue;
					if (vid_modelist[i].height != devmode.dmPelsHeight) continue;

					existingmode = true;
					break;
				}

				if (!existingmode)
				{
					sprintf (vid_modelist[vid_nummodes].description, "%i x %i\n", devmode.dmPelsWidth, devmode.dmPelsHeight);
					vid_modelist[vid_nummodes].width = devmode.dmPelsWidth;
					vid_modelist[vid_nummodes].height = devmode.dmPelsHeight;
					vid_modelist[vid_nummodes].mode = vid_nummodes;

					// add to the list for the menu and ensure NULL termination
					vid_resolutions[vid_nummodes] = vid_modelist[vid_nummodes].description;
					vid_resolutions[vid_nummodes + 1] = NULL;
					vid_nummodes++;

					// Com_Printf ("Enumerated mode %i x %i\n", devmode.dmPelsWidth, devmode.dmPelsHeight);
				}
			}
		}

		modenum++;
	} while (stat);
}


qboolean VID_GetModeInfo (int *width, int *height, int mode)
{
	if (mode < 0 || mode >= vid_nummodes)
		return false;

	*width = vid_modelist[mode].width;
	*height = vid_modelist[mode].height;

	return true;
}

/*
VID_NewWindow
*/
void VID_NewWindow (int width, int height)
{
	viddef.width = width;
	viddef.height = height;

	cl.force_refdef = true;		// can't use a paused refdef
}

void VID_FreeReflib (void)
{
	reflib_active = false;
}

/*
==============
VID_LoadRefresh
==============
*/

qboolean VID_LoadRefresh (void)
{
	if (reflib_active)
	{
		R_Shutdown ();
		VID_FreeReflib ();
	}

	VID_EnumResolutions ();

	Swap_Init ();

	if (R_Init (global_hInstance, MainWndProc) == -1)
	{
		R_Shutdown ();
		VID_FreeReflib ();
		return false;
	}

	Com_Printf ("------------------------------------\n");
	reflib_active = true;

	return true;
}

/*
============
VID_CheckChanges

This function gets called once just before drawing each frame, and it's sole purpose in life
is to check to see if any of the video mode parameters have changed, and if they have to
update the rendering DLL and/or video mode to match.
============
*/
// this must be initialized to true so that the first frame will init the ref properly
qboolean ref_modified = true;

void VID_CheckChanges (void)
{
	if (win_noalttab->modified)
	{
		if (win_noalttab->value)
			WIN_DisableAltTab ();
		else
			WIN_EnableAltTab ();

		win_noalttab->modified = false;
	}

	if (ref_modified)
	{
		cl.force_refdef = true;		// can't use a paused refdef
		S_StopAllSounds ();
	}

	while (ref_modified)
	{
		// refresh has changed
		ref_modified = false;
		vid_fullscreen->modified = true;
		cl.refresh_prepped = false;
		cls.disable_screen = true;

		if (!VID_LoadRefresh ())
		{
			Com_Error (ERR_FATAL, "Couldn't fall back to software refresh!");
		}

		cls.disable_screen = false;
	}
}


/*
============
VID_Init
============
*/
void VID_Init (void)
{
	// Create the video variables so we know how to start the graphics drivers
	vid_fullscreen = Cvar_Get ("vid_fullscreen", "0", CVAR_ARCHIVE);
	vid_gamma = Cvar_Get ("vid_gamma", "1", CVAR_ARCHIVE);
	win_noalttab = Cvar_Get ("win_noalttab", "0", CVAR_ARCHIVE);

	// Add some console commands that we want to handle
	Cmd_AddCommand ("vid_restart", VID_Restart_f);
	Cmd_AddCommand ("vid_front", VID_Front_f);

	// Start the graphics mode and load refresh
	VID_CheckChanges ();
}

/*
============
VID_Shutdown
============
*/
void VID_Shutdown (void)
{
	if (reflib_active)
	{
		R_Shutdown ();
		VID_FreeReflib ();
	}
}


