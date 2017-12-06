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

// draw.c

#include "d3d_local.h"

// this is our default half-pixel offset because y is inverted in the ortho projection
// it's verified by comparing screenshots before and after with marginal vid_gamma differences
// to ensure that everything lines up correctly
#define DEFAULT_HALFPIXEL	0.5f, -0.5f, 0.0f, 0.0f

// this is for drawing that doesn't require a half-pixel offset
#define NO_HALFPIXEL		0.0f, 0.0f, 0.0f, 0.0f

static image_t *draw_chars;

cvar_t *r_waterwarpscale;
cvar_t *r_waterwarpfactor;
cvar_t *r_waterwarptime;

void Draw_NewCinematic (void);

samplerstate_t d3d_CharSampler (D3DTEXF_POINT, D3DTEXF_NONE, D3DTEXF_POINT, D3DTADDRESS_WRAP, 1);

// this needs a linear filter for waterwarp quality and should always clamp
samplerstate_t d3d_RTTSampler (D3DTEXF_LINEAR, D3DTEXF_NONE, D3DTEXF_LINEAR, D3DTADDRESS_CLAMP, 1);


/*
===============
D3D_Draw_InitLocal
===============
*/
void D3D_Draw_InitLocal (void)
{
	if (!r_waterwarpscale) r_waterwarpscale = Cvar_Get ("r_waterwarpscale", "100", 0);
	if (!r_waterwarpfactor) r_waterwarpfactor = Cvar_Get ("r_waterwarpfactor", "0.125", 0);
	if (!r_waterwarptime) r_waterwarptime = Cvar_Get ("r_waterwarptime", "10", 0);

	draw_chars = D3D9_FindImage ("pics/conchars.pcx", it_pic);
	Draw_NewCinematic ();
}


struct drawquaddef_t
{
	float xywh[4];
	DWORD color;
	float stlh[4];
};

#define MAX_DRAW_QUADS			4096
#define MAX_QUAD_VERTEXBUFFER	65536

drawquaddef_t d3d_DrawQuads[MAX_DRAW_QUADS];
int d3d_NumDrawQuads = 0;
IDirect3DTexture9 *d3d_DrawLastTexture = NULL;

IDirect3DTexture9 *d3d_BrightpassTexture = NULL;
IDirect3DTexture9 *d3d_WaterwarpTexture = NULL;

IDirect3DVertexShader9 *d3d_DrawVS = NULL;
IDirect3DPixelShader9 *d3d_DrawTexturedPS = NULL;
IDirect3DPixelShader9 *d3d_DrawColouredPS = NULL;
IDirect3DVertexDeclaration9 *d3d_DrawDecl = NULL;

IDirect3DVertexShader9 *d3d_PolyblendVS = NULL;
IDirect3DPixelShader9 *d3d_PolyblendPS = NULL;
IDirect3DVertexDeclaration9 *d3d_PolyblendDecl = NULL;
IDirect3DVertexBuffer9 *d3d_PolyblendBuffer = NULL;

IDirect3DVertexShader9 *d3d_BrightpassVS = NULL;
IDirect3DPixelShader9 *d3d_BrightpassPS = NULL;

IDirect3DVertexShader9 *d3d_WaterwarpVS = NULL;
IDirect3DPixelShader9 *d3d_WaterwarpPS = NULL;

IDirect3DVertexDeclaration9 *d3d_RTTDecl = NULL;
IDirect3DVertexBuffer9 *d3d_RTTBuffer = NULL;

IDirect3DVertexBuffer9 *d3d_DrawPerInstance = NULL;
IDirect3DVertexBuffer9 *d3d_DrawPerVertex = NULL;
IDirect3DIndexBuffer9 *d3d_DrawIndexes = NULL;

// if this != d3d_state.framecount we must begin a new 2D frame
static int r_2Dframe = -1;
static int r_firstdrawquad = 0;

void Draw_LoadObjects (void)
{
	D3DVERTEXELEMENT9 decllayout1[] =
	{
		VDECL (1, offsetof (drawquaddef_t, xywh), D3DDECLTYPE_FLOAT4, D3DDECLUSAGE_POSITION, 0),
		VDECL (1, offsetof (drawquaddef_t, color), D3DDECLTYPE_D3DCOLOR, D3DDECLUSAGE_COLOR, 0),
		VDECL (1, offsetof (drawquaddef_t, stlh), D3DDECLTYPE_FLOAT4, D3DDECLUSAGE_TEXCOORD, 0),
		VDECL (0, 0, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 1),
		D3DDECL_END ()
	};

	d3d_Device->CreateVertexDeclaration (decllayout1, &d3d_DrawDecl);

	d3d_DrawVS = D3D_CreateVertexShader ("hlsl/fxDraw.fx", "DrawVS");
	d3d_DrawTexturedPS = D3D_CreatePixelShader ("hlsl/fxDraw.fx", "DrawTexturedPS");
	d3d_DrawColouredPS = D3D_CreatePixelShader ("hlsl/fxDraw.fx", "DrawColouredPS");

	void *drawbufferdata = NULL;
	float drawpervert[] = {0, 0, 1, 0, 0, 1, 1, 1};
	unsigned short drawindexes[] = {0, 1, 2, 3};

	d3d_Device->CreateVertexBuffer (
		sizeof (drawpervert),
		D3DUSAGE_WRITEONLY,
		0,
		D3DPOOL_MANAGED,
		&d3d_DrawPerVertex,
		NULL
	);

	d3d_DrawPerVertex->Lock (0, 0, (void **) &drawbufferdata, 0);
	memcpy (drawbufferdata, drawpervert, sizeof (drawpervert));
	d3d_DrawPerVertex->Unlock ();
	d3d_DrawPerVertex->PreLoad ();

	d3d_Device->CreateIndexBuffer (
		sizeof (drawindexes),
		D3DUSAGE_WRITEONLY,
		D3DFMT_INDEX16,
		D3DPOOL_MANAGED,
		&d3d_DrawIndexes,
		NULL
	);

	d3d_DrawIndexes->Lock (0, 0, (void **) &drawbufferdata, 0);
	memcpy (drawbufferdata, drawindexes, sizeof (drawindexes));
	d3d_DrawIndexes->Unlock ();
	d3d_DrawIndexes->PreLoad ();

	D3DVERTEXELEMENT9 decllayout2[] =
	{
		VDECL (0, 0, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_POSITION, 0),
		D3DDECL_END ()
	};

	float polyblendverts[4][2] = {{-1, -1}, {1, -1}, {-1, 1}, {1, 1}};

	d3d_Device->CreateVertexBuffer (
		sizeof (polyblendverts),
		D3DUSAGE_WRITEONLY,
		0,
		D3DPOOL_MANAGED,
		&d3d_PolyblendBuffer,
		NULL
	);

	d3d_PolyblendBuffer->Lock (0, 0, (void **) &drawbufferdata, 0);
	memcpy (drawbufferdata, polyblendverts, sizeof (polyblendverts));
	d3d_PolyblendBuffer->Unlock ();
	d3d_PolyblendBuffer->PreLoad ();

	d3d_Device->CreateVertexDeclaration (decllayout2, &d3d_PolyblendDecl);

	d3d_PolyblendVS = D3D_CreateVertexShader ("hlsl/fxDraw.fx", "PolyblendVS");
	d3d_PolyblendPS = D3D_CreatePixelShader ("hlsl/fxDraw.fx", "PolyblendPS");

	D3DVERTEXELEMENT9 decllayout3[] =
	{
		VDECL (0, 0, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_POSITION, 0),
		VDECL (0, 8, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 0),
		D3DDECL_END ()
	};

	d3d_Device->CreateVertexDeclaration (decllayout3, &d3d_RTTDecl);

	d3d_BrightpassVS = D3D_CreateVertexShader ("hlsl/fxDraw.fx", "BrightpassVS");
	d3d_BrightpassPS = D3D_CreatePixelShader ("hlsl/fxDraw.fx", "BrightpassPS");

	d3d_WaterwarpVS = D3D_CreateVertexShader ("hlsl/fxDraw.fx", "WaterwarpVS");
	d3d_WaterwarpPS = D3D_CreatePixelShader ("hlsl/fxDraw.fx", "WaterwarpPS");
}


void Draw_Shutdown (void)
{
	SAFE_RELEASE (d3d_DrawPerVertex);
	SAFE_RELEASE (d3d_DrawIndexes);
	SAFE_RELEASE (d3d_DrawDecl);
	SAFE_RELEASE (d3d_DrawVS);
	SAFE_RELEASE (d3d_DrawTexturedPS);
	SAFE_RELEASE (d3d_DrawColouredPS);

	SAFE_RELEASE (d3d_PolyblendDecl);
	SAFE_RELEASE (d3d_PolyblendVS);
	SAFE_RELEASE (d3d_PolyblendPS);
	SAFE_RELEASE (d3d_PolyblendBuffer);

	SAFE_RELEASE (d3d_RTTDecl);

	SAFE_RELEASE (d3d_BrightpassVS);
	SAFE_RELEASE (d3d_BrightpassPS);

	SAFE_RELEASE (d3d_WaterwarpVS);
	SAFE_RELEASE (d3d_WaterwarpPS);
}


IDirect3DTexture9 *Draw_CreateRenderTarget (void)
{
	IDirect3DTexture9 *rt_Tex = NULL;

	d3d_Device->CreateTexture (
		vid.width,
		vid.height,
		1,
		D3DUSAGE_RENDERTARGET,
		D3DFMT_X8R8G8B8,
		D3DPOOL_DEFAULT,
		&rt_Tex,
		NULL
	);

	return rt_Tex;
}


void Draw_ResetDevice (void)
{
	void *drawbufferdata = NULL;

	d3d_BrightpassTexture = Draw_CreateRenderTarget ();
	d3d_WaterwarpTexture = Draw_CreateRenderTarget ();

	//float rttverts[4][4] = {{-1, -1, 0, 1}, {1, -1, 1, 1}, {-1, 1, 0, 0}, {1, 1, 1, 0}};
	float rttverts[4][4] = {{0, 0, 0, 1}, {(float)vid.width, 0, 1, 1}, {0, (float)vid.height, 0, 0}, {(float)vid.width, (float)vid.height, 1, 0}};

	d3d_Device->CreateVertexBuffer (
		sizeof (rttverts),
		D3DUSAGE_WRITEONLY,
		0,
		D3DPOOL_DEFAULT,
		&d3d_RTTBuffer,
		NULL
	);

	d3d_RTTBuffer->Lock (0, 0, (void **) &drawbufferdata, 0);
	memcpy (drawbufferdata, rttverts, sizeof (rttverts));
	d3d_RTTBuffer->Unlock ();

	d3d_Device->CreateVertexBuffer (
		MAX_QUAD_VERTEXBUFFER * sizeof (drawquaddef_t),
		D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC,
		0,
		D3DPOOL_DEFAULT,
		&d3d_DrawPerInstance,
		NULL
	);

	r_firstdrawquad = 0;
}


void Draw_LostDevice (void)
{
	SAFE_RELEASE (d3d_RTTBuffer);
	SAFE_RELEASE (d3d_BrightpassTexture);
	SAFE_RELEASE (d3d_WaterwarpTexture);
	SAFE_RELEASE (d3d_DrawPerInstance);
}


CD3DHandler Draw_Handler (
	Draw_LoadObjects,
	Draw_Shutdown,
	Draw_LostDevice,
	Draw_ResetDevice
);

void Draw_Flush (void)
{
	if (!d3d_NumDrawQuads) return;

	DWORD vblock = D3DLOCK_NOOVERWRITE | D3DLOCK_NOSYSLOCK;
	drawquaddef_t *quads = NULL;

	if (r_firstdrawquad + d3d_NumDrawQuads >= MAX_QUAD_VERTEXBUFFER)
	{
		vblock = D3DLOCK_DISCARD | D3DLOCK_NOSYSLOCK;
		r_firstdrawquad = 0;
	}

	d3d_DrawPerInstance->Lock (
		r_firstdrawquad * sizeof (drawquaddef_t),
		d3d_NumDrawQuads * sizeof (drawquaddef_t),
		(void **) &quads,
		vblock
	);

	memcpy (quads, d3d_DrawQuads, d3d_NumDrawQuads * sizeof (drawquaddef_t));
	d3d_DrawPerInstance->Unlock ();

	d3d_State->SetStreamSource (0, d3d_DrawPerVertex, 0, sizeof (float) * 2);
	d3d_State->SetStreamSource (1, d3d_DrawPerInstance, r_firstdrawquad * sizeof (drawquaddef_t), sizeof (drawquaddef_t));

	d3d_State->SetStreamSourceFreq (0, D3DSTREAMSOURCE_INDEXEDDATA | d3d_NumDrawQuads);
	d3d_State->SetStreamSourceFreq (1, D3DSTREAMSOURCE_INSTANCEDATA | 1);

	d3d_State->SetIndices (d3d_DrawIndexes);

	// other common state
	d3d_State->SetVertexDeclaration (d3d_DrawDecl);
	d3d_State->SetVertexShader (d3d_DrawVS);

	// and draw it
	d3d_Device->DrawIndexedPrimitive (D3DPT_TRIANGLESTRIP, 0, 0, 4, 0, 2);
	c_draw_calls++;

	// next batch
	r_firstdrawquad += d3d_NumDrawQuads;
	d3d_NumDrawQuads = 0;
}


void Draw_TexturedQuad (IDirect3DTexture9 *tex, float x, float y, float w, float h, DWORD color, float sl, float sh, float tl, float th)
{
	if (r_2Dframe != d3d_state.framecount)
	{
		// go into 2D mode when first encountered
		D3D_Set2D ();
		r_2Dframe = d3d_state.framecount;
	}

	if (d3d_DrawLastTexture != tex)
	{
		Draw_Flush ();

		if (tex)
		{
			if (tex == draw_chars->d3d_Texture)
				d3d_State->SetTexture (0, &d3d_CharSampler, tex);
			else d3d_State->SetTexture (0, &d3d_DrawSampler, tex);

			d3d_State->SetVertexShaderConstant4f (5, DEFAULT_HALFPIXEL);
			d3d_State->SetPixelShader (d3d_DrawTexturedPS);
		}
		else
		{
			d3d_State->SetVertexShaderConstant4f (5, NO_HALFPIXEL);
			d3d_State->SetPixelShader (d3d_DrawColouredPS);
		}

		d3d_DrawLastTexture = tex;
	}

	if (d3d_NumDrawQuads + 1 >= MAX_DRAW_QUADS)
	{
		Draw_Flush ();
		d3d_NumDrawQuads = 0;
	}

	Vector4Set (d3d_DrawQuads[d3d_NumDrawQuads].xywh, x, w, y, h);
	d3d_DrawQuads[d3d_NumDrawQuads].color = color;
	Vector4Set (d3d_DrawQuads[d3d_NumDrawQuads].stlh, sl, sh, tl, th);

	d3d_NumDrawQuads++;
}


void Draw_ColouredQuad (float x, float y, float w, float h, DWORD color)
{
	Draw_TexturedQuad (NULL, x, y, w, h, color, 0, 1, 0, 1);
}


/*
================
Draw_Char

Draws one 8*8 graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
*/
void Draw_CharScaled (int x, int y, int num, float scale)
{
	num &= 255;

	if ((num & 127) == 32) return;
	if (y <= -8) return;

	int row = num >> 4;
	int col = num & 15;

	float frow = row * 0.0625;
	float fcol = col * 0.0625;
	float size = 0.0625;

	float scaledSize = 8 * scale;

	Draw_TexturedQuad (draw_chars->d3d_Texture, x, y, scaledSize, scaledSize, 0xffffffff, fcol, size, frow, size);
}

void Draw_Char(int x, int y, int num)
{
	Draw_CharScaled (x, y, num, 1.0f);
}

/*
=============
Draw_FindPic
=============
*/
image_t	*Draw_FindPic (char *name)
{
	image_t *gl;
	char	fullname[MAX_QPATH];

	if (name[0] != '/' && name[0] != '\\')
	{
		Com_sprintf (fullname, sizeof (fullname), "pics/%s.pcx", name);
		gl = D3D9_FindImage(fullname, it_pic);
	}
	else gl = D3D9_FindImage(name + 1, it_pic);

	return gl;
}

/*
=============
Draw_GetPicSize
=============
*/
void Draw_GetPicSize (int *w, int *h, char *pic)
{
	image_t *gl;

	gl = Draw_FindPic (pic);

	if (!gl)
	{
		*w = *h = -1;
		return;
	}

	*w = gl->width;
	*h = gl->height;
}

/*
=============
Draw_Pic
=============
*/
void Draw_Pic (image_t *gl, int x, int y, int w, int h, float alpha)
{
	int ialpha = (int) (alpha * 255.0f);

	if (ialpha < 0) ialpha = 0;
	if (ialpha > 255) ialpha = 255;

	Draw_TexturedQuad (gl->d3d_Texture, x, y, w, h, D3DCOLOR_ARGB (ialpha, 255, 255, 255), gl->sl, gl->sh, gl->tl, gl->th);
}

/*
=============
Draw_StretchPic
=============
*/
void Draw_StretchPic (int x, int y, int w, int h, char *pic)
{
	image_t *gl = Draw_FindPic (pic);

	if (!gl)
	{
		Com_Printf ("Can't find pic: %s\n", pic);
		return;
	}

	Draw_Pic (gl, x, y, w, h, 1.0f);
}

/*
=============
Draw_PicScaled
=============
*/
void Draw_PicScaled(int x, int y, char *pic, float scale)
{
	image_t *gl = Draw_FindPic (pic);

	if (!gl)
	{
		Com_Printf ("Can't find pic: %s\n", pic);
		return;
	}

	Draw_Pic (gl, x, y, gl->width * scale, gl->height * scale, 1.0f);
}

/*
=============
Draw_Pic
=============
*/
void Draw_Pic(int x, int y, char *pic)
{
	Draw_PicScaled(x, y, pic, 1.0f);
}

/*
=============
Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void Draw_TileClear (int x, int y, int w, int h, char *pic)
{
	image_t	*image;

	image = Draw_FindPic (pic);

	if (!image)
	{
		Com_Printf ("Can't find pic: %s\n", pic);
		return;
	}

	Draw_TexturedQuad (image->d3d_Texture, x, y, w, h, 0xffffffff, x / 64.0f, w / 64.0f, y / 64.0f, h / 64.0f);
}


/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
*/
void Draw_Fill (int x, int y, int w, int h, int c)
{
	DWORD color = D3DCOLOR_XRGB (
		d_8to24table[c & 255].peRed,
		d_8to24table[c & 255].peGreen,
		d_8to24table[c & 255].peBlue
	);

	Draw_ColouredQuad (x, y, w, h, color);
}


//=============================================================================

/*
================
Draw_FadeScreen

================
*/
void Draw_FadeScreen (void)
{
	DWORD FadeColor = D3DCOLOR_ARGB (204, 0, 0, 0);

	Draw_ColouredQuad (0, 0, vid.width, vid.height, FadeColor);
}


void Draw_Polyblend (void)
{
	if (!gl_polyblend->value) return;
	if (!(v_blend[3] > 0)) return;

	d3d_PolyblendState->Apply ();

	// these can't be in a stateblock as they would mess with cached states
	d3d_State->SetStreamSource (0, d3d_PolyblendBuffer, 0, sizeof (float) * 2);
	d3d_State->SetVertexShader (d3d_PolyblendVS);
	d3d_State->SetPixelShader (d3d_PolyblendPS);
	d3d_State->SetVertexDeclaration (d3d_PolyblendDecl);
	d3d_State->SetPixelShaderConstant4fv (0, v_blend, 1);
	d3d_State->SetVertexShaderConstant4f (5, NO_HALFPIXEL);

	d3d_Device->DrawPrimitive (D3DPT_TRIANGLESTRIP, 0, 2);
}


//====================================================================


/*
=============
Draw_StretchRaw
=============
*/
extern PALETTEENTRY	r_rawpalette[256];
IDirect3DTexture9 *d3d_CinematicTexture = NULL;
int cached_width = -1;
int cached_height = -1;

void Draw_StretchRaw (int x, int y, int w, int h, int cols, int rows, byte *data)
{
	texdimension_t scaled = {cols, rows};

	D3D_GetScaledTextureDimensions (cols, rows, false, &scaled);

	// if the size changes release and recreate the texture
	if (cached_width != scaled.Width || cached_height != scaled.Height) SAFE_RELEASE (d3d_CinematicTexture);

	// create it before the skipframe check as we may have just released it above
	if (!d3d_CinematicTexture)
	{
		// create it if it doesn't exist
		if (FAILED (d3d_Device->CreateTexture (scaled.Width, scaled.Height, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_MANAGED, &d3d_CinematicTexture, NULL)))
		{
			// force a recreation next frame (it will anyway...)
			cached_width = -1;
			cached_height = -1;
			return;
		}

		cached_width = scaled.Width;
		cached_height = scaled.Height;
	}

	if (!D3D_FillTexture (d3d_CinematicTexture, data, cols, rows, false, r_rawpalette))
	{
		// force the texture to update next frame in case there was something wacky about it which caused this to fail
		cached_width = -1;
		cached_height = -1;
		return;
	}

	d3d_CinematicTexture->PreLoad ();
	Draw_TexturedQuad (d3d_CinematicTexture, x, y, w, h, 0xffffffff, 0, 1, 0, 1);
}


void Draw_NewCinematic (void)
{
	// when a new cinematic begins we force the texture to update
	SAFE_RELEASE (d3d_CinematicTexture);
	cached_width = -1;
	cached_height = -1;
}


void D3D_Set2D (void)
{
	// blend/z/vp/etc
	d3d_2DGUIState->Apply ();

	D3DQMATRIX orthomatrix;

	orthomatrix.Ortho (0, vid.width, vid.height, 0, 0, 1);

	d3d_State->SetViewport (0, 0, vid.width, vid.height, 0, 1);
	d3d_State->SetVertexShaderConstant4fv (0, orthomatrix.m16, 4);

	// re-init the quad batches
	d3d_NumDrawQuads = 0;

	// this is a hack - just init to something invalid, as NULL is valid as a texture
	d3d_DrawLastTexture = (IDirect3DTexture9 *) -1;
}


void D3D_End2D (void)
{
	// commit any outstanding drawing (cross-check this with the weird setup in SCR_UpdateScreen)
	Draw_Flush ();

	// go back to uninstanced
	d3d_State->SetStreamSourceFreq (0, 1);
	d3d_State->SetStreamSourceFreq (1, 1);

	// this can happen multiple times per frame so set this to invalid to force an update
	r_2Dframe = -1;
}


CRenderToTexture::CRenderToTexture (void)
{
	this->BackBuffer = NULL;
	this->RTTSurface = NULL;
	this->RTTTexture = NULL;
}


void CRenderToTexture::Begin (IDirect3DTexture9 *rttTexture)
{
	if (!rttTexture) return;

	if (FAILED (d3d_Device->GetRenderTarget (0, &this->BackBuffer))) return;

	if (FAILED (rttTexture->GetSurfaceLevel (0, &this->RTTSurface)))
	{
		this->BackBuffer->Release ();
		this->BackBuffer = NULL;
		return;
	}

	if (FAILED (d3d_Device->SetRenderTarget (0, this->RTTSurface)))
	{
		this->BackBuffer->Release ();
		this->BackBuffer = NULL;

		this->RTTSurface->Release ();
		this->RTTSurface = NULL;
	}

	this->RTTTexture = rttTexture;
	this->RTTTexture->AddRef ();
}


void CRenderToTexture::End (int pass)
{
	if (!this->BackBuffer) return;
	if (!this->RTTSurface) return;
	if (!this->RTTTexture) return;

	d3d_Device->SetRenderTarget (0, this->BackBuffer);

	this->BackBuffer->Release ();
	this->BackBuffer = NULL;

	this->RTTSurface->Release ();
	this->RTTSurface = NULL;

	// parameters
	float vsparams[4];
	float psparams[4];

	// draw it
	switch (pass)
	{
	case RTTPASS_WATERWARP:
		d3d_State->SetVertexShader (d3d_WaterwarpVS);
		d3d_State->SetPixelShader (d3d_WaterwarpPS);

		if (gl_polyblend->value)
			d3d_State->SetPixelShaderConstant4fv (0, v_blend, 1);
		else d3d_State->SetPixelShaderConstant4f (0, 0, 0, 0, 0);

		if (r_waterwarpscale->value > 1)
		{
			float sx = r_waterwarpscale->value;
			float sy = r_waterwarpscale->value;

			// rescale for aspect
			if (vid.width > vid.height)
				sy = (sy / (float) vid.width) * (float) vid.height;
			else if (vid.width < vid.height)
				sy = (sy / (float) vid.height) * (float) vid.width;

			vsparams[0] = sx;
			vsparams[1] = sy;
			psparams[0] = 1.0f / sx;
			psparams[1] = 1.0f / sy;
		}
		else
		{
			vsparams[0] = vsparams[1] = 1.0f;
			psparams[0] = psparams[1] = 1.0f;
		}

		vsparams[2] = psparams[2] = r_waterwarpfactor->value;
		vsparams[3] = psparams[3] = r_waterwarptime->value * r_newrefdef.time;

		d3d_State->SetVertexShaderConstant4fv (4, vsparams, 1);
		d3d_State->SetVertexShaderConstant4f (5, NO_HALFPIXEL);
		d3d_State->SetPixelShaderConstant4fv (2, psparams, 1);
		break;

	case RTTPASS_BRIGHTPASS:
		d3d_State->SetVertexShader (d3d_BrightpassVS);
		d3d_State->SetPixelShader (d3d_BrightpassPS);
		d3d_State->SetPixelShaderConstant1f (1, vid_gamma->value);
		d3d_State->SetPixelShaderConstant1f (7, vid_contrast->value);
		d3d_State->SetVertexShaderConstant4f (5, DEFAULT_HALFPIXEL);
		break;

	default:
		// should never happen; can't just return yet as this class holds a reference to the texture used to create the state
		break;
	}

	d3d_RTTState->Apply ();

	d3d_State->SetVertexShaderConstant2f (6, 2.0f / (float)vid.width, 2.0f / (float)vid.height);
	d3d_State->SetPixelShaderConstant1f (3, r_newrefdef.time);

	d3d_State->SetVertexDeclaration (d3d_RTTDecl);
	d3d_State->SetStreamSource (0, d3d_RTTBuffer, 0, sizeof (float) * 4);
	d3d_State->SetTexture (0, &d3d_RTTSampler, this->RTTTexture);

	d3d_Device->DrawPrimitive (D3DPT_TRIANGLESTRIP, 0, 2);

	this->RTTTexture->Release ();
	this->RTTTexture = NULL;

	// disable view blending as we'll merge it into the rtt pass (this pass should be run before the polyblend)
	v_blend[3] = -1;
}



