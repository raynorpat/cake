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

char	skyname[MAX_QPATH];
float	skyrotate;
vec3_t	skyaxis;
D3DQMATRIX SkyMatrix;
image_t	*sky_images[6];

IDirect3DCubeTexture9 *d3d_SkyCubemap = NULL;
IDirect3DVertexShader9 *d3d_SkyVS = NULL;
IDirect3DPixelShader9 *d3d_SkyPS = NULL;
IDirect3DVertexDeclaration9 *d3d_SkyDecl = NULL;

void Sky_LoadObjects (void)
{
	D3DVERTEXELEMENT9 decllayout[] = {
		// intentionally these point to the same data
		VDECL (0, offsetof (brushpolyvert_t, xyz), D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 0),
		VDECL (0, offsetof (brushpolyvert_t, xyz), D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_TEXCOORD, 0),
		D3DDECL_END ()
	};

	d3d_Device->CreateVertexDeclaration (decllayout, &d3d_SkyDecl);

	d3d_SkyVS = D3D_CreateVertexShader ("hlsl/fxSky.fx", "SkyVS");
	d3d_SkyPS = D3D_CreatePixelShader ("hlsl/fxSky.fx", "SkyPS");

	// force a load the first time we see it
	skyname[0] = 0;
}


void Sky_Shutdown (void)
{
	SAFE_RELEASE (d3d_SkyCubemap);
	SAFE_RELEASE (d3d_SkyDecl);
	SAFE_RELEASE (d3d_SkyVS);
	SAFE_RELEASE (d3d_SkyPS);
}


void Sky_ResetDevice (void)
{
}


void Sky_LostDevice (void)
{
}


CD3DHandler Sky_Handler (
	Sky_LoadObjects,
	Sky_Shutdown,
	Sky_LostDevice,
	Sky_ResetDevice
);


void R_DrawSkySurfaces (msurface_t *surfaces)
{
	if (!surfaces) return;
	if (!d3d_SkyCubemap) return;

	// this is z-going-up with rotate and flip to orient the sky correctly
	SkyMatrix.Load (0, 0, 1, 0, -1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1);

	// and position it
	SkyMatrix.Rotate (r_newrefdef.time * skyrotate, skyaxis[0], skyaxis[2], skyaxis[1]);
	SkyMatrix.Translate (-d3d_state.r_origin[0], -d3d_state.r_origin[1], -d3d_state.r_origin[2]);

	d3d_State->SetVertexShaderConstant4fv (12, SkyMatrix.m16, 4);

	d3d_State->SetVertexDeclaration (d3d_SkyDecl);
	d3d_State->SetVertexShader (d3d_SkyVS);
	d3d_State->SetPixelShader (d3d_SkyPS);

	d3d_State->SetTexture (0, &d3d_SkySampler, d3d_SkyCubemap);

	R_BeginBatchingSurfaces ();

	for (msurface_t *surf = surfaces; surf; surf = surf->texturechain)
	{
		R_BatchSurface (surf);
	}

	R_EndBatchingSurfaces ();
}


/*
============
D3D9_SetSky
============
*/
void D3D9_SetSky (char *name, float rotate, vec3_t axis)
{
	if (!name || !name[0])
	{
		SAFE_RELEASE (d3d_SkyCubemap);
		skyname[0] = 0;
		return;
	}

	char pathname[MAX_QPATH];
	int maxextent = 0;

	char *suf[6] = {"ft", "bk", "up", "dn", "rt", "lf"};

	// to do - if the name hasn't changed we can keep the old sky (provided we have one)
	if (!strcmp (skyname, name) && d3d_SkyCubemap) return;

	strncpy (skyname, name, sizeof (skyname) - 1);
	skyrotate = rotate;
	VectorCopy (axis, skyaxis);

	// because we're building a cubemap here we must first load the individual textures, find the size we need for the cubemap,
	// then load the faces from those.  the individual textures can be discarded when done.
	for (int i = 0; i < 6; i++)
	{
		Com_sprintf (pathname, sizeof (pathname), "env/%s%s.tga", skyname, suf[i]);

		sky_images[i] = D3D9_FindImage (pathname, it_sky);

		// the image is only needed while building the cubemap and can be released as an unused image when done
		if (!sky_images[i])
			sky_images[i] = r_greytexture;
		else sky_images[i]->registration_sequence = -1;

		// find the max size we're going to use for the cubemap (fixme: could this ever be 0?)
		D3DSURFACE_DESC desc;

		sky_images[i]->d3d_Texture->GetLevelDesc (0, &desc);

		if (desc.Width > maxextent) maxextent = desc.Width;
		if (desc.Height > maxextent) maxextent = desc.Height;
	}

	// this should never happen as we replace a sky face that failed to load with the grey texture
	if (!maxextent)
	{
		Com_Printf ("Failed to load sky %s\n", name);
		return;
	}

	// the old cubemap is no longer valid
	SAFE_RELEASE (d3d_SkyCubemap);

	// now build the cubemap
	if (SUCCEEDED (d3d_Device->CreateCubeTexture (maxextent, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_MANAGED, &d3d_SkyCubemap, NULL)))
	{
		// load the faces from the sky_images array to the cubemap
		for (int i = 0; i < 6; i++)
		{
			IDirect3DSurface9 *src = NULL;
			IDirect3DSurface9 *dst = NULL;

			// note workaround for excessive type-safety weenieness here
			if (SUCCEEDED (sky_images[i]->d3d_Texture->GetSurfaceLevel (0, &src)))
				if (SUCCEEDED (d3d_SkyCubemap->GetCubeMapSurface ((D3DCUBEMAP_FACES) ((int) D3DCUBEMAP_FACE_POSITIVE_X + i), 0, &dst)))
					D3DXLoadSurfaceFromSurface (dst, NULL, NULL, src, NULL, NULL, D3DX_FILTER_LINEAR, 0);

			SAFE_RELEASE (src);
			SAFE_RELEASE (dst);
		}

		d3d_SkyCubemap->PreLoad ();
	}
}


