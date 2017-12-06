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
// gfx.h -- graphics layer core

#ifndef __GFX_H__
#define __GFX_H__

extern void (* RE_BeginFrame)( float camera_separation );
extern void (* RE_RenderFrame)( refdef_t *fd );
extern void (* RE_EndFrame)( void );

extern void (* RE_SetPalette)( const unsigned char *palette );

extern int (* RE_Init)( void );
extern void (* RE_Shutdown)( void );

extern void (* RE_Draw_StretchRaw)( int x, int y, int w, int h, int cols, int rows, byte *data );
extern void (* RE_Draw_FadeScreen)( void );
extern void (* RE_Draw_Fill)( int x, int y, int w, int h, int c );
extern void (* RE_Draw_TileClear)( int x, int y, int w, int h, char *pic );
extern void (* RE_Draw_CharScaled)( int x, int y, int num, float scale );
extern void (* RE_Draw_Char)( int x, int y, int num );
extern void	(* RE_Draw_StretchPic)( int x, int y, int w, int h, char *pic );
extern void (* RE_Draw_PicScaled)( int x, int y, char *pic, float scale );
extern void	(* RE_Draw_Pic)( int x, int y, char *pic );
extern void (* RE_Draw_GetPicSize)( int *w, int *h, char *pic );

extern void (* RE_BeginRegistration)( char *model );
extern struct model_s * (* RE_RegisterModel)( char *name );
extern struct image_s * (* RE_RegisterSkin)( char *name );
extern struct image_s * (* RE_Draw_RegisterPic)( char *name );
extern void  (* RE_SetSky)( char *name, float rotate, vec3_t axis );
extern void (* RE_EndRegistration)( void );

#define REF_API_UNDETERMINED	1
#define REF_API_SOFT	 		2
#define REF_API_OPENGL			3
#define REF_API_DIRECT3D_9 		4
extern int RE_gfxVal;

void GFX_CoreInit (char *name);
void GFX_CoreShutdown (void);

// refresh entry points
void GL_GFX_CoreInit (void);
void SW_GFX_CoreInit (void);
void D3D9_GFX_CoreInit (void);

#endif