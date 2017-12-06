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

// should these have normals and be lit?
#define NUM_CYLINDER_VERTS	152

// this is a dump from a radius 1, 75 slices, 1 stacks gluCylinder call and it's rotated, positioned and scaled on the GPU
// per http://www.opengl.org/documentation/specs/version1.1/glspec1.1/node17.html a quad strip and a triangle strip have the same layout and winding
float cylinderverts[NUM_CYLINDER_VERTS * 3] =
{
	0.000000f, 1.000000f, 1.000000f, 0.000000f, 1.000000f, 0.000000f, 0.083678f, 0.996493f, 1.000000f, 0.083678f, 0.996493f, 0.000000f, 0.166769f, 0.985996f, 1.000000f, 0.166769f, 0.985996f, 0.000000f,
	0.248690f, 0.968583f, 1.000000f, 0.248690f, 0.968583f, 0.000000f, 0.328867f, 0.944376f, 1.000000f, 0.328867f, 0.944376f, 0.000000f, 0.406737f, 0.913545f, 1.000000f, 0.406737f, 0.913545f, 0.000000f,
	0.481754f, 0.876307f, 1.000000f, 0.481754f, 0.876307f, 0.000000f, 0.553392f, 0.832921f, 1.000000f, 0.553392f, 0.832921f, 0.000000f, 0.621148f, 0.783693f, 1.000000f, 0.621148f, 0.783693f, 0.000000f,
	0.684547f, 0.728969f, 1.000000f, 0.684547f, 0.728969f, 0.000000f, 0.743145f, 0.669131f, 1.000000f, 0.743145f, 0.669131f, 0.000000f, 0.796530f, 0.604599f, 1.000000f, 0.796530f, 0.604599f, 0.000000f,
	0.844328f, 0.535827f, 1.000000f, 0.844328f, 0.535827f, 0.000000f, 0.886204f, 0.463296f, 1.000000f, 0.886204f, 0.463296f, 0.000000f, 0.921863f, 0.387516f, 1.000000f, 0.921863f, 0.387516f, 0.000000f,
	0.951057f, 0.309017f, 1.000000f, 0.951057f, 0.309017f, 0.000000f, 0.973579f, 0.228351f, 1.000000f, 0.973579f, 0.228351f, 0.000000f, 0.989272f, 0.146083f, 1.000000f, 0.989272f, 0.146083f, 0.000000f,
	0.998027f, 0.062790f, 1.000000f, 0.998027f, 0.062790f, 0.000000f, 0.999781f, -0.020942f, 1.000000f, 0.999781f, -0.020942f, 0.000000f, 0.994522f, -0.104529f, 1.000000f, 0.994522f, -0.104529f,
	0.000000f, 0.982287f, -0.187381f, 1.000000f, 0.982287f, -0.187381f, 0.000000f, 0.963163f, -0.268920f, 1.000000f, 0.963163f, -0.268920f, 0.000000f, 0.937282f, -0.348572f, 1.000000f, 0.937282f,
	-0.348572f, 0.000000f, 0.904827f, -0.425779f, 1.000000f, 0.904827f, -0.425779f, 0.000000f, 0.866025f, -0.500000f, 1.000000f, 0.866025f, -0.500000f, 0.000000f, 0.821149f, -0.570714f, 1.000000f,
	0.821149f, -0.570714f, 0.000000f, 0.770513f, -0.637424f, 1.000000f, 0.770513f, -0.637424f, 0.000000f, 0.714473f, -0.699663f, 1.000000f, 0.714473f, -0.699663f, 0.000000f, 0.653421f, -0.756995f,
	1.000000f, 0.653421f, -0.756995f, 0.000000f, 0.587785f, -0.809017f, 1.000000f, 0.587785f, -0.809017f, 0.000000f, 0.518027f, -0.855364f, 1.000000f, 0.518027f, -0.855364f, 0.000000f, 0.444635f,
	-0.895712f, 1.000000f, 0.444635f, -0.895712f, 0.000000f, 0.368125f, -0.929776f, 1.000000f, 0.368125f, -0.929776f, 0.000000f, 0.289032f, -0.957319f, 1.000000f, 0.289032f, -0.957319f, 0.000000f,
	0.207912f, -0.978148f, 1.000000f, 0.207912f, -0.978148f, 0.000000f, 0.125333f, -0.992115f, 1.000000f, 0.125333f, -0.992115f, 0.000000f, 0.041876f, -0.999123f, 1.000000f, 0.041876f, -0.999123f,
	0.000000f, -0.041876f, -0.999123f, 1.000000f, -0.041876f, -0.999123f, 0.000000f, -0.125333f, -0.992115f, 1.000000f, -0.125333f, -0.992115f, 0.000000f, -0.207912f, -0.978148f, 1.000000f, -0.207912f,
	-0.978148f, 0.000000f, -0.289032f, -0.957319f, 1.000000f, -0.289032f, -0.957319f, 0.000000f, -0.368125f, -0.929776f, 1.000000f, -0.368125f, -0.929776f, 0.000000f, -0.444635f, -0.895712f, 1.000000f,
	-0.444635f, -0.895712f, 0.000000f, -0.518027f, -0.855364f, 1.000000f, -0.518027f, -0.855364f, 0.000000f, -0.587785f, -0.809017f, 1.000000f, -0.587785f, -0.809017f, 0.000000f, -0.653421f, -0.756995f,
	1.000000f, -0.653421f, -0.756995f, 0.000000f, -0.714473f, -0.699663f, 1.000000f, -0.714473f, -0.699663f, 0.000000f, -0.770513f, -0.637424f, 1.000000f, -0.770513f, -0.637424f, 0.000000f, -0.821149f,
	-0.570714f, 1.000000f, -0.821149f, -0.570714f, 0.000000f, -0.866025f, -0.500000f, 1.000000f, -0.866025f, -0.500000f, 0.000000f, -0.904827f, -0.425779f, 1.000000f, -0.904827f, -0.425779f, 0.000000f,
	-0.937282f, -0.348572f, 1.000000f, -0.937282f, -0.348572f, 0.000000f, -0.963163f, -0.268920f, 1.000000f, -0.963163f, -0.268920f, 0.000000f, -0.982287f, -0.187381f, 1.000000f, -0.982287f, -0.187381f,
	0.000000f, -0.994522f, -0.104528f, 1.000000f, -0.994522f, -0.104528f, 0.000000f, -0.999781f, -0.020943f, 1.000000f, -0.999781f, -0.020943f, 0.000000f, -0.998027f, 0.062791f, 1.000000f, -0.998027f,
	0.062791f, 0.000000f, -0.989272f, 0.146083f, 1.000000f, -0.989272f, 0.146083f, 0.000000f, -0.973579f, 0.228351f, 1.000000f, -0.973579f, 0.228351f, 0.000000f, -0.951056f, 0.309017f, 1.000000f,
	-0.951056f, 0.309017f, 0.000000f, -0.921863f, 0.387515f, 1.000000f, -0.921863f, 0.387515f, 0.000000f, -0.886204f, 0.463296f, 1.000000f, -0.886204f, 0.463296f, 0.000000f, -0.844328f, 0.535827f,
	1.000000f, -0.844328f, 0.535827f, 0.000000f, -0.796530f, 0.604599f, 1.000000f, -0.796530f, 0.604599f, 0.000000f, -0.743145f, 0.669131f, 1.000000f, -0.743145f, 0.669131f, 0.000000f, -0.684547f,
	0.728969f, 1.000000f, -0.684547f, 0.728969f, 0.000000f, -0.621148f, 0.783693f, 1.000000f, -0.621148f, 0.783693f, 0.000000f, -0.553392f, 0.832921f, 1.000000f, -0.553392f, 0.832921f, 0.000000f,
	-0.481754f, 0.876307f, 1.000000f, -0.481754f, 0.876307f, 0.000000f, -0.406736f, 0.913546f, 1.000000f, -0.406736f, 0.913546f, 0.000000f, -0.328867f, 0.944376f, 1.000000f, -0.328867f, 0.944376f,
	0.000000f, -0.248690f, 0.968583f, 1.000000f, -0.248690f, 0.968583f, 0.000000f, -0.166769f, 0.985996f, 1.000000f, -0.166769f, 0.985996f, 0.000000f, -0.083678f, 0.996493f, 1.000000f, -0.083678f,
	0.996493f, 0.000000f, 0.000000f, 1.000000f, 1.000000f, 0.000000f, 1.000000f, 0.000000f
};

IDirect3DVertexShader9 *d3d_BeamVS = NULL;
IDirect3DPixelShader9 *d3d_BeamPS = NULL;
IDirect3DVertexDeclaration9 *d3d_BeamDecl = NULL;
IDirect3DVertexBuffer9 *d3d_BeamBuffer = NULL;


void Beam_LoadObjects (void)
{
	float *beamverts = NULL;

	d3d_Device->CreateVertexBuffer (
		sizeof (cylinderverts),
		D3DUSAGE_WRITEONLY,
		0,
		D3DPOOL_MANAGED,
		&d3d_BeamBuffer,
		NULL
	);

	d3d_BeamBuffer->Lock (0, 0, (void **) &beamverts, 0);
	memcpy (beamverts, cylinderverts, sizeof (cylinderverts));
	d3d_BeamBuffer->Unlock ();
	d3d_BeamBuffer->PreLoad ();

	D3DVERTEXELEMENT9 decllayout[] =
	{
		VDECL (0, 0, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 0),
		D3DDECL_END ()
	};

	d3d_Device->CreateVertexDeclaration (decllayout, &d3d_BeamDecl);

	d3d_BeamVS = D3D_CreateVertexShader ("hlsl/fxBeam.fx", "BeamVS");
	d3d_BeamPS = D3D_CreatePixelShader ("hlsl/fxBeam.fx", "BeamPS");
}


void Beam_Shutdown (void)
{
	SAFE_RELEASE (d3d_BeamBuffer);
	SAFE_RELEASE (d3d_BeamDecl);
	SAFE_RELEASE (d3d_BeamVS);
	SAFE_RELEASE (d3d_BeamPS);
}


CD3DHandler Beam_Handler (
	Beam_LoadObjects,
	Beam_Shutdown
);


void R_DrawBeam (entity_t *e)
{
	float dir[3], len;
	const float beam_epsilon = 0.000001f;

	VectorSubtract (e->lastorigin, e->currorigin, dir);

	// catch 0 length beams
	if ((len = DotProduct (dir, dir)) < beam_epsilon) return;

	// handle the degenerate case of z1 == z2 with an approximation
	// (should we not just rotate around a different axis instead?)
	if (fabs (dir[2]) < beam_epsilon) dir[2] = beam_epsilon;

	// we already got the dotproduct so we just need to sqrt it here
	float ang = 57.2957795f * acos (dir[2] / (len = sqrt (len))) * ((dir[2] < 0.0f) ? -1.0f : 1.0f);

	// get proper radius
	float rad = (float) e->currframe / 2.0f;

	D3DQMATRIX LocalMatrix (&d3d_state.WorldMatrix);

	LocalMatrix.Translate (e->currorigin);
	LocalMatrix.Rotate (ang, -dir[1] * dir[2], dir[0] * dir[2], 0.0f);
	LocalMatrix.Scale (rad, rad, len);

	d3d_State->SetVertexShaderConstant4fv (8, LocalMatrix.m16, 4);

	d3d_State->SetVertexShaderConstant4f (
		12,
		(float) d_8to24table[e->skinnum & 0xff].peRed / 255.0f,
		(float) d_8to24table[e->skinnum & 0xff].peGreen / 255.0f,
		(float) d_8to24table[e->skinnum & 0xff].peBlue / 255.0f,
		e->alpha
	);

	d3d_State->SetVertexShader (d3d_BeamVS);
	d3d_State->SetPixelShader (d3d_BeamPS);
	d3d_State->SetVertexDeclaration (d3d_BeamDecl);

	d3d_State->SetStreamSource (0, d3d_BeamBuffer, 0, sizeof (float) * 3);

	d3d_Device->DrawPrimitive (D3DPT_TRIANGLESTRIP, 0, NUM_CYLINDER_VERTS - 2);
	c_draw_calls++;
}


