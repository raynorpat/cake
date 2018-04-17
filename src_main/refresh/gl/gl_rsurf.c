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
// GL_RSURF.C: surface-related refresh code
#include <assert.h>
#ifndef _WIN32
#include <sys/mman.h>
#endif
#include "gl_local.h"

GLenum gl_index_type = GL_UNSIGNED_INT;
int gl_index_size = sizeof (unsigned int);

#define SURF_UBO_MAX_BLOCKS		128

typedef struct surfubo_s
{
	glmatrix localMatrix;
	float surfAlpha;
} surfubo_t;

typedef struct surfubocommon_s
{
	glmatrix skyMatrix;
	glmatrix colorMatrix;
	float warpParms[4];
	float surfscroll;
	float warpscroll;
} surfubocommon_t;

GLuint r_surfacevbo = 0;
GLuint r_surfacevao = 0;
GLuint r_surfaceibo = 0;
GLuint r_surfaceubo = 0;
GLuint r_surfubobinding = 0;

GLuint r_surfacecommonubo = 0;
GLuint r_surfubocommonbinding = 1;

int r_surfuboblocksize = 0;
int r_surfubonumblocks = 0;

surfubo_t gl_surfuboupdate;
surfubocommon_t gl_surfubocommonupdate;

int gl_firstindex = 0;
int gl_maxindexes = 0;

GLuint gl_lightmappedsurfprog = 0;

GLuint u_brushlocalMatrix;
GLuint u_brushcolormatrix;
GLuint u_brushsurfalpha;
GLuint u_brushdebugParams;
GLuint u_brushscroll;
GLuint u_brushMaxLights;
GLuint u_brushlightMatrix;
GLuint u_brushLightPos[MAX_LIGHTS];
GLuint u_brushLightColor[MAX_LIGHTS];
GLuint u_brushLightAtten[MAX_LIGHTS];

void RSurf_CreatePrograms (void)
{
	glGetIntegerv (GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &r_surfuboblocksize);

	if (r_surfuboblocksize < sizeof (surfubo_t))
	{
		int addsize = r_surfuboblocksize;

		while (r_surfuboblocksize < sizeof (surfubo_t))
			r_surfuboblocksize += addsize;
	}

	gl_lightmappedsurfprog = GL_CreateShaderFromName ("glsl/brush.glsl", "LightmappedVS", "LightmappedFS");

	glProgramUniform1i (gl_lightmappedsurfprog, glGetUniformLocation (gl_lightmappedsurfprog, "diffuse"), 0);
	glProgramUniform1i (gl_lightmappedsurfprog, glGetUniformLocation (gl_lightmappedsurfprog, "lightmap"), 2);

	u_brushlocalMatrix = glGetUniformLocation (gl_lightmappedsurfprog, "localMatrix");
	u_brushcolormatrix = glGetUniformLocation (gl_lightmappedsurfprog, "colormatrix");
	u_brushsurfalpha = glGetUniformLocation (gl_lightmappedsurfprog, "surfalpha");
	u_brushdebugParams = glGetUniformLocation (gl_lightmappedsurfprog, "debugParams");
	u_brushscroll = glGetUniformLocation (gl_lightmappedsurfprog, "scroll");
	u_brushMaxLights = glGetUniformLocation (gl_lightmappedsurfprog, "maxLights");
	u_brushlightMatrix = glGetUniformLocation (gl_lightmappedsurfprog, "lightMatrix");
	for (int i = 0; i < MAX_LIGHTS; ++i)
	{
		u_brushLightPos[i] = glGetUniformLocation (gl_lightmappedsurfprog, va("Lights.origin[%i]", i));
		u_brushLightColor[i] = glGetUniformLocation (gl_lightmappedsurfprog, va("Lights.color[%i]", i));
		u_brushLightAtten[i] = glGetUniformLocation (gl_lightmappedsurfprog, va("Lights.radius[%i]", i));
	}

	glGenBuffers (1, &r_surfaceubo);
	glNamedBufferDataEXT (r_surfaceubo, SURF_UBO_MAX_BLOCKS * r_surfuboblocksize, NULL, GL_STREAM_DRAW);

	glGenBuffers (1, &r_surfacecommonubo);
	glNamedBufferDataEXT (r_surfacecommonubo, sizeof (surfubocommon_t), NULL, GL_STREAM_DRAW);

	// force a lighting param update on the first frame
	gl_monolightmap->modified = true;
	r_lightscale->modified = true;
}


void R_BeginWaterPolys (glmatrix *matrix, float alpha);

static vec3_t	modelorg;		// relative to viewpoint

static msurface_t *r_alpha_surfaces;
static msurface_t *r_sky_surfaces;

typedef struct brushpolyvert_s
{
	float position[3];
	float texcoord[3];
	float lightmap[3];
	float normal[3];
} brushpolyvert_t;

static gllightmapstate_t gl_lms;

extern void R_SetCacheState (msurface_t *surf);
extern void R_BuildLightMap (msurface_t *surf, unsigned *dest, int stride);

/*
=============================================================

	BRUSH MODELS

=============================================================
*/

/*
===============
GL_R_TextureAnimation

Returns the proper texture for a given time and base texture
===============
*/
image_t *GL_R_TextureAnimation(entity_t *e, mtexinfo_t *tex)
{
	int c;

	if (!e) return tex->image;
	if (!e->currframe) return tex->image;
	if (!tex->next) return tex->image;

	c = e->currframe % tex->numframes;

	while (c)
	{
		tex = tex->next;
		c--;
	}

	return tex->image;
}


void RSurf_BeginFrame (void)
{
	int i;
	glmatrix colormatrix;
	float lightscale = 2.0f * r_lightscale->value;
	float scroll = -64 * ((r_newrefdef.time / 40.0) - (int) (r_newrefdef.time / 40.0));

	while (scroll >= 0.0)
		scroll -= 64.0;

	glProgramUniform1f (gl_lightmappedsurfprog, u_brushscroll, scroll);

	glProgramUniform4f (gl_lightmappedsurfprog, u_brushdebugParams, gl_showlightmap->value, gl_shownormals->value, 0, 0);

	if (!gl_monolightmap->modified && !r_lightscale->modified)
		return;

	if (gl_monolightmap->value)
	{
		// greyscale
		for (i = 0; i < 4; i++)
		{
			colormatrix.m[0][i] = 0.3f * lightscale;
			colormatrix.m[1][i] = 0.59f * lightscale;
			colormatrix.m[2][i] = 0.11f * lightscale;
			colormatrix.m[3][i] = 0.0f;
		}
	}
	else
	{
		// coloured
		GL_LoadIdentity (&colormatrix);
		GL_ScaleMatrix (&colormatrix, lightscale, lightscale, lightscale);

		// prevent scaling in the alpha channel
		colormatrix.m[3][3] = 0;
	}

	glProgramUniformMatrix4fv (gl_lightmappedsurfprog, u_brushcolormatrix, 1, GL_FALSE, colormatrix.m[0]);

	gl_monolightmap->modified = false;
	r_lightscale->modified = false;
}


void RSurf_SelectProgramAndStates (glmatrix *matrix, float alpha)
{
	int i;
	qboolean stateset = false;

	glProgramUniformMatrix4fv (gl_lightmappedsurfprog, u_brushlocalMatrix, 1, GL_FALSE, matrix->m[0]);
	glProgramUniform1f (gl_lightmappedsurfprog, u_brushsurfalpha, alpha);

	GL_UseProgram (gl_lightmappedsurfprog);
	GL_BindTexture (GL_TEXTURE2, GL_TEXTURE_2D_ARRAY, r_lightmapsampler, gl_state.lightmap_textures);

	// upload any lightmaps that were modified
	for (i = 0; i < gl_lms.current_lightmap_texture; i++)
	{
		if (gl_lms.modified[i])
		{
			if (!stateset)
			{
				// allow proper subrect usage
				glPixelStorei (GL_UNPACK_ROW_LENGTH, LIGHTMAP_SIZE);
				stateset = true;
			}

			// texture is already bound so just modify it
			// oh lord won't you buy me a proper rectangle struct
			// i code opengl and my head is fucked
			glTextureSubImage3DEXT (
				gl_state.lightmap_textures,
				GL_TEXTURE_2D_ARRAY,
				0,
				gl_lms.lightrect[i].left,
				gl_lms.lightrect[i].top,
				i,
				(gl_lms.lightrect[i].right - gl_lms.lightrect[i].left),
				(gl_lms.lightrect[i].bottom - gl_lms.lightrect[i].top),
				1,
				GL_BGRA,
				GL_UNSIGNED_INT_8_8_8_8_REV,
				gl_lms.lightmap_data[i] + (gl_lms.lightrect[i].top * LIGHTMAP_SIZE) + gl_lms.lightrect[i].left);

			gl_lms.lightrect[i].left = LIGHTMAP_SIZE;
			gl_lms.lightrect[i].right = 0;
			gl_lms.lightrect[i].top = LIGHTMAP_SIZE;
			gl_lms.lightrect[i].bottom = 0;

			gl_lms.modified[i] = false;
		}
	}

	if (stateset) glPixelStorei (GL_UNPACK_ROW_LENGTH, 0);
}


/*
================
R_DrawAlphaSurfaces

Draw water surfaces and windows.
The BSP tree is walked front to back, so unwinding the chain
of alpha_surfaces will draw back to front, giving proper ordering.
================
*/
void R_DrawAlphaSurfaces (void)
{
	msurface_t *s = NULL;
	image_t *lasttexture = NULL;
	int lastsurfflags = -1;
	msurface_t *reversechain = NULL;
	int numindexes = 0;

	if (!r_alpha_surfaces) return;

	GL_BindVertexArray (r_surfacevao);
	GL_Enable (DEPTHTEST_BIT | (gl_cull->value ? CULLFACE_BIT : 0) | BLEND_BIT);

	for (s = r_alpha_surfaces; s; s = s->texturechain)
	{
		if ((s->flags != lastsurfflags) || (s->texinfo->image != lasttexture))
		{
			float alpha = (s->texinfo->flags & SURF_TRANS33) ? 0.333f : ((s->texinfo->flags & SURF_TRANS66) ? 0.666f : 1.0f);

			R_DrawSurfaceChain (reversechain, numindexes);
			reversechain = NULL;
			numindexes = 0;

			GL_BindTexture (GL_TEXTURE0, GL_TEXTURE_2D, r_surfacesampler, s->texinfo->image->texnum);

			if (s->flags & SURF_DRAWTURB)
				R_BeginWaterPolys (&r_mvpmatrix, alpha);
			else
				RSurf_SelectProgramAndStates (&r_mvpmatrix, alpha);

			lastsurfflags = s->flags;
			lasttexture = s->texinfo->image;
		}

		s->reversechain = reversechain;
		reversechain = s;
		numindexes += s->numindexes;
	}

	R_DrawSurfaceChain (reversechain, numindexes);

	r_alpha_surfaces = NULL;
}


void R_DrawSurfaceChain (msurface_t *chain, int numindexes)
{
	byte *ndx = NULL;
	msurface_t *surf = NULL;
	GLbitfield access;
	int offset, size;

	if (!chain) return;
	if (!numindexes) return;

	if (!chain->reversechain)
	{
		glDrawArrays (GL_TRIANGLE_FAN, chain->firstvertex, chain->numvertexes);
		c_brush_polys++;
		return;
	}

	if (gl_firstindex + numindexes >= gl_maxindexes)
	{
		access = BUFFER_DISCARD;
		gl_firstindex = 0;
	}
	else access = BUFFER_NO_OVERWRITE;

	offset = gl_firstindex * gl_index_size;
	size = numindexes * gl_index_size;

	if ((ndx = glMapNamedBufferRangeEXT (r_surfaceibo, offset, size, access)) != NULL)
	{
		for (surf = chain; surf; surf = surf->reversechain)
		{
			memcpy (ndx, surf->indexes, surf->numindexes * gl_index_size);
			ndx += surf->numindexes * gl_index_size;
			c_brush_polys++;
		}

		glUnmapNamedBufferEXT (r_surfaceibo);
		glDrawElements (GL_TRIANGLES, numindexes, gl_index_type, (void *) offset);

		gl_firstindex += numindexes;
	}
}


void R_DrawTextureChains (entity_t *e)
{
	int i;
	image_t *image = NULL;
	msurface_t *surf = NULL;
	float *v = NULL;
	model_t *mod = e->model;
	glmatrix *matrix = &e->matrix;

	GL_BindVertexArray (r_surfacevao);

	if (e->flags & RF_TRANSLUCENT)
	{
		GL_Enable (DEPTHTEST_BIT | (gl_cull->value ? CULLFACE_BIT : 0) | BLEND_BIT);
		RSurf_SelectProgramAndStates (matrix, 0.25);
	}
	else
	{
		RSurf_SelectProgramAndStates (matrix, 1);
		GL_Enable (DEPTHTEST_BIT | (gl_cull->value ? CULLFACE_BIT : 0) | DEPTHWRITE_BIT);
	}

	for (i = 0; i < mod->numtexinfo; i++)
	{
		msurface_t *reversechain = NULL;
		int numindexes = 0;

		if (!mod->texinfo[i].image) continue;
		if (!mod->texinfo[i].image->texturechain) continue;

		image = GL_R_TextureAnimation (e, &mod->texinfo[i]);
		surf = mod->texinfo[i].image->texturechain;

		if (surf->flags & SURF_DRAWTURB) continue;

		GL_BindTexture (GL_TEXTURE0, GL_TEXTURE_2D, r_surfacesampler, image->texnum);

		// reverse the chain to get f2b ordering
		for (; surf; surf = surf->texturechain)
		{
			surf->reversechain = reversechain;
			reversechain = surf;
			numindexes += surf->numindexes;
		}

		R_DrawSurfaceChain (reversechain, numindexes);

		mod->texinfo[i].image->texturechain = NULL;
	}

	R_BeginWaterPolys (matrix, (e->flags & RF_TRANSLUCENT) ? 0.25 : 1);

	for (i = 0; i < mod->numtexinfo; i++)
	{
		msurface_t *reversechain = NULL;
		int numindexes = 0;

		if (!mod->texinfo[i].image) continue;
		if (!mod->texinfo[i].image->texturechain) continue;

		surf = mod->texinfo[i].image->texturechain;

		if (!(surf->flags & SURF_DRAWTURB)) continue;

		GL_BindTexture (GL_TEXTURE0, GL_TEXTURE_2D, r_surfacesampler, mod->texinfo[i].image->texnum);

		for (; surf; surf = surf->texturechain)
		{
			surf->reversechain = reversechain;
			reversechain = surf;
			numindexes += surf->numindexes;
		}

		R_DrawSurfaceChain (reversechain, numindexes);

		mod->texinfo[i].image->texturechain = NULL;
	}

	if (r_sky_surfaces)
	{
		R_DrawSkyChain (r_sky_surfaces);
		r_sky_surfaces = NULL;
	}
}


void R_ModifySurfaceLightmap (msurface_t *surf)
{
	int map;

	if (surf->texinfo->flags & SURF_SKY) return;
	if (surf->texinfo->flags & SURF_WARP) return;

	if (!gl_dynamic->integer) return;

	// set dynamic lights
	R_EnableLights (surf->dlightframe, surf->dlightbits);

	// set lightmap styles
	for (map = 0; map < MAXLIGHTMAPS && surf->styles[map] != 255; map++)
	{
		if (r_newrefdef.lightstyles[surf->styles[map]].white != surf->cached_light[map])
		{
			int	smax, tmax;
			unsigned *base = gl_lms.lightmap_data[surf->lightmaptexturenum];
			RECT *rect = &gl_lms.lightrect[surf->lightmaptexturenum];

			smax = (surf->extents[0] >> 4) + 1;
			tmax = (surf->extents[1] >> 4) + 1;
			base += (surf->light_t * LIGHTMAP_SIZE) + surf->light_s;

			R_BuildLightMap(surf, base, LIGHTMAP_SIZE);
			R_SetCacheState(surf);

			gl_lms.modified[surf->lightmaptexturenum] = true;

			if (surf->lightrect.left < rect->left) rect->left = surf->lightrect.left;
			if (surf->lightrect.right > rect->right) rect->right = surf->lightrect.right;
			if (surf->lightrect.top < rect->top) rect->top = surf->lightrect.top;
			if (surf->lightrect.bottom > rect->bottom) rect->bottom = surf->lightrect.bottom;
		}
	}
}


void R_ChainSurface (msurface_t *surf)
{
	if (surf->texinfo->flags & SURF_SKY)
	{
		// just adds to visible sky bounds
		surf->texturechain = r_sky_surfaces;
		r_sky_surfaces = surf;
	}
	else if (surf->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66))
	{
		R_ModifySurfaceLightmap (surf);
		surf->texturechain = r_alpha_surfaces;
		r_alpha_surfaces = surf;
	}
	else
	{
		R_ModifySurfaceLightmap (surf);
		surf->texturechain = surf->texinfo->image->texturechain;
		surf->texinfo->image->texturechain = surf;
	}
}


/*
=================
R_DrawInlineBModel
=================
*/
void R_DrawInlineBModel (entity_t *e)
{
	int			i;
	cplane_t	*pplane;
	float		dot;
	msurface_t	*surf;
	glmatrix	localMatrix;
	
	// compute everything on the local matrix so that we can use it for lighting transforms
	GL_LoadIdentity (&localMatrix);
	GL_TranslateMatrix (&localMatrix, e->currorigin[0], e->currorigin[1], e->currorigin[2]);
	
	GL_RotateMatrix (&localMatrix, e->angles[1], 0, 0, 1);
	GL_RotateMatrix (&localMatrix, e->angles[0], 0, 1, 0);
	GL_RotateMatrix (&localMatrix, e->angles[2], 1, 0, 0);
	
	// now multiply out for the final model transform and take it's inverse for lighting
	GL_MultMatrix (&e->matrix, &localMatrix, &r_mvpmatrix);
	GL_InvertMatrix (&localMatrix, NULL, &localMatrix);
	
	// and now calculate dynamic lighting for bmodel
	R_MarkLights (e->model->nodes + e->model->firstnode, &localMatrix);

	// draw texture
	surf = &e->model->surfaces[e->model->firstmodelsurface];
	for (i = 0; i < e->model->nummodelsurfaces; i++, surf++)
	{
		// find which side of the node we are on
		pplane = surf->plane;

		dot = DotProduct (modelorg, pplane->normal) - pplane->dist;

		// draw the polygon
		if (((surf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) || (!(surf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
			R_ChainSurface (surf);
		}
	}

	R_DrawTextureChains (e);
}


/*
=================
R_DrawBrushModel
=================
*/
void R_DrawBrushModel (entity_t *e)
{
	vec3_t		mins, maxs;
	int			i;
	qboolean	rotated;

	if (e->model->firstmodelsurface == 0) return;
	if (e->model->nummodelsurfaces == 0) return;

	if (e->angles[0] || e->angles[1] || e->angles[2])
	{
		rotated = true;

		for (i = 0; i < 3; i++)
		{
			mins[i] = e->currorigin[i] - e->model->radius;
			maxs[i] = e->currorigin[i] + e->model->radius;
		}
	}
	else
	{
		rotated = false;
		VectorAdd (e->currorigin, e->model->mins, mins);
		VectorAdd (e->currorigin, e->model->maxs, maxs);
	}

	if (R_CullBox (mins, maxs))
		return;

	VectorSubtract (r_newrefdef.vieworg, e->currorigin, modelorg);

	if (rotated)
	{
		vec3_t	temp;
		vec3_t	forward, right, up;

		VectorCopy (modelorg, temp);
		AngleVectors (e->angles, forward, right, up);
		modelorg[0] = DotProduct (temp, forward);
		modelorg[1] = -DotProduct (temp, right);
		modelorg[2] = DotProduct (temp, up);
	}

	R_DrawInlineBModel (e);
}

/*
=============================================================

	WORLD MODEL

=============================================================
*/

/*
================
R_RecursiveWorldNode
================
*/
void R_RecursiveWorldNode (mnode_t *node)
{
	int			c, side, sidebit;
	cplane_t	*plane;
	msurface_t	*surf, **mark;
	mleaf_t		*pleaf;
	float		dot;

	if (node->contents == CONTENTS_SOLID)
		return;		// solid

	if (node->visframe != r_visframecount)
		return;

	if (R_CullBox (node->minmaxs, node->minmaxs + 3))
		return;

	// if a leaf node, draw stuff
	if (node->contents != -1)
	{
		pleaf = (mleaf_t *) node;

		// check for door connected areas
		if (r_newrefdef.areabits)
		{
			if (!(r_newrefdef.areabits[pleaf->area >> 3] & (1 << (pleaf->area & 7))))
				return;		// not visible
		}

		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

		if (c)
		{
			do
			{
				(*mark)->visframe = r_framecount;
				mark++;
			} while (--c);
		}

		return;
	}

	// node is just a decision point, so go down the apropriate sides
	// find which side of the node we are on
	plane = node->plane;

	switch (plane->type)
	{
	case PLANE_X:
		dot = modelorg[0] - plane->dist;
		break;
	case PLANE_Y:
		dot = modelorg[1] - plane->dist;
		break;
	case PLANE_Z:
		dot = modelorg[2] - plane->dist;
		break;
	default:
		dot = DotProduct (modelorg, plane->normal) - plane->dist;
		break;
	}

	if (dot >= 0)
	{
		side = 0;
		sidebit = 0;
	}
	else
	{
		side = 1;
		sidebit = SURF_PLANEBACK;
	}

	// recurse down the children, front side first
	R_RecursiveWorldNode (node->children[side]);

	// draw stuff
	for (c = node->numsurfaces, surf = r_worldmodel->surfaces + node->firstsurface; c; c--, surf++)
	{
		if (surf->visframe != r_framecount)
			continue;

		if ((surf->flags & SURF_PLANEBACK) != sidebit)
			continue;		// wrong side

		// add to the appropriate chain
		R_ChainSurface (surf);
	}

	// recurse down the back side
	R_RecursiveWorldNode (node->children[!side]);
}


/*
=============
R_DrawWorld
=============
*/
void R_DrawWorld (void)
{
	entity_t ent;
	glmatrix localMatrix;

	if (!r_drawworld->value)
		return;

	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
		return;

	VectorCopy (r_newrefdef.vieworg, modelorg);

	// auto cycle the world frame for texture animation
	memset (&ent, 0, sizeof (ent));
	ent.currframe = (int) (r_newrefdef.time * 2);
	ent.model = r_worldmodel;
	memcpy (&ent.matrix, &r_mvpmatrix, sizeof (glmatrix));

	// mark dynamic lights for world
	R_MarkLights (r_worldmodel->nodes, GL_LoadIdentity(&localMatrix));

	// recurse world
	R_RecursiveWorldNode (r_worldmodel->nodes);

	// draw world
	R_DrawTextureChains (&ent);
}


/*
===============
R_MarkLeaves

Mark the leaves and nodes that are in the PVS for the current
cluster
===============
*/
void R_MarkLeaves (void)
{
	byte	*vis;
	byte	fatvis[MAX_MAP_LEAFS/8];
	mnode_t	*node;
	int		i, c;
	mleaf_t	*leaf;
	int		cluster;

	if (r_oldviewcluster == r_viewcluster && r_oldviewcluster2 == r_viewcluster2 && !r_novis->value && r_viewcluster != -1)
		return;

	// development aid to let you run around and see exactly where
	// the pvs ends
	if (gl_lockpvs->value)
		return;

	r_visframecount++;
	r_oldviewcluster = r_viewcluster;
	r_oldviewcluster2 = r_viewcluster2;

	if (r_novis->value || r_viewcluster == -1 || !r_worldmodel->vis)
	{
		// mark everything
		for (i = 0; i < r_worldmodel->numleafs; i++)
			r_worldmodel->leafs[i].visframe = r_visframecount;

		for (i = 0; i < r_worldmodel->numnodes; i++)
			r_worldmodel->nodes[i].visframe = r_visframecount;

		return;
	}

	vis = Mod_ClusterPVS (r_viewcluster, r_worldmodel);

	// may have to combine two clusters because of solid water boundaries
	if (r_viewcluster2 != r_viewcluster)
	{
		memcpy (fatvis, vis, (r_worldmodel->numleafs + 7) / 8);
		vis = Mod_ClusterPVS (r_viewcluster2, r_worldmodel);
		c = (r_worldmodel->numleafs + 31) / 32;

		for (i = 0; i < c; i++)
			((int *) fatvis) [i] |= ((int *) vis) [i];

		vis = fatvis;
	}

	for (i = 0, leaf = r_worldmodel->leafs; i < r_worldmodel->numleafs; i++, leaf++)
	{
		cluster = leaf->cluster;

		if (cluster == -1)
			continue;

		if (vis[cluster >> 3] & (1 << (cluster & 7)))
		{
			node = (mnode_t *) leaf;

			do
			{
				if (node->visframe == r_visframecount)
					break;

				node->visframe = r_visframecount;
				node = node->parent;
			} while (node);
		}
	}
}



void GL_BeginBuildingVBO (int numverts, int numindexes)
{
	glDeleteBuffers (1, &r_surfacevbo);
	glGenBuffers (1, &r_surfacevbo);

	// this is done so that we don't leave a bound vbo dangling if we sys_error during building (e.g. running out of lightmaps)
	// hopefully drivers are going to behave well with it. ;)
	glNamedBufferDataEXT (r_surfacevbo, numverts * sizeof (brushpolyvert_t), NULL, GL_STATIC_DRAW);

	if (numverts < 65535)
	{
		gl_index_size = sizeof (unsigned short);
		gl_index_type = GL_UNSIGNED_SHORT;
	}
	else
	{
		gl_index_size = sizeof (unsigned int);
		gl_index_type = GL_UNSIGNED_INT;
	}

	glDeleteBuffers (1, &r_surfaceibo);
	glGenBuffers (1, &r_surfaceibo);
	glNamedBufferDataEXT (r_surfaceibo, numindexes * gl_index_size, NULL, GL_STREAM_DRAW);

	gl_firstindex = 0;
	gl_maxindexes = numindexes;
}


void GL_EndBuildingVBO (void)
{
	glGenVertexArrays (1, &r_surfacevao);

	glEnableVertexArrayAttribEXT (r_surfacevao, 0);
	glVertexArrayVertexAttribOffsetEXT (r_surfacevao, r_surfacevbo, 0, 3, GL_FLOAT, GL_FALSE, sizeof (brushpolyvert_t), 0);

	glEnableVertexArrayAttribEXT (r_surfacevao, 1);
	glVertexArrayVertexAttribOffsetEXT (r_surfacevao, r_surfacevbo, 1, 3, GL_FLOAT, GL_FALSE, sizeof (brushpolyvert_t), 12);

	glEnableVertexArrayAttribEXT (r_surfacevao, 2);
	glVertexArrayVertexAttribOffsetEXT (r_surfacevao, r_surfacevbo, 2, 3, GL_FLOAT, GL_FALSE, sizeof (brushpolyvert_t), 24);

	glEnableVertexArrayAttribEXT (r_surfacevao, 3);
	glVertexArrayVertexAttribOffsetEXT (r_surfacevao, r_surfacevbo, 3, 3, GL_FLOAT, GL_FALSE, sizeof (brushpolyvert_t), 36);

	GL_BindVertexArray (r_surfacevao);
	glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, r_surfaceibo);
}


/*
================
GL_BuildPolygonFromSurface
================
*/
void GL_BuildPolygonFromSurface (model_t *mod, msurface_t *surf)
{
	int	i;
	float s, t;
	vec3_t normal;
	static brushpolyvert_t vertbuf[64];

	// copy out surface normal
	VectorCopy(surf->plane->normal, normal);
	if (surf->flags & SURF_PLANEBACK || surf->texinfo->flags & SURF_PLANEBACK)
	{
		// if for some reason the normal sticks to the back of the plane, invert it
		// so it's usable for the shader
		for (i = 0; i < 3; i++)
			normal[i] = -normal[i];
	}

	// reconstruct the polygon
	for (i = 0; i < surf->numvertexes; i++)
	{
		int lindex = mod->surfedges[surf->firstedge + i];
		float *vec;

		if (lindex > 0)
			vec = mod->vertexes[mod->edges[lindex].v[0]].position;
		else vec = mod->vertexes[mod->edges[-lindex].v[1]].position;

		// polygon vertexes
		vertbuf[i].position[0] = vec[0];
		vertbuf[i].position[1] = vec[1];
		vertbuf[i].position[2] = vec[2];

		// polygon texture coords
		s = DotProduct (vec, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3];
		t = DotProduct (vec, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3];

		if (surf->texinfo->flags & SURF_WARP)
		{
			vertbuf[i].texcoord[0] = s;
			vertbuf[i].texcoord[1] = t;
		}
		else
		{
			vertbuf[i].texcoord[0] = s / surf->texinfo->image->width;
			vertbuf[i].texcoord[1] = t / surf->texinfo->image->height;

			// lightmap texture coordinates
			s = DotProduct (vec, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3];
			s -= surf->texturemins[0];
			s += surf->light_s * 16;
			s += 8;
			s /= LIGHTMAP_SIZE * 16;

			t = DotProduct (vec, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3];
			t -= surf->texturemins[1];
			t += surf->light_t * 16;
			t += 8;
			t /= LIGHTMAP_SIZE * 16;

			// lightmaps must always be built before surfaces so these are valid to set
			// texcoord[3] contains the slice index for a texture array
			vertbuf[i].lightmap[0] = s;
			vertbuf[i].lightmap[1] = t;
			vertbuf[i].lightmap[2] = surf->lightmaptexturenum;
		}

		if (surf->texinfo->flags & SURF_FLOWING)
			vertbuf[i].texcoord[2] = 1;
		else vertbuf[i].texcoord[2] = 0;

		// polygon normal
		VectorCopy (normal, vertbuf[i].normal);
	}

	// this is done so that we don't leave a bound vbo dangling if we sys_error during building (e.g. running out of lightmaps)
	// hopefully drivers are going to behave well with it. ;)
	glNamedBufferSubDataEXT (r_surfacevbo,
		surf->firstvertex * sizeof (brushpolyvert_t),
		surf->numvertexes * sizeof (brushpolyvert_t),
		vertbuf);
}


/*
=============================================================================

 LIGHTMAP ALLOCATION

=============================================================================
*/

static void LM_GL_BeginBlock (void)
{
	memset (gl_lms.allocated, 0, sizeof (gl_lms.allocated));
}

static void LM_GL_FinishBlock (void)
{
	gl_lms.lightrect[gl_lms.current_lightmap_texture].left = LIGHTMAP_SIZE;
	gl_lms.lightrect[gl_lms.current_lightmap_texture].right = 0;
	gl_lms.lightrect[gl_lms.current_lightmap_texture].top = LIGHTMAP_SIZE;
	gl_lms.lightrect[gl_lms.current_lightmap_texture].bottom = 0;

	gl_lms.modified[gl_lms.current_lightmap_texture] = false;

	if (++gl_lms.current_lightmap_texture == MAX_LIGHTMAPS)
		VID_Error (ERR_DROP, "LM_GL_FinishBlock() - MAX_LIGHTMAPS exceeded\n");
}

// returns a texture number and the position inside it
static qboolean LM_GL_AllocBlock (int w, int h, int *x, int *y)
{
	int		i, j;
	int		best, best2;

	best = LIGHTMAP_SIZE;

	for (i = 0; i < LIGHTMAP_SIZE - w; i++)
	{
		best2 = 0;

		for (j = 0; j < w; j++)
		{
			if (gl_lms.allocated[i+j] >= best)
				break;

			if (gl_lms.allocated[i+j] > best2)
				best2 = gl_lms.allocated[i+j];
		}

		if (j == w)
		{
			// this is a valid spot
			*x = i;
			*y = best = best2;
		}
	}

	if (best + h > LIGHTMAP_SIZE)
		return false;

	for (i = 0; i < w; i++)
		gl_lms.allocated[*x + i] = best + h;

	return true;
}



/*
========================
GL_CreateSurfaceLightmap
========================
*/
void GL_CreateSurfaceLightmap (msurface_t *surf)
{
	int		smax, tmax;
	unsigned	*base;

	if (surf->flags & SURF_DRAWSKY) return;
	if (surf->flags & SURF_DRAWTURB) return;
	if (surf->texinfo->flags & SURF_SKY) return;
	if (surf->texinfo->flags & SURF_WARP) return;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;

	if (!LM_GL_AllocBlock(smax, tmax, &surf->light_s, &surf->light_t))
	{
		LM_GL_FinishBlock ();
		LM_GL_BeginBlock ();

		if (!LM_GL_AllocBlock(smax, tmax, &surf->light_s, &surf->light_t))
		{
			VID_Error (ERR_FATAL, "Consecutive calls to LM_GL_AllocBlock(%d, %d) failed\n", smax, tmax);
		}
	}

	surf->lightmaptexturenum = gl_lms.current_lightmap_texture;
	surf->dlightframe = -1;

	surf->lightrect.left = surf->light_s;
	surf->lightrect.top = surf->light_t;
	surf->lightrect.right = surf->light_s + smax;
	surf->lightrect.bottom = surf->light_t + tmax;

	if (!gl_lms.lightmap_data[surf->lightmaptexturenum])
	{
#ifdef _WIN32
		gl_lms.lightmap_data[surf->lightmaptexturenum] = VirtualAlloc (
			gl_lms.lmhunkbase + gl_lms.lmhunkmark, LIGHTMAP_SIZE * LIGHTMAP_SIZE * 4, MEM_COMMIT, PAGE_READWRITE);
#else
		gl_lms.lightmap_data[surf->lightmaptexturenum] = mmap (
							gl_lms.lmhunkbase + gl_lms.lmhunkmark,
              LIGHTMAP_SIZE * LIGHTMAP_SIZE * 4,
              PROT_READ | PROT_WRITE,
              MAP_ANONYMOUS | MAP_PRIVATE,
              -1,
              0);
#endif

		gl_lms.lmhunkmark += LIGHTMAP_SIZE * LIGHTMAP_SIZE * 4;
	}

	base = gl_lms.lightmap_data[surf->lightmaptexturenum];
	base += (surf->light_t * LIGHTMAP_SIZE) + surf->light_s;

	R_SetCacheState (surf);
	R_BuildLightMap (surf, base, LIGHTMAP_SIZE);
}


/*
==================
GL_BeginBuildingLightmaps
==================
*/
void GL_BeginBuildingLightmaps (model_t *m)
{
	static lightstyle_t	lightstyles[MAX_LIGHTSTYLES];
	int i;

	// NULL the pointers
	for (i = 0; i < MAX_LIGHTMAPS; i++)
		gl_lms.lightmap_data[i] = NULL;

	// decommit or free as needed
	if (gl_lms.lmhunkbase)
	{
#ifdef _WIN32
		VirtualFree (gl_lms.lmhunkbase, MAX_LIGHTMAPS * LIGHTMAP_SIZE * LIGHTMAP_SIZE * 4, MEM_DECOMMIT);
#else
		munmap(gl_lms.lmhunkbase, MAX_LIGHTMAPS * LIGHTMAP_SIZE * LIGHTMAP_SIZE * 4);
#endif
	}
	else
	{
#ifdef _WIN32
		gl_lms.lmhunkbase = VirtualAlloc (NULL, MAX_LIGHTMAPS * LIGHTMAP_SIZE * LIGHTMAP_SIZE * 4, MEM_RESERVE, PAGE_READWRITE);
#else
		gl_lms.lmhunkbase = mmap (0,
					MAX_LIGHTMAPS * LIGHTMAP_SIZE * LIGHTMAP_SIZE * 4,
					PROT_READ | PROT_WRITE,
					MAP_ANONYMOUS | MAP_PRIVATE,
					-1,
					0);
#endif
	}

	// beginning again at zero
	gl_lms.lmhunkmark = 0;

	memset (gl_lms.allocated, 0, sizeof (gl_lms.allocated));

	r_framecount = 1;

	// setup the base lightstyles so the lightmaps won't have to be regenerated
	// the first time they're seen
	for (i = 0; i < MAX_LIGHTSTYLES; i++)
	{
		lightstyles[i].rgb[0] = 1;
		lightstyles[i].rgb[1] = 1;
		lightstyles[i].rgb[2] = 1;
		lightstyles[i].white = 3;
	}

	r_newrefdef.lightstyles = lightstyles;
	gl_lms.current_lightmap_texture = 0;
}


/*
=======================
GL_EndBuildingLightmaps
=======================
*/
void GL_EndBuildingLightmaps (void)
{
	int i;

	LM_GL_FinishBlock ();

	// respecify lightmap texture objects
	glDeleteTextures (1, &gl_state.lightmap_textures);
	glGenTextures (1, &gl_state.lightmap_textures);

	// upload all lightmaps that were filled
	glTextureStorage3DEXT (gl_state.lightmap_textures, GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, LIGHTMAP_SIZE, LIGHTMAP_SIZE, gl_lms.current_lightmap_texture);
	glPixelStorei (GL_UNPACK_ROW_LENGTH, LIGHTMAP_SIZE);

	for (i = 0; i < gl_lms.current_lightmap_texture; i++)
	{
		glTextureSubImage3DEXT (
			gl_state.lightmap_textures,
			GL_TEXTURE_2D_ARRAY, 0,
			0, 0, i,
			LIGHTMAP_SIZE, LIGHTMAP_SIZE, 1,
			GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
			gl_lms.lightmap_data[i]);
	}

	glPixelStorei (GL_UNPACK_ROW_LENGTH, 0);
}
