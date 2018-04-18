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
// r_light.c

#include "gl_local.h"

/*
=============================================================================

DYNAMIC LIGHTS

=============================================================================
*/

extern GLuint gl_lightmappedsurfprog;
extern GLuint u_brushMaxLights;
extern GLuint u_brushlightMatrix;
extern GLuint u_brushLightPos[MAX_LIGHTS];
extern GLuint u_brushLightColor[MAX_LIGHTS];
extern GLuint u_brushLightAtten[MAX_LIGHTS];

#define DLIGHT_CUTOFF 64.0

int r_dlightframecount;

/*
=============
R_MarkLights_r
=============
*/
static void R_MarkLights_r (dlight_t *light, int bit, mnode_t *node)
{
	cplane_t	*splitplane;
	msurface_t	*surf;
	float		dist;
	int			i;

	// leaf
	if (node->contents != -1)
		return;

	splitplane = node->plane;
	dist = DotProduct(light->transformed, splitplane->normal) - splitplane->dist;

	if (dist > light->radius - DLIGHT_CUTOFF)
	{
		// front 
		R_MarkLights_r (light, bit, node->children[0]);
		return;
	}

	if (dist < -light->radius + DLIGHT_CUTOFF)
	{
		// back
		R_MarkLights_r (light, bit, node->children[1]);
		return;
	}

	// mark all surfaces in this node
	surf = r_worldmodel->surfaces + node->firstsurface;
	for (i = 0; i < node->numsurfaces; i++, surf++)
	{
		// reset light bitmask
		if (surf->dlightframe != r_dlightframecount)
		{
			surf->dlightbits = 0;
			surf->dlightframe = r_dlightframecount;
		}

		// add this light
		surf->dlightbits |= bit;
	}

	// now go down both sides
	R_MarkLights_r (light, bit, node->children[0]);
	R_MarkLights_r (light, bit, node->children[1]);
}

/*
=============
R_MarkLights

Recurses the world, populating the light source bit masks of surfaces that receive dynamic light
=============
*/
void R_MarkLights (mnode_t *headnode, glmatrix *transform)
{
	int	i;
	dlight_t *l;

	// because the count hasn't advanced yet for this frame
	r_dlightframecount = r_framecount + 1;
	l = r_newrefdef.dlights;

	// send the number of current dynamic lights to shader
	glProgramUniform1i (gl_lightmappedsurfprog, u_brushMaxLights, r_newrefdef.num_dlights);

	// send light transform matrix to shader
	glProgramUniformMatrix4fv (gl_lightmappedsurfprog, u_brushlightMatrix, 1, GL_FALSE, transform->m[0]);

	// flag all surfaces for each light source
	for (i = 0; i < r_newrefdef.num_dlights; i++, l++)
	{
		// transform the light by the matrix to get it's new position for surface marking and mark the surfaces
		GL_TransformPoint (transform, l->origin, l->transformed);		
		R_MarkLights_r (l, 1 << i, headnode);
	}
}

/*
=============
R_EnableLights
=============
*/
void R_EnableLights (int framecount, int bitmask)
{
	dlight_t *l;
	int i;

	if (framecount != r_dlightframecount)
		return;

	if (!bitmask)
		return;

	// send light source information to shader
	for (i = 0, l = r_newrefdef.dlights; i < r_newrefdef.num_dlights; i++, l++)
	{		
		glProgramUniform3fv (gl_lightmappedsurfprog, u_brushLightPos[i], 1, l->transformed);
		glProgramUniform3fv (gl_lightmappedsurfprog, u_brushLightColor[i], 1, l->color);
		glProgramUniform1f (gl_lightmappedsurfprog, u_brushLightAtten[i], l->radius);
	}
}


/*
=============================================================================

LIGHT SAMPLING

=============================================================================
*/

static int RecursiveLightPoint (mnode_t *node, vec3_t start, vec3_t end, float *pointcolor, float *lightspot)
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
	front = DotProduct (start, plane->normal) - plane->dist;
	back = DotProduct (end, plane->normal) - plane->dist;
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
		if (surf->flags & (SURF_DRAWTURB | SURF_DRAWSKY))
			continue;	// no lightmaps

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
			vec3_t scale;

			lightmap += 3 * (dt * ((surf->extents[0] >> 4) + 1) + ds);

			for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
			{
				for (i = 0; i < 3; i++)
					scale[i] = r_newrefdef.lightstyles[surf->styles[maps]].rgb[i];

				pointcolor[0] += lightmap[0] * scale[0] * (1.0 / 255);
				pointcolor[1] += lightmap[1] * scale[1] * (1.0 / 255);
				pointcolor[2] += lightmap[2] * scale[2] * (1.0 / 255);

				lightmap += 3 * ((surf->extents[0] >> 4) + 1) * ((surf->extents[1] >> 4) + 1);
			}
		}

		VectorCopy(lightspot, mid);

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
void R_LightPoint (vec3_t p, vec3_t color, float *lightspot)
{
	float		r;
	vec3_t		end;
	vec3_t		pointcolor;

	if (!r_worldmodel->lightdata)
	{
		color[0] = color[1] = color[2] = 1.0;
		return;
	}

	// the downtrace is adjusted to the worldmodel bounds so that it will always hit
	end[0] = p[0];
	end[1] = p[1];
	end[2] = r_worldmodel->mins[2] - 10.0f;

	if ((r = RecursiveLightPoint (r_worldmodel->nodes, p, end, pointcolor, lightspot)) == -1)
		VectorCopy (vec3_origin, color);
	else
		VectorCopy (pointcolor, color);
}


//===================================================================

/*
===============
R_SetCacheState
===============
*/
void R_SetCacheState (msurface_t *surf)
{
	int maps;

	for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
		surf->cached_light[maps] = r_newrefdef.lightstyles[surf->styles[maps]].white;
}

/*
===============
R_BuildLightMap

Combine and scale multiple lightmaps into the floating format in blocklights
===============
*/
void R_BuildLightMap (msurface_t *surf, unsigned *dest, int stride)
{
	int maps, max;
	int	smax, tmax;
	int	r, g, b, a;
	int	i, j, size;
	byte *lightmap;
	float scale[4];
	float *bl;
	static float s_blocklights[34 * 34 * 3];

	if (surf->texinfo->flags & SURF_SKY) return;
	if (surf->texinfo->flags & SURF_WARP) return;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	size = smax * tmax;

	if (size > (sizeof (s_blocklights) >> 4))
		VID_Error (ERR_DROP, "Bad s_blocklights size");

	// set to full bright if no light data
	if (!surf->samples)
	{
		bl = s_blocklights;

		for (i = 0; i < size; i++, bl += 3)
		{
			bl[0] = 255;
			bl[1] = 255;
			bl[2] = 255;
		}
	}
	else
	{
		lightmap = surf->samples;

		// add all the lightmaps
		for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
		{
			bl = s_blocklights;

			scale[0] = r_newrefdef.lightstyles[surf->styles[maps]].rgb[0];
			scale[1] = r_newrefdef.lightstyles[surf->styles[maps]].rgb[1];
			scale[2] = r_newrefdef.lightstyles[surf->styles[maps]].rgb[2];

			if (!maps)
			{
				for (i = 0; i < size; i++, bl += 3)
				{
					bl[0] = lightmap[i * 3 + 0] * scale[0];
					bl[1] = lightmap[i * 3 + 1] * scale[1];
					bl[2] = lightmap[i * 3 + 2] * scale[2];
				}
			}
			else
			{
				for (i = 0; i < size; i++, bl += 3)
				{
					bl[0] += lightmap[i * 3 + 0] * scale[0];
					bl[1] += lightmap[i * 3 + 1] * scale[1];
					bl[2] += lightmap[i * 3 + 2] * scale[2];
				}
			}

			lightmap += size * 3;		// skip to next lightmap
		}
	}

	// put into texture format
	bl = s_blocklights;

	for (i = 0; i < tmax; i++, dest += stride)
	{
		for (j = 0; j < smax; j++, bl += 3)
		{
			// catch negative lights
			if (bl[0] < 0) r = 0; else r = Q_ftol (bl[0]);
			if (bl[1] < 0) g = 0; else g = Q_ftol (bl[1]);
			if (bl[2] < 0) b = 0; else b = Q_ftol (bl[2]);

			max = (r > g) ? ((r > b) ? r : b) : ((g > b) ? g : b);

			if (max > 255)
			{
				float t = 255.0f / max;

				r = r * t;
				g = g * t;
				b = b * t;
				a = 255 * t;
			}
			else a = 255;

			dest[j] = (a << 24) | (r << 16) | (g << 8) | b;
		}
	}
}
