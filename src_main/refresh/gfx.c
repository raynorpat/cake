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
void            (* RE_EndRegistration)( void ) = NULL;

int				RE_gfxVal;


/*
============
GFX_GL_CoreInit

OpenGL refresh core
============
*/
void RE_GL_BeginFrame(float camera_separation);
void RE_GL_RenderFrame(refdef_t *fd);
void RE_GL_EndFrame(void);
void RE_GL_SetPalette(const unsigned char *palette);
int RE_GL_Init(void);
void RE_GL_Shutdown(void);
void RE_GL_Draw_StretchRaw(int x, int y, int w, int h, int cols, int rows, byte *data);
void RE_GL_Draw_FadeScreen(void);
void RE_GL_Draw_Fill(int x, int y, int w, int h, int c);
void RE_GL_Draw_TileClear(int x, int y, int w, int h, char *name);
void RE_GL_Draw_CharScaled(int x, int y, int num, float scale);
void RE_GL_Draw_Char(int x, int y, int c);
void RE_GL_Draw_StretchPic(int x, int y, int w, int h, char *name);
void RE_GL_Draw_PicScaled(int x, int y, char *name, float scale);
void RE_GL_Draw_Pic(int x, int y, char *name);
void RE_GL_Draw_GetPicSize(int *w, int *h, char *name);
void RE_GL_BeginRegistration(char *model);
struct model_s *RE_GL_RegisterModel(char *name);
struct image_s *RE_GL_RegisterSkin(char *name);
struct image_s *RE_GL_Draw_RegisterPic(char *name);
void RE_GL_SetSky(char *name, float rotate, vec3_t axis);
void RE_GL_EndRegistration(void);

static void GFX_GL_CoreInit (void)
{
	RE_BeginFrame = RE_GL_BeginFrame;
	RE_RenderFrame = RE_GL_RenderFrame;
	RE_EndFrame = RE_GL_EndFrame;

	RE_SetPalette = RE_GL_SetPalette;

	RE_Init = RE_GL_Init;
	RE_Shutdown = RE_GL_Shutdown;

	RE_Draw_StretchRaw = RE_GL_Draw_StretchRaw;
	RE_Draw_FadeScreen = RE_GL_Draw_FadeScreen;
	RE_Draw_Fill = RE_GL_Draw_Fill;
	RE_Draw_TileClear = RE_GL_Draw_TileClear;
	RE_Draw_CharScaled = RE_GL_Draw_CharScaled;
	RE_Draw_Char = RE_GL_Draw_Char;
	RE_Draw_StretchPic = RE_GL_Draw_StretchPic;
	RE_Draw_PicScaled = RE_GL_Draw_PicScaled;
	RE_Draw_Pic = RE_GL_Draw_Pic;
	RE_Draw_GetPicSize = RE_GL_Draw_GetPicSize;
    
	RE_BeginRegistration = RE_GL_BeginRegistration;
	RE_RegisterModel = RE_GL_RegisterModel;
	RE_RegisterSkin = RE_GL_RegisterSkin;
	RE_Draw_RegisterPic = RE_GL_Draw_RegisterPic;
	RE_SetSky = RE_GL_SetSky;
	RE_EndRegistration = RE_GL_EndRegistration;
}


/*
============
GFX_SOFT_CoreInit

Software refresh core
============
*/
void RE_SW_BeginFrame(float camera_separation);
void RE_SW_RenderFrame(refdef_t *fd);
void RE_SW_EndFrame(void);
void RE_SW_SetPalette(const unsigned char *palette);
int RE_SW_Init(void);
void RE_SW_Shutdown(void);
void RE_SW_Draw_StretchRaw(int x, int y, int w, int h, int cols, int rows, byte *data);
void RE_SW_Draw_FadeScreen(void);
void RE_SW_Draw_Fill(int x, int y, int w, int h, int c);
void RE_SW_Draw_TileClear(int x, int y, int w, int h, char *name);
void RE_SW_Draw_CharScaled(int x, int y, int num, float scale);
void RE_SW_Draw_StretchPic(int x, int y, int w, int h, char *name);
void RE_SW_Draw_PicScaled(int x, int y, char *name, float scale);
void RE_SW_Draw_GetPicSize(int *w, int *h, char *name);
void RE_SW_BeginRegistration(char *model);
struct model_s *RE_SW_RegisterModel(char *name);
struct image_s *RE_SW_RegisterSkin(char *name);
struct image_s *RE_SW_Draw_RegisterPic(char *name);
void RE_SW_SetSky(char *name, float rotate, vec3_t axis);
void RE_SW_EndRegistration(void);

static void GFX_SOFT_CoreInit (void)
{
	RE_BeginFrame = RE_SW_BeginFrame;
	RE_RenderFrame = RE_SW_RenderFrame;
	RE_EndFrame = RE_SW_EndFrame;

	RE_SetPalette = RE_SW_SetPalette;

	RE_Init = RE_SW_Init;
	RE_Shutdown = RE_SW_Shutdown;

	RE_Draw_StretchRaw = RE_SW_Draw_StretchRaw;
	RE_Draw_FadeScreen = RE_SW_Draw_FadeScreen;
	RE_Draw_Fill = RE_SW_Draw_Fill;
	RE_Draw_TileClear = RE_SW_Draw_TileClear;
	RE_Draw_CharScaled = RE_SW_Draw_CharScaled;
	RE_Draw_Char = NULL;
	RE_Draw_StretchPic = RE_SW_Draw_StretchPic;
	RE_Draw_PicScaled = RE_SW_Draw_PicScaled;
	RE_Draw_Pic = NULL;
	RE_Draw_GetPicSize = RE_SW_Draw_GetPicSize;

	RE_BeginRegistration = RE_SW_BeginRegistration;
	RE_RegisterModel = RE_SW_RegisterModel;
	RE_RegisterSkin = RE_SW_RegisterSkin;
	RE_Draw_RegisterPic = RE_SW_Draw_RegisterPic;
	RE_SetSky = RE_SW_SetSky;
	RE_EndRegistration = RE_SW_EndRegistration;
}


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
		if (!strcmp(vid_ref->string, "soft"))
			RE_gfxVal = REF_API_SOFT;
		else if (!strcmp(vid_ref->string, "gl"))
			RE_gfxVal = REF_API_OPENGL;
		else if (!strcmp(vid_ref->string, "d3d9"))
			RE_gfxVal = REF_API_DIRECT3D_9;
	}

	// init function pointers according to vid_ref cvar
	switch (RE_gfxVal)
	{
	case REF_API_SOFT:
		GFX_SOFT_CoreInit ();
		break;
	case REF_API_OPENGL:
		GFX_GL_CoreInit ();
		break;
	case REF_API_DIRECT3D_9:
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
	case REF_API_SOFT:
		// software shutdown works fine with call to RE_Shutdown
		break;
	case REF_API_OPENGL:
		// OpenGL needs to destroy everything, including the context, hence this extra call
		VID_Shutdown_GL (false);
		break;
	case REF_API_DIRECT3D_9:
		break;
	case REF_API_UNDETERMINED:
	default:
		break;
	}
}
