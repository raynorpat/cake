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

#define DF_SHADE				0x00000400	// 1024
#define DF_NOTIMESCALE			0x00000800	// 2048
#define INSTANT_DECAL			-10000.0

#define MAX_DECALS				256

#define MAX_DECAL_VERTS			256
#define MAX_FRAGMENTS_PER_DECAL	32
#define MAX_VERTS_PER_FRAGMENT	8

typedef struct cdecal_t
{
	struct 		cdecal_t *prev, *next;
	float		time;

	int			numverts;
	vec3_t		verts[MAX_VERTS_PER_FRAGMENT];
	vec2_t		stcoords[MAX_VERTS_PER_FRAGMENT];
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
GLuint gl_decalmvpMatrix = 0;

GLuint gl_decalvao = 0;
GLuint gl_decalvbo_xyz = 0;
GLuint gl_decalvbo_st = 0;
GLuint gl_decalvbo_color = 0;

GLuint r_decalImages[MAX_DECAL_TEX];

/*
=================
RDecal_CreatePrograms
=================
*/
void RDecal_CreatePrograms(void)
{
	byte *data = NULL;
	int width, height;

	// create textures for decals
	LoadImageThruSTB("pics/particles/blood.png", "png", &data, &width, &height);
	if (data)
		r_decalImages[DECAL_BLOOD] = GL_UploadTexture(data, width, height, false, 32);
	LoadImageThruSTB("pics/particles/blood2.png", "png", &data, &width, &height);
	if (data)
		r_decalImages[DECAL_BLOOD_2] = GL_UploadTexture(data, width, height, false, 32);
	LoadImageThruSTB("pics/particles/blood3.png", "png", &data, &width, &height);
	if (data)
		r_decalImages[DECAL_BLOOD_3] = GL_UploadTexture(data, width, height, false, 32);
	LoadImageThruSTB("pics/particles/blood4.png", "png", &data, &width, &height);
	if (data)
		r_decalImages[DECAL_BLOOD_4] = GL_UploadTexture(data, width, height, false, 32);
	LoadImageThruSTB("pics/particles/blood5.png", "png", &data, &width, &height);
	if (data)
		r_decalImages[DECAL_BLOOD_5] = GL_UploadTexture(data, width, height, false, 32);
	LoadImageThruSTB("pics/particles/bullet_mrk.png", "png", &data, &width, &height);
	if (data)
		r_decalImages[DECAL_BHOLE] = GL_UploadTexture(data, width, height, false, 32);
	LoadImageThruSTB("pics/particles/burn_mrk.png", "png", &data, &width, &height);
	if (data)
		r_decalImages[DECAL_BURNMRK] = GL_UploadTexture(data, width, height, false, 32);
	LoadImageThruSTB("pics/particles/bigburn_mrk.png", "png", &data, &width, &height);
	if (data)
		r_decalImages[DECAL_BIGBURNMRK] = GL_UploadTexture(data, width, height, false, 32);
	LoadImageThruSTB("pics/particles/tracker_mrk.png", "png", &data, &width, &height);
	if (data)
		r_decalImages[DECAL_TRACKERMRK] = GL_UploadTexture(data, width, height, false, 32);
	LoadImageThruSTB("pics/particles/footprint.png", "png", &data, &width, &height);
	if (data)
		r_decalImages[DECAL_FOOTPRINT] = GL_UploadTexture(data, width, height, false, 32);

	gl_decalprog = GL_CreateShaderFromName("glsl/decals.glsl", "DecalVS", "DecalFS");
	gl_decalmvpMatrix = glGetUniformLocation(gl_decalprog, "mvpMatrix");
	glProgramUniform1i(gl_decalprog, glGetUniformLocation(gl_decalprog, "decalTex"), 0);

	glGenVertexArrays(1, &gl_decalvao);

	glEnableVertexArrayAttribEXT(gl_decalvao, 0);
	glEnableVertexArrayAttribEXT(gl_decalvao, 1);
	glEnableVertexArrayAttribEXT(gl_decalvao, 2);

	glGenBuffers(1, &gl_decalvbo_xyz);
	glGenBuffers(1, &gl_decalvbo_st);
	glGenBuffers(1, &gl_decalvbo_color);
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
	vec3_t		verts[MAX_DECAL_VERTS], shade, temp;
	markFragment_t *fr, fragments[MAX_FRAGMENTS_PER_DECAL];
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
	numfragments = R_GetClippedFragments(origin, axis, size, MAX_DECAL_VERTS, verts, MAX_FRAGMENTS_PER_DECAL, fragments);
	if (!numfragments)
		return; // no valid fragments

	// store out vertex data
	size = 0.5f / size;
	VectorScale(axis[1], size, axis[1]);
	VectorScale(axis[2], size, axis[2]);

	for (i = 0, fr = fragments; i < numfragments; i++, fr++)
	{
		d = R_AllocDecal();

		d->time = r_newrefdef.time;

		d->node = fr->node;

		VectorCopy(fr->surf->plane->normal, d->direction);
		if (!(fr->surf->flags & SURF_PLANEBACK))
			VectorNegate(d->direction, d->direction); // reverse direction

		Vector4Set(d->color, color[0], color[1], color[2], color[3]);
		VectorCopy(origin, d->org);

		//if (flags & DF_SHADE)
		{
			R_LightPoint(origin, shade, lightspot);
			for (j = 0; j < 3; j++)
				d->color[j] = (d->color[j] * shade[j] * 0.6) + (d->color[j] * 0.4);
		}
		d->type = type;
		d->flags = flags;

		// make the decal vert
		d->numverts = fr->numPoints;
		for (j = 0; j < fr->numPoints && j < MAX_VERTS_PER_FRAGMENT; j++)
		{
			// xyz
			VectorCopy(verts[fr->firstPoint + j], d->verts[j]);

			// st
			VectorSubtract(d->verts[j], origin, temp);
			d->stcoords[j][0] = DotProduct(temp, axis[1]) + 0.5f;
			d->stcoords[j][1] = DotProduct(temp, axis[2]) + 0.5f;
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

	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL)
		return;

	active = &active_decals;

	mindist = DotProduct(r_origin, vpn) + 4.0;

	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(-1, -1);

	GL_Enable(BLEND_BIT | DEPTHTEST_BIT);

	GL_UseProgram(gl_decalprog);

	glProgramUniformMatrix4fv (gl_decalprog, gl_decalmvpMatrix, 1, GL_FALSE, r_mvpmatrix.m[0]);

	GL_BindVertexArray(gl_decalvao);
	
	for (dl = active->next; dl != active; dl = next)
	{
		next = dl->next;

		if (dl->node == NULL || dl->node->visframe != r_visframecount)
			continue;

		// check type
		if (dl->type < 0 || dl->type > MAX_DECAL_TEX)
		{
			R_FreeDecal (dl);
			continue;
		}

		// have we faded out yet?
		if (dl->time + gl_decalsTime->value <= r_newrefdef.time)
		{
			R_FreeDecal (dl);
			continue;
		}

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

		// bind texture
		GL_BindTexture(GL_TEXTURE0, GL_TEXTURE_2D, r_drawnearestclampsampler, r_decalImages[dl->type]);

		// bind data
		glBindBuffer(GL_ARRAY_BUFFER, gl_decalvbo_xyz);
		glBufferData(GL_ARRAY_BUFFER, sizeof(dl->verts), dl->verts, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, gl_decalvbo_st);
		glBufferData(GL_ARRAY_BUFFER, sizeof(dl->stcoords), dl->stcoords, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, gl_decalvbo_color);
		glBufferData(GL_ARRAY_BUFFER, sizeof(color), color, GL_DYNAMIC_DRAW);

		// draw
		glBindBuffer(GL_ARRAY_BUFFER, gl_decalvbo_xyz);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
		glBindBuffer(GL_ARRAY_BUFFER, gl_decalvbo_st);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);
		glBindBuffer(GL_ARRAY_BUFFER, gl_decalvbo_color);
		glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 0, 0);

		glDrawArrays(GL_TRIANGLE_FAN, 0, dl->numverts);

		r_numdecals++;
		if (r_numdecals >= MAX_DECALS)
			break;
	}

	glDisable(GL_POLYGON_OFFSET_FILL);
}
