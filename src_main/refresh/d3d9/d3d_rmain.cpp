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
// r_main.c
#include "d3d_local.h"

// eww
void VID_MenuInit (void);

void R_Clear (void);
void Draw_Polyblend (void);

model_t		*r_worldmodel;

float		gldepthmin, gldepthmax;

glconfig_t gl_config;
d3dstate_t  d3d_state;

image_t		*r_greytexture;		// use for gl_lightmap
image_t		*r_whitetexture;	// use for r_fullbright
image_t		*r_blacktexture;	// not sure where we'll use this yet, but it's there anyway...!
image_t		*r_notexture;		// use for bad textures

entity_t	*currententity;

int			r_visframecount;	// bumped when going to a new PVS
int			r_framecount;		// used for dlight push checking

int			c_brush_polys, c_alias_polys, c_draw_calls;

float		v_blend[4];			// final blending color

float	r_world_matrix[16];
float	r_base_world_matrix[16];

// screen size info
refdef_t	r_newrefdef;

int		r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;

cvar_t	*r_ambient;
cvar_t	*r_norefresh;
cvar_t	*r_drawentities;
cvar_t	*r_drawworld;
cvar_t	*r_speeds;
cvar_t	*r_fullbright;
cvar_t	*r_novis;
cvar_t	*r_nocull;
cvar_t	*r_lerpmodels;
cvar_t	*r_lefthand;

cvar_t	*r_lightlevel;	// FIXME: This is a HACK to get the client's light level

cvar_t	*gl_lightmap;
cvar_t	*gl_shadows;
cvar_t	*gl_mode;
cvar_t	*gl_dynamic;
cvar_t  *gl_monolightmap;
cvar_t	*gl_nobind;
cvar_t	*gl_round_down;
cvar_t	*gl_picmip;
cvar_t	*gl_skymip;
cvar_t	*gl_showtris;
cvar_t	*gl_finish;
cvar_t	*gl_clear;
cvar_t	*gl_cull;
cvar_t	*gl_polyblend;
cvar_t	*gl_texturemode;
cvar_t	*gl_textureanisotropy;
cvar_t	*gl_lockpvs;
cvar_t	*gl_lockfrustum;
cvar_t	*gl_testnullmodels;

cvar_t	*vid_fullscreen;
cvar_t	*vid_gamma;
cvar_t	*vid_contrast;

CRenderToTexture rtt_Waterwarp;


/*
=================
R_CullBox

Returns true if the box is completely outside the frustom
=================
*/
qboolean R_CullBox (vec3_t mins, vec3_t maxs)
{
	int		i;

	if (r_nocull->value)
		return false;

	for (i = 0; i < 4; i++)
		if (BoxOnPlaneSide (mins, maxs, &d3d_state.frustum[i]) == 2)
			return true;

	return false;
}


void R_DrawAliasModel (entity_t *e);
void R_DrawBrushModel (entity_t *e);

void R_DrawSingleEntity (entity_t *ent)
{
	if (ent->flags & RF_BEAM)
	{
		R_DrawBeam (ent);
	}
	else
	{
		if (!ent->model || gl_testnullmodels->value)
		{
			R_DrawNullModel (ent);
			return;
		}

		switch (ent->model->type)
		{
		case mod_alias:
			R_DrawAliasModel (ent);
			break;

		case mod_brush:
			R_DrawBrushModel (ent);
			break;

		case mod_sprite:
			R_DrawSpriteModel (ent);
			break;

		default:
			Com_Error (ERR_DROP, "Bad modeltype");
			break;
		}
	}
}


void R_DrawEntitiesOnList (void)
{
	if (!r_drawentities->value)
		return;

	qboolean alphapass = false;

	for (int i = 0; i < r_newrefdef.num_entities; i++)
	{
		currententity = &r_newrefdef.entities[i];

		if (r_newrefdef.entities[i].flags & RF_TRANSLUCENT)
		{
			alphapass = true;
			continue;
		}

		R_DrawSingleEntity (&r_newrefdef.entities[i]);
	}

	if (!alphapass) return;

	d3d_EnableAlphaObjects->Apply ();

	for (int i = 0; i < r_newrefdef.num_entities; i++)
	{
		currententity = &r_newrefdef.entities[i];

		if (!(r_newrefdef.entities[i].flags & RF_TRANSLUCENT))
			continue;

		R_DrawSingleEntity (&r_newrefdef.entities[i]);
	}

	d3d_DisableAlphaObjects->Apply ();
}


/*
===============
R_SetupFrame
===============
*/
void R_SetupFrame (void)
{
	mleaf_t	*leaf;

	r_framecount++;

	// build the transformation matrix for the given view angles
	VectorCopy (r_newrefdef.vieworg, d3d_state.r_origin);

	AngleVectors (r_newrefdef.viewangles, d3d_state.vpn, d3d_state.vright, d3d_state.vup);

	// current viewcluster
	if (!(r_newrefdef.rdflags & RDF_NOWORLDMODEL))
	{
		r_oldviewcluster = r_viewcluster;
		r_oldviewcluster2 = r_viewcluster2;
		leaf = Mod_PointInLeaf (d3d_state.r_origin, r_worldmodel);
		r_viewcluster = r_viewcluster2 = leaf->cluster;

		// check above and below so crossing solid water doesn't draw wrong
		if (!leaf->contents)
		{
			// look down a bit
			vec3_t	temp;

			VectorCopy (d3d_state.r_origin, temp);
			temp[2] -= 16;
			leaf = Mod_PointInLeaf (temp, r_worldmodel);

			if (!(leaf->contents & CONTENTS_SOLID) && (leaf->cluster != r_viewcluster2))
				r_viewcluster2 = leaf->cluster;
		}
		else
		{
			// look up a bit
			vec3_t	temp;

			VectorCopy (d3d_state.r_origin, temp);
			temp[2] += 16;
			leaf = Mod_PointInLeaf (temp, r_worldmodel);

			if (!(leaf->contents & CONTENTS_SOLID) && (leaf->cluster != r_viewcluster2))
				r_viewcluster2 = leaf->cluster;
		}
	}

	Vector4Copy (v_blend, r_newrefdef.blend);

	// allow gl_polyblend to scale the blend factor 
	v_blend[3] *= gl_polyblend->value;

	c_brush_polys = 0;
	c_alias_polys = 0;
	c_draw_calls = 0;
}


/*
=============
R_SetupGL
=============
*/
void R_SetupGL (void)
{
	d3d_State->SetViewport (r_newrefdef.x, r_newrefdef.y, r_newrefdef.width, r_newrefdef.height, 0.0f, 1.0f);

	// evaluate the new MVP matrix from the viewpoint
	// these could be calculated in reverse order and then the matrix inverted
	d3d_state.WorldMatrix.LoadIdentity ();
	d3d_state.WorldMatrix.Perspective (r_newrefdef.fov_x, r_newrefdef.fov_y, 4, 4096);

	// put z going up
	d3d_state.WorldMatrix.Mult (0, 0, -1, 0, -1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1);

	d3d_state.WorldMatrix.Rotate (-r_newrefdef.viewangles[2], 1, 0, 0);
	d3d_state.WorldMatrix.Rotate (-r_newrefdef.viewangles[0], 0, 1, 0);
	d3d_state.WorldMatrix.Rotate (-r_newrefdef.viewangles[1], 0, 0, 1);

	d3d_state.WorldMatrix.Translate (-r_newrefdef.vieworg[0], -r_newrefdef.vieworg[1], -r_newrefdef.vieworg[2]);

	// should the frustum go in gl_state too?
	if (!gl_lockfrustum->value || r_framecount < 5)
		d3d_state.WorldMatrix.ExtractFrustum (d3d_state.frustum);

	// shared crap for all shader types
	d3d_State->SetVertexShaderConstant4fv (0, d3d_state.Viewpoint, 8);

	// state for the main scene
	d3d_MainSceneState->Apply ();

	// clear out the portion of the screen that the NOWORLDMODEL defines
	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
	{
		D3DRECT ClearRect = {
			r_newrefdef.x,
			r_newrefdef.y,
			r_newrefdef.x + r_newrefdef.width,
			r_newrefdef.y + r_newrefdef.height
		};

		d3d_Device->Clear (1, &ClearRect, D3DCLEAR_ZBUFFER | D3DCLEAR_TARGET, D3DCOLOR_XRGB (48, 48, 48), 1.0f, 1);
	}

	// set up some other params for reuse
	d3d_state.surfscroll = -64 * ((r_newrefdef.time / 40.0f) - (int) (r_newrefdef.time / 40.0f));
	while (d3d_state.surfscroll >= 0.0f) d3d_state.surfscroll = -64.0f;

	if (r_currentspeed->value > 0)
	{
		d3d_state.warpscroll = -64 * ((r_newrefdef.time * r_currentspeed->value) - (int) (r_newrefdef.time * r_currentspeed->value));
		while (d3d_state.warpscroll >= 0.0f) d3d_state.warpscroll = -64.0f;
	}
	else d3d_state.warpscroll = 0;
}


/*
=============
R_Clear
=============
*/
void R_Clear (void)
{
	if ((gl_clear->value || gl_lockfrustum->value || gl_lockpvs->value) && !(r_newrefdef.rdflags & RDF_NOWORLDMODEL))
		d3d_Device->Clear (0, NULL, D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL | D3DCLEAR_TARGET, D3DCOLOR_XRGB (128, 64, 0), 1.0f, 1);
	else d3d_Device->Clear (0, NULL, D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0x0, 1.0f, 1);
}


/*
================
R_RenderView

r_newrefdef must be set before the first call
================
*/
void R_RenderView (refdef_t *fd)
{
	if (r_norefresh->value)
		return;

	r_newrefdef = *fd;

	if (!r_worldmodel && !(r_newrefdef.rdflags & RDF_NOWORLDMODEL))
		Com_Error (ERR_DROP, "R_RenderView: NULL worldmodel");

	if (r_speeds->value)
	{
		c_brush_polys = 0;
		c_alias_polys = 0;
		c_draw_calls = 0;
	}

	R_SetupFrame ();

	R_SetupGL ();

	// conditionally begin the waterwarp pass
	if (r_newrefdef.rdflags & RDF_UNDERWATER)
		rtt_Waterwarp.Begin (d3d_WaterwarpTexture);

	// r_clear needs to be done here in case we switch the rendertarget
	R_Clear ();

	R_MarkLeaves ();	// done here so we know if we're in water

	R_DrawWorld ();

	R_DrawEntitiesOnList ();

	// the common case has particles drawing over alpha surfaces so we adjust for that
	R_DrawAlphaSurfaces ();

	R_DrawParticles ();

	// end the waterwarp pass
	rtt_Waterwarp.End (RTTPASS_WATERWARP);

	// polyblend should use the same viewport as the main view but is really 2D GUI-style work
	Draw_Polyblend ();

	if (r_speeds->value)
	{
		Com_Printf (
			"%4i wpoly  -  %4i epoly  -  %4i draw  -  %i tex  -  %i lmap\n",
			c_brush_polys,
			c_alias_polys,
			c_draw_calls,
			c_visible_textures,
			c_visible_lightmaps
		);
	}
}


/*
====================
R_SetLightLevel

====================
*/
void R_SetLightLevel (void)
{
	vec3_t shadelight;
	float lightspot[3];

	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
		return;

	// save off light value for server to look at (BIG HACK!)
	R_LightPoint (r_newrefdef.vieworg, shadelight, lightspot);

	// pick the greatest component, which should be the same
	// as the mono value returned by software
	if (shadelight[0] > shadelight[1])
	{
		if (shadelight[0] > shadelight[2])
			r_lightlevel->value = 150 * shadelight[0];
		else r_lightlevel->value = 150 * shadelight[2];
	}
	else
	{
		if (shadelight[1] > shadelight[2])
			r_lightlevel->value = 150 * shadelight[1];
		else r_lightlevel->value = 150 * shadelight[2];
	}
}

/*
@@@@@@@@@@@@@@@@@@@@@
R_RenderFrame
@@@@@@@@@@@@@@@@@@@@@
*/
void R_RenderFrame (refdef_t *fd)
{
	// if any 2D drawing had happened, end it now
	D3D_End2D ();

	R_RenderView (fd);
	R_SetLightLevel ();
}


void R_Register (void)
{
	r_currentspeed = Cvar_Get ("r_currentspeed", "0.5", 0);
	r_lefthand = Cvar_Get ("hand", "0", CVAR_USERINFO | CVAR_ARCHIVE);
	r_ambient = Cvar_Get ("r_ambient", "0", 0);
	r_norefresh = Cvar_Get ("r_norefresh", "0", 0);
	r_fullbright = Cvar_Get ("r_fullbright", "0", 0);
	r_drawentities = Cvar_Get ("r_drawentities", "1", 0);
	r_drawworld = Cvar_Get ("r_drawworld", "1", 0);
	r_novis = Cvar_Get ("r_novis", "0", 0);
	r_nocull = Cvar_Get ("r_nocull", "0", 0);
	r_lerpmodels = Cvar_Get ("r_lerpmodels", "1", 0);
	r_speeds = Cvar_Get ("r_speeds", "0", 0);

	r_lightlevel = Cvar_Get ("r_lightlevel", "0", 0);

	gl_mode = Cvar_Get ("gl_mode", "10", CVAR_ARCHIVE);
	gl_lightmap = Cvar_Get ("gl_lightmap", "0", 0);
	gl_shadows = Cvar_Get ("gl_shadows", "0", CVAR_ARCHIVE);
	gl_dynamic = Cvar_Get ("gl_dynamic", "1", 0);
	gl_nobind = Cvar_Get ("gl_nobind", "0", 0);
	gl_round_down = Cvar_Get ("gl_round_down", "0", 0);
	gl_picmip = Cvar_Get ("gl_picmip", "0", 0);
	gl_skymip = Cvar_Get ("gl_skymip", "0", 0);
	gl_showtris = Cvar_Get ("gl_showtris", "0", 0);
	gl_finish = Cvar_Get ("gl_finish", "0", CVAR_ARCHIVE);
	gl_clear = Cvar_Get ("gl_clear", "0", 0);
	gl_cull = Cvar_Get ("gl_cull", "1", 0);
	gl_polyblend = Cvar_Get ("gl_polyblend", "1", 0);
	gl_monolightmap = Cvar_Get ("gl_monolightmap", "0", CVAR_ARCHIVE);
	gl_texturemode = Cvar_Get ("gl_texturemode", "GL_NEAREST_MIPMAP_NEAREST", CVAR_ARCHIVE);
	gl_textureanisotropy = Cvar_Get ("gl_textureanisotropy", "1", CVAR_ARCHIVE);
	gl_lockpvs = Cvar_Get ("gl_lockpvs", "0", 0);
	gl_lockfrustum = Cvar_Get ("gl_lockfrustum", "0", 0);

	gl_testnullmodels = Cvar_Get ("gl_testnullmodels", "0", 0);

	vid_fullscreen = Cvar_Get ("vid_fullscreen", "0", CVAR_ARCHIVE);
	vid_gamma = Cvar_Get ("vid_gamma", "1.0", CVAR_ARCHIVE);
	vid_contrast = Cvar_Get("vid_contrast", "1.0", CVAR_ARCHIVE);

	Cmd_AddCommand ("imagelist", GL_ImageList_f);
	Cmd_AddCommand ("screenshot", GL_ScreenShot_f);
	Cmd_AddCommand ("modellist", Mod_Modellist_f);
}

// the following is only used in the next to functions,
// no need to put it in a header
enum
{
	rserr_ok,

	rserr_invalid_fullscreen,
	rserr_invalid_mode,

	rserr_unknown
};

static int SetMode_impl(int *pwidth, int *pheight, int mode, int fullscreen)
{
	Com_Printf("setting mode %d:", mode);

	// mode -1 is not in the vid mode table - so we keep the values in pwidth
	// and pheight and don't even try to look up the mode info
	if ((mode != -1) && !VID_GetModeInfo(pwidth, pheight, mode))
	{
		Com_Printf(" invalid mode\n");
		return rserr_invalid_mode;
	}

	Com_Printf(" %d x %d", *pwidth, *pheight);
	if (fullscreen)
	{
		Com_Printf(" (fullscreen)\n");
	}
	else
	{
		Com_Printf(" (windowed)\n");
	}

	if (!GLimp_SetMode(fullscreen, pwidth, pheight))
	{
		return rserr_invalid_mode;
	}

	return rserr_ok;
}

static qboolean R_SetMode(void)
{
	int err;
	int fullscreen;

	fullscreen = (int)vid_fullscreen->value;

	vid_fullscreen->modified = false;
	gl_mode->modified = false;

	// a bit hackish approach to enable custom resolutions:
	// Glimp_SetMode needs these values set for mode -1
	vid.width = 1024;
	vid.height = 768;

	if ((err = SetMode_impl(&vid.width, &vid.height, gl_mode->value, fullscreen)) == rserr_ok)
	{
		if (gl_mode->value == -1)
		{
			d3d_state.prev_mode = 10; // safe default for custom mode
		}
		else
		{
			d3d_state.prev_mode = gl_mode->value;
		}
	}
	else
	{
		if (err == rserr_invalid_fullscreen)
		{
			Cvar_SetValue("vid_fullscreen", 0);
			vid_fullscreen->modified = false;
			Com_Printf("ref_d3d9::R_SetMode() - fullscreen unavailable in this mode\n");

			if ((err = SetMode_impl(&vid.width, &vid.height, gl_mode->value, 0)) == rserr_ok)
			{
				return true;
			}
		}
		else if (err == rserr_invalid_mode)
		{
			Com_Printf("ref_d3d9::R_SetMode() - invalid mode\n");

			if (gl_mode->value == d3d_state.prev_mode)
			{
				// trying again would result in a crash anyway, give up already
				// (this would happen if your initing fails at all and your resolution already was 640x480)
				return false;
			}

			Cvar_SetValue("gl_mode", d3d_state.prev_mode);
			gl_mode->modified = false;
		}

		// try setting it back to something safe
		if ((err = SetMode_impl(&vid.width, &vid.height, d3d_state.prev_mode, 0)) != rserr_ok)
		{
			Com_Printf("ref_d3d9::R_SetMode() - could not revert to safe mode\n");
			return false;
		}
	}

	return true;
}

/*
===============
R_Init
===============
*/
int R_Init (void)
{
	Com_Printf ("ref_d3d version: " REF_VERSION "\n");

	Draw_GetPalette ();

	R_Register ();

	// initialize OS-specific parts
	if (!VID_Init_D3D9()) {
		return -1;
	}

	// set our "safe" modes
	d3d_state.prev_mode = 10;
	
	// create the window and set up the context
	if (!R_SetMode())
	{
		Com_Printf ("ref_gl::R_Init() - could not R_SetMode()\n");
		return -1;
	}

	VID_MenuInit ();

	Mod_FreeAll();
	GL_ShutdownImages();

	VID_ResetDevice();

	D3D9_SetDefaultState ();
	GL_InitImages ();
	Mod_Init ();
	R_InitTextures ();
	D3D_Draw_InitLocal ();

	return 1;
}

/*
===============
R_Shutdown
===============
*/
void R_Shutdown (void)
{
	Mod_FreeAll ();
	GL_ShutdownImages ();
	GLimp_Shutdown ();
}

/*
@@@@@@@@@@@@@@@@@@@@@
R_BeginFrame
@@@@@@@@@@@@@@@@@@@@@
*/
void R_BeginFrame (float camera_separation)
{
	// signals that we're going to a new frame so that the 2D stuff can know that it needs to switch when first encountered
	d3d_state.framecount++;
	d3d_state.camera_separation = camera_separation;

	// check modes and states for modification
	if (gl_mode->modified || vid_fullscreen->modified)
	{
		VID_ResetDevice ();

		gl_mode->modified = false;
		vid_fullscreen->modified = false;
	}

	if (gl_texturemode->modified || gl_textureanisotropy->modified)
	{
		GL_TextureMode (gl_texturemode->string, (int) gl_textureanisotropy->value);

		gl_texturemode->modified = false;
		gl_textureanisotropy->modified = false;
	}

	GLimp_BeginFrame (camera_separation);
}


/*
=============
R_SetPalette
=============
*/
PALETTEENTRY r_rawpalette[256];

void R_SetPalette (const unsigned char *palette)
{
	// d_8to24table is 4 component but a palette passed in will be 3 component
	if (!palette)
	{
		memcpy (r_rawpalette, d_8to24table, sizeof (d_8to24table));
		return;
	}

	for (int i = 0; i < 256; i++, palette += 3)
	{
		r_rawpalette[i].peRed = palette[0];
		r_rawpalette[i].peGreen = palette[1];
		r_rawpalette[i].peBlue = palette[2];
		r_rawpalette[i].peFlags = 0xff;
	}
}

/*
============
D3D9_GFX_CoreInit

Direct3D 9 refresh core entry point
============
*/
extern "C" void D3D9_GFX_CoreInit (void)
{
	RE_BeginFrame = GLimp_BeginFrame;
	RE_RenderFrame = R_RenderFrame;
	RE_EndFrame = GLimp_EndFrame;

	RE_SetPalette = R_SetPalette;

	RE_Init = R_Init;
	RE_Shutdown = R_Shutdown;

	RE_Draw_StretchRaw = Draw_StretchRaw;
	RE_Draw_FadeScreen = Draw_FadeScreen;
	RE_Draw_Fill = Draw_Fill;
	RE_Draw_TileClear = Draw_TileClear;
	RE_Draw_CharScaled = Draw_CharScaled;
	RE_Draw_Char = Draw_Char;
	RE_Draw_StretchPic = Draw_StretchPic;
	RE_Draw_PicScaled = Draw_PicScaled;
	RE_Draw_Pic = Draw_Pic;
	RE_Draw_GetPicSize = Draw_GetPicSize;

	RE_BeginRegistration = D3D9_BeginRegistration;
	RE_RegisterModel = D3D9_RegisterModel;
	RE_RegisterSkin = D3D9_RegisterSkin;
	RE_Draw_RegisterPic = Draw_FindPic;
	RE_SetSky = D3D9_SetSky;
	RE_EndRegistration = D3D9_EndRegistration;
}

