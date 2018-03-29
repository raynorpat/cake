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
// gfx.c -- graphics layer core
#include "ref_public.h"
#include "vid_public.h"

extern cvar_t	*vid_ref;

void            (* RE_BeginFrame)( float camera_separation ) = NULL;
void			(* RE_RenderFrame)( refdef_t *fd ) = NULL;
void			(* RE_EndFrame)( void ) = NULL;

void            (* RE_SetPalette)( const unsigned char *palette ) = NULL;

int		        (* RE_Init)( void ) = NULL;
void            (* RE_Shutdown)( void ) = NULL;

void            (* RE_Draw_StretchRaw)( int x, int y, int w, int h, int cols, int rows, byte *data ) = NULL;
void            (* RE_Draw_FadeScreen)( void ) = NULL;
void            (* RE_Draw_Fill)( int x, int y, int w, int h, int c ) = NULL;
void            (* RE_Draw_TileClear)( int x, int y, int w, int h, char *pic ) = NULL;
void            (* RE_Draw_CharScaled)( int x, int y, int num, float scale ) = NULL;
void            (* RE_Draw_Char)( int x, int y, int num ) = NULL;
void		    (* RE_Draw_StretchPic)( int x, int y, int w, int h, char *pic ) = NULL;
void            (* RE_Draw_PicScaled)( int x, int y, char *pic, float scale ) = NULL;
void	        (* RE_Draw_Pic)( int x, int y, char *pic ) = NULL;
void            (* RE_Draw_GetPicSize)( int *w, int *h, char *pic ) = NULL;

void            (* RE_BeginRegistration)( char *model ) = NULL;
struct model_s * (* RE_RegisterModel)( char *name ) = NULL;
struct image_s * (* RE_RegisterSkin)( char *name ) = NULL;
struct image_s * (* RE_Draw_RegisterPic)( char *name ) = NULL;
void            (* RE_SetSky)( char *name, float rotate, vec3_t axis ) = NULL;
void			(* RE_SetFog)( vec4_t fog ) = NULL;
void            (* RE_EndRegistration)( void ) = NULL;

int				RE_gfxVal;


/*
============
GFX_CoreInit

Figures out which refresh core to startup and init based on the vid_ref cvar
============
*/
void GFX_CoreInit (char *name)
{
	RE_gfxVal = REF_API_UNDETERMINED;
	if (vid_ref)
	{
		if (!strcmp(vid_ref->string, "gl"))
			RE_gfxVal = REF_API_OPENGL;
	}

	// init function pointers according to vid_ref cvar
	switch (RE_gfxVal)
	{
	case REF_API_OPENGL:
#ifndef WIN_UWP
		GL_GFX_CoreInit ();
#endif
		break;
	case REF_API_UNDETERMINED:
	default:
		Com_Error (ERR_FATAL, "Couldn't load refresh subsystem: undetermined refresh!");
		break;
	}
}

/*
============
GFX_CoreShutdown

Does any kind of shutdown for whichever refresh core we are using
============
*/
void GFX_CoreShutdown (void)
{
	switch (RE_gfxVal)
	{
	case REF_API_OPENGL:
#ifndef WIN_UWP
		// OpenGL needs to destroy everything, including the context, hence this extra call
		VID_Shutdown_GL (false);
#endif
		break;
	case REF_API_UNDETERMINED:
	default:
		break;
	}
}

/*
============
GFX_Core_GetRefreshRate

Grabs current refresh rate from core
============
*/
int GFX_Core_GetRefreshRate (void)
{
	int refreshRate = 60;

	switch (RE_gfxVal)
	{
	case REF_API_OPENGL:
#ifndef WIN_UWP
		refreshRate = VID_GL_GetRefreshRate ();
#endif
		break;
	case REF_API_UNDETERMINED:
	default:
		// should never get here
		break;
	}

	return refreshRate;
}
