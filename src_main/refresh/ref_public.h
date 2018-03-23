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
#ifndef __REF_H
#define __REF_H

#include "qcommon.h"

// TODO: move these math functions out of here
typedef struct _QVECTOR {
	float x;
	float y;
	float z;
} QVECTOR;
typedef struct _QVECTOR QXVECTOR3, *LPQXVECTOR3;

typedef struct _QMATRIX {
	union {
		struct {
			float        _11, _12, _13, _14;
			float        _21, _22, _23, _24;
			float        _31, _32, _33, _34;
			float        _41, _42, _43, _44;
		};
		float m[4][4];
	};
} QMATRIX, *LPQMATRIX;
typedef struct _QMATRIX QXMATRIX, *LPQXMATRIX;

QXMATRIX* QXMatrixIdentity(QXMATRIX *pout);
QXMATRIX* QXMatrixMultiply(QXMATRIX *pout, const QXMATRIX *pm1, const QXMATRIX *pm2);
QMATRIX* QXMatrixInverse(QXMATRIX *pout, float *pdeterminant, const QXMATRIX *pm);

// defines to keep API consistent
#define glmatrix QMATRIX
#define GL_LoadIdentity QXMatrixIdentity
#define GL_MultMatrix QXMatrixMultiply
#define GL_InvertMatrix QXMatrixInverse

// replacement functions that multiply in-place rather than construct a new matrix and leave it at that
glmatrix *GL_TranslateMatrix (glmatrix *m, float x, float y, float z);
glmatrix *GL_ScaleMatrix (glmatrix *m, float x, float y, float z);
glmatrix *GL_RotateMatrix (glmatrix *m, float a, float x, float y, float z);
glmatrix *GL_RadianRotateMatrix (glmatrix *m, float y, float p, float r);
glmatrix *GL_LoadMatrix (glmatrix *dst, glmatrix *src);
glmatrix *GL_OrthoMatrix (glmatrix *m, float left, float right, float bottom, float top, float zNear, float zFar);
glmatrix *GL_PerspectiveMatrix (glmatrix *m, float fovy, float aspect, float zNear, float zFar);
void GL_TransformPoint (glmatrix *m, float *in, float *out);


#define	MAX_DLIGHTS		32
#define	MAX_ENTITIES	1024	// same as max_edicts
#define	MAX_PARTICLES	16000
#define	MAX_LIGHTSTYLES	256

#define POWERSUIT_SCALE		4.0f

#define SHELL_RED_COLOR		0xF2
#define SHELL_GREEN_COLOR	0xD0
#define SHELL_BLUE_COLOR	0xF3

#define SHELL_RG_COLOR		0xDC
#define SHELL_RB_COLOR		0x68
#define SHELL_BG_COLOR		0x78

//ROGUE
#define SHELL_DOUBLE_COLOR	0xDF // 223
#define	SHELL_HALF_DAM_COLOR	0x90
#define SHELL_CYAN_COLOR	0x72
//ROGUE

#define SHELL_WHITE_COLOR	0xD7

typedef struct entity_s
{
	struct model_s		*model;			// opaque type outside refresh
	float				angles[3];

	// most recent data
	float				currorigin[3];		// also used as RF_BEAM's "from"
	int					currframe;			// also used as RF_BEAM's diameter

	// previous data for lerping
	float				lastorigin[3];	// also used as RF_BEAM's "to"
	int					lastframe;

	// misc
	float	backlerp;				// 0.0 = current, 1.0 = old
	int		skinnum;				// also used as RF_BEAM's palette index

	int		lightstyle;				// for flashing entities
	float	alpha;					// ignore if RF_TRANSLUCENT isn't set

	struct image_s	*skin;			// NULL for inline skin
	int		flags;

	glmatrix	matrix;
	int		entnum;
} entity_t;

#define ENTITY_FLAGS 68

typedef struct
{
	vec3_t	origin;
	vec3_t	transformed;
	vec3_t	color;
	float	intensity;
} dlight_t;

typedef struct
{
	vec3_t	origin;

	union
	{
		unsigned int color;
		byte rgba[4];
	};

	// leftover values for the software refresh
	int		soft_color;
	float	alpha;
} particle_t;

typedef struct
{
	float		rgb[3];			// 0.0 - 2.0
	float		white;			// highest of rgb
} lightstyle_t;

typedef struct
{
	int			x, y, width, height;// in virtual screen coordinates
	float		fov_x, fov_y;
	float		vieworg[3];
	float		viewangles[3];
	float		blend[4];			// rgba 0-1 full screen blend
	float		time;				// time is uesed to auto animate
	int			rdflags;			// RDF_UNDERWATER, etc

	byte		*areabits;			// if not NULL, only areas with set bits will be drawn

	lightstyle_t *lightstyles;	// [MAX_LIGHTSTYLES]

	int			num_entities;
	entity_t	*entities;

	int			num_dlights;
	dlight_t	*dlights;

	int			num_particles;
	particle_t	*particles;
} refdef_t;

// ref import functions
cvar_t *Cvar_Get (char *var_name, char *var_value, int flags);
cvar_t *Cvar_Set (char *var_name, char *value);
void Cvar_SetValue (char *var_name, float value);

void Cmd_AddCommand (char *cmd_name, xcommand_t function);
void Cmd_RemoveCommand (char *cmd_name);

int Cmd_Argc (void);
char *Cmd_Argv (int arg);
void Cbuf_ExecuteText (int exec_when, char *text);

int FS_LoadFile (char *path, void **buffer);
void FS_FreeFile (void *buffer);
char *FS_Gamedir (void);

// ref export functions
extern void(*RE_BeginFrame)(float camera_separation);
extern void(*RE_RenderFrame)(refdef_t *fd);
extern void(*RE_EndFrame)(void);

extern void(*RE_SetPalette)(const unsigned char *palette);

extern int(*RE_Init)(void);
extern void(*RE_Shutdown)(void);

extern void(*RE_Draw_StretchRaw)(int x, int y, int w, int h, int cols, int rows, byte *data);
extern void(*RE_Draw_FadeScreen)(void);
extern void(*RE_Draw_Fill)(int x, int y, int w, int h, int c);
extern void(*RE_Draw_TileClear)(int x, int y, int w, int h, char *pic);
extern void(*RE_Draw_CharScaled)(int x, int y, int num, float scale);
extern void(*RE_Draw_Char)(int x, int y, int num);
extern void(*RE_Draw_StretchPic)(int x, int y, int w, int h, char *pic);
extern void(*RE_Draw_PicScaled)(int x, int y, char *pic, float scale);
extern void(*RE_Draw_Pic)(int x, int y, char *pic);
extern void(*RE_Draw_GetPicSize)(int *w, int *h, char *pic);

extern void(*RE_BeginRegistration)(char *model);
extern struct model_s * (*RE_RegisterModel)(char *name);
extern struct image_s * (*RE_RegisterSkin)(char *name);
extern struct image_s * (*RE_Draw_RegisterPic)(char *name);
extern void(*RE_SetSky)(char *name, float rotate, vec3_t axis);
extern void(*RE_EndRegistration)(void);

#define REF_API_UNDETERMINED	1
#define REF_API_OPENGL			2
extern int RE_gfxVal;

// ref entry points
void GL_GFX_CoreInit(void);

// gfx core functions
void GFX_CoreInit(char *name);
void GFX_CoreShutdown(void);
int GFX_Core_GetRefreshRate(void);

#endif // __REF_H
