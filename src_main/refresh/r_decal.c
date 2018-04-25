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

// r_decal.c - decal adding and drawing

#include "r_local.h"

#define DF_SHADE			0x00000400	// 1024
#define DF_NOTIMESCALE		0x00000800	// 2048
#define INSTANT_DECAL		-10000.0

#define MAX_DECALS			256
#define MAX_DECAL_VERTS		64
#define MAX_DECAL_FRAGMENTS	64

#define DECAL_BHOLE			1
#define	DECAL_BLOOD			2

typedef struct
{
	vec3_t origin;
	vec2_t texcoords;
	vec4_t color;
} decal_t;

GLuint r_decalvbo = 0;
GLuint r_decalvao = 0;

typedef struct cdecal_t
{
	struct 		cdecal_t *prev, *next;
	float		time;

	int			numverts;
	decal_t		decalvert[MAX_DECAL_VERTS];
	mnode_t     *node;

	vec3_t		direction;

	vec4_t		color;
	vec3_t		org;

	int			type;
	int			flags;
} cdecal_t;

static cdecal_t	decals[MAX_DECALS];
static cdecal_t	active_decals, *free_decals;

GLuint gl_decalprog = 0;
GLuint gl_decalWorldMatrix = 0;

/*
=================
RDecal_CreatePrograms
=================
*/
void RDecal_CreatePrograms(void)
{
	gl_decalprog = GL_CreateShaderFromName("glsl/decals.glsl", "DecalVS", "DecalFS");
	gl_decalWorldMatrix = glGetUniformLocation(gl_decalprog, "worldMatrix");

	glGenBuffers(1, &r_decalvbo);
	glNamedBufferDataEXT(r_decalvbo, MAX_DECALS * sizeof(decal_t) * 4, NULL, GL_STREAM_DRAW);

	glGenVertexArrays(1, &r_decalvao);

	glEnableVertexArrayAttribEXT(r_decalvao, 0);
	glVertexArrayVertexAttribOffsetEXT(r_decalvao, r_decalvbo, 0, 3, GL_FLOAT, GL_FALSE, sizeof(decal_t), 0);

	glEnableVertexArrayAttribEXT(r_decalvao, 1);
	glVertexArrayVertexAttribOffsetEXT(r_decalvao, r_decalvbo, 1, 2, GL_FLOAT, GL_FALSE, sizeof(decal_t), 12);

	glEnableVertexArrayAttribEXT(r_decalvao, 2);
	glVertexArrayVertexAttribOffsetEXT(r_decalvao, r_decalvbo, 2, 4, GL_FLOAT, GL_FALSE, sizeof(decal_t), 18);
}

/*
=================
R_ClearDecals
=================
*/
void R_ClearDecals (void)
{
	int		i;

	memset (decals, 0, sizeof(decals));

	// link decals
	free_decals = decals;
	active_decals.prev = &active_decals;
	active_decals.next = &active_decals;
	for (i = 0; i < MAX_DECALS - 1; i++)
		decals[i].next = &decals[i + 1];
}

/*
=================
R_AllocDecal

Returns either a free decal or the oldest one
=================
*/
static cdecal_t *R_AllocDecal(void)
{
	cdecal_t       *dl;

	if (free_decals)
	{
		// take a free decal if possible
		dl = free_decals;
		free_decals = dl->next;
	}
	else
	{
		// grab the oldest one otherwise
		dl = active_decals.prev;
		dl->prev->next = dl->next;
		dl->next->prev = dl->prev;
	}

	// put the decal at the start of the list
	dl->prev = &active_decals;
	dl->next = active_decals.next;
	dl->next->prev = dl;
	dl->prev->next = dl;

	return dl;
}

/*
=================
R_FreeDecal
=================
*/
static void R_FreeDecal(cdecal_t * dl)
{
	if (!dl->prev)
		return;

	// remove from linked active list
	dl->prev->next = dl->next;
	dl->next->prev = dl->prev;

	// insert into linked free list
	dl->next = free_decals;
	free_decals = dl;
}

/*
=================
RE_AddDecal

Adds a single decal to the decal list
=================
*/
void RE_GL_AddDecal (vec3_t origin, vec3_t dir, vec4_t color, float size, int type, int flags, float angle)
{
	int			i, j, numfragments;
	vec3_t		verts[MAX_DECAL_VERTS], shade;
	markFragment_t *fr, fragments[MAX_DECAL_FRAGMENTS];
	vec3_t		axis[3];
	cdecal_t	*d;
	float		lightspot[3];

	if (!gl_decals->value)
		return;

	// invalid decal
	if (size <= 0 || VectorCompare(dir, vec3_origin))
		return;

	// calculate orientation matrix
	VectorNormalize2(dir, axis[0]);
	PerpendicularVector(axis[1], axis[0]);
	RotatePointAroundVector(axis[2], axis[0], axis[1], angle);
	CrossProduct(axis[0], axis[2], axis[1]);

	// clip it against the world
	numfragments = R_GetClippedFragments(origin, axis, size, MAX_DECAL_VERTS, verts, MAX_DECAL_FRAGMENTS, fragments);
	if (!numfragments)
		return; // no valid fragments

	VectorScale(axis[1], 0.5 / size, axis[1]);
	VectorScale(axis[2], 0.5 / size, axis[2]);

	for (i = 0, fr = fragments; i < numfragments; i++, fr++)
	{
		if (fr->numPoints > MAX_DECAL_VERTS)
			fr->numPoints = MAX_DECAL_VERTS;
		else if (fr->numPoints <= 0)
			continue;

		d = R_AllocDecal();

		d->time = r_newrefdef.time;

		d->numverts = fr->numPoints;
		d->node = fr->node;

		VectorCopy(fr->surf->plane->normal, d->direction);
		if (!(fr->surf->flags & SURF_PLANEBACK))
			VectorNegate(d->direction, d->direction); // reverse direction

		Vector4Set(d->color, color[0], color[1], color[2], color[3]);
		VectorCopy(origin, d->org);

		if (flags & DF_SHADE)
		{
			R_LightPoint(origin, shade, lightspot);
			for (j = 0; j < 3; j++)
				d->color[j] = (d->color[j] * shade[j] * 0.6) + (d->color[j] * 0.4);
		}
		d->type = type;
		d->flags = flags;

		// make the decal vert
		for (j = 0; j < fr->numPoints; j++)
		{
			vec3_t v;

			// xyz
			VectorCopy(verts[fr->firstPoint + j], d->decalvert[j].origin);

			// st
			VectorSubtract(d->decalvert[j].origin, origin, v);
			d->decalvert[j].texcoords[0] = DotProduct(v, axis[1]) + 0.5;
			d->decalvert[j].texcoords[1] = DotProduct(v, axis[2]) + 0.5;

			// color
			VectorCopy(d->color, d->decalvert[j].color);
		}
	}
}

/*
=================
R_DrawDecals

Draws all decals on the decal list
=================
*/
void R_DrawDecals (void)
{
	cdecal_t    *dl, *next, *active;
	float		mindist, time;
	int			r_numdecals = 0;
	vec3_t		v;
	vec4_t		color;

	if (!gl_decals->value)
		return;

	active = &active_decals;

	mindist = DotProduct(r_origin, vpn) + 4.0;

	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(-1, -2);

	glDepthMask(GL_FALSE);
	glEnable(GL_BLEND);

	GL_UseProgram(gl_decalprog);

	glProgramUniformMatrix4fv (gl_decalprog, gl_decalWorldMatrix, 1, GL_FALSE, r_worldmatrix.m[0]);

	// TODO: bind decal texture

	for (dl = active->next; dl != active; dl = next)
	{
		next = dl->next;

		if (dl->time + gl_decalsTime->value <= r_newrefdef.time)
		{
			R_FreeDecal (dl);
			continue;
		}

		if (dl->node == NULL || dl->node->visframe != r_visframecount)
			continue;

		// do not render if the decal is behind the view
		if (DotProduct(dl->org, vpn) < mindist)
			continue;

		// do not render if the view origin is behind the decal
		VectorSubtract(dl->org, r_origin, v);
		if (DotProduct(dl->direction, v) < 0)
			continue;

		Vector4Copy(dl->color, color);

		time = dl->time + gl_decalsTime->value - r_newrefdef.time;

		if (time < 1.5)
			color[3] *= time / 1.5;

		// draw it
		GL_BindVertexArray(r_decalvao);
		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

		r_numdecals++;
		if (r_numdecals >= MAX_DECALS)
			break;
	}

	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);
	glDisable(GL_POLYGON_OFFSET_FILL);
}
