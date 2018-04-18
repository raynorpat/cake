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

#ifdef _WIN32
#include <windows.h>
#endif

#include <stdio.h>

#include <GL/glew.h>
#ifdef _WIN32
#include <GL/wglew.h>
#endif
#include <GL/gl.h>
#include <GL/glu.h>
#include <math.h>

#include "ref_public.h"
#include "vid_public.h"

#define	LIGHTMAP_SIZE	128
#define	MAX_LIGHTMAPS	128

#ifndef _WIN32
typedef struct _RECT {
  long left;
  long top;
  long right;
  long bottom;
} RECT, *PRECT;
#endif

typedef struct gllightmapstate_s
{
	int	current_lightmap_texture;

	int			allocated[LIGHTMAP_SIZE];
	qboolean	modified[MAX_LIGHTMAPS];
	RECT		lightrect[MAX_LIGHTMAPS];

	// the lightmap texture data needs to be kept in
	// main memory so texsubimage can update properly
	int			lmhunkmark;
	byte		*lmhunkbase;
	unsigned	*lightmap_data[MAX_LIGHTMAPS];
} gllightmapstate_t;

#define	REF_VERSION	"GL 0.01"

extern float v_blend[4];

void RMesh_MakeVertexBuffers (struct model_s *mod);
void Sprite_MakeVertexBuffers (struct model_s *mod);

#define BUFFER_DISCARD		(GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT)
#define BUFFER_NO_OVERWRITE (GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT | GL_MAP_INVALIDATE_RANGE_BIT)

GLuint GL_CreateShaderFromName (char *name, char *vsentry, char *fsentry);
GLuint GL_CreateComputeShaderFromName (char *name);

typedef struct ubodef_s
{
	GLuint ubo;
	GLuint binding;
} ubodef_t;

void GL_UseProgram (GLuint progid);
void GL_UseProgramWithUBOs (GLuint progid, ubodef_t *ubodef, int numubos);

void RSurf_CreatePrograms (void);
void RWarp_CreatePrograms (void);
void RSky_CreatePrograms (void);
void RDraw_CreatePrograms (void);
void RMesh_CreatePrograms (void);
void RBeam_CreatePrograms (void);
void RNull_CreatePrograms (void);
void Sprite_CreatePrograms (void);
void RPart_CreatePrograms (void);

void RPostProcess_Init(void);
void RPostProcess_CreatePrograms(void);
void RPostProcess_Begin (void);
void RPostProcess_FinishToScreen (void);
void RPostProcess_MenuBackground (void);

qboolean LoadImageThruSTB (char *origname, char* type, byte **pic, int *width, int *height);

// up / down
#define	PITCH	0

// left / right
#define	YAW		1

// fall over
#define	ROLL	2


extern	viddef_t	vid;


/*

 skins will be outline flood filled and mip mapped
 pics and sprites with alpha will be outline flood filled
 pic won't be mip mapped

 model skin
 sprite frame
 wall texture
 pic

*/

typedef enum
{
	it_skin,
	it_sprite,
	it_wall,
	it_pic,
	it_sky
} imagetype_t;

typedef struct image_s
{
	char	name[MAX_QPATH];			// game path, including extension
	imagetype_t	type;
	int		width, height;				// source image
	int		upload_width, upload_height;	// after power of two and picmip
	int		registration_sequence;		// 0 = free
	struct msurface_s	*texturechain;	// for sort-by-texture world drawing
	int		texnum;						// gl texture binding
	float	sl, tl, sh, th;				// 0,0 - 1,1
	qboolean	mipmap;
} image_t;

#define		MAX_GLTEXTURES	1024

//===================================================================

typedef struct FBO_s
{
	char            name[MAX_QPATH];

	int             index;

	GLuint          frameBuffer;

	GLuint          colorBuffers[16];
	int             colorFormat;
	GLuint          depthBuffer;
	int             depthFormat;
	GLuint          stencilBuffer;
	int             stencilFormat;

	int             width;
	int             height;
} FBO_t;

#define	MAX_FBOS			64

#define	MAX_BLOOM_BUFFERS	2

void R_FBOList_f(void);

void R_InitFBOs(void);
void R_ShutdownFBOs(void);

qboolean R_CheckFBO(const FBO_t * fbo);

void R_BindNullFBO(void);
void R_BindFBO(FBO_t * fbo);

extern FBO_t *basicRenderFBO;
extern FBO_t *hdrRenderFBO;
extern FBO_t *hdrDownscale64;
extern FBO_t *brightpassRenderFBO;
extern FBO_t *bloomRenderFBO[MAX_BLOOM_BUFFERS];
extern FBO_t *AORenderFBO;

//===================================================================

#include "gl_model.h"

void GL_SetDefaultState (void);
void GL_UpdateSwapInterval (void);
void GL_Clear (GLbitfield mask);

extern	float	gldepthmin, gldepthmax;

typedef struct
{
	float	x, y, z;
	float	s, t;
	float	r, g, b;
} glvert_t;

#define	MAX_LBM_HEIGHT		480

#define BACKFACE_EPSILON	0.01

//====================================================

extern	image_t		gltextures[MAX_GLTEXTURES];
extern	int			numgltextures;

extern FBO_t		*fbos[MAX_FBOS];
extern int			numFBOs;

extern	image_t		*r_notexture;
extern	int			r_visframecount;
extern	int			r_framecount;
extern	cplane_t	frustum[4];
extern	int			c_brush_polys, c_alias_polys;

extern	int			gl_filter_min, gl_filter_max;

//
// view origin
//
extern	vec3_t	vup;
extern	vec3_t	vpn;
extern	vec3_t	vright;
extern	vec3_t	r_origin;

//
// screen size info
//
extern	refdef_t	r_newrefdef;
extern	int		r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;

extern	cvar_t	*r_norefresh;
extern	cvar_t	*r_lefthand;
extern	cvar_t	*r_drawentities;
extern	cvar_t	*r_drawworld;
extern	cvar_t	*r_speeds;
extern	cvar_t	*r_fullbright;
extern	cvar_t	*r_novis;
extern	cvar_t	*r_nocull;
extern  cvar_t  *r_nofog;
extern	cvar_t	*r_lerpmodels;
extern	cvar_t	*r_postprocessing;

extern	cvar_t	*r_lightlevel;	// FIXME: This is a HACK to get the client's light level

extern	cvar_t	*gl_mode;
extern	cvar_t	*gl_showlightmap;
extern	cvar_t	*gl_shownormals;
extern	cvar_t	*gl_shadows;
extern	cvar_t	*gl_dynamic;
extern  cvar_t  *gl_monolightmap;
extern	cvar_t	*gl_finish;
extern	cvar_t	*gl_clear;
extern	cvar_t	*gl_cull;
extern	cvar_t	*gl_swapinterval;
extern	cvar_t	*gl_texturemode;
extern	cvar_t	*gl_textureanisotropy;
extern  cvar_t  *gl_lockpvs;
extern  cvar_t  *gl_forcefog;

extern	cvar_t	*vid_fullscreen;
extern	cvar_t	*vid_gamma;
extern	cvar_t	*vid_contrast;
extern	cvar_t	*r_lightscale;

extern	glmatrix	r_drawmatrix;
extern	glmatrix	r_worldmatrix;
extern	glmatrix	r_projectionmatrix;
extern	glmatrix	r_mvpmatrix;

extern GLuint r_surfacesampler;
extern GLuint r_lightmapsampler;
extern GLuint r_drawclampsampler;
extern GLuint r_drawwrapsampler;
extern GLuint r_drawnearestclampsampler;
extern GLuint r_skysampler;
extern GLuint r_modelsampler;

extern vec3_t post_fogColor;
extern float post_fogDensity;

void RImage_CreateSamplers (void);

void GL_BindTexture (GLenum tmu, GLenum target, GLuint sampler, GLuint texnum);

void R_LightPoint (vec3_t p, vec3_t color, float *lightspot);

void R_MarkLights (mnode_t *headnode, glmatrix *transform);
void R_EnableLights (int framecount, int bitmask);

//====================================================================

extern	model_t	*r_worldmodel;

extern	unsigned	d_8to24table_rgba[256];

extern	int		registration_sequence;

int RE_GL_Init (void);
void RE_GL_Shutdown (void);

void R_RenderView (refdef_t *fd);
void GL_ScreenShot_f (void);
void R_DrawAliasModel (entity_t *e);
void R_DrawBrushModel (entity_t *e);
void R_DrawSpriteModel (entity_t *e);
void R_DrawBeam (entity_t *e);
void R_DrawWorld (void);
void R_DrawAlphaSurfaces (void);
void R_InitParticleTexture (void);
void Draw_InitLocal (void);
qboolean R_CullBox (vec3_t mins, vec3_t maxs);
void R_MarkLeaves (void);
void R_DrawNullModel (entity_t *e);
void R_DrawParticles (void);

void R_DrawSkyChain (msurface_t *surf);
void R_DrawSurfaceChain (msurface_t *chain, int numindexes);

void RE_GL_Draw_GetPicSize (int *w, int *h, char *name);
void RE_GL_Draw_Pic (int x, int y, char *name, float scale);
void RE_GL_Draw_StretchPic (int x, int y, int w, int h, char *name);
void RE_GL_Draw_StretchPicExt (float x, float y, float w, float h, float sl, float tl, float sh, float th, char *pic);
void RE_GL_Draw_SetColor (float *rgba);
void RE_GL_Draw_Char (int x, int y, int num, float scale);
void RE_GL_Draw_TileClear (int x, int y, int w, int h, char *name);
void RE_GL_Draw_Fill (int x, int y, int w, int h, int c);
void RE_GL_Draw_FadeScreen (void);
void RE_GL_Draw_StretchRaw (int x, int y, int w, int h, int cols, int rows, byte *data);

void Draw_Begin2D (void);
void Draw_End2D (void);

void RE_GL_BeginFrame (float camera_separation);
void RE_GL_EndFrame (void);

void RE_GL_BeginRegistration(char *model);
struct model_s *RE_GL_RegisterModel(char *name);
struct image_s *RE_GL_RegisterSkin(char *name);
struct image_s *RE_GL_Draw_RegisterPic(char *name);
void RE_GL_SetSky(char *name, float rotate, vec3_t axis);
void RE_GL_SetFog(vec4_t axis);
void RE_GL_EndRegistration(void);

int	Draw_GetPalette (void);

void GL_ResampleTexture (unsigned *in, int inwidth, int inheight, unsigned *out, int outwidth, int outheight);

image_t *GL_LoadPic (char *name, byte *pic, int width, int height, imagetype_t type, int bits);
image_t	*GL_FindImage (char *name, imagetype_t type);
void GL_TextureMode (char *string, int anisotropy);
void GL_ImageList_f (void);

void GL_InitImages (void);
void GL_ShutdownImages (void);

void GL_FreeUnusedImages (void);

void R_InitFreeType (void);
void R_DoneFreeType (void);
void RE_GL_RegisterFont (char *fontName, int pointSize, fontInfo_t * font);

int RE_GL_MarkFragments (vec3_t origin, vec3_t axis[3], float radius, int maxPoints, vec3_t *points, int maxFragments, markFragment_t *fragments);

typedef struct
{
	const char *renderer_string;
	const char *vendor_string;
	const char *version_string;
	const char *extension_string;

	qboolean	gl_ext_GPUShader5_support;
	qboolean	gl_ext_computeShader_support;
} glconfig_t;

typedef struct
{
	GLuint	lightmap_textures;

	FBO_t	*currentFBO;

	GLuint	currentsamplers[32];
	GLuint	currenttextures[32];
	GLuint	currentprogram;
	int		currentvertexarray;
	int		statebits;

	GLenum srcblend;
	GLenum dstblend;
} glstate_t;

extern glconfig_t gl_config;
extern glstate_t  gl_state;

extern GLuint gl_sharedubo;

void RMain_InvalidateCachedState (void);

#define CULLFACE_BIT		(1 << 0)
#define BLEND_BIT			(1 << 1)
#define DEPTHTEST_BIT		(1 << 2)
#define SCISSOR_BIT			(1 << 3)
#define DEPTHWRITE_BIT		(1 << 4)
#define GOURAUD_BIT			(1 << 5)

#define VAA0	(1 << 0)
#define VAA1	(1 << 1)
#define VAA2	(1 << 2)
#define VAA3	(1 << 3)

#define VAA4	(1 << 4)
#define VAA5	(1 << 5)
#define VAA6	(1 << 6)
#define VAA7	(1 << 7)

#define VAA8	(1 << 8)
#define VAA9	(1 << 9)
#define VAA10	(1 << 10)
#define VAA11	(1 << 11)

#define VAA12	(1 << 12)
#define VAA13	(1 << 13)
#define VAA14	(1 << 14)
#define VAA15	(1 << 15)

void GL_Enable (int bits);
void GL_BlendFunc (GLenum sfactor, GLenum dfactor);
void GL_BindVertexArray (GLuint vertexarray);

void GL_CheckError (char *str);

typedef struct cubeface_s
{
	int width;
	int height;
	byte *data;
} cubeface_t;

GLuint GL_UploadTexture (byte *data, int width, int height, qboolean mipmap, int bits);
GLuint GL_LoadCubeMap (cubeface_t *faces);

void *Img_Alloc (int size);
void Img_Free (void);

typedef struct sharedubo_s
{
	glmatrix worldMatrix;

	// padded for std140
	float forwardVec[4];
	float upVec[4];
	float rightVec[4];
	float viewOrigin[4];
} sharedubo_t;
