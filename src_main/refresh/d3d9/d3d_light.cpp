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
// r_light.c

#include "d3d_local.h"
#include <malloc.h>

static int allocated[LIGHTMAP_SIZE];
static int r_current_lightmap = 0;
static int r_lightproperty = 0;

std::vector<lightmap_t *> Lightmaps;

void D3D_CreateLightmapTexture (IDirect3DTexture9 **texture, D3DPOOL pool)
{
	d3d_Device->CreateTexture (
		LIGHTMAP_SIZE,
		LIGHTMAP_SIZE,
		1,
		0,
		D3DFMT_A32B32G32R32F,
		pool,
		texture,
		NULL
	);
}


void Light_LoadObjects (void) {}

void Light_Shutdown (void)
{
	for (int i = 0; i < Lightmaps.size (); i++)
	{
		if (!Lightmaps[i]) continue;

		SAFE_RELEASE (Lightmaps[i]->DefTexture);
		SAFE_RELEASE (Lightmaps[i]->SysTexture);

		delete Lightmaps[i];
		Lightmaps[i] = NULL;
	}

	r_current_lightmap = 0;
	Lightmaps.clear ();
}


void Light_ResetDevice (void)
{
	for (int i = 0; i < Lightmaps.size (); i++)
	{
		if (!Lightmaps[i]) continue;
		if (!Lightmaps[i]->SysTexture) continue;

		D3D_CreateLightmapTexture (&Lightmaps[i]->DefTexture, D3DPOOL_DEFAULT);

		Lightmaps[i]->SysTexture->AddDirtyRect (NULL);
		d3d_Device->UpdateTexture (Lightmaps[i]->SysTexture, Lightmaps[i]->DefTexture);
		Lightmaps[i]->Modified = false;
	}
}


void Light_LostDevice (void)
{
	for (int i = 0; i < Lightmaps.size (); i++)
	{
		if (!Lightmaps[i]) continue;

		SAFE_RELEASE (Lightmaps[i]->DefTexture);
	}
}


CD3DHandler Light_Handler (
	Light_LoadObjects,
	Light_Shutdown,
	Light_LostDevice,
	Light_ResetDevice
);


#define	DLIGHT_CUTOFF	64


/*
=============================================================================

DYNAMIC LIGHTS

=============================================================================
*/

float R_MarkLightClamp (msurface_t *surf, float *impact, int vec)
{
	float l = DotProduct (impact, surf->texinfo->vecs[vec]) + surf->texinfo->vecs[vec][3] - surf->texturemins[vec];
	float s = l + 0.5; if (s < 0) s = 0; else if (s > surf->extents[vec]) s = surf->extents[vec];

	return l - s;
}


/*
=============
R_MarkLights
=============
*/
void R_MarkLights (dlight_t *light, int bit, mnode_t *node)
{
	if (node->contents != -1) return;

	float dist = Mod_PlaneDot (light->transformed, node->plane);

	if (dist > light->intensity - DLIGHT_CUTOFF)
	{
		R_MarkLights (light, bit, node->children[0]);
		return;
	}

	if (dist < -light->intensity + DLIGHT_CUTOFF)
	{
		R_MarkLights (light, bit, node->children[1]);
		return;
	}

	// mark the polygons
	msurface_t *surf = r_worldmodel->surfaces + node->firstsurface;
	float maxdist = light->intensity * light->intensity;

	for (int i = 0; i < node->numsurfaces; i++, surf++)
	{
		float impact[3];

		// this is some CPU overhead but it's a tradeoff against unnecessary lightmap uploads
		// (the node may be in range but individual surfs in it may not)
		Vector3Scale (impact, surf->plane->normal, dist);
		Vector3Subtract (impact, light->transformed, impact);

		// clamp center of light to corner and check brightness
		int s = R_MarkLightClamp (surf, impact, 0);
		int t = R_MarkLightClamp (surf, impact, 1);

		if ((s * s + t * t + dist * dist) < maxdist)
		{
			if (surf->dlightframe != r_framecount)
			{
				surf->dlightbits = bit;
				surf->dlightframe = r_framecount;
			}
			else surf->dlightbits |= bit;
		}
	}

	R_MarkLights (light, bit, node->children[0]);
	R_MarkLights (light, bit, node->children[1]);
}


/*
=============
R_PushDlights
=============
*/
void R_PushDlights (mnode_t *headnode, float *origin)
{
	for (int i = 0; i < r_newrefdef.num_dlights; i++)
	{
		dlight_t *l = &r_newrefdef.dlights[i];

		l->transformed[0] = l->origin[0] - origin[0];
		l->transformed[1] = l->origin[1] - origin[1];
		l->transformed[2] = l->origin[2] - origin[2];

		R_MarkLights (l, 1 << i, headnode);
	}
}


/*
=============================================================================

LIGHT SAMPLING

=============================================================================
*/

int RecursiveLightPoint (mnode_t *node, vec3_t start, vec3_t end, float *pointcolor, float *lightspot)
{
	float		front, back, frac;
	int			side;
	cplane_t	*plane;
	vec3_t		mid;
	msurface_t	*surf;
	int			s, t, ds, dt;
	int			i;
	mtexinfo_t	*tex;
	byte		*lightmap;
	int			maps;
	int			r;

	if (node->contents != -1)
		return -1;		// didn't hit anything

	// calculate mid point
	// FIXME: optimize for axial
	plane = node->plane;
	front = Mod_PlaneDot (start, plane);
	back = Mod_PlaneDot (end, plane);
	side = front < 0;

	if ((back < 0) == side)
		return RecursiveLightPoint (node->children[side], start, end, pointcolor, lightspot);

	frac = front / (front - back);
	mid[0] = start[0] + (end[0] - start[0]) * frac;
	mid[1] = start[1] + (end[1] - start[1]) * frac;
	mid[2] = start[2] + (end[2] - start[2]) * frac;

	// go down front side
	r = RecursiveLightPoint (node->children[side], start, mid, pointcolor, lightspot);

	if (r >= 0)
		return r;		// hit something

	if ((back < 0) == side)
		return -1;		// didn't hit anuthing

	// check for impact on this node
	surf = r_worldmodel->surfaces + node->firstsurface;

	for (i = 0; i < node->numsurfaces; i++, surf++)
	{
		if (surf->texinfo->flags & (SURF_SKY | SURF_TRANS33 | SURF_TRANS66 | SURF_WARP)) continue;

		tex = surf->texinfo;

		s = DotProduct (mid, tex->vecs[0]) + tex->vecs[0][3];
		t = DotProduct (mid, tex->vecs[1]) + tex->vecs[1][3];

		if (s < surf->texturemins[0] || t < surf->texturemins[1])
			continue;

		ds = s - surf->texturemins[0];
		dt = t - surf->texturemins[1];

		if (ds > surf->extents[0] || dt > surf->extents[1])
			continue;

		if (!surf->samples)
			return 0;

		ds >>= 4;
		dt >>= 4;

		lightmap = surf->samples;
		VectorCopy (vec3_origin, pointcolor);

		if (lightmap)
		{
			lightmap += 3 * (dt * ((surf->extents[0] >> 4) + 1) + ds);

			int line3 = ((surf->extents[0] >> 4) + 1) * 3;
			float dsfrac = ds & 15;
			float dtfrac = dt & 15;
			float r00 = 0, g00 = 0, b00 = 0, r01 = 0, g01 = 0, b01 = 0, r10 = 0, g10 = 0, b10 = 0, r11 = 0, g11 = 0, b11 = 0;

			for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
			{
				float *style = r_newrefdef.lightstyles[surf->styles[maps]].color;

				r00 += lightmap[0] * style[0];
				g00 += lightmap[1] * style[1];
				b00 += lightmap[2] * style[2];

				r01 += lightmap[3] * style[0];
				g01 += lightmap[4] * style[1];
				b01 += lightmap[5] * style[2];

				r10 += lightmap[line3 + 0] * style[0];
				g10 += lightmap[line3 + 1] * style[1];
				b10 += lightmap[line3 + 2] * style[2];

				r11 += lightmap[line3 + 3] * style[0];
				g11 += lightmap[line3 + 4] * style[1];
				b11 += lightmap[line3 + 5] * style[2];

				lightmap += 3 * ((surf->extents[0] >> 4) + 1) * ((surf->extents[1] >> 4) + 1);
			}

			pointcolor[0] += (((((((((r11 - r10) * dsfrac) * 0.0625f) + r10) - ((((r01 - r00) * dsfrac) * 0.0625f) + r00)) * dtfrac) * 0.0625f) + ((((r01 - r00) * dsfrac) * 0.0625f) + r00)));
			pointcolor[1] += (((((((((g11 - g10) * dsfrac) * 0.0625f) + g10) - ((((g01 - g00) * dsfrac) * 0.0625f) + g00)) * dtfrac) * 0.0625f) + ((((g01 - g00) * dsfrac) * 0.0625f) + g00)));
			pointcolor[2] += (((((((((b11 - b10) * dsfrac) * 0.0625f) + b10) - ((((b01 - b00) * dsfrac) * 0.0625f) + b00)) * dtfrac) * 0.0625f) + ((((b01 - b00) * dsfrac) * 0.0625f) + b00)));
		}

		Vector3Copy (lightspot, mid);

		return 1;
	}

	// go down back side
	return RecursiveLightPoint (node->children[!side], mid, end, pointcolor, lightspot);
}


/*
===============
R_LightPoint
===============
*/
void R_LightPoint (vec3_t p, float *color, float *lightspot)
{
	float		r;
	vec3_t		dist;
	vec3_t		pointcolor;

	if (!r_worldmodel->lightdata || r_fullbright->value)
	{
		color[0] = color[1] = color[2] = 1.0;
		return;
	}

	// the downtrace is adjusted to the worldmodel bounds so that it will always hit
	float end[] = {
		p[0],
		p[1],
		r_worldmodel->mins[2] - 10.0f
	};

	if ((r = RecursiveLightPoint (r_worldmodel->nodes, p, end, pointcolor, lightspot)) == -1)
		Vector3Copy (color, vec3_origin);
	else Vector3Copy (color, pointcolor);

	// add ambient lighting
	Vector3Add (color, color, r_ambient->value);

	// add dynamic lights
	dlight_t *dl = r_newrefdef.dlights;

	for (int lnum = 0; lnum < r_newrefdef.num_dlights; lnum++, dl++)
	{
		VectorSubtract (p, dl->origin, dist);

		// fixme - same calcs as BSP (minlight/etc) should go here
		float add = (dl->intensity - VectorLength (dist)) * 0.5f;

		if (add > 0)
		{
			color[0] += (float) dl->color[0] * add;
			color[1] += (float) dl->color[1] * add;
			color[2] += (float) dl->color[2] * add;
		}
	}

	float ntsc[] = {0.3f, 0.59f, 0.11f};
	float white = Vector3Dot (color, ntsc);
	float grey[] = {white, white, white};

	Vector3Lerp (color, color, grey, gl_monolightmap->value);
}


//===================================================================


/*
===============
R_AddDynamicLights
===============
*/
void R_AddDynamicLights (msurface_t *surf, int stride, float *dest)
{
	mtexinfo_t *tex = surf->texinfo;

	for (int lnum = 0; lnum < r_newrefdef.num_dlights; lnum++)
	{
		if (!(surf->dlightbits & (1 << lnum))) continue;

		dlight_t *dl = &r_newrefdef.dlights[lnum];
		float fdist = DotProduct (dl->transformed, surf->plane->normal) - surf->plane->dist;
		float frad = dl->intensity - fabs (fdist);

		// rad is now the highest intensity on the plane
		float fminlight = DLIGHT_CUTOFF;	// FIXME: make configurable?

		if (frad < fminlight)
			continue;

		fminlight = frad - fminlight;

		float impact[] = {
			dl->transformed[0] - surf->plane->normal[0] * fdist,
			dl->transformed[1] - surf->plane->normal[1] * fdist,
			dl->transformed[2] - surf->plane->normal[2] * fdist
		};

		float local[] = {
			DotProduct (impact, tex->vecs[0]) + tex->vecs[0][3] - surf->texturemins[0],
			DotProduct (impact, tex->vecs[1]) + tex->vecs[1][3] - surf->texturemins[1]
		};

		float *bl = dest;

		for (int t = 0; t < surf->tmax; t++, bl += stride)
		{
			int td = local[1] - t * 16;

			if (td < 0)
				td = -td;

			for (int s = 0; s < surf->smax; s++, bl += 4)
			{
				int sd = local[0] - s * 16;

				if (sd < 0)
					sd = -sd;

				if (sd > td)
					fdist = sd + (td >> 1);
				else fdist = td + (sd >> 1);

				if (fdist < fminlight)
				{
					bl[0] += (fminlight - fdist) * dl->color[0];
					bl[1] += (fminlight - fdist) * dl->color[1];
					bl[2] += (fminlight - fdist) * dl->color[2];

					// we have a dlight now
					surf->cached_dlight = true;
				}
			}
		}
	}
}


/*
===============
R_BuildLightmap

Combine and scale multiple lightmaps into the floating format in blocklights
===============
*/
void R_ClearLightToBase (float *dest, int smax, int tmax, int stride, float baselight)
{
	for (int i = 0; i < tmax; i++, dest += stride)
	{
		for (int j = 0; j < smax; j++, dest += 4)
		{
			dest[0] = baselight;
			dest[1] = baselight;
			dest[2] = baselight;
		}
	}
}


void R_LightAccumulateMap (float *dest, int smax, int tmax, int stride, byte *lightmap, float *scale)
{
	for (int i = 0; i < tmax; i++, dest += stride)
	{
		for (int j = 0; j < smax; j++, dest += 4, lightmap += 3)
		{
			dest[0] += lightmap[0] * scale[0];
			dest[1] += lightmap[1] * scale[1];
			dest[2] += lightmap[2] * scale[2];
		}
	}
}


void R_BuildLightmap (msurface_t *surf)
{
	if (surf->texinfo->flags & (SURF_SKY | SURF_TRANS33 | SURF_TRANS66 | SURF_WARP)) return;

	if (surf->LightmapTextureNum < 0) return;
	if (surf->LightmapTextureNum >= Lightmaps.size ()) return;

	lightmap_t *lm = Lightmaps[surf->LightmapTextureNum];
	D3DLOCKED_RECT lockrect;

	if (!lm) return;
	if (!lm->SysTexture) return;
	if (!lm->DefTexture) return;

	if (FAILED (lm->SysTexture->LockRect (0, &lockrect, &surf->LightRect, 0))) return;

	float *blocklights = (float *) lockrect.pBits;
	int stride = (lockrect.Pitch >> 2) - (surf->smax << 2);

	// set to full bright if no light data
	if (!surf->samples || r_fullbright->value)
	{
		R_ClearLightToBase(blocklights, surf->smax, surf->tmax, stride, 255);
	}
	else
	{
		byte *lightmap = surf->samples;

		// clear to no light
		R_ClearLightToBase (blocklights, surf->smax, surf->tmax, stride, r_ambient->value);

		// add all the lightmaps
		for (int maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
		{
			float *style = r_newrefdef.lightstyles[surf->styles[maps]].color;

			R_LightAccumulateMap(blocklights, surf->smax, surf->tmax, stride, lightmap, style);

			// this works because VectorCopy is a #define and not typed
			Vector3Copy (surf->cached_light[maps], style);
			lightmap += surf->smax * surf->tmax * 3;
		}
	}

	// add all the dynamic lights
	if (surf->dlightframe == r_framecount)
	{
		R_AddDynamicLights(surf, stride, blocklights);
	}
	else
	{
		surf->cached_dlight = false;
	}

	// store this so the surf can track any other lighting cvar changes (r_ambient, etc)
	surf->lightproperty = r_lightproperty;

	lm->SysTexture->UnlockRect (0);
	lm->Modified = true;
}


void R_RenderDynamicLightmaps (msurface_t *surf)
{
	// no lights on these surface types
	if (surf->texinfo->flags & (SURF_SKY | SURF_TRANS33 | SURF_TRANS66 | SURF_WARP)) return;

	if ((surf->dlightframe == r_framecount) || surf->cached_dlight || surf->lightproperty != r_lightproperty)
	{
		R_BuildLightmap (surf);
		return;
	}

	// test the styles
	for (int maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
	{
		for (int i = 0; i < 3; i++)
		{
			if (r_newrefdef.lightstyles[surf->styles[maps]].color[i] != surf->cached_light[maps][i])
			{
				// one is all we need
				R_BuildLightmap (surf);
				return;
			}
		}
	}
}


/*
=============================================================================

  LIGHTMAP ALLOCATION

=============================================================================
*/

static qboolean LM_D3D_AllocBlock (int w, int h, LONG *x, LONG *y)
{
	int		i, j;
	int		best, best2;

	best = LIGHTMAP_SIZE;

	for (i = 0; i < LIGHTMAP_SIZE - w; i++)
	{
		best2 = 0;

		for (j = 0; j < w; j++)
		{
			if (allocated[i + j] >= best)
				break;

			if (allocated[i + j] > best2)
				best2 = allocated[i + j];
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
		allocated[*x + i] = best + h;

	return true;
}


/*
========================
D3D9_CreateSurfaceLightmap
========================
*/
void D3D9_CreateSurfaceLightmap (msurface_t *surf)
{
	// no lightmaps on these surfs
	if (surf->texinfo->flags & (SURF_SKY | SURF_TRANS33 | SURF_TRANS66 | SURF_WARP)) return;

	surf->smax = (surf->extents[0] >> 4) + 1;
	surf->tmax = (surf->extents[1] >> 4) + 1;

	if (!LM_D3D_AllocBlock(surf->smax, surf->tmax, &surf->LightRect.left, &surf->LightRect.top))
	{
		memset (allocated, 0, sizeof (allocated));
		r_current_lightmap++;

		if (!LM_D3D_AllocBlock (surf->smax, surf->tmax, &surf->LightRect.left, &surf->LightRect.top))
		{
			Com_Error (ERR_DROP, "Consecutive calls to LM_D3D_AllocBlock (%d, %d) failed\n", surf->smax, surf->tmax);
			return;
		}
	}

	surf->LightmapTextureNum = r_current_lightmap;
	surf->dlightframe = -1;

	while (surf->LightmapTextureNum >= Lightmaps.size ())
	{
		Lightmaps.push_back (new lightmap_t);
		memset (Lightmaps[Lightmaps.size () - 1], 0, sizeof (lightmap_t));
	}

	if (!Lightmaps[surf->LightmapTextureNum])
	{
		Lightmaps[surf->LightmapTextureNum] = new lightmap_t;
		memset (Lightmaps[surf->LightmapTextureNum], 0, sizeof (lightmap_t));
	}

	if (!Lightmaps[surf->LightmapTextureNum]->SysTexture) D3D_CreateLightmapTexture (&Lightmaps[surf->LightmapTextureNum]->SysTexture, D3DPOOL_SYSTEMMEM);
	if (!Lightmaps[surf->LightmapTextureNum]->DefTexture) D3D_CreateLightmapTexture (&Lightmaps[surf->LightmapTextureNum]->DefTexture, D3DPOOL_DEFAULT);

	// fill in the rest of the rect
	surf->LightRect.right = surf->LightRect.left + surf->smax;
	surf->LightRect.bottom = surf->LightRect.top + surf->tmax;

	R_BuildLightmap (surf);
}


void D3D_UpdateLightmaps (void)
{
	// this is an inclusive range
	for (int i = 0; i <= r_current_lightmap; i++)
	{
		if (!Lightmaps[i]) continue;

		if (!Lightmaps[i]->SysTexture) continue;
		if (!Lightmaps[i]->DefTexture) continue;
		if (!Lightmaps[i]->Modified) continue;

		d3d_Device->UpdateTexture (Lightmaps[i]->SysTexture, Lightmaps[i]->DefTexture);
		Lightmaps[i]->Modified = false;
	}

	// any modified lighting cvars need a full rebuild so increment the global light property to flag it
	if (r_ambient->modified || r_fullbright->modified)
	{
		r_lightproperty++;
		r_ambient->modified = false;
		r_fullbright->modified = false;
	}
}


/*
==================
D3D9_BeginBuildingLightmaps
==================
*/
static int R_SortSurfacesByOrder (msurface_t *s1, msurface_t *s2)
{
	return s1->surfaceorder - s2->surfaceorder;
}

static int R_SortSurfacesByTexture (msurface_t *s1, msurface_t *s2)
{
	if (s1->texinfo->image == s2->texinfo->image)
		return s1->surfaceorder - s2->surfaceorder;
	else return (int) s1->texinfo->image - (int) s2->texinfo->image;
}

static void HackLightstylesForD3D(lightstyle_t *ls, float r, float g, float b)
{
	// scale styles for gl_dynamic and for overbright range
	ls->color[0] = (((r - 1.0f) * gl_dynamic->value) + 1.0f) * 0.0078125f;
	ls->color[1] = (((g - 1.0f) * gl_dynamic->value) + 1.0f) * 0.0078125f;
	ls->color[2] = (((b - 1.0f) * gl_dynamic->value) + 1.0f) * 0.0078125f;
}

void D3D9_BeginBuildingLightmaps (model_t *m)
{
	static lightstyle_t	lightstyles[MAX_LIGHTSTYLES];

	r_framecount = 1;		// no dlightcache
	r_lightproperty++;

	// setup the base lightstyles so the lightmaps which use them won't have to be regenerated the first time they're seen
	// keep this consistent with runtime lightstyle adding so that we're guaranteed the above assumption
	for (int i = 0; i < MAX_LIGHTSTYLES; i++)
		HackLightstylesForD3D(&lightstyles[i], 1.0f, 1.0f, 1.0f);

	r_newrefdef.lightstyles = lightstyles;
	memset (allocated, 0, sizeof (allocated));
	r_current_lightmap = 0;

	// build lightmaps in texture order for better cache coherence
	qsort (m->surfaces, m->numsurfaces, sizeof (msurface_t), (sortfunc_t) R_SortSurfacesByTexture);
}


/*
=======================
D3D9_EndBuildingLightmaps
=======================
*/
void D3D9_EndBuildingLightmaps (model_t *m)
{
	// revert the original surface load order for actually drawing
	qsort (m->surfaces, m->numsurfaces, sizeof (msurface_t), (sortfunc_t) R_SortSurfacesByOrder);

	// update any lightmaps that were filled
	D3D_UpdateLightmaps ();

	// release any lightmaps that weren't used - this is an inclusive range
	for (int i = r_current_lightmap + 1; i < Lightmaps.size (); i++)
	{
		if (!Lightmaps[i]) continue;

		SAFE_RELEASE (Lightmaps[i]->DefTexture);
		SAFE_RELEASE (Lightmaps[i]->SysTexture);

		delete Lightmaps[i];
		Lightmaps[i] = NULL;
	}
}


