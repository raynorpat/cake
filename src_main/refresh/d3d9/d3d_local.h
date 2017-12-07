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

#include <windows.h>
#include <stdio.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
#include "ref_public.h"
#include "vid_public.h"
#ifdef __cplusplus
}
#endif
#include <vector>

#include "d3d_matrix.h"

#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>

#define SAFE_RELEASE(COM_Generic) {if ((COM_Generic)) {(COM_Generic)->Release (); (COM_Generic) = NULL;}}

// this macro just exists so that i don't need to remember the order these elements go in
#define VDECL(Stream, Offset, Type, Usage, UsageIndex) \
	{Stream, Offset, Type, D3DDECLMETHOD_DEFAULT, Usage, UsageIndex}

// make qsort calls easier on the eye...
typedef int(*sortfunc_t) (const void *, const void *);

extern IDirect3D9 *d3d_Object;
extern IDirect3DDevice9 *d3d_Device;

#define	REF_VERSION	"D3D 2.00"

// up / down
#define	PITCH	0

// left / right
#define	YAW		1

// fall over
#define	ROLL	2


extern "C" viddef_t vid;

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
	int		registration_sequence;		// 0 = free

	float	sl, tl, sh, th;				// 0,0 - 1,1 unless part of the scrap
	qboolean	scrap;
	qboolean	mipmap;
	qboolean	has_alpha;

	struct msurface_s *texturechain;	// for sort-by-texture world drawing
	struct msurface_s **texturechain_tail;	// for sort-by-texture world drawing
	IDirect3DTexture9 *d3d_Texture;
} image_t;

#define	TEXNUM_LIGHTMAPS	1024
#define	TEXNUM_SCRAPS		1152
#define	TEXNUM_IMAGES		1153


//===================================================================

#include "d3d_model.h"

void GL_BeginRendering (int *x, int *y, int *width, int *height);
void GL_EndRendering (void);

void D3D9_UpdateSwapInterval (void);
void D3D9_SetDefaultState (void);

extern	float	gldepthmin, gldepthmax;

#define	MAX_LBM_HEIGHT		480

#define BACKFACE_EPSILON	0.01


//====================================================


extern	std::vector<image_t *> gl_textures;

extern	image_t		*r_greytexture;
extern	image_t		*r_whitetexture;
extern	image_t		*r_blacktexture;
extern	image_t		*r_notexture;
extern	entity_t	*currententity;
extern	int			r_visframecount;
extern	int			r_framecount;
extern	int			c_brush_polys, c_alias_polys, c_draw_calls;


extern	int			gl_filter_min, gl_filter_max;


// screen size info
extern	refdef_t	r_newrefdef;
extern	int		r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;

extern	cvar_t	*r_ambient;
extern	cvar_t	*r_norefresh;
extern	cvar_t	*r_lefthand;
extern	cvar_t	*r_drawentities;
extern	cvar_t	*r_drawworld;
extern	cvar_t	*r_speeds;
extern	cvar_t	*r_fullbright;
extern	cvar_t	*r_novis;
extern	cvar_t	*r_nocull;
extern	cvar_t	*r_lerpmodels;

extern	cvar_t	*r_lightlevel;	// FIXME: This is a HACK to get the client's light level

extern cvar_t	*gl_monolightmap;

extern	cvar_t	*gl_mode;
extern	cvar_t	*gl_lightmap;
extern	cvar_t	*gl_shadows;
extern	cvar_t	*gl_dynamic;
extern	cvar_t	*gl_nobind;
extern	cvar_t	*gl_round_down;
extern	cvar_t	*gl_picmip;
extern	cvar_t	*gl_skymip;
extern	cvar_t	*gl_showtris;
extern	cvar_t	*gl_finish;
extern	cvar_t	*gl_clear;
extern	cvar_t	*gl_cull;
extern	cvar_t	*gl_poly;
extern	cvar_t	*gl_texsort;
extern	cvar_t	*gl_polyblend;
extern	cvar_t	*gl_lightmaptype;
extern	cvar_t	*gl_swapinterval;
extern	cvar_t	*gl_texturemode;
extern	cvar_t	*gl_textureanisotropy;
extern  cvar_t  *gl_lockpvs;
extern  cvar_t  *gl_lockfrustum;

extern	cvar_t	*vid_fullscreen;
extern	cvar_t	*vid_gamma;
extern	cvar_t	*vid_contrast;

extern	int		c_visible_lightmaps;
extern	int		c_visible_textures;

extern	float	r_world_matrix[16];

extern	float	v_blend[4];			// final blending color

void R_TranslatePlayerSkin (int playernum);
void GL_TexEnv (int value);
void GL_EnableMultitexture (qboolean enable);
void GL_SelectTexture (int);

void R_LightPoint (vec3_t p, float *color, float *lightspot);
void R_PushDlights (mnode_t *headnode, float *origin);

//====================================================================

extern	model_t	*r_worldmodel;

extern PALETTEENTRY d_8to24table[256];

extern	int		registration_sequence;


void V_AddBlend (float r, float g, float b, float a, float *v_blend);

int 	R_Init (void);
void	R_Shutdown (void);

void R_RenderView (refdef_t *fd);
void GL_ScreenShot_f (void);
void R_DrawAliasModel (entity_t *e);
void R_DrawSpriteModel (entity_t *e);
void R_DrawBeam (entity_t *e);
void R_DrawNullModel (entity_t *e);
void R_DrawWorld (void);
void R_DrawAlphaSurfaces (void);
void R_InitTextures (void);
void D3D_Draw_InitLocal (void);
qboolean R_CullBox (vec3_t mins, vec3_t maxs);
void R_MarkLeaves (void);

extern cvar_t *r_currentspeed;

void R_SetLiquidState (void);
void R_SetSolidState (int flags);
void R_SetAlphaState (int flags);

// sky is drawn separately
void R_DrawSkySurfaces (msurface_t *surfaces);

void R_DrawLiquidChain (msurface_t *surf, image_t *image);
void R_DrawSolidChain (msurface_t *surf, image_t *image);

void R_SetSurfaceScroll (msurface_t *surf, float scrollspeed);

void R_MarkLights (dlight_t *light, int bit, mnode_t *node);

#if 0
short LittleShort (short l);
short BigShort (short l);
int	LittleLong (int l);
float LittleFloat (float f);

char	*va (char *format, ...);
// does a varargs printf into a temp buffer
#endif

void COM_StripExtension (char *in, char *out);

void Draw_GetPicSize (int *w, int *h, char *name);
void Draw_CharScaled (int x, int y, int num, float scale);
void Draw_Char (int x, int y, int c);
void Draw_TileClear (int x, int y, int w, int h, char *name);
void Draw_Fill (int x, int y, int w, int h, int c);
void Draw_FadeScreen (void);
void Draw_StretchRaw (int x, int y, int w, int h, int cols, int rows, byte *data);
void Draw_StretchPic (int x, int y, int w, int h, char *pic);
void Draw_PicScaled (int x, int y, char *pic, float scale);
void Draw_Pic (int x, int y, char *pic);

void R_BeginFrame (float camera_separation);
void R_SwapBuffers (int);
void R_SetPalette (const unsigned char *palette);

int  Draw_GetPalette (void);

void D3D9_BeginRegistration (char *model);
struct image_s *D3D9_RegisterSkin (char *name);
struct model_s *D3D9_RegisterModel (char *name);
struct image_s *Draw_FindPic (char *name);
void D3D9_SetSky(char *name, float rotate, vec3_t axis);
void D3D9_EndRegistration(void);

image_t *GL_LoadPic (char *name, byte *pic, int width, int height, imagetype_t type, int bits);
image_t *D3D9_FindImage (char *name, imagetype_t type);
void GL_TextureMode (char *string, int anisotropy);
void GL_ImageList_f (void);

void GL_SetTexturePalette (unsigned palette[256]);

void GL_InitImages (void);
void GL_ShutdownImages (void);

void GL_FreeUnusedImages (void);
void R_FreeUnusedMeshBuffers (void);
void R_FreeUnusedSpriteBuffers (void);


typedef struct
{
	int renderer;
	const char *renderer_string;
	const char *vendor_string;
	const char *version_string;
	const char *extensions_string;
} glconfig_t;

typedef struct d3dstate_s
{
	union
	{
		struct
		{
			// keep these contiguous in memory
			D3DQMATRIX WorldMatrix;

			// padded for shader registers
			float	r_origin[4];
			float	vpn[4];
			float	vup[4];
			float	vright[4];
		};

		float Viewpoint[32];
	};

	qboolean fullscreen;
	int		prev_mode;

	float	surfscroll;
	float	warpscroll;

	cplane_t frustum[4];
	int		framecount;

	float	camera_separation;
	qboolean stereo_enabled;

	inline d3dstate_s() {}
} d3dstate_t;

extern glconfig_t  gl_config;
extern d3dstate_t   d3d_state;

struct shadeinfo_t
{
	float light[4];
	float vector[3];
};


/*
====================================================================

IMPLEMENTATION SPECIFIC FUNCTIONS

====================================================================
*/

qboolean VID_Init_D3D9 (void);

void GLimp_BeginFrame (float camera_separation);
void GLimp_EndFrame (void);
void GLimp_Shutdown (void);
int GLimp_SetMode(int fullscreen, int *pwidth, int *pheight);
void VID_ResetDevice (void);

#define BYTE_CLAMP(c) (int) ((c) > 255 ? 255 : (c < 0 ? 0 : c))

extern D3DCAPS9 d3d_DeviceCaps;

void D3D_Set2D (void);
void D3D_End2D (void);

IDirect3DVertexShader9 *D3D_CreateVertexShader (char *name, char *entrypoint);
IDirect3DPixelShader9 *D3D_CreatePixelShader (char *name, char *entrypoint);

void R_BeginBatchingSurfaces (void);
void R_BatchSurface (msurface_t *surf);
void R_EndBatchingSurfaces (void);

void R_DrawParticles (void);

// image loading
void *Scratch_Alloc (int size);
void Scratch_Free (void);
int Scratch_LowMark (void);
void Scratch_FreeToLowMark (int mark);

void LoadTGAFile (char *name, byte **pic, int *width, int *height);
void LoadPCX (char *filename, byte **pic, byte **palette, int *width, int *height);
void R_FloodFillSkin (byte *skin, int skinwidth, int skinheight);
void GL_Image8To32 (byte *data8, unsigned *data32, int size, unsigned *palette);

#define	SCRAP_SIZE 256

class ImageScrap
{
public:
	ImageScrap (void);
	~ImageScrap (void);
	bool AllocBlock (int w, int h, int *x, int *y);
	void LoadTexels (int x, int y, image_t *image, byte *data, int bits);

	static void Init (void);
	static void Shutdown (void);
	static ImageScrap *Scrap_AllocBlock (int w, int h, int *x, int *y);

private:
	IDirect3DTexture9 *Texture;
	int Allocated[SCRAP_SIZE];
};


extern std::vector<ImageScrap *> Scraps;

struct texdimension_t
{
	int Width;
	int Height;
};

// shared by a few places
void D3D_GetScaledTextureDimensions (int width, int height, qboolean mipmap, texdimension_t *scaled);
qboolean D3D_FillTexture (IDirect3DTexture9 *tex, void *data, int width, int height, qboolean mipmap, PALETTEENTRY *palette);

// helpers/etc
void D3D_CollapseRowPitch (unsigned *data, int width, int height, int pitch);
void D3D_Compress32To24 (byte *data, int width, int height);
void D3D_WriteDataToTGA (char *name, void *data, int width, int height, int bpp);

// object creation/destruction
#define VH_LOADOBJECTS		1
#define VH_SHUTDOWN			2
#define VH_LOSTDEVICE		4
#define VH_RESETDEVICE		8

typedef void (*d3d9func_t) (void);

class CD3DHandler
{
public:
	CD3DHandler (
		d3d9func_t loadobjects = NULL,
		d3d9func_t shutdown = NULL,
		d3d9func_t lostdevice = NULL,
		d3d9func_t resetdevice = NULL
	);
};

void VID_RunHandlers (int handlerflags);

extern IDirect3DStateBlock9 *d3d_EnableAlphaObjects;
extern IDirect3DStateBlock9 *d3d_DisableAlphaObjects;
extern IDirect3DStateBlock9 *d3d_MainSceneState;
extern IDirect3DStateBlock9 *d3d_2DGUIState;
extern IDirect3DStateBlock9 *d3d_RTTState;
extern IDirect3DStateBlock9 *d3d_PolyblendState;
extern IDirect3DStateBlock9 *d3d_EnableShadows;
extern IDirect3DStateBlock9 *d3d_DisableShadows;


struct samplerstate_t
{
	// address mode is not user-configurable; everything else is
	samplerstate_t (D3DTEXTUREFILTERTYPE magfilter, D3DTEXTUREFILTERTYPE mipfilter, D3DTEXTUREFILTERTYPE minfilter, D3DTEXTUREADDRESS addressmode, DWORD maxanisotropy);
	void Update (D3DTEXTUREFILTERTYPE magfilter, D3DTEXTUREFILTERTYPE mipfilter, D3DTEXTUREFILTERTYPE minfilter, DWORD maxanisotropy);

	D3DTEXTUREFILTERTYPE MagFilter;
	D3DTEXTUREFILTERTYPE MipFilter;
	D3DTEXTUREFILTERTYPE MinFilter;
	D3DTEXTUREADDRESS AddressMode;
	DWORD MaxAnisotropy;
};


extern samplerstate_t d3d_DrawSampler;
extern samplerstate_t d3d_MeshSampler;
extern samplerstate_t d3d_SpriteSampler;
extern samplerstate_t d3d_SurfDiffuseSampler;
extern samplerstate_t d3d_SkySampler;


struct streamdef_t
{
	IDirect3DVertexBuffer9 *pStreamData;
	UINT OffsetInBytes;
	UINT Stride;
	UINT FrequencyParameter;
};


class CStateManager
{
public:
	CStateManager (void);
	~CStateManager (void);

	// these are the states we wish to manage
    void SetTexture (DWORD Stage, samplerstate_t *SamplerState, IDirect3DBaseTexture9 *pTexture);
	void SetSamplerState (DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value);
    void SetVertexDeclaration (IDirect3DVertexDeclaration9 *pDecl);
	void SetVertexShader (IDirect3DVertexShader9 *pShader);
    void SetPixelShader (IDirect3DPixelShader9 *pShader);
	void SetStreamSource (UINT StreamNumber, IDirect3DVertexBuffer9 *pStreamData, UINT OffsetInBytes, UINT Stride);
	void SetStreamSourceFreq (UINT StreamNumber, UINT FrequencyParameter);
	void SetIndices (IDirect3DIndexBuffer9 *pIndexData);
	void GetViewport (D3DVIEWPORT9 *vp);
	void SetViewport (D3DVIEWPORT9 *vp);
	void SetViewport (int x, int y, int w, int h, float zn, float zf);

	// just for convenience
	void SetVertexShaderConstant1f (int reg, float a);
	void SetVertexShaderConstant2f (int reg, float a, float b);
	void SetVertexShaderConstant3f (int reg, float a, float b, float c);
	void SetVertexShaderConstant4f (int reg, float a, float b, float c, float d);
	void SetVertexShaderConstant2fv (int reg, float *a);
	void SetVertexShaderConstant3fv (int reg, float *a);
	void SetVertexShaderConstant4fv (int reg, float *constant, int numfloat4 = 1);
	void SetPixelShaderConstant1f (int reg, float a);
	void SetPixelShaderConstant2f (int reg, float a, float b);
	void SetPixelShaderConstant3f (int reg, float a, float b, float c);
	void SetPixelShaderConstant4f (int reg, float a, float b, float c, float d);
	void SetPixelShaderConstant2fv (int reg, float *a);
	void SetPixelShaderConstant3fv (int reg, float *a);
	void SetPixelShaderConstant4fv (int reg, float *constant, int numfloat4 = 1);

private:
	D3DVIEWPORT9 currentViewPort;
	IDirect3DBaseTexture9 *currentTextures[8];
	DWORD currentSamplerState[8][16];
	IDirect3DVertexDeclaration9 *currentVertexDeclaration;
	IDirect3DVertexShader9 *currentVertexShader;
	IDirect3DPixelShader9 *currentPixelShader;
	streamdef_t currentStreams[16];
	IDirect3DIndexBuffer9 *currentIndexBuffer;
};


extern CStateManager *d3d_State;


#define	LIGHTMAP_SIZE	128

struct lightmap_t
{
	qboolean Modified;
	IDirect3DTexture9 *DefTexture;
	IDirect3DTexture9 *SysTexture;
};


extern std::vector<lightmap_t *> Lightmaps;

void D3D_UpdateLightmaps (void);


class CRenderToTexture
{
public:
	CRenderToTexture (void);
	void Begin (IDirect3DTexture9 *rttTexture);
	void End (int pass);

private:
	IDirect3DSurface9 *BackBuffer;
	IDirect3DSurface9 *RTTSurface;
	IDirect3DTexture9 *RTTTexture;
};


extern IDirect3DTexture9 *d3d_BrightpassTexture;
extern IDirect3DTexture9 *d3d_WaterwarpTexture;

#define RTTPASS_BRIGHTPASS		1
#define RTTPASS_WATERWARP		2

