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
// r_main.c
#include "gl_local.h"

void RSky_BeginFrame (void);
void RWarp_BeginFrame (void);
void RSurf_BeginFrame (void);

GLuint gl_sharedubo = 0;

void GL_Clear (GLbitfield mask);
void R_Clear (void);

viddef_t	vid;

model_t		*r_worldmodel;

float		gldepthmin, gldepthmax;

glconfig_t	gl_config;
glstate_t	gl_state;

image_t		*r_notexture;		// use for bad textures

cplane_t	frustum[4];

int			r_visframecount;	// bumped when going to a new PVS
int			r_framecount;		// used for dlight push checking

int			c_brush_polys, c_alias_polys;

float		v_blend[4];			// final blending color

//
// view origin
//
vec3_t	vup;
vec3_t	vpn;
vec3_t	vright;
vec3_t	r_origin;

glmatrix	r_drawmatrix;
glmatrix	r_worldmatrix;
glmatrix	r_projectionmatrix;
glmatrix	r_mvpmatrix;

//
// screen size info
//
refdef_t	r_newrefdef;

int		r_viewcluster, r_viewcluster2, r_oldviewcluster, r_oldviewcluster2;

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
cvar_t  *gl_customwidth;
cvar_t  *gl_customheight;
cvar_t	*gl_customPixelAspect;
cvar_t	*gl_dynamic;
cvar_t  *gl_monolightmap;
cvar_t	*gl_showtris;
cvar_t	*gl_finish;
cvar_t	*gl_clear;
cvar_t	*gl_cull;
cvar_t	*gl_swapinterval;
cvar_t	*gl_texturemode;
cvar_t	*gl_textureanisotropy;
cvar_t	*gl_lockpvs;
cvar_t	*gl_lockfrustum;

cvar_t	*vid_fullscreen;
cvar_t	*vid_gamma;
cvar_t	*vid_contrast;
cvar_t	*r_lightscale;


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
		if (BOX_ON_PLANE_SIDE (mins, maxs, &frustum[i]) == 2)
			return true;

	return false;
}


//==================================================================================

entity_t *aliasents[MAX_ENTITIES];
int numaliasents;

void R_DrawSingleEntity (entity_t *ent)
{
	if (ent->flags & RF_BEAM)
	{
		R_DrawBeam (ent);
	}
	else
	{
		if (!ent->model)
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
			VID_Error (ERR_DROP, "Bad modeltype");
			break;
		}
	}
}


/*
=============
R_DrawEntitiesOnList
=============
*/
static void R_DrawEntitiesOnList (void)
{
	int i;
	entity_t *ent;

	if (!r_drawentities->value)
		return;

	// draw non-transparent first
	for (i = 0; i < r_newrefdef.num_entities; i++)
	{
		ent = &r_newrefdef.entities[i];

		if (ent->flags & RF_TRANSLUCENT)
			continue;	// solid

		R_DrawSingleEntity (ent);
	}

	for (i = 0; i < r_newrefdef.num_entities; i++)
	{
		ent = &r_newrefdef.entities[i];

		if (!(ent->flags & RF_TRANSLUCENT))
			continue;	// solid

		R_DrawSingleEntity (ent);
	}
}


//=======================================================================

int SignbitsForPlane (cplane_t *out)
{
	int	bits, j;

	// for fast box on planeside test
	bits = 0;

	for (j = 0; j < 3; j++)
	{
		if (out->normal[j] < 0)
			bits |= 1 << j;
	}

	return bits;
}


/*
===============
R_SetupFrame
===============
*/
static void R_SetupFrame (void)
{
	int i;
	mleaf_t	*leaf;

	r_framecount++;

	// build the transformation matrix for the given view angles
	VectorCopy (r_newrefdef.vieworg, r_origin);
	AngleVectors (r_newrefdef.viewangles, vpn, vright, vup);

	// current viewcluster
	if (!(r_newrefdef.rdflags & RDF_NOWORLDMODEL))
	{
		r_oldviewcluster = r_viewcluster;
		r_oldviewcluster2 = r_viewcluster2;
		leaf = Mod_PointInLeaf (r_origin, r_worldmodel);
		r_viewcluster = r_viewcluster2 = leaf->cluster;

		// check above and below so crossing solid water doesn't draw wrong
		if (!leaf->contents)
		{
			// look down a bit
			vec3_t	temp;

			VectorCopy (r_origin, temp);
			temp[2] -= 16;
			leaf = Mod_PointInLeaf (temp, r_worldmodel);

			if (!(leaf->contents & CONTENTS_SOLID) && (leaf->cluster != r_viewcluster2))
				r_viewcluster2 = leaf->cluster;
		}
		else
		{
			// look up a bit
			vec3_t	temp;

			VectorCopy (r_origin, temp);
			temp[2] += 16;
			leaf = Mod_PointInLeaf (temp, r_worldmodel);

			if (!(leaf->contents & CONTENTS_SOLID) && (leaf->cluster != r_viewcluster2))
				r_viewcluster2 = leaf->cluster;
		}
	}

	for (i = 0; i < 4; i++)
		v_blend[i] = r_newrefdef.blend[i];

	c_brush_polys = 0;
	c_alias_polys = 0;

	// clear out the portion of the screen that the NOWORLDMODEL defines
	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
	{
		GL_Enable (gl_state.statebits | SCISSOR_BIT);
		glClearColor (0.3, 0.3, 0.3, 1);
		glScissor (r_newrefdef.x, vid.height - r_newrefdef.height - r_newrefdef.y, r_newrefdef.width, r_newrefdef.height);
		GL_Clear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glClearColor (1, 0, 0.5, 0.5);
		GL_Enable (gl_state.statebits & ~SCISSOR_BIT);
	}
}


/*
=============
R_SetupGL
=============
*/
void R_SetupGL (void)
{
	int i;
	sharedubo_t sharedubo;

	// keep this consistent with the scissor region for player models
	glViewport (r_newrefdef.x, vid.height - r_newrefdef.height - r_newrefdef.y, r_newrefdef.width, r_newrefdef.height);

	// set up projection matrix
	GL_LoadIdentity (&r_projectionmatrix);
	GL_PerspectiveMatrix (&r_projectionmatrix, r_newrefdef.fov_y, (float) r_newrefdef.width / (float) r_newrefdef.height, 4, 4096);

	// set up modelview matrix
	GL_LoadIdentity (&r_worldmatrix);

	GL_RotateMatrix (&r_worldmatrix, -90, 1, 0, 0);
	GL_RotateMatrix (&r_worldmatrix, 90, 0, 0, 1);

	GL_RotateMatrix (&r_worldmatrix, -r_newrefdef.viewangles[2], 1, 0, 0);
	GL_RotateMatrix (&r_worldmatrix, -r_newrefdef.viewangles[0], 0, 1, 0);
	GL_RotateMatrix (&r_worldmatrix, -r_newrefdef.viewangles[1], 0, 0, 1);

	GL_TranslateMatrix (&r_worldmatrix, -r_newrefdef.vieworg[0], -r_newrefdef.vieworg[1], -r_newrefdef.vieworg[2]);

	// compute the global mvp as it's used for multiple objects
	GL_LoadMatrix (&r_mvpmatrix, &r_worldmatrix);
	GL_MultMatrix (&r_mvpmatrix, &r_mvpmatrix, &r_projectionmatrix);

	// glClear is affected by the current depth write mask so meake sure that depth writing is enabled
	GL_Enable (gl_state.statebits | DEPTHWRITE_BIT);

	if (!gl_lockfrustum->value)
	{
		// extract the frustum from the MVP matrix
		frustum[0].normal[0] = r_mvpmatrix._14 - r_mvpmatrix._11;
		frustum[0].normal[1] = r_mvpmatrix._24 - r_mvpmatrix._21;
		frustum[0].normal[2] = r_mvpmatrix._34 - r_mvpmatrix._31;

		frustum[1].normal[0] = r_mvpmatrix._14 + r_mvpmatrix._11;
		frustum[1].normal[1] = r_mvpmatrix._24 + r_mvpmatrix._21;
		frustum[1].normal[2] = r_mvpmatrix._34 + r_mvpmatrix._31;

		frustum[2].normal[0] = r_mvpmatrix._14 + r_mvpmatrix._12;
		frustum[2].normal[1] = r_mvpmatrix._24 + r_mvpmatrix._22;
		frustum[2].normal[2] = r_mvpmatrix._34 + r_mvpmatrix._32;

		frustum[3].normal[0] = r_mvpmatrix._14 - r_mvpmatrix._12;
		frustum[3].normal[1] = r_mvpmatrix._24 - r_mvpmatrix._22;
		frustum[3].normal[2] = r_mvpmatrix._34 - r_mvpmatrix._32;
	}

	for (i = 0; i < 4; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
	}

	GL_LoadMatrix (&sharedubo.worldMatrix, &r_mvpmatrix);
	VectorCopy (vpn, sharedubo.forwardVec);
	VectorCopy (vup, sharedubo.upVec);
	VectorCopy (vright, sharedubo.rightVec);
	VectorCopy (r_origin, sharedubo.viewOrigin);

	glNamedBufferDataEXT (gl_sharedubo, sizeof (sharedubo_t), NULL, GL_STREAM_DRAW);
	glNamedBufferSubDataEXT (gl_sharedubo, 0, sizeof (sharedubo_t), &sharedubo);

	// set up everything that only needs to be done once per frame
	RSky_BeginFrame ();
	RWarp_BeginFrame ();
	RSurf_BeginFrame ();
}


/*
=============
R_Clear
=============
*/
void R_Clear (void)
{
	if (gl_lockpvs->value || gl_lockfrustum->value)
	{
		glClearColor (0.3, 0.3, 0.3, 1);
		GL_Clear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
	else if (gl_clear->value)
	{
		glClearColor (1, 0, 0.5, 0.5);
		GL_Clear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
	else
		GL_Clear (GL_DEPTH_BUFFER_BIT);
}


/*
================
R_RenderView

r_newrefdef must be set before the first call
================
*/
void R_RenderView (refdef_t *fd)
{
	// in case a cinematic or tileclear was drawn first
	Draw_End2D ();

	if (r_norefresh->value)
		return;

	r_newrefdef = *fd;

	if (!r_worldmodel && !(r_newrefdef.rdflags & RDF_NOWORLDMODEL))
		VID_Error (ERR_DROP, "R_RenderView: NULL worldmodel");

	if (r_speeds->value)
	{
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

	if (gl_finish->value)
		glFinish ();

	R_SetupFrame ();

	R_SetupGL ();

	if (!(r_newrefdef.rdflags & RDF_NOWORLDMODEL))
	{
		R_MarkLeaves ();	// done here so we know if we're in water

		RPostProcess_Begin ();

		R_Clear ();

		R_DrawWorld ();

		R_DrawEntitiesOnList ();

		// the common case has particles drawing over alpha surfaces so we adjust for that
		R_DrawAlphaSurfaces ();

		R_DrawParticles ();

		RPostProcess_FinishToScreen();
	}
	else
	{
		R_Clear ();
		R_DrawEntitiesOnList ();
	}
}


/*
====================
R_SetLightLevel
====================
*/
static void R_SetLightLevel (void)
{
	vec3_t		shadelight;
	float		lightspot[3];

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
=====================
RE_RenderFrame
=====================
*/
void RE_GL_RenderFrame (refdef_t *fd)
{
	R_RenderView (fd);
	R_SetLightLevel ();

	if (r_speeds->value)
	{
		VID_Printf (PRINT_ALL, "%4i wpoly %4i epoly\n",
					  c_brush_polys,
					  c_alias_polys);
	}
}


static void R_Register (void)
{
	r_lefthand = Cvar_Get ("hand", "0", CVAR_USERINFO | CVAR_ARCHIVE);
	r_norefresh = Cvar_Get ("r_norefresh", "0", 0);
	r_fullbright = Cvar_Get ("r_fullbright", "0", 0);
	r_drawentities = Cvar_Get ("r_drawentities", "1", 0);
	r_drawworld = Cvar_Get ("r_drawworld", "1", 0);
	r_novis = Cvar_Get ("r_novis", "0", 0);
	r_nocull = Cvar_Get ("r_nocull", "0", 0);
	r_lerpmodels = Cvar_Get ("r_lerpmodels", "1", 0);
	r_speeds = Cvar_Get ("r_speeds", "0", 0);

	r_lightlevel = Cvar_Get ("r_lightlevel", "0", 0);

	gl_mode = Cvar_Get ("gl_mode", "8", CVAR_ARCHIVE);
	gl_customwidth = Cvar_Get("gl_customwidth", "1024", CVAR_ARCHIVE);
	gl_customheight = Cvar_Get("gl_customheight", "768", CVAR_ARCHIVE);
	gl_customPixelAspect = Cvar_Get("gl_customPixelAspect", "1", CVAR_ARCHIVE);
	gl_lightmap = Cvar_Get ("gl_lightmap", "0", 0);
	gl_shadows = Cvar_Get ("gl_shadows", "0", CVAR_ARCHIVE);
	gl_dynamic = Cvar_Get ("gl_dynamic", "1", 0);
	gl_showtris = Cvar_Get ("gl_showtris", "0", 0);
	gl_finish = Cvar_Get ("gl_finish", "0", CVAR_ARCHIVE);
	gl_clear = Cvar_Get ("gl_clear", "0", 0);
	gl_cull = Cvar_Get ("gl_cull", "1", 0);
	gl_monolightmap = Cvar_Get ("gl_monolightmap", "0", 0);
	gl_texturemode = Cvar_Get ("gl_texturemode", "GL_NEAREST_MIPMAP_NEAREST", CVAR_ARCHIVE);
	gl_textureanisotropy = Cvar_Get ("gl_textureanisotropy", "1", CVAR_ARCHIVE);
	gl_lockpvs = Cvar_Get ("gl_lockpvs", "0", 0);
	gl_lockfrustum = Cvar_Get ("gl_lockfrustum", "0", 0);
	
	gl_swapinterval = Cvar_Get ("gl_swapinterval", "1", CVAR_ARCHIVE);

	vid_fullscreen = Cvar_Get ("vid_fullscreen", "0", CVAR_ARCHIVE);
	vid_gamma = Cvar_Get ("vid_gamma", "1.0", CVAR_ARCHIVE);
	vid_contrast = Cvar_Get("vid_contrast", "1.0", CVAR_ARCHIVE);
	r_lightscale = Cvar_Get ("r_lightscale", "1.0", CVAR_ARCHIVE);

	RPostProcess_Init();

	Cmd_AddCommand ("imagelist", GL_ImageList_f);
	Cmd_AddCommand ("screenshot", GL_ScreenShot_f);
	Cmd_AddCommand ("modellist", Mod_Modellist_f);
	Cmd_AddCommand ("fbolist", R_FBOList_f);
}

/*
** R_GetModeInfo
*/
typedef struct glvidmode_s
{
	const char *description;
	int width, height;
	float pixelAspect;		// pixel width / height
} glvidmode_t;

glvidmode_t r_vidModes[] =
{
	{ "Mode  0:  320x240", 320, 240, 1 },
	{ "Mode  1:  400x300", 400, 300, 1 },
	{ "Mode  2:  512x384", 512, 384, 1 },
	{ "Mode  3:  640x400", 640, 400, 1 },
	{ "Mode  4:  640x480", 640, 480, 1 },
	{ "Mode  5:  800x500", 800, 500, 1 },
	{ "Mode  6:  800x600", 800, 600, 1 },
	{ "Mode  7:  960x720", 960, 720, 1 },
	{ "Mode  8: 1024x480", 1024, 480, 1 },
	{ "Mode  9: 1024x640", 1024, 640, 1 },
	{ "Mode 10: 1024x768", 1024, 768, 1 },
	{ "Mode 11: 1152x768", 1152, 768, 1 },
	{ "Mode 12: 1152x864", 1152, 864, 1 },
	{ "Mode 13: 1280x800", 1280, 800, 1 },
	{ "Mode 14: 1280x720", 1280, 720, 1 },
	{ "Mode 15: 1280x960", 1280, 960, 1 },
	{ "Mode 16: 1280x1024", 1280, 1024, 1 },
	{ "Mode 17: 1366x768", 1366, 768, 1 },
	{ "Mode 18: 1440x900", 1440, 900, 1 },
	{ "Mode 19: 1600x1200", 1600, 1200, 1 },
	{ "Mode 20: 1680x1050", 1680, 1050, 1 },
	{ "Mode 21: 1920x1080", 1920, 1080, 1 },
	{ "Mode 22: 1920x1200", 1920, 1200, 1 },
	{ "Mode 23: 2048x1536", 2048, 1536, 1 },
	{ "Mode 24: 3440x1440", 3440, 1440, 1 },
	{ "Mode 25: 3840x2160", 3840, 2160, 1 },
};
static int	s_numVidModes = ARRAY_LEN(r_vidModes);

qboolean R_GetModeInfo(int *width, int *height, float *windowAspect, int mode)
{
	glvidmode_t	*vm;
	float		pixelAspect;

	if (mode < -1)
	{
		return false;
	}
	if (mode >= s_numVidModes)
	{
		return false;
	}

	if (mode == -1)
	{
		*width = gl_customwidth->integer;
		*height = gl_customheight->integer;
		pixelAspect = gl_customPixelAspect->value;
	}
	else
	{
		vm = &r_vidModes[mode];

		*width = vm->width;
		*height = vm->height;
		pixelAspect = vm->pixelAspect;
	}

	*windowAspect = (float)*width / (*height * pixelAspect);

	return true;
}


void RMain_InvalidateCachedState (void)
{
	int i;

	for (i = 0; i < 32; i++)
	{
		gl_state.currentsamplers[i] = 0xffffffff;
		gl_state.currenttextures[i] = 0xffffffff;
	}

	gl_state.currentprogram = 0xffffffff;

	gl_state.srcblend = GL_INVALID_VALUE;
	gl_state.dstblend = GL_INVALID_VALUE;

	gl_state.currentvertexarray = 0xffffffff;

	// force a sync up
	glFinish ();
}

/*
================
R_PrintLongString

Workaround for Com_Printf's 1024 characters buffer limit.
================
*/
static void R_PrintLongString (const char *string)
{
	char buffer[1024];
	const char *p;
	int size = strlen (string);

	p = string;
	while (size > 0)
	{
		Q_strlcpy (buffer, p, sizeof(buffer));
		VID_Printf (PRINT_ALL, "%s", buffer);
		p += 1023;
		size -= 1023;
	}
}

static void RMain_PrintSetExtensionList (void)
{
	GLint n = 0;

	VID_Printf (PRINT_ALL, "GL_EXTENSIONS: ");

	if (glGetStringi)
	{
		int buffer_size = 1024;
		int buffer_pos = 0;
		char *ext_strings_buffer = (char *)malloc(buffer_size * sizeof(char));

		// grab number of extensions supported
		glGetIntegerv(GL_NUM_EXTENSIONS, &n);
		for (int i = 0; i < n; i++)
		{
			const char * extension_string = (char *)glGetStringi(GL_EXTENSIONS, i);

			// print this guy out
			VID_Printf (PRINT_ALL, "%s ", extension_string);

			// set full extension string, thanks OpenGL 3...
			int extension_string_length = strlen(extension_string);
			if (buffer_pos + extension_string_length + 1 > buffer_size)
			{
				buffer_size += 1024;
				ext_strings_buffer = (char *)realloc(ext_strings_buffer, buffer_size * sizeof(char));
			}
			strcpy(ext_strings_buffer + buffer_pos, extension_string);
			buffer_pos += extension_string_length;
			ext_strings_buffer[buffer_pos++] = ' '; // space separated, overwrites NULL
		}

		// finally set the string
		ext_strings_buffer[++buffer_pos] = '\0'; // NULL terminate
		gl_config.extension_string = ext_strings_buffer;
	}
	else
	{
		// the old-fashioned way of setting the extension string
		Q_strlcpy ((char *)gl_config.extension_string, (const char *)glGetString(GL_EXTENSIONS), sizeof(gl_config.extension_string));
		R_PrintLongString (gl_config.extension_string);
	}

	VID_Printf(PRINT_ALL, "\n");
}

static void RMain_CheckExtension (char *ext)
{
	// check in glew first...
	if (!glewIsSupported(ext))
	{
		// lets double check the actual extension list we grabbed earlier,
		// glew is known to be buggy checking for extensions...
		if (!strcmp(ext, gl_config.extension_string))
		{
			return;
		}
	
		VID_Error (ERR_FATAL, "RMain_CheckExtension : could not find %s", ext);
		return;
	}
}

extern int GL_Init_DSA_Emulation(qboolean inject_always_, qboolean allow_arb_dsa_, qboolean allow_ext_dsa_);
static qboolean RMain_CheckFor_DirectStateAccess(void)
{
	VID_Printf(PRINT_ALL, "RMain_CheckExtension : checking for GL_EXT_direct_state_access...\n");

	// check in glew first...
	if (!glewIsSupported("GL_EXT_direct_state_access "))
	{
		// lets double check the actual extension list we grabbed earlier,
		// glew is known to be buggy checking for extensions...
		if (!strcmp("GL_EXT_direct_state_access ", gl_config.extension_string))
		{
			// found it in our list
			VID_Printf(PRINT_ALL, " ...found GL_EXT_direct_state_access\n");
			return true;
		}
	}
	else
	{
		// found it in glew's list
		VID_Printf(PRINT_ALL, " ...found GL_EXT_direct_state_access\n");
		return true;
	}

	// okay, we don't have direct state access, so we need to wrap the functions
	// and emulate what they actually do...
	if (GL_Init_DSA_Emulation(true, false, true) != 1)
	{
		// we got a buggy driver here or something is fucked...
		return false;
	}

	VID_Printf(PRINT_ALL, " ...emulating GL_EXT_direct_state_access\n");
	return true;
}

static void RMain_CheckFor_GPUShader5(void)
{
	VID_Printf(PRINT_ALL, "RMain_CheckExtension : checking for GL_ARB_gpu_shader5...\n");

	// check in glew first...
	if (!glewIsSupported("GL_ARB_gpu_shader5 "))
	{
		// lets double check the actual extension list we grabbed earlier,
		// glew is known to be buggy checking for extensions...
		if (!strcmp("GL_ARB_gpu_shader5 ", gl_config.extension_string))
		{
			// found it in our list
			VID_Printf(PRINT_ALL, " ...found GL_ARB_gpu_shader5\n");
			gl_config.gl_ext_GPUShader5_support = true;
			return;
		}
	}
	else
	{
		// found it in glew's list
		VID_Printf(PRINT_ALL, " ...found GL_ARB_gpu_shader5\n");
		gl_config.gl_ext_GPUShader5_support = true;
		return;
	}

	VID_Printf(PRINT_ALL, " ...missing GL_ARB_gpu_shader5\n");
	gl_config.gl_ext_GPUShader5_support = false;
}

static void RMain_CheckFor_ComputeShader(void)
{
	VID_Printf(PRINT_ALL, "RMain_CheckExtension : checking for GL_ARB_compute_shader...\n");

	// check in glew first...
	if (!glewIsSupported("GL_ARB_compute_shader "))
	{
		// lets double check the actual extension list we grabbed earlier,
		// glew is known to be buggy checking for extensions...
		if (!strcmp("GL_ARB_compute_shader ", gl_config.extension_string))
		{
			// found it in our list
			VID_Printf(PRINT_ALL, " ...found GL_ARB_compute_shader\n");
			gl_config.gl_ext_computeShader_support = true;
			return;
		}
	}
	else
	{
		// found it in glew's list
		VID_Printf(PRINT_ALL, " ...found GL_ARB_compute_shader\n");
		gl_config.gl_ext_computeShader_support = true;
		return;
	}

	VID_Printf(PRINT_ALL, " ...missing GL_ARB_compute_shader\n");
	gl_config.gl_ext_computeShader_support = false;
}

#define R_MODE_FALLBACK 10 // 1024x768

static int SetMode_impl(int mode, int fullscreen)
{
	vidrserr_t err;

	err = VID_InitWindow(mode, fullscreen);
	switch (err)
	{
		case RSERR_INVALID_FULLSCREEN:
			VID_Printf(PRINT_ALL, "...WARNING: fullscreen unavailable in this mode\n");
			return false;
		case RSERR_INVALID_MODE:
			VID_Printf(PRINT_ALL, "...WARNING: could not set the given mode (%d)\n", mode);
			return false;
		default:
			break;
	}

	return true;
}

static qboolean R_SetMode(void)
{
	char renderer_buffer[1000];
	char vendor_buffer[1000];

	vid_fullscreen->modified = false;
	gl_mode->modified = false;

	// create the window and set up the context
	if (SetMode_impl(gl_mode->integer, vid_fullscreen->integer))
		goto success;

	// try again, with the default screen resolution
	if (gl_mode->integer != R_MODE_FALLBACK)
	{
		VID_Printf(PRINT_ALL, "Setting gl_mode %d failed, falling back on gl_mode %d\n", gl_mode->integer, R_MODE_FALLBACK);
		if (SetMode_impl(R_MODE_FALLBACK, false))
			goto success;
	}

	// yea this failed... fallback to software or something else
	VID_Printf(PRINT_ALL, "GLimp_Init() - could not load OpenGL subsystem");
	return false;

success:
	vid.displayAspect = viddef.displayAspect;
	vid.height = viddef.height;
	vid.width = viddef.width;
	vid.refreshRate = viddef.refreshRate;
	vid.vsyncActive = viddef.vsyncActive;

	// get our various GL strings
	gl_config.vendor_string = glGetString(GL_VENDOR);
	VID_Printf(PRINT_ALL, "GL_VENDOR: %s\n", gl_config.vendor_string);

	gl_config.renderer_string = glGetString(GL_RENDERER);
	VID_Printf(PRINT_ALL, "GL_RENDERER: %s\n", gl_config.renderer_string);

	gl_config.version_string = glGetString(GL_VERSION);
	VID_Printf(PRINT_ALL, "GL_VERSION: %s\n", gl_config.version_string);

	strcpy(renderer_buffer, gl_config.renderer_string);
	Q_strlwr(renderer_buffer);

	strcpy(vendor_buffer, gl_config.vendor_string);
	Q_strlwr(vendor_buffer);

	RMain_PrintSetExtensionList();

	// check for required feature support
	// GLEW isn't always reliable so check them manually
	RMain_CheckExtension("GL_ARB_multitexture ");
	RMain_CheckExtension("GL_ARB_sampler_objects ");
	RMain_CheckExtension("GL_ARB_vertex_buffer_object ");
	RMain_CheckExtension("GL_ARB_vertex_array_object ");
	RMain_CheckExtension("GL_ARB_texture_non_power_of_two ");
	RMain_CheckExtension("GL_ARB_framebuffer_object ");
	RMain_CheckExtension("GL_ARB_instanced_arrays ");
	RMain_CheckExtension("GL_ARB_base_instance ");
	RMain_CheckExtension("GL_ARB_map_buffer_range ");
	RMain_CheckExtension("GL_ARB_texture_storage ");
	RMain_CheckExtension("GL_ARB_seamless_cube_map ");
	RMain_CheckExtension("GL_ARB_uniform_buffer_object ");
	RMain_CheckExtension("GL_ARB_separate_shader_objects ");

	// we have to check for direct state access separate from the others
	// due to weird issues across GPUs
	if (!RMain_CheckFor_DirectStateAccess())
	{
		// we are buggggggged, get out of here and fallback to like software or something
		VID_Printf(PRINT_ALL, "RMain_CheckExtension : unable to emulate GL_EXT_direct_state_access\n");
		return false;
	}

	// check for GPUShader5 support
	RMain_CheckFor_GPUShader5();

	// check for compute shader support
	RMain_CheckFor_ComputeShader();

	return true;
}

/*
===============
RE_Init
===============
*/
int RE_GL_Init (void)
{
	Draw_GetPalette ();

	R_Register ();

	// initialize OS-specific parts of OpenGL
	if (!VID_Init_GL())
		return -1;

	// create the window and set up the context
	if (!R_SetMode())
	{
		VID_Printf (PRINT_ALL, "ref_gl::RE_GL_Init() - could not R_SetMode()\n");
		return -1;
	}

	// now invalidate the cached state to force everything to recache
	RMain_InvalidateCachedState ();

	// clear all errors
	glGetError ();

	RImage_CreateSamplers ();

	// build the 2D ortho matrix that we use for everything up-front so that we can pre-load GLSL programs with it
	GL_LoadIdentity (&r_drawmatrix);
	GL_OrthoMatrix (&r_drawmatrix, 0, vid.width, vid.height, 0, -99999, 99999);

	// create all of our vertex/fragment programs
	RSurf_CreatePrograms ();
	RWarp_CreatePrograms ();
	RSky_CreatePrograms ();
	RDraw_CreatePrograms ();
	RMesh_CreatePrograms ();
	Sprite_CreatePrograms ();
	RBeam_CreatePrograms ();
	RNull_CreatePrograms ();
	RPart_CreatePrograms ();

	// create our ubo for shared stuff
	glGenBuffers (1, &gl_sharedubo);
	glNamedBufferDataEXT (gl_sharedubo, sizeof (sharedubo_t), NULL, GL_STREAM_DRAW);

	VID_MenuInit ();

	GL_SetDefaultState ();

	GL_InitImages ();
	Mod_Init ();
	R_InitParticleTexture ();
	Draw_InitLocal ();

	// create post process programs
	RPostProcess_CreatePrograms ();

	// create framebuffer objects
	R_InitFBOs ();

	// we don't the user to be able to change the list of modes
	Cvar_Get("r_availableModes", "", CVAR_NOSET);

	return 1;
}

/*
===============
RE_Shutdown
===============
*/
void RE_GL_Shutdown (void)
{
	Cmd_RemoveCommand ("modellist");
	Cmd_RemoveCommand ("screenshot");
	Cmd_RemoveCommand ("imagelist");
	Cmd_RemoveCommand ("fbolist");

	Mod_FreeAll ();

	GL_ShutdownImages ();

	R_ShutdownFBOs ();

	// shut down OS specific OpenGL stuff like contexts, etc.
	VID_Shutdown_GL(true);
}

/*
=====================
RE_BeginFrame
=====================
*/
void RE_GL_BeginFrame (float camera_separation)
{
	// change modes if necessary
	if (gl_mode->modified || vid_fullscreen->modified)
	{
		cvar_t	*ref = Cvar_Get("vid_ref", "gl", 0);
		ref->modified = true;
	}

	VID_GL_BeginFrame (camera_separation);

	if (gl_texturemode->modified || gl_textureanisotropy->modified)
	{
		GL_TextureMode (gl_texturemode->string, (int) gl_textureanisotropy->value);
		gl_textureanisotropy->modified = false;
		gl_texturemode->modified = false;
	}

	GL_UpdateSwapInterval ();
}

/*
=====================
RE_EndFrame
=====================
*/
void RE_GL_EndFrame (void)
{
	// do a swap buffer
	VID_GL_EndFrame ();
}

/*
=============
RE_SetPalette
=============
*/
unsigned r_rawpalette[256];

void RE_GL_SetPalette (const unsigned char *palette)
{
	int		i;

	byte *rp = (byte *) r_rawpalette;

	if (palette)
	{
		for (i = 0; i < 256; i++)
		{
			rp[i * 4 + 2] = palette[i * 3 + 0];
			rp[i * 4 + 1] = palette[i * 3 + 1];
			rp[i * 4 + 0] = palette[i * 3 + 2];
			rp[i * 4 + 3] = 0xff;
		}
	}
	else
	{
		for (i = 0; i < 256; i++)
		{
			rp[i * 4 + 2] = d_8to24table_rgba[i] & 0xff;
			rp[i * 4 + 1] = (d_8to24table_rgba[i] >> 8) & 0xff;
			rp[i * 4 + 0] = (d_8to24table_rgba[i] >> 16) & 0xff;
			rp[i * 4 + 3] = 0xff;
		}
	}

	glClearColor (0, 0, 0, 0);
	GL_Clear (GL_COLOR_BUFFER_BIT);
	glClearColor (1, 0, 0.5, 0.5);
}

/*
============
GL_GFX_CoreInit

OpenGL refresh core entry point
============
*/
void GL_GFX_CoreInit(void)
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

