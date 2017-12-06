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
// gl_mesh.c: triangle model functions

#include "d3d_local.h"

struct meshbuffer_t
{
	IDirect3DVertexBuffer9 *PositionNormal;
	IDirect3DVertexBuffer9 *TexCoords;
	IDirect3DIndexBuffer9 *Indexes;

	char name[MAX_PATH];
	int numindexes;
	int numtris;
	int numverts;
	int registration_sequence;
};


struct drawvertx_t
{
	float position[3];
	float normal[3];
};


typedef struct mdlst_s
{
	float texcoords[2];
} mdlst_t;


meshbuffer_t d3d_MeshBuffers[MAX_MODELS];

IDirect3DVertexDeclaration9 *d3d_MeshDecl = NULL;
IDirect3DVertexShader9 *d3d_MeshVS = NULL;
IDirect3DPixelShader9 *d3d_MeshPS = NULL;

IDirect3DVertexDeclaration9 *d3d_ShadowDecl = NULL;
IDirect3DVertexShader9 *d3d_ShadowVS = NULL;
IDirect3DPixelShader9 *d3d_ShadowPS = NULL;

IDirect3DVertexDeclaration9 *d3d_ShellDecl = NULL;
IDirect3DVertexShader9 *d3d_ShellVS = NULL;
IDirect3DPixelShader9 *d3d_ShellPS = NULL;


void Mesh_LoadObjects (void)
{
	D3DVERTEXELEMENT9 meshlayout[] =
	{
		VDECL (0, offsetof (drawvertx_t, position), D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 0),
		VDECL (0, offsetof (drawvertx_t, normal), D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_NORMAL, 0),
		VDECL (1, offsetof (drawvertx_t, position), D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 1),
		VDECL (1, offsetof (drawvertx_t, normal), D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_NORMAL, 1),
		VDECL (2, 0, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 0),
		D3DDECL_END ()
	};

	d3d_Device->CreateVertexDeclaration (meshlayout, &d3d_MeshDecl);

	d3d_MeshVS = D3D_CreateVertexShader ("hlsl/fxMesh.fx", "MeshVS");
	d3d_MeshPS = D3D_CreatePixelShader ("hlsl/fxMesh.fx", "MeshPS");

	D3DVERTEXELEMENT9 shadowlayout[] =
	{
		VDECL (0, offsetof (drawvertx_t, position), D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 0),
		VDECL (1, offsetof (drawvertx_t, position), D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 1),
		D3DDECL_END ()
	};

	d3d_Device->CreateVertexDeclaration (shadowlayout, &d3d_ShadowDecl);

	d3d_ShadowVS = D3D_CreateVertexShader ("hlsl/fxMesh.fx", "ShadowVS");
	d3d_ShadowPS = D3D_CreatePixelShader ("hlsl/fxMesh.fx", "ShadowPS");

	D3DVERTEXELEMENT9 shelllayout[] =
	{
		VDECL (0, offsetof (drawvertx_t, position), D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 0),
		VDECL (0, offsetof (drawvertx_t, normal), D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_NORMAL, 0),
		VDECL (1, offsetof (drawvertx_t, position), D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 1),
		VDECL (1, offsetof (drawvertx_t, normal), D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_NORMAL, 1),
		D3DDECL_END ()
	};

	d3d_Device->CreateVertexDeclaration (shelllayout, &d3d_ShellDecl);

	d3d_ShellVS = D3D_CreateVertexShader ("hlsl/fxMesh.fx", "ShellVS");
	d3d_ShellPS = D3D_CreatePixelShader ("hlsl/fxMesh.fx", "ShellPS");
}


void Mesh_Shutdown (void)
{
	for (int i = 0; i < MAX_MODELS; i++)
	{
		SAFE_RELEASE (d3d_MeshBuffers[i].PositionNormal);
		SAFE_RELEASE (d3d_MeshBuffers[i].TexCoords);
		SAFE_RELEASE (d3d_MeshBuffers[i].Indexes);

		memset (&d3d_MeshBuffers[i], 0, sizeof (meshbuffer_t));
	}

	SAFE_RELEASE (d3d_MeshVS);
	SAFE_RELEASE (d3d_MeshPS);
	SAFE_RELEASE (d3d_MeshDecl);

	SAFE_RELEASE (d3d_ShadowVS);
	SAFE_RELEASE (d3d_ShadowPS);
	SAFE_RELEASE (d3d_ShadowDecl);

	SAFE_RELEASE (d3d_ShellVS);
	SAFE_RELEASE (d3d_ShellPS);
	SAFE_RELEASE (d3d_ShellDecl);
}


CD3DHandler Mesh_Handler (
	Mesh_LoadObjects,
	Mesh_Shutdown
);


/*
=============================================================

  ALIAS MODELS

=============================================================
*/

float r_d3d9_avertexnormals[NUMVERTEXNORMALS][3] =
{
#include "anorms.h"
};


struct meshlerpstruct_t
{
	float move[3];
	float frontv[3];
	float backv[3];
};


void R_DrawAliasFrame (entity_t *e, dmdl_t *hdr, bool shadowpass)
{
	meshbuffer_t *bufferset = &d3d_MeshBuffers[e->model->bufferset];

	d3d_State->SetStreamSource (0, bufferset->PositionNormal, e->currframe * bufferset->numverts * sizeof (drawvertx_t), sizeof (drawvertx_t));
	d3d_State->SetStreamSource (1, bufferset->PositionNormal, e->lastframe * bufferset->numverts * sizeof (drawvertx_t), sizeof (drawvertx_t));
	d3d_State->SetIndices (bufferset->Indexes);

	if (shadowpass)
	{
		d3d_State->SetVertexDeclaration (d3d_ShadowDecl);
		d3d_State->SetVertexShader (d3d_ShadowVS);
		d3d_State->SetPixelShader (d3d_ShadowPS);
	}
	else if (!(e->flags & (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE | RF_SHELL_DOUBLE | RF_SHELL_HALF_DAM)))
	{
		d3d_State->SetStreamSource (2, bufferset->TexCoords, 0, sizeof (float) * 2);
		d3d_State->SetVertexDeclaration (d3d_MeshDecl);
		d3d_State->SetVertexShader (d3d_MeshVS);
		d3d_State->SetPixelShader (d3d_MeshPS);
	}
	else
	{
		d3d_State->SetVertexDeclaration (d3d_ShellDecl);
		d3d_State->SetVertexShader (d3d_ShellVS);
		d3d_State->SetPixelShader (d3d_ShellPS);
	}

	meshlerpstruct_t lerpstruct;

	d3d_State->SetVertexShaderConstant1f (12, e->backlerp);

	float frontlerp = 1.0 - e->backlerp;
	vec3_t delta, vectors[3];
	float movedot[3] = {1, -1, 1};

	daliasframe_t *frame = (daliasframe_t *) ((byte *) hdr + hdr->ofs_frames + e->currframe * hdr->framesize);
	daliasframe_t *oldframe = (daliasframe_t *) ((byte *) hdr + hdr->ofs_frames + e->lastframe * hdr->framesize);

	// move should be the delta back to the previous frame * backlerp
	Vector3Subtract (delta, e->lastorigin, e->currorigin);
	AngleVectors (e->angles, vectors[0], vectors[1], vectors[2]);

	for (int i = 0; i < 3; i++)
	{
		lerpstruct.move[i] = e->backlerp * (DotProduct (delta, vectors[i]) * movedot[i] + oldframe->translate[i]) + frontlerp * frame->translate[i];
		lerpstruct.frontv[i] = frontlerp * frame->scale[i];
		lerpstruct.backv[i] = e->backlerp * oldframe->scale[i];
	}

	d3d_State->SetVertexShaderConstant3fv (13, lerpstruct.move);
	d3d_State->SetVertexShaderConstant3fv (14, lerpstruct.frontv);
	d3d_State->SetVertexShaderConstant3fv (15, lerpstruct.backv);

	d3d_Device->DrawIndexedPrimitive (
		D3DPT_TRIANGLELIST,
		0,
		0,
		bufferset->numverts,
		0,
		bufferset->numtris
	);

	c_alias_polys += hdr->num_tris;
	c_draw_calls++;
}


static qboolean R_CullAliasModel (vec3_t bbox[8], entity_t *e)
{
	vec3_t mins, maxs;
	vec3_t vectors[3];
	vec3_t angles;

	dmdl_t *hdr = (dmdl_t *) e->model->extradata;

	if ((e->currframe >= hdr->num_frames) || (e->currframe < 0))
	{
		Com_Printf ("R_CullAliasModel %s: no such frame %d\n", e->model->name, e->currframe);
		e->currframe = 0;
	}

	if ((e->lastframe >= hdr->num_frames) || (e->lastframe < 0))
	{
		Com_Printf ("R_CullAliasModel %s: no such oldframe %d\n", e->model->name, e->lastframe);
		e->lastframe = 0;
	}

	daliasframe_t *pframe = (daliasframe_t *) ((byte *) hdr + hdr->ofs_frames + e->currframe * hdr->framesize);
	daliasframe_t *poldframe = (daliasframe_t *) ((byte *) hdr + hdr->ofs_frames + e->lastframe * hdr->framesize);

	// compute axially aligned mins and maxs
	if (pframe == poldframe)
	{
		for (int i = 0; i < 3; i++)
		{
			mins[i] = pframe->translate[i];
			maxs[i] = mins[i] + pframe->scale[i] * 255;
		}
	}
	else
	{
		for (int i = 0; i < 3; i++)
		{
			float thismins = pframe->translate[i];
			float thismaxs = thismins + pframe->scale[i] * 255;

			float oldmins = poldframe->translate[i];
			float oldmaxs = oldmins + poldframe->scale[i] * 255;

			if (thismins < oldmins)
				mins[i] = thismins;
			else mins[i] = oldmins;

			if (thismaxs > oldmaxs)
				maxs[i] = thismaxs;
			else maxs[i] = oldmaxs;
		}
	}

	// compute a full bounding box
	for (int i = 0; i < 8; i++)
	{
		vec3_t  tmp;

		if (i & 1) tmp[0] = mins[0]; else tmp[0] = maxs[0];
		if (i & 2) tmp[1] = mins[1]; else tmp[1] = maxs[1];
		if (i & 4) tmp[2] = mins[2]; else tmp[2] = maxs[2];

		Vector3Copy (bbox[i], tmp);
	}

	// rotate the bounding box
	Vector3Copy (angles, e->angles);
	angles[YAW] = -angles[YAW];
	AngleVectors (angles, vectors[0], vectors[1], vectors[2]);

	for (int i = 0; i < 8; i++)
	{
		vec3_t tmp;

		Vector3Copy (tmp, bbox[i]);

		bbox[i][0] = DotProduct (vectors[0], tmp);
		bbox[i][1] = -DotProduct (vectors[1], tmp);
		bbox[i][2] = DotProduct (vectors[2], tmp);

		Vector3Add (bbox[i], bbox[i], e->currorigin);
	}

	int aggregatemask = ~0;

	for (int p = 0; p < 8; p++)
	{
		int mask = 0;

		for (int f = 0; f < 4; f++)
		{
			float dp = DotProduct (d3d_state.frustum[f].normal, bbox[p]);

			if ((dp - d3d_state.frustum[f].dist) < 0)
			{
				mask |= (1 << f);
			}
		}

		aggregatemask &= mask;
	}

	if (aggregatemask)
		return true;
	else return false;
}


void R_LightAliasModel (entity_t *e, dmdl_t *hdr, float *lightspot)
{
	shadeinfo_t shade;

	if (e->flags & (RF_SHELL_HALF_DAM | RF_SHELL_GREEN | RF_SHELL_RED | RF_SHELL_BLUE | RF_SHELL_DOUBLE))
	{
		VectorClear (shade.light);

		if (e->flags & RF_SHELL_HALF_DAM) Vector3Set (shade.light, 0.56f, 0.59f, 0.45f);
		if (e->flags & RF_SHELL_DOUBLE) Vector2Set (shade.light, 0.9f, 0.7f);
		if (e->flags & RF_SHELL_RED) shade.light[0] = 1.0f;
		if (e->flags & RF_SHELL_GREEN) shade.light[1] = 1.0f;
		if (e->flags & RF_SHELL_BLUE) shade.light[2] = 1.0f;
	}
	else if (e->flags & RF_FULLBRIGHT)
		Vector3Set (shade.light, 1.0f, 1.0f, 1.0f);
	else
	{
		R_LightPoint (e->currorigin, shade.light, lightspot);

		// player lighting hack for communication back to server - big hack!
		if (e->flags & RF_WEAPONMODEL)
		{
			// pick the greatest component, which should be the same as the mono value returned by software
			r_lightlevel->value = shade.light[0];

			if (shade.light[1] > r_lightlevel->value) r_lightlevel->value = shade.light[1];
			if (shade.light[2] > r_lightlevel->value) r_lightlevel->value = shade.light[2];
		}
	}

	// bonus items will pulse with time
	if (e->flags & RF_GLOW)
		Vector3Scale (shade.light, shade.light, 1.0f + 0.25f * sin (r_newrefdef.time * 7.0f));

	// bonus items need minlight too (for the new glow formula)
	if ((e->flags & RF_MINLIGHT) || (e->flags & RF_GLOW))
	{
		Vector3Add (shade.light, shade.light, 0.1f);
		Vector3Scale (shade.light, shade.light, 1.0f / 1.1f);
	}

	// PGM	ir goggles color override
	if ((r_newrefdef.rdflags & RDF_IRGOGGLES) && (e->flags & RF_IR_VISIBLE))
		Vector3Set (shade.light, 1.0f, 0.0f, 0.0f);

	float an = (e->angles[0] + e->angles[1]) / 180.0f * D3DX_PI;

	Vector3Set (shade.vector, cos (-an), sin (-an), 1.0f);
	Vector3Normalize (shade.vector);

	if (e->flags & RF_TRANSLUCENT)
		shade.light[3] = e->alpha;
	else shade.light[3] = 1.0f;

	d3d_State->SetPixelShaderConstant4fv (0, shade.light);
	d3d_State->SetPixelShaderConstant3fv (1, shade.vector);
}


/*
=================
R_DrawAliasModel

=================
*/
void R_SelectAliasTexture (entity_t *e, dmdl_t *hdr, image_t **skins)
{
	image_t *skin = NULL;

	// select skin
	if (e->skin)
		skin = e->skin;	// custom player skin
	else
	{
		if (e->skinnum >= MAX_MD2SKINS)
			skin = skins[0];
		else
		{
			skin = skins[e->skinnum];

			if (!skin)
				skin = skins[0];
		}
	}

	// fallback...
	if (!skin) skin = r_notexture;

	if (gl_lightmap->value)
		d3d_State->SetTexture (0, &d3d_MeshSampler, r_greytexture->d3d_Texture);
	else d3d_State->SetTexture (0, &d3d_MeshSampler, skin->d3d_Texture);
}


#define SHADOW_SKEW_X -0.7f		// skew along x axis. -0.7 to mimic glquake shadows
#define SHADOW_SKEW_Y 0			// skew along y axis. 0 to mimic glquake shadows
#define SHADOW_VSCALE 0			// 0 = completely flat
#define SHADOW_HEIGHT 0.1f		// how far above the floor to render the shadow


void R_DrawAliasModel (entity_t *e)
{
	dmdl_t *hdr = (dmdl_t *) e->model->extradata;
	vec3_t bbox[8];
	float lightspot[3];

	if (!(e->flags & RF_WEAPONMODEL))
		if (R_CullAliasModel (bbox, e))
			return;

	if (e->flags & RF_WEAPONMODEL)
		if (r_lefthand->value == 2)
			return;

	if ((e->currframe >= hdr->num_frames) || (e->currframe < 0))
	{
		Com_Printf ("R_DrawAliasModel %s: no such frame %d\n", e->model->name, e->currframe);
		e->currframe = 0;
		e->lastframe = 0;
	}

	if ((e->lastframe >= hdr->num_frames) || (e->lastframe < 0))
	{
		Com_Printf ("R_DrawAliasModel %s: no such oldframe %d\n", e->model->name, e->lastframe);
		e->currframe = 0;
		e->lastframe = 0;
	}

	if (!r_lerpmodels->value) e->backlerp = 0;

	D3DQMATRIX LocalMatrix;

	// this only happens once per frame so it doesn't need state filtering
	if ((e->flags & RF_WEAPONMODEL) && r_lefthand->value)
	{
		LocalMatrix.Scale (-1, 1, 1);
		d3d_Device->SetRenderState (D3DRS_CULLMODE, D3DCULL_CW);
	}

	// run the scale first which is the same as using a depth hack in our shader
	if (e->flags & RF_DEPTHHACK)
		LocalMatrix.Scale (1.0f, 1.0f, 0.3f);

	LocalMatrix.Mult (&d3d_state.WorldMatrix);
	LocalMatrix.Translate (e->currorigin);
	LocalMatrix.Rotate (e->angles[YAW], e->angles[PITCH], -e->angles[ROLL]);

	d3d_State->SetVertexShaderConstant4fv (8, LocalMatrix.m16, 4);

	R_SelectAliasTexture (e, hdr, e->model->skins);
	R_LightAliasModel (e, hdr, lightspot);
	R_DrawAliasFrame (e, hdr, false);

	// this only happens once per frame so it doesn't need state filtering
	if ((e->flags & RF_WEAPONMODEL) && r_lefthand->value)
		d3d_Device->SetRenderState (D3DRS_CULLMODE, D3DCULL_CCW);

	// no shadows on these models
	if (e->flags & RF_WEAPONMODEL) return;
	if (e->flags & RF_TRANSLUCENT) return;
	if (e->flags & RF_NOSHADOW) return;

	// not drawing shadows
	if (!gl_shadows->value) return;

	// recreate the local matrix for the model, this time appropriately for shadows
	LocalMatrix.LoadIdentity ();

	// run the scale first which is the same as using a depth hack in our shader
	if (e->flags & RF_DEPTHHACK)
		LocalMatrix.Scale (1.0f, 1.0f, 0.3f);

	float lheight = e->currorigin[2] - lightspot[2];

	LocalMatrix.Mult (&d3d_state.WorldMatrix);
	LocalMatrix.Translate (e->currorigin);
	LocalMatrix.Translate (0, 0, -lheight);

	LocalMatrix.Mult (
		1,				0,				0,				0,
		0,				1,				0,				0,
		SHADOW_SKEW_X,	SHADOW_SKEW_Y,	SHADOW_VSCALE,	0,
		0,				0,				SHADOW_HEIGHT,	1
	);

	LocalMatrix.Translate (0, 0, lheight);
	LocalMatrix.Rotate (e->angles[YAW], e->angles[PITCH], -e->angles[ROLL]);

	d3d_State->SetVertexShaderConstant4fv (8, LocalMatrix.m16, 4);

	// co-opt register 0 as the shadow color
	d3d_State->SetPixelShaderConstant4f (0, 0, 0, 0, gl_shadows->value);

	// shadow pass
	d3d_EnableShadows->Apply ();
	R_DrawAliasFrame (e, hdr, true);
	d3d_DisableShadows->Apply ();
}


void RMesh_CreateFrames (model_t *mod, dmdl_t *hdr, meshbuffer_t *bufferset)
{
	drawvertx_t *verts = (drawvertx_t *) Scratch_Alloc ();
	int numverts = 0;

	for (int frame = 0; frame < hdr->num_frames; frame++)
	{
		int *order = (int *) ((byte *) hdr + hdr->ofs_glcmds);
		dtrivertx_t *vert = ((daliasframe_t *) ((byte *) hdr + hdr->ofs_frames + frame * hdr->framesize))->verts;

		for (;;)
		{
			// get the vertex count and primitive type
			int count = *order++;

			if (!count) break;
			if (count < 0) count = -count;

			for (int i = 0; i < count; i++, verts++, order += 3)
			{
				verts->position[0] = vert[order[2]].v[0];
				verts->position[1] = vert[order[2]].v[1];
				verts->position[2] = vert[order[2]].v[2];

				verts->normal[0] = r_d3d9_avertexnormals[vert[order[2]].lightnormalindex][0];
				verts->normal[1] = r_d3d9_avertexnormals[vert[order[2]].lightnormalindex][1];
				verts->normal[2] = r_d3d9_avertexnormals[vert[order[2]].lightnormalindex][2];
			}

			numverts += count;
		}
	}

	d3d_Device->CreateVertexBuffer (
		numverts * sizeof (drawvertx_t),
		D3DUSAGE_WRITEONLY,
		0,
		D3DPOOL_MANAGED,
		&bufferset->PositionNormal,
		NULL
	);

	bufferset->PositionNormal->Lock (0, 0, (void **) &verts, 0);
	memcpy (verts, Scratch_Alloc (), numverts * sizeof (drawvertx_t));
	bufferset->PositionNormal->Unlock ();
	bufferset->PositionNormal->PreLoad ();
}


void RMesh_CreateTexCoords (model_t *mod, dmdl_t *hdr, meshbuffer_t *bufferset)
{
	mdlst_t *st = (mdlst_t *) Scratch_Alloc ();
	int *order = (int *) ((byte *) hdr + hdr->ofs_glcmds);

	bufferset->numverts = 0;

	for (;;)
	{
		// get the vertex count and primitive type
		int count = *order++;

		if (!count) break;
		if (count < 0) count = -count;

		for (int i = 0; i < count; i++, st++, order += 3)
		{
			st->texcoords[0] = ((float *) order)[0];
			st->texcoords[1] = ((float *) order)[1];
		}

		bufferset->numverts += count;
	}

	d3d_Device->CreateVertexBuffer (
		bufferset->numverts * sizeof (mdlst_t),
		D3DUSAGE_WRITEONLY,
		0,
		D3DPOOL_MANAGED,
		&bufferset->TexCoords,
		NULL
	);

	bufferset->TexCoords->Lock (0, 0, (void **) &st, 0);
	memcpy (st, Scratch_Alloc (), bufferset->numverts * sizeof (mdlst_t));
	bufferset->TexCoords->Unlock ();
	bufferset->TexCoords->PreLoad ();
}


void RMesh_CreateIndexes (model_t *mod, dmdl_t *hdr, meshbuffer_t *bufferset)
{
	unsigned short *ndx = (unsigned short *) Scratch_Alloc ();
	int *order = (int *) ((byte *) hdr + hdr->ofs_glcmds);
	int i, firstvertex = 0;

	bufferset->numindexes = 0;

	for (;;)
	{
		// get the vertex count and primitive type
		int count = *order++;
		if (!count) break;

		if (count < 0)
		{
			for (i = 2, count = -count; i < count; i++, ndx += 3)
			{
				ndx[0] = firstvertex + 0;
				ndx[1] = firstvertex + (i - 1);
				ndx[2] = firstvertex + i;
			}
		}
		else
		{
			for (i = 2; i < count; i++, ndx += 3)
			{
				ndx[0] = firstvertex + i - 2;
				ndx[1] = firstvertex + ((i & 1) ? i : (i - 1));
				ndx[2] = firstvertex + ((i & 1) ? (i - 1) : i);
			}
		}

		order += count * 3;
		firstvertex += count;
		bufferset->numindexes += (count - 2) * 3;
	}

	d3d_Device->CreateIndexBuffer (
		bufferset->numindexes * sizeof (unsigned short),
		D3DUSAGE_WRITEONLY,
		D3DFMT_INDEX16,
		D3DPOOL_MANAGED,
		&bufferset->Indexes,
		NULL
	);

	bufferset->numtris = bufferset->numindexes / 3;

	bufferset->Indexes->Lock (0, 0, (void **) &ndx, 0);
	memcpy (ndx, Scratch_Alloc (), bufferset->numindexes * sizeof (unsigned short));
	bufferset->Indexes->Unlock ();
	bufferset->Indexes->PreLoad ();
}


void R_CreateAliasBuffers (model_t *mod, dmdl_t *hdr)
{
	meshbuffer_t *freeslot = NULL;

	// try to find a buffer with the same model
	for (int i = 0; i < MAX_MODELS; i++)
	{
		// find the first free buffer for potential use
		if (!freeslot && !d3d_MeshBuffers[i].name[0])
		{
			freeslot = &d3d_MeshBuffers[i];
			mod->bufferset = i;
			continue;
		}

		if (!strcmp (d3d_MeshBuffers[i].name, mod->name))
		{
			d3d_MeshBuffers[i].registration_sequence = registration_sequence;
			mod->bufferset = i;
			return;
		}
	}

	// out of buffers!!!
	if (!freeslot)
		Com_Error (ERR_FATAL, "too many mesh buffers");

	// create a new buffer set
	RMesh_CreateFrames (mod, hdr, freeslot);
	RMesh_CreateTexCoords (mod, hdr, freeslot);
	RMesh_CreateIndexes (mod, hdr, freeslot);

	// copy this off so that we know to use it
	strcpy (freeslot->name, mod->name);
	freeslot->registration_sequence = registration_sequence;
}


void R_FreeUnusedMeshBuffers (void)
{
	for (int i = 0; i < MAX_MODELS; i++)
	{
		if (d3d_MeshBuffers[i].registration_sequence == registration_sequence) continue;

		SAFE_RELEASE (d3d_MeshBuffers[i].PositionNormal);
		SAFE_RELEASE (d3d_MeshBuffers[i].TexCoords);
		SAFE_RELEASE (d3d_MeshBuffers[i].Indexes);

		memset (&d3d_MeshBuffers[i], 0, sizeof (meshbuffer_t));
	}
}

