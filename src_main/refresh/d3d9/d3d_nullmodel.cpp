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

#define NUM_NULL_VERTS		12

// these are the null model verts dumped to file
float nullverts[NUM_NULL_VERTS * 3] =
{
	0.000000, 0.000000, -16.000000, 16.000000, 0.000000, 0.000000, 0.000000, 16.000000, 0.000000, -16.000000, 0.000000, 0.000000, -0.000000, -16.000000, 0.000000, 16.000000,
	-0.000000, 0.000000, 0.000000, 0.000000, 16.000000, 16.000000, -0.000000, 0.000000, -0.000000, -16.000000, 0.000000, -16.000000, 0.000000, 0.000000, 0.000000, 16.000000,
	0.000000, 16.000000, 0.000000, 0.000000, 
};


IDirect3DVertexShader9 *d3d_NullVS = NULL;
IDirect3DPixelShader9 *d3d_NullPS = NULL;
IDirect3DVertexDeclaration9 *d3d_NullDecl = NULL;
IDirect3DVertexBuffer9 *d3d_NullBuffer = NULL;


void Null_LoadObjects (void)
{
	float *verts = NULL;

	d3d_Device->CreateVertexBuffer (
		sizeof (nullverts),
		D3DUSAGE_WRITEONLY,
		0,
		D3DPOOL_MANAGED,
		&d3d_NullBuffer,
		NULL
	);

	d3d_NullBuffer->Lock (0, 0, (void **) &verts, 0);
	memcpy (verts, nullverts, sizeof (nullverts));
	d3d_NullBuffer->Unlock ();
	d3d_NullBuffer->PreLoad ();

	D3DVERTEXELEMENT9 decllayout[] =
	{
		VDECL (0, 0, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 0),
		D3DDECL_END ()
	};

	d3d_Device->CreateVertexDeclaration (decllayout, &d3d_NullDecl);

	d3d_NullVS = D3D_CreateVertexShader ("hlsl/fxNull.fx", "NullVS");
	d3d_NullPS = D3D_CreatePixelShader ("hlsl/fxNull.fx", "NullPS");
}


void Null_Shutdown (void)
{
	SAFE_RELEASE (d3d_NullBuffer);
	SAFE_RELEASE (d3d_NullDecl);
	SAFE_RELEASE (d3d_NullVS);
	SAFE_RELEASE (d3d_NullPS);
}


CD3DHandler Null_Handler (
	Null_LoadObjects,
	Null_Shutdown
);


void R_DrawNullModel (entity_t *e)
{
	shadeinfo_t shade;
	float lightspot[3];

	if (e->flags & RF_FULLBRIGHT)
		shade.light[0] = shade.light[1] = shade.light[2] = 1.0f;
	else R_LightPoint (e->currorigin, shade.light, lightspot);

	shade.light[3] = (e->flags & RF_TRANSLUCENT) ? e->alpha : 1.0f;

	D3DQMATRIX LocalMatrix (&d3d_state.WorldMatrix);

	LocalMatrix.Translate (e->currorigin);
	LocalMatrix.Rotate (e->angles[YAW], -e->angles[PITCH], -e->angles[ROLL]);

	float an = (e->angles[0] + e->angles[1]) / 180 * D3DX_PI;

	shade.vector[0] = cos (-an);
	shade.vector[1] = sin (-an);
	shade.vector[2] = 1;

	Vector3Normalize (shade.vector);

	d3d_State->SetVertexShaderConstant4fv (8, LocalMatrix.m16, 4);
	d3d_State->SetPixelShaderConstant4fv (0, shade.light);
	d3d_State->SetPixelShaderConstant3fv (1, shade.vector);

	d3d_State->SetVertexShader (d3d_NullVS);
	d3d_State->SetPixelShader (d3d_NullPS);
	d3d_State->SetVertexDeclaration (d3d_NullDecl);

	d3d_State->SetStreamSource (0, d3d_NullBuffer, 0, sizeof (float) * 3);

	d3d_Device->DrawPrimitive (D3DPT_TRIANGLEFAN, 0, (NUM_NULL_VERTS >> 1) - 2);
	d3d_Device->DrawPrimitive (D3DPT_TRIANGLEFAN, 6, (NUM_NULL_VERTS >> 1) - 2);

	c_draw_calls += 2;
}


