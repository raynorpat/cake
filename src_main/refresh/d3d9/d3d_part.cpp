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

#define MAX_PART_INSTANCES		65536

int r_firstparticle = 0;
cvar_t *gl_particle_size = NULL;

IDirect3DVertexShader9 *d3d_PartVS = NULL;
IDirect3DPixelShader9 *d3d_PartPS = NULL;
IDirect3DVertexDeclaration9 *d3d_PartDecl = NULL;
IDirect3DVertexBuffer9 *d3d_PartPerInstance = NULL;
IDirect3DVertexBuffer9 *d3d_PartPerVertex = NULL;
IDirect3DIndexBuffer9 *d3d_PartIndexes = NULL;

void Part_LoadObjects (void)
{
	// consistency with original
	if (!gl_particle_size) gl_particle_size = Cvar_Get ("gl_particle_size", "40", CVAR_ARCHIVE);

	// fix V_TestParticles for this...
	D3DVERTEXELEMENT9 decllayout[] =
	{
		VDECL (1, offsetof (particle_t, origin), D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 0),
//		VDECL (1, offsetof (particle_t, vel), D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_TEXCOORD, 0),
//		VDECL (1, offsetof (particle_t, accel), D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_TEXCOORD, 1),
//		VDECL (1, offsetof (particle_t, time), D3DDECLTYPE_FLOAT1, D3DDECLUSAGE_TEXCOORD, 2),
		VDECL (1, offsetof (particle_t, color), D3DDECLTYPE_D3DCOLOR, D3DDECLUSAGE_COLOR, 0),
		VDECL (1, offsetof (particle_t, alpha), D3DDECLTYPE_FLOAT1, D3DDECLUSAGE_TEXCOORD, 3),
		VDECL (0, 0, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 4),
		D3DDECL_END ()
	};

	d3d_Device->CreateVertexDeclaration (decllayout, &d3d_PartDecl);

	unsigned short partindexes[] = {0, 1, 2, 3};
	float partpervert[] = {-1, -1, -1, 1, 1, -1, 1, 1};
	void *partbufferdata = NULL;

	d3d_Device->CreateVertexBuffer (
		sizeof (partpervert),
		D3DUSAGE_WRITEONLY,
		0,
		D3DPOOL_MANAGED,
		&d3d_PartPerVertex,
		NULL
	);

	d3d_PartPerVertex->Lock (0, 0, (void **) &partbufferdata, 0);
	memcpy (partbufferdata, partpervert, sizeof (partpervert));
	d3d_PartPerVertex->Unlock ();
	d3d_PartPerVertex->PreLoad ();

	d3d_Device->CreateIndexBuffer (
		sizeof (partindexes),
		D3DUSAGE_WRITEONLY,
		D3DFMT_INDEX16,
		D3DPOOL_MANAGED,
		&d3d_PartIndexes,
		NULL
	);

	d3d_PartIndexes->Lock (0, 0, (void **) &partbufferdata, 0);
	memcpy (partbufferdata, partindexes, sizeof (partindexes));
	d3d_PartIndexes->Unlock ();
	d3d_PartIndexes->PreLoad ();

	d3d_PartVS = D3D_CreateVertexShader ("hlsl/fxPart.fx", "PartVS");
	d3d_PartPS = D3D_CreatePixelShader ("hlsl/fxPart.fx", "PartPS");
}


void Part_Shutdown (void)
{
	SAFE_RELEASE (d3d_PartPerVertex);
	SAFE_RELEASE (d3d_PartIndexes);
	SAFE_RELEASE (d3d_PartDecl);
	SAFE_RELEASE (d3d_PartVS);
	SAFE_RELEASE (d3d_PartPS);
}


void Part_ResetDevice (void)
{
	d3d_Device->CreateVertexBuffer (
		MAX_PART_INSTANCES * sizeof (particle_t),
		D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC,
		0,
		D3DPOOL_DEFAULT,
		&d3d_PartPerInstance,
		NULL
	);

	r_firstparticle = 0;
}


void Part_LostDevice (void)
{
	SAFE_RELEASE (d3d_PartPerInstance);
}


CD3DHandler Part_Handler (
	Part_LoadObjects,
	Part_Shutdown,
	Part_LostDevice,
	Part_ResetDevice
);


void R_DrawParticles (void)
{
	if (!r_newrefdef.num_particles) return;

	DWORD vblock = D3DLOCK_NOOVERWRITE | D3DLOCK_NOSYSLOCK;
	particle_t *particles = NULL;
	vec3_t up, right;

	// for consistency with the original engine we scale the original default of 40 down to our size of 0.5
	VectorScale (d3d_state.vup, gl_particle_size->value * 0.0125f, up);
	VectorScale (d3d_state.vright, gl_particle_size->value * 0.0125f, right);

	if (r_firstparticle + r_newrefdef.num_particles >= MAX_PART_INSTANCES)
	{
		vblock = D3DLOCK_DISCARD | D3DLOCK_NOSYSLOCK;
		r_firstparticle = 0;
	}

	d3d_PartPerInstance->Lock (
		r_firstparticle * sizeof (particle_t),
		r_newrefdef.num_particles * sizeof (particle_t),
		(void **) &particles,
		vblock
	);

	memcpy (particles, r_newrefdef.particles, r_newrefdef.num_particles * sizeof (particle_t));
	d3d_PartPerInstance->Unlock ();

	d3d_EnableAlphaObjects->Apply ();

	d3d_State->SetVertexShader (d3d_PartVS);
	d3d_State->SetPixelShader (d3d_PartPS);
	d3d_State->SetVertexDeclaration (d3d_PartDecl);

	d3d_State->SetStreamSource (0, d3d_PartPerVertex, 0, sizeof (float) * 2);
	d3d_State->SetStreamSource (1, d3d_PartPerInstance, r_firstparticle * sizeof (particle_t), sizeof (particle_t));

	d3d_State->SetStreamSourceFreq (0, D3DSTREAMSOURCE_INDEXEDDATA | r_newrefdef.num_particles);
	d3d_State->SetStreamSourceFreq (1, D3DSTREAMSOURCE_INSTANCEDATA | 1);

	d3d_State->SetIndices (d3d_PartIndexes);

	d3d_State->SetVertexShaderConstant3fv (8, up);
	d3d_State->SetVertexShaderConstant3fv (9, right);

	d3d_Device->DrawIndexedPrimitive (D3DPT_TRIANGLESTRIP, 0, 0, 4, 0, 2);
	c_draw_calls++;

	// go back to uninstanced
	d3d_State->SetStreamSourceFreq (0, 1);
	d3d_State->SetStreamSourceFreq (1, 1);

	d3d_DisableAlphaObjects->Apply ();

	r_firstparticle += r_newrefdef.num_particles;
}

