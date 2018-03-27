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
// vid.c -- Main windowed and fullscreen graphics interface module.
#include <assert.h>
#include <float.h>

#include "client.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// Console variables that we need to access from this module
cvar_t		*vid_gamma;
cvar_t		*vid_contrast;
cvar_t		*vid_ref;			// Name of Refresh currently loaded
cvar_t		*vid_fullscreen;
cvar_t		*vid_refreshrate;

// Global variables used internally by this module
viddef_t	viddef;				// global video state; used by other modules
qboolean	reflib_active = 0;

vidmode_t vid_modes[] = {
	{ "Mode  0:  320x240", 320, 240, 0 },
	{ "Mode  1:  400x300", 400, 300, 1 },
	{ "Mode  2:  512x384", 512, 384, 2 },
	{ "Mode  3:  640x400", 640, 400, 3 },
	{ "Mode  4:  640x480", 640, 480, 4 },
	{ "Mode  5:  800x500", 800, 500, 5 },
	{ "Mode  6:  800x600", 800, 600, 6 },
	{ "Mode  7:  960x720", 960, 720, 7 },
	{ "Mode  8: 1024x480", 1024, 480, 8 },
	{ "Mode  9: 1024x640", 1024, 640, 9 },
	{ "Mode 10: 1024x768", 1024, 768, 10 },
	{ "Mode 11: 1152x768", 1152, 768, 11 },
	{ "Mode 12: 1152x864", 1152, 864, 12 },
	{ "Mode 13: 1280x800", 1280, 800, 13 },
	{ "Mode 14: 1280x720", 1280, 720, 14 },
	{ "Mode 15: 1280x960", 1280, 960, 15 },
	{ "Mode 16: 1280x1024", 1280, 1024, 16 },
	{ "Mode 17: 1366x768", 1366, 768, 17 },
	{ "Mode 18: 1440x900", 1440, 900, 18 },
	{ "Mode 19: 1600x1200", 1600, 1200, 19 },
	{ "Mode 20: 1680x1050", 1680, 1050, 20 },
	{ "Mode 21: 1920x1080", 1920, 1080, 21 },
	{ "Mode 22: 1920x1200", 1920, 1200, 22 },
	{ "Mode 23: 2048x1536", 2048, 1536, 23 },
	{ "Mode 24: 3440x1440", 3440, 1440, 24 },
	{ "Mode 25: 3840x2160", 3840, 2160, 25 },
};

#define VID_NUM_MODES (sizeof(vid_modes) / sizeof(vid_modes[0]))


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
		Sys_ShowMessageBox ("Alert", msg);
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

/*
============
VID_Restart_f

Console command to re-start the video mode and refresh.
============
*/
void VID_Restart_f (void)
{
	vid_ref->modified = true;
}

/*
============
VID_ListModes_f

Console command to list out video modes
============
*/
void VID_ListModes_f(void)
{
	int i;
	Com_Printf("Supported video modes (gl_mode):\n");
	for (i = 0; i<VID_NUM_MODES; ++i)
	{
		Com_Printf("  %s\n", vid_modes[i].description);
	}
	Com_Printf("  Mode -1: gl_customwidth x gl_customheight\n");
}

/*
==============
VID_GetModeInfo
==============
*/
qboolean VID_GetModeInfo (int *width, int *height, int mode)
{
	if ((mode < 0) || (mode >= VID_NUM_MODES))
	{
		return false;
	}

	*width = vid_modes[mode].width;
	*height = vid_modes[mode].height;

	return true;
}

/*
==============
VID_IsVSyncActive
==============
*/
qboolean VID_IsVSyncActive(void)
{
	return viddef.vsyncActive;
}

/*
==============
VID_GetRefreshRate

Returns the current display refresh rate.
==============
*/
int VID_GetRefreshRate(void)
{
	return GFX_Core_GetRefreshRate();
}

/*
==============
VID_NewWindow
==============
*/
void VID_NewWindow (int width, int height)
{
	viddef.width = width;
	viddef.height = height;

	cl.force_refdef = true;		// can't use a paused refdef
}

/*
==============
VID_FreeReflib
==============
*/
static void VID_FreeReflib (void)
{
	reflib_active = false;

	GFX_CoreShutdown ();
}

/*
==============
VID_LoadRefresh
==============
*/
qboolean VID_LoadRefresh (char *name)
{
	if (reflib_active)
	{
		RE_Shutdown ();
		VID_FreeReflib ();
	}

	Com_Printf("------- Loading %s -------\n", name);

	// Make sure to init GFX core
	GFX_CoreInit( name );

	// Init refresh
	if (RE_Init() == -1)
	{
		RE_Shutdown ();
		VID_FreeReflib ();
		return false;
	}

	// Ensure that all key states are cleared
	Key_ClearStates();

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
void VID_CheckChanges (void)
{
	char name[100];

	if (vid_ref->modified)
	{
		cl.force_refdef = true;		// can't use a paused refdef
		S_StopAllSounds ();
		BGM_Stop ();
	}

	while (vid_ref->modified)
	{
		// refresh has changed
		vid_ref->modified = false;
		vid_fullscreen->modified = true;
		cl.refresh_prepped = false;
		cls.disable_screen = true;

		Com_sprintf(name, sizeof(name), "ref_%s", vid_ref->string);
		if (!VID_LoadRefresh(name))
		{
			if (strcmp(vid_ref->string, "soft") == 0)
				Com_Error (ERR_FATAL, "Couldn't load refresh subsystem, couldn't fallback to software refresh!");
			Cvar_Set ("vid_ref", "soft");

			// drop the console if we fail to load a refresh
			if (cls.key_dest != key_console)
			{
				Con_ToggleConsole_f ();
			}
		}

		cls.disable_screen = false;
	}
}

// called with image data of width*height pixel which comp bytes per pixel (must be 3 or 4 for RGB or RGBA)
// expects the pixels data to be row-wise, starting at top left
void VID_WriteScreenshot(int width, int height, int comp, const void* data)
{
	char picname[80];
	char checkname[MAX_OSPATH];
	int i, success = 0;
	static char* supportedFormats[] = { "png", "jpg", "tga" };
	static const int numFormats = sizeof(supportedFormats) / sizeof(supportedFormats[0]);
	int format = 0; // 0=png 1=jpg 2=tga
	int quality = 85;
	int argc = Cmd_Argc();
	char* gameDir = FS_Gamedir();
	
	// create the scrnshots directory if it doesn't exist
	Com_sprintf(checkname, sizeof(checkname), "%s/scrnshot", gameDir);
	Sys_Mkdir(checkname);

	// check if we have any args
	if (argc > 1)
	{
		char* maybeFormat = Cmd_Argv(1);
		
		for (i = 0; i<numFormats; ++i)
		{
			if (Q_stricmp(maybeFormat, supportedFormats[i]) == 0)
			{
				format = i;
				break;
			}
		}
		if (i == numFormats)
		{
			Com_Printf("the (optional) second argument to 'screenshot' is the format, one of \"tga\", \"png\", \"jpg\"\n");
			return;
		}
		
		if (argc > 2)
		{
			const char* q = Cmd_Argv(2);
			int qualityStrLen = strlen(q);
			for (i = 0; i<qualityStrLen; ++i)
			{
				if (q[i] < '0' || q[i] > '9')
				{
					Com_Printf("the (optional!) third argument to 'screenshot' is jpg quality, a number between 1 and 100\n");
					Com_Printf("  or png compression level, between 0 and 9!\n");
					return;
				}
			}
			quality = atoi(q);
			if (format == 1) // png
			{
				if (quality < 0)  quality = 0;
				else if (quality > 9)  quality = 9;
			}
			else if (format == 2) // jpg
			{
				if (quality < 1)  quality = 1;
				else if (quality > 100)  quality = 100;
			}
		}
	}

	// find a file name to save it to
	for (i = 0; i <= 9999; i++)
	{
		FILE *f;
		Com_sprintf(checkname, sizeof(checkname), "%s/scrnshot/q2_%04d.%s", gameDir, i, supportedFormats[format]);
		f = fopen(checkname, "rb");
		
		if (!f)
		{
			Com_sprintf(picname, sizeof(picname), "q2_%04d.%s", i, supportedFormats[format]);
			break; // file doesn't exist
		}
		
		fclose(f);
	}
	
	if (i == 10000)
	{
		Com_Printf("VID_WriteScreenshot: Couldn't create a file\n");
		return;
	}
	
	switch (format) // 0=png 1=jpg 2=tga
	{
		case 0:
			stbi_png_level = (quality < 10) ? quality : 7;
			success = stbi_write_png(checkname, width, height, comp, data, 0);
			break;
		case 1:
			success = stbi_write_jpg(checkname, width, height, comp, data, quality);
			break;
		case 2:
			success = stbi_write_tga(checkname, width, height, comp, data);
			break;
	}
	
	if (success)
	{
		Com_Printf("Wrote %s\n", picname);
	}
	else
	{
		Com_Printf("VID_WriteScreenshot: Couldn't write %s\n", picname);
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
	vid_ref = Cvar_Get ("vid_ref", "gl", CVAR_ARCHIVE);
	vid_fullscreen = Cvar_Get ("vid_fullscreen", "0", CVAR_ARCHIVE);
	vid_gamma = Cvar_Get ("vid_gamma", "1.0", CVAR_ARCHIVE);
	vid_contrast = Cvar_Get("vid_contrast", "1.0", CVAR_ARCHIVE);
	vid_refreshrate = Cvar_Get("vid_refreshrate", "-1", CVAR_ARCHIVE);

	// Add some console commands that we want to handle
	Cmd_AddCommand ("vid_restart", VID_Restart_f);
	Cmd_AddCommand ("vid_listmodes", VID_ListModes_f);

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
		RE_Shutdown ();
		VID_FreeReflib ();
	}
}
