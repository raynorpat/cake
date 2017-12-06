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

#include "d3d_local.h"

#define MAX_BATCHED_SURFINDEXES		0x100000
#define MAX_BATCHED_SURFACES		0x4000

msurface_t *r_batchedsurfaces[MAX_BATCHED_SURFACES];
int r_numbatchedsurfaces = 0;

int r_surfacevertexes = 0;
int r_numsurfindexes = 0;
int r_firstsurfindex = 0;

samplerstate_t d3d_SurfLightmapSampler (D3DTEXF_LINEAR, D3DTEXF_NONE, D3DTEXF_LINEAR, D3DTADDRESS_CLAMP, 1);

IDirect3DVertexShader9 *d3d_SurfVS = NULL;
IDirect3DPixelShader9 *d3d_SurfPS = NULL;
IDirect3DPixelShader9 *d3d_AlphaPS = NULL;
IDirect3DVertexDeclaration9 *d3d_SurfDecl = NULL;
IDirect3DVertexBuffer9 *d3d_SurfBuffer = NULL;
IDirect3DIndexBuffer9 *d3d_SurfIndexes = NULL;

void Surf_LoadObjects (void)
{
	D3DVERTEXELEMENT9 decllayout[] =
	{
		VDECL (0, offsetof (brushpolyvert_t, xyz), D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 0),
		VDECL (0, offsetof (brushpolyvert_t, st), D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 0),
		VDECL (0, offsetof (brushpolyvert_t, lm), D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 1),
		D3DDECL_END ()
	};

	d3d_Device->CreateVertexDeclaration (decllayout, &d3d_SurfDecl);

	d3d_SurfVS = D3D_CreateVertexShader ("hlsl/fxSurf.fx", "SurfVS");
	d3d_SurfPS = D3D_CreatePixelShader ("hlsl/fxSurf.fx", "SurfPS");
	d3d_AlphaPS = D3D_CreatePixelShader ("hlsl/fxSurf.fx", "AlphaPS");
}


void Surf_Shutdown (void)
{
	SAFE_RELEASE (d3d_SurfBuffer);
	SAFE_RELEASE (d3d_SurfDecl);
	SAFE_RELEASE (d3d_SurfVS);
	SAFE_RELEASE (d3d_AlphaPS);
	SAFE_RELEASE (d3d_SurfPS);
}


void Surf_ResetDevice (void)
{
	d3d_Device->CreateIndexBuffer (
		MAX_BATCHED_SURFINDEXES * sizeof (unsigned int),
		D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC,
		D3DFMT_INDEX32,
		D3DPOOL_DEFAULT,
		&d3d_SurfIndexes,
		NULL
	);

	r_firstsurfindex = 0;
}


void Surf_LostDevice (void)
{
	SAFE_RELEASE (d3d_SurfIndexes);
}


CD3DHandler Surf_Handler (
	Surf_LoadObjects,
	Surf_Shutdown,
	Surf_LostDevice,
	Surf_ResetDevice
);


void R_BeginBatchingSurfaces (void)
{
	d3d_State->SetStreamSource (0, d3d_SurfBuffer, 0, sizeof (brushpolyvert_t));
	d3d_State->SetIndices (d3d_SurfIndexes);

	r_numsurfindexes = 0;
	r_numbatchedsurfaces = 0;
}


void R_EndBatchingSurfaces (void)
{
	if (r_numbatchedsurfaces && r_numsurfindexes)
	{
		int i;
		DWORD iblock = D3DLOCK_NOOVERWRITE | D3DLOCK_NOSYSLOCK;
		unsigned int *ndx = NULL;
		int r_firstdipvert = 0x7fffffff;
		int r_lastdipvert = 0;

		if (r_firstsurfindex + r_numsurfindexes >= MAX_BATCHED_SURFINDEXES)
		{
			iblock = D3DLOCK_DISCARD | D3DLOCK_NOSYSLOCK;
			r_firstsurfindex = 0;
		}

		d3d_SurfIndexes->Lock (
			r_firstsurfindex * sizeof (unsigned int),
			r_numsurfindexes * sizeof (unsigned int),
			(void **) &ndx,
			iblock
		);

		for (i = 0; i < r_numbatchedsurfaces; i++)
		{
			msurface_t *surf = r_batchedsurfaces[i];

			memcpy (ndx, surf->indexes, surf->numindexes * sizeof (unsigned int));
			ndx += surf->numindexes;

			if (surf->firstvertex < r_firstdipvert) r_firstdipvert = surf->firstvertex;
			if (surf->firstvertex + surf->numedges > r_lastdipvert) r_lastdipvert = surf->firstvertex + surf->numedges;
		}

		d3d_SurfIndexes->Unlock ();

		// this is an inclusive range so drop the last one
		d3d_Device->DrawIndexedPrimitive (D3DPT_TRIANGLELIST, 0, r_firstdipvert, r_lastdipvert - 1, r_firstsurfindex, r_numsurfindexes / 3);
		c_draw_calls++;
		r_firstsurfindex += r_numsurfindexes;
	}

	c_brush_polys += r_numbatchedsurfaces;
	r_numsurfindexes = 0;
	r_numbatchedsurfaces = 0;
}


void R_BatchSurface (msurface_t *surf)
{
	if (r_numbatchedsurfaces + 1 >= MAX_BATCHED_SURFACES) R_EndBatchingSurfaces ();
	if (r_numsurfindexes + surf->numindexes >= MAX_BATCHED_SURFINDEXES) R_EndBatchingSurfaces ();

	r_batchedsurfaces[r_numbatchedsurfaces] = surf;
	r_numbatchedsurfaces++;
	r_numsurfindexes += surf->numindexes;
}


static vec3_t	modelorg;		// relative to viewpoint

static msurface_t *r_alpha_surfaces;
static msurface_t *r_sky_surfaces;

int		c_visible_lightmaps;
int		c_visible_textures;

int maxverts = 0;

void R_RenderDynamicLightmaps (msurface_t *surf);


/*
=============================================================

	BRUSH MODELS

=============================================================
*/

/*
===============
R_TextureAnimation

Returns the proper texture for a given time and base texture
===============
*/
image_t *R_TextureAnimation (mtexinfo_t *tex)
{
	if (!tex->next || tex->numframes < 2)
		return tex->image;

	int c = currententity->currframe % tex->numframes;

	while (c)
	{
		// catch broken sequences (should never happen but just in case
		if (!tex->next) return tex->image;

		tex = tex->next;
		c--;
	}

	return tex->image;
}


void R_SetAlphaState (int flags)
{
	if (flags & SURF_TRANS33)
		d3d_State->SetPixelShaderConstant1f (0, 0.333333f);
	else if (flags & SURF_TRANS66)
		d3d_State->SetPixelShaderConstant1f (0, 0.666666f);
	else d3d_State->SetPixelShaderConstant1f (0, 1.0f);
}


/*
================
R_DrawAlphaSurfaces

Draw water surfaces and windows.
================
*/
void R_DrawAlphaSurfaces (void)
{
	if (!r_alpha_surfaces) return;

	image_t *lastimage = NULL;
	int lastimageflags = -1;

	// go back to the world matrix
	d3d_State->SetVertexShaderConstant4fv (8, d3d_state.WorldMatrix.m16, 4);

	d3d_EnableAlphaObjects->Apply ();
	R_BeginBatchingSurfaces ();

	for (msurface_t *surf = r_alpha_surfaces; surf; surf = surf->texturechain)
	{
		// it's an error if sky is ever included here
		if (surf->texinfo->flags & SURF_SKY) continue;

		// surely we could just change this on texinfo???  probably not because other properties could be different
		if ((surf->texinfo->flags != lastimageflags) || (surf->texinfo->image != lastimage))
		{
			R_EndBatchingSurfaces ();

			// done first because sky may override the texture
			d3d_State->SetTexture (0, &d3d_SurfDiffuseSampler, surf->texinfo->image->d3d_Texture);

			if (surf->texinfo->flags & SURF_WARP)
			{
				R_SetLiquidState ();
				R_SetSurfaceScroll (surf, d3d_state.warpscroll);
			}
			else
			{
				R_SetSolidState (surf->texinfo->flags);
				R_SetSurfaceScroll (surf, d3d_state.surfscroll);
			}

			// and finally the alpha state
			R_SetAlphaState (surf->texinfo->flags);

			lastimageflags = surf->texinfo->flags;
			lastimage = surf->texinfo->image;
		}

		R_BatchSurface (surf);
	}

	R_EndBatchingSurfaces ();
	d3d_DisableAlphaObjects->Apply ();
}


void R_SetSolidState (int flags)
{
	d3d_State->SetVertexDeclaration (d3d_SurfDecl);
	d3d_State->SetVertexShader (d3d_SurfVS);

	if ((flags & SURF_TRANS33) || (flags & SURF_TRANS66))
		d3d_State->SetPixelShader (d3d_AlphaPS);
	else d3d_State->SetPixelShader (d3d_SurfPS);

	d3d_State->SetPixelShaderConstant1f (1, gl_monolightmap->value);
}

void R_SetSurfaceScroll (msurface_t *surf, float scrollspeed)
{
	if (surf->texinfo->flags & SURF_FLOWING)
		d3d_State->SetVertexShaderConstant1f (13, scrollspeed);
	else d3d_State->SetVertexShaderConstant1f (13, 0);
}


void R_DrawSolidChain (msurface_t *surf, image_t *image)
{
	int LastLightmap = -1;
	int LastScroll = -1;

	R_SetSolidState (surf->texinfo->flags);

	if (gl_lightmap->value)
		d3d_State->SetTexture (0, &d3d_SurfDiffuseSampler, r_greytexture->d3d_Texture);
	else d3d_State->SetTexture (0, &d3d_SurfDiffuseSampler, image->d3d_Texture);

	// same drawing func for all surf types
	for (; surf; surf = surf->texturechain)
	{
		if (LastScroll != (surf->texinfo->flags & SURF_FLOWING))
		{
			R_EndBatchingSurfaces ();
			R_SetSurfaceScroll (surf, d3d_state.surfscroll);
			LastScroll = (surf->texinfo->flags & SURF_FLOWING);
		}

		if (LastLightmap != surf->LightmapTextureNum)
		{
			R_EndBatchingSurfaces ();
			d3d_State->SetTexture (1, &d3d_SurfLightmapSampler, Lightmaps[surf->LightmapTextureNum]->DefTexture);
			LastLightmap = surf->LightmapTextureNum;
		}

		R_BatchSurface (surf);
	}
}


void R_DrawTextureChains (model_t *mod)
{
	// bring all lightmaps up to date
	D3D_UpdateLightmaps ();

	// no alpha here (to do - alpha brush models)
	R_BeginBatchingSurfaces ();

	// note to self - it's done this way rather than using mod->texinfo because multiple texinfos can use
	// the same image, which would result in chains being drawn twice or more!!!!
	// splitting this off into 3 passes doesn't matter a damn performance wise - another myth busted.
	for (int i = 0; i < gl_textures.size (); i++)
	{
		image_t *image = gl_textures[i];

		if (!image->registration_sequence) continue;
		if (!image->d3d_Texture) continue;
		if (!image->texturechain) continue;

		msurface_t *surf = image->texturechain;

		if (surf->texinfo->flags & SURF_WARP)
			R_DrawLiquidChain (surf, image);
		else if (!(surf->texinfo->flags & SURF_SKY))
			R_DrawSolidChain (surf, image);

		R_EndBatchingSurfaces ();
	}

	// sky is drawn separately
	if (r_sky_surfaces)
	{
		R_DrawSkySurfaces (r_sky_surfaces);
		r_sky_surfaces = NULL;
	}
}


void R_TextureChainSurface (msurface_t *surf)
{
	if (surf->texinfo->flags & SURF_SKY)
	{
		surf->texturechain = r_sky_surfaces;
		r_sky_surfaces = surf;
	}
	else if (surf->texinfo->flags & (SURF_TRANS33 | SURF_TRANS66))
	{
		surf->texturechain = r_alpha_surfaces;
		r_alpha_surfaces = surf;
	}
	else
	{
		R_RenderDynamicLightmaps (surf);

		// get the animation here so that we can chain by the animated version rather than by the non-animated version
		image_t *image = R_TextureAnimation (surf->texinfo);

		// chain in f2b order
		*image->texturechain_tail = surf;
		image->texturechain_tail = &surf->texturechain;
		surf->texturechain = NULL;
	}
}


void R_ClearTextureChains (model_t *mod)
{
	for (int i = 0; i < gl_textures.size (); i++)
	{
		image_t *image = gl_textures[i];

		image->texturechain = NULL;
		image->texturechain_tail = &image->texturechain;
	}

	r_sky_surfaces = NULL;
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
	if (node->contents == CONTENTS_SOLID) return;
	if (node->visframe != r_visframecount) return;
	if (R_CullBox (node->minmaxs, node->minmaxs + 3)) return;

	// if a leaf node, draw stuff
	if (node->contents != -1)
	{
		mleaf_t *pleaf = (mleaf_t *) node;

		// check for door connected areas
		if (r_newrefdef.areabits)
		{
			if (!(r_newrefdef.areabits[pleaf->area >> 3] & (1 << (pleaf->area & 7))))
				return;
		}

		msurface_t **mark = pleaf->firstmarksurface;
		int c = pleaf->nummarksurfaces;

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
	float dot = Mod_PlaneDot (modelorg, node->plane);
	int side = (dot >= 0) ? 0 : 1;
	int sidebit = (dot >= 0) ? 0 : SURF_PLANEBACK;

	// recurse down the children, front side first
	R_RecursiveWorldNode (node->children[side]);

	// draw stuff
	if (node->numsurfaces)
	{
		msurface_t *surf = r_worldmodel->surfaces + node->firstsurface;

		for (int c = node->numsurfaces; c; c--, surf++)
		{
			if (surf->visframe != r_framecount) continue;
			if ((surf->flags & SURF_PLANEBACK) != sidebit) continue;

			// add it to the draw lists
			R_TextureChainSurface (surf);
		}
	}

	// recurse down the back side
	R_RecursiveWorldNode (node->children[!side]);
}


void R_DrawBrushModel (entity_t *e)
{
	vec3_t		mins, maxs;
	qboolean	rotated;

	if (e->model->firstmodelsurface == 0) return;
	if (e->model->nummodelsurfaces == 0) return;

	if (e->angles[0] || e->angles[1] || e->angles[2])
	{
		rotated = true;

		for (int i = 0; i < 3; i++)
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

	model_t *clmodel = e->model;
	D3DQMATRIX LocalMatrix (&d3d_state.WorldMatrix);

	LocalMatrix.Translate (e->currorigin);
	LocalMatrix.Rotate (e->angles[YAW], e->angles[PITCH], e->angles[ROLL]);

	d3d_State->SetVertexShaderConstant4fv (8, LocalMatrix.m16, 4);

	msurface_t *surf = &clmodel->surfaces[clmodel->firstmodelsurface];

	R_PushDlights (clmodel->nodes + clmodel->firstnode, e->currorigin);
	R_ClearTextureChains (clmodel);

	for (int i = 0; i < clmodel->nummodelsurfaces; i++, surf++)
	{
		// find which side of the node we are on
		float dot = Mod_PlaneDot (modelorg, surf->plane);

		if (((surf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) || (!(surf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
			R_TextureChainSurface (surf);
		}
	}

	// set proper translucent level (blend/etc was already set)
	if (e->flags & RF_TRANSLUCENT)
		d3d_State->SetPixelShaderConstant1f (0, 0.25f);
	else d3d_State->SetPixelShaderConstant1f (0, 1.0f);

	R_DrawTextureChains (clmodel);
}


/*
=============
R_DrawWorld
=============
*/
void R_DrawWorld (void)
{
	entity_t ent;

	if (!r_drawworld->value) return;
	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL) return;

	VectorCopy (r_newrefdef.vieworg, modelorg);

	// auto cycle the world frame for texture animation
	memset (&ent, 0, sizeof (ent));
	ent.currframe = (int) (r_newrefdef.time * 2);
	currententity = &ent;

	// clear the alpha and sky surface chains
	r_alpha_surfaces = NULL;
	r_sky_surfaces = NULL;

	// push the dlights for the world
	R_PushDlights (r_worldmodel->nodes, vec3_origin);

	// clear down texturechains from previous frame
	R_ClearTextureChains (r_worldmodel);

	// add the world to the draw list
	R_RecursiveWorldNode (r_worldmodel->nodes);

	// textures
	d3d_State->SetVertexShaderConstant4fv (8, d3d_state.WorldMatrix.m16, 4);

	// the world model never has alpha
	d3d_State->SetPixelShaderConstant1f (0, 1.0f);

	R_DrawTextureChains (r_worldmodel);
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
	byte	*fatvis = (byte *) Scratch_Alloc ();
	mnode_t	*node;
	int		i, c;
	mleaf_t	*leaf;
	int		cluster;

	if (r_oldviewcluster == r_viewcluster && r_oldviewcluster2 == r_viewcluster2 && !r_novis->value && r_viewcluster != -1)
		return;

	// development aid to let you run around and see exactly where
	// the pvs ends
	if (gl_lockpvs->value) return;

	r_visframecount++;
	r_oldviewcluster = r_viewcluster;
	r_oldviewcluster2 = r_viewcluster2;

	if (r_novis->value || r_viewcluster == -1 || !r_worldmodel->vis)
	{
		// mark everything
		for (i = 0; i < r_worldmodel->numleafs; i++) r_worldmodel->leafs[i].visframe = r_visframecount;
		for (i = 0; i < r_worldmodel->numnodes; i++) r_worldmodel->nodes[i].visframe = r_visframecount;

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

		if (cluster == -1) continue;

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



/*
================
D3D9_BuildPolygonFromSurface
================
*/
void D3D9_BuildPolygonFromSurface (model_t *mod, msurface_t *surf)
{
	float *vec;

	// reconstruct the polygon (this needs to go on hunk as we don't know the total verts at load time
	brushpolyvert_t *verts = (brushpolyvert_t *) Hunk_Alloc (surf->numedges * sizeof (brushpolyvert_t));
	surf->verts = verts;

	for (int i = 0; i < surf->numedges; i++, verts++)
	{
		int lindex = mod->surfedges[surf->firstedge + i];

		if (lindex > 0)
			vec = mod->vertexes[mod->edges[lindex].v[0]].position;
		else vec = mod->vertexes[mod->edges[-lindex].v[1]].position;

		VectorCopy (vec, verts->xyz);

		if (surf->texinfo->flags & SURF_WARP)
		{
			verts->st[0] = DotProduct (vec, surf->texinfo->vecs[0]) * 0.015625f;
			verts->st[1] = DotProduct (vec, surf->texinfo->vecs[1]) * 0.015625f;

			verts->lm[0] = DotProduct (vec, surf->texinfo->vecs[1]) * D3DX_PI / 64.0f;
			verts->lm[1] = DotProduct (vec, surf->texinfo->vecs[0]) * D3DX_PI / 64.0f;
		}
		else if (!(surf->texinfo->flags & SURF_SKY))
		{
			verts->st[0] = (DotProduct (vec, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3]) / surf->texinfo->image->width;
			verts->st[1] = (DotProduct (vec, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3]) / surf->texinfo->image->height;

			// lightmap texture coordinates
			// D3D9_CreateSurfaceLightmap MUST be called before this
			verts->lm[0] = DotProduct (vec, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3];
			verts->lm[0] -= surf->texturemins[0];
			verts->lm[0] += surf->LightRect.left * 16;
			verts->lm[0] += 8;
			verts->lm[0] /= LIGHTMAP_SIZE * 16; //surf->texinfo->texture->width;

			verts->lm[1] = DotProduct (vec, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3];
			verts->lm[1] -= surf->texturemins[1];
			verts->lm[1] += surf->LightRect.top * 16;
			verts->lm[1] += 8;
			verts->lm[1] /= LIGHTMAP_SIZE * 16; //surf->texinfo->texture->height;
		}
	}

	// and now build the indexes
	unsigned int *indexes = (unsigned int *) Hunk_Alloc (surf->numindexes * sizeof (unsigned int));
	surf->indexes = indexes;

	for (int i = 2; i < surf->numedges; i++, indexes += 3)
	{
		indexes[0] = surf->firstvertex;
		indexes[1] = surf->firstvertex + (i - 1);
		indexes[2] = surf->firstvertex + i;
	}
}


void D3D9_BuildSurfaceVertexBuffer (model_t *m)
{
	SAFE_RELEASE (d3d_SurfBuffer);

	d3d_Device->CreateVertexBuffer (
		m->numsurfaceverts * sizeof (brushpolyvert_t),
		D3DUSAGE_WRITEONLY,
		0,
		D3DPOOL_MANAGED,
		&d3d_SurfBuffer,
		NULL
	);

	brushpolyvert_t *verts = NULL;

	d3d_SurfBuffer->Lock (0, 0, (void **) &verts, 0);

	for (int i = 0; i < m->numsurfaces; i++)
		memcpy (&verts[m->surfaces[i].firstvertex], m->surfaces[i].verts, m->surfaces[i].numedges * sizeof (brushpolyvert_t));

	d3d_SurfBuffer->Unlock ();
	d3d_SurfBuffer->PreLoad ();
}

