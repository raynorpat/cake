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
// gl_warp.c -- sky and water polygons

#include "d3d_local.h"

cvar_t *r_currentspeed = NULL;

IDirect3DVertexShader9 *d3d_WarpVS = NULL;
IDirect3DPixelShader9 *d3d_WarpPS = NULL;
IDirect3DVertexDeclaration9 *d3d_WarpDecl = NULL;

void Warp_LoadObjects (void)
{
	D3DVERTEXELEMENT9 decllayout[] =
	{
		VDECL (0, offsetof (brushpolyvert_t, xyz), D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 0),
		VDECL (0, offsetof (brushpolyvert_t, st), D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 0),
		VDECL (0, offsetof (brushpolyvert_t, lm), D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 1),
		D3DDECL_END ()
	};

	d3d_Device->CreateVertexDeclaration (decllayout, &d3d_WarpDecl);

	d3d_WarpVS = D3D_CreateVertexShader ("hlsl/fxWarp.fx", "WarpVS");
	d3d_WarpPS = D3D_CreatePixelShader ("hlsl/fxWarp.fx", "WarpPS");
}


void Warp_Shutdown (void)
{
	SAFE_RELEASE (d3d_WarpDecl);
	SAFE_RELEASE (d3d_WarpVS);
	SAFE_RELEASE (d3d_WarpPS);
}


CD3DHandler Warp_Handler (
	Warp_LoadObjects,
	Warp_Shutdown
);


void R_SetLiquidState (void)
{
	d3d_State->SetVertexDeclaration (d3d_WarpDecl);
	d3d_State->SetVertexShader (d3d_WarpVS);
	d3d_State->SetPixelShader (d3d_WarpPS);
	d3d_State->SetVertexShaderConstant1f (12, r_newrefdef.time);
}


void R_DrawLiquidChain (msurface_t *surf, image_t *image)
{
	int LastScroll = -1;

	R_SetLiquidState ();
	d3d_State->SetTexture (0, &d3d_SurfDiffuseSampler, surf->texinfo->image->d3d_Texture);

	for (; surf; surf = surf->texturechain)
	{
		if (LastScroll != (surf->texinfo->flags & SURF_FLOWING))
		{
			R_EndBatchingSurfaces ();
			R_SetSurfaceScroll (surf, d3d_state.warpscroll);
			LastScroll = (surf->texinfo->flags & SURF_FLOWING);
		}

		R_BatchSurface (surf);
	}
}

