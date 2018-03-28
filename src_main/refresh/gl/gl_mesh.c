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
// gl_mesh.c: triangle model functions
// this stuff was fucking nuts

#include "gl_local.h"

#define NUMVERTEXNORMALS	162

extern float r_avertexnormals[NUMVERTEXNORMALS][3];

#define SHADOW_SKEW_X -0.7f		// skew along x axis. -0.7 to mimic glquake shadows
#define SHADOW_SKEW_Y 0			// skew along y axis. 0 to mimic glquake shadows
#define SHADOW_VSCALE 0			// 0 = completely flat
#define SHADOW_HEIGHT 0.1f		// how far above the floor to render the shadow

typedef struct meshubo_s
{
	glmatrix localMatrix;
	float move[4];
	float frontv[4];
	float backv[4];
	float shadelight[4];
	float shadevector[3];
	float lerpfrac;
	float powersuitscale;
	float shellmix;
} meshubo_t;


#define MESH_UBO_MAX_BLOCKS		128

GLuint gl_meshprog = 0;
GLuint gl_meshubo = 0;
GLuint gl_meshubobinding = 0;

int gl_meshuboblocksize = 0;
int gl_meshubonumblocks = 0;

meshubo_t gl_meshuboupdate;

void RMesh_CreatePrograms (void)
{
	glGetIntegerv (GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &gl_meshuboblocksize);

	if (gl_meshuboblocksize < sizeof (meshubo_t))
	{
		int addsize = gl_meshuboblocksize;

		while (gl_meshuboblocksize < sizeof (meshubo_t))
			gl_meshuboblocksize += addsize;
	}

	gl_meshprog = GL_CreateShaderFromName ("glsl/mesh.glsl", "MeshVS", "MeshFS");

	glUniformBlockBinding (gl_meshprog, glGetUniformBlockIndex (gl_meshprog, "MeshUniforms"), gl_meshubobinding);

	glProgramUniform1i (gl_meshprog, glGetUniformLocation (gl_meshprog, "diffuse"), 0);
	glProgramUniform3fv (gl_meshprog, glGetUniformLocation (gl_meshprog, "lightnormal"), 162, (float *) r_avertexnormals);

	glGenBuffers (1, &gl_meshubo);
	glNamedBufferDataEXT (gl_meshubo, MESH_UBO_MAX_BLOCKS * gl_meshuboblocksize, NULL, GL_STREAM_DRAW);
}


/*
=============================================================

 ALIAS MODELS

=============================================================
*/


typedef struct posevert_s
{
	byte position[4];
} posevert_t;


typedef struct mdlst_s
{
	float texcoords[2];
} mdlst_t;


#define VERTOFFSET(fnum) ((fnum) * e->model->numframeverts * sizeof (posevert_t))


/*
=============
GL_DrawAliasFrameLerp

interpolates between two frames and origins
FIXME: batch lerp all vertexes
=============
*/
void GL_DrawAliasFrameLerp (entity_t *e, dmdl_t *hdr, float backlerp, qboolean shadow)
{
	int		i;
	float	frontlerp;
	float	alpha;
	vec3_t	delta, vectors[3];

	daliasframe_t *currframe = (daliasframe_t *) ((byte *) hdr + hdr->ofs_frames + e->currframe * hdr->framesize);
	daliasframe_t *lastframe = (daliasframe_t *) ((byte *) hdr + hdr->ofs_frames + e->lastframe * hdr->framesize);

	meshubo_t *meshubo = NULL;
	GLbitfield access;

	if (e->flags & RF_TRANSLUCENT)
		alpha = e->alpha;
	else alpha = 1.0;

	frontlerp = 1.0 - backlerp;

	// move should be the delta back to the previous frame * backlerp
	VectorSubtract (e->lastorigin, e->currorigin, delta);
	AngleVectors (e->angles, vectors[0], vectors[1], vectors[2]);

	gl_meshuboupdate.move[0] = lastframe->translate[0] + DotProduct (delta, vectors[0]);	// forward
	gl_meshuboupdate.move[1] = lastframe->translate[1] - DotProduct (delta, vectors[1]);	// left
	gl_meshuboupdate.move[2] = lastframe->translate[2] + DotProduct (delta, vectors[2]);	// up

	for (i = 0; i < 3; i++)
	{
		gl_meshuboupdate.move[i] = backlerp * gl_meshuboupdate.move[i] + frontlerp * currframe->translate[i];
		gl_meshuboupdate.frontv[i] = frontlerp * currframe->scale[i];
		gl_meshuboupdate.backv[i] = backlerp * lastframe->scale[i];
	}

	if (!(e->flags & (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE)))
	{
		if (gl_monolightmap->value)
		{
			float ntsc[] = {0.3f, 0.59f, 0.11f};
			float gs = DotProduct (gl_meshuboupdate.shadelight, ntsc);

			gl_meshuboupdate.shadelight[0] = gl_meshuboupdate.shadelight[1] = gl_meshuboupdate.shadelight[2] = gs;
		}

		gl_meshuboupdate.shadelight[0] *= r_lightscale->value * 2.0f;
		gl_meshuboupdate.shadelight[1] *= r_lightscale->value * 2.0f;
		gl_meshuboupdate.shadelight[2] *= r_lightscale->value * 2.0f;

		gl_meshuboupdate.powersuitscale = 0.0f;
		gl_meshuboupdate.shellmix = 0.0f;
	}
	else
	{
		gl_meshuboupdate.powersuitscale = POWERSUIT_SCALE;
		gl_meshuboupdate.shellmix = 1.0f;
	}

	gl_meshuboupdate.shadelight[3] = alpha;
	gl_meshuboupdate.lerpfrac = backlerp;

	if (e->model->lastcurrframe != e->currframe)
	{
		glVertexArrayVertexAttribOffsetEXT (e->model->meshvao, e->model->meshvbo, 0, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof (posevert_t), VERTOFFSET (e->currframe));
		e->model->lastcurrframe = e->currframe;
	}

	if (e->model->lastlastframe != e->lastframe)
	{
		glVertexArrayVertexAttribOffsetEXT (e->model->meshvao, e->model->meshvbo, 1, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof (posevert_t), VERTOFFSET (e->lastframe));
		e->model->lastlastframe = e->lastframe;
	}

	GL_BindVertexArray (e->model->meshvao);
	GL_Enable ((GOURAUD_BIT | DEPTHTEST_BIT) | ((e->flags & RF_TRANSLUCENT) ? BLEND_BIT : DEPTHWRITE_BIT) | (gl_cull->value ? CULLFACE_BIT : 0));
	GL_UseProgram (gl_meshprog);

	if (gl_meshubonumblocks + 1 >= MESH_UBO_MAX_BLOCKS)
	{
		access = BUFFER_DISCARD;
		gl_meshubonumblocks = 0;
	}
	else access = BUFFER_NO_OVERWRITE;

	if ((meshubo = glMapNamedBufferRangeEXT (gl_meshubo, gl_meshubonumblocks * gl_meshuboblocksize, sizeof (meshubo_t), access)) != NULL)
	{
		memcpy (meshubo, &gl_meshuboupdate, sizeof (meshubo_t));

		glUnmapNamedBufferEXT (gl_meshubo);
		glBindBufferRange (GL_UNIFORM_BUFFER, gl_meshubobinding, gl_meshubo, gl_meshubonumblocks * gl_meshuboblocksize, sizeof (meshubo_t));

		glDrawElements (GL_TRIANGLES, e->model->numindexes, GL_UNSIGNED_SHORT, NULL);
		gl_meshubonumblocks++;
	}
}

/*
R_CullAliasModel
*/
static qboolean R_CullAliasModel (vec3_t bbox[8], entity_t *e)
{
	int i;
	vec3_t		mins, maxs;
	dmdl_t		*hdr;
	vec3_t		vectors[3];
	vec3_t		thismins, oldmins, thismaxs, oldmaxs;
	daliasframe_t *pframe, *poldframe;
	vec3_t angles;

	hdr = (dmdl_t *) e->model->extradata;

	if ((e->currframe >= hdr->num_frames) || (e->currframe < 0))
	{
		VID_Printf (PRINT_ALL, "R_CullAliasModel %s: no such frame %d\n", e->model->name, e->currframe);
		e->currframe = 0;
	}

	if ((e->lastframe >= hdr->num_frames) || (e->lastframe < 0))
	{
		VID_Printf (PRINT_ALL, "R_CullAliasModel %s: no such oldframe %d\n", e->model->name, e->lastframe);
		e->lastframe = 0;
	}

	pframe = (daliasframe_t *) ((byte *) hdr + hdr->ofs_frames + e->currframe * hdr->framesize);
	poldframe = (daliasframe_t *) ((byte *) hdr + hdr->ofs_frames + e->lastframe * hdr->framesize);

	// compute axially aligned mins and maxs
	if (pframe == poldframe)
	{
		for (i = 0; i < 3; i++)
		{
			mins[i] = pframe->translate[i];
			maxs[i] = mins[i] + pframe->scale[i] * 255;
		}
	}
	else
	{
		for (i = 0; i < 3; i++)
		{
			thismins[i] = pframe->translate[i];
			thismaxs[i] = thismins[i] + pframe->scale[i] * 255;

			oldmins[i] = poldframe->translate[i];
			oldmaxs[i] = oldmins[i] + poldframe->scale[i] * 255;

			if (thismins[i] < oldmins[i])
				mins[i] = thismins[i];
			else mins[i] = oldmins[i];

			if (thismaxs[i] > oldmaxs[i])
				maxs[i] = thismaxs[i];
			else maxs[i] = oldmaxs[i];
		}
	}

	// compute a full bounding box
	for (i = 0; i < 8; i++)
	{
		vec3_t  tmp;

		if (i & 1) tmp[0] = mins[0]; else tmp[0] = maxs[0];
		if (i & 2) tmp[1] = mins[1]; else tmp[1] = maxs[1];
		if (i & 4) tmp[2] = mins[2]; else tmp[2] = maxs[2];

		VectorCopy (tmp, bbox[i]);
	}

	// rotate the bounding box
	VectorCopy (e->angles, angles);
	angles[YAW] = -angles[YAW];
	AngleVectors (angles, vectors[0], vectors[1], vectors[2]);

	for (i = 0; i < 8; i++)
	{
		vec3_t tmp;

		VectorCopy (bbox[i], tmp);

		bbox[i][0] = DotProduct (vectors[0], tmp);
		bbox[i][1] = -DotProduct (vectors[1], tmp);
		bbox[i][2] = DotProduct (vectors[2], tmp);

		VectorAdd (e->currorigin, bbox[i], bbox[i]);
	}

	{
		int p, f, aggregatemask = ~0;

		for (p = 0; p < 8; p++)
		{
			int mask = 0;

			for (f = 0; f < 4; f++)
			{
				float dp = DotProduct (frustum[f].normal, bbox[p]);

				if ((dp - frustum[f].dist) < 0)
				{
					mask |= (1 << f);
				}
			}

			aggregatemask &= mask;
		}

		if (aggregatemask)
		{
			return true;
		}

		return false;
	}
}

/*
=================
R_DrawAliasModel
=================
*/
void R_DrawAliasModel (entity_t *e)
{
	int			i;
	dmdl_t		*hdr;
	float		an;
	vec3_t		bbox[8];
	image_t		*skin;
	float		lightspot[3];

	if (!(e->flags & RF_WEAPONMODEL))
	{
		if (R_CullAliasModel (bbox, e))
			return;
	}

	if (e->flags & RF_WEAPONMODEL)
	{
		if (r_lefthand->value == 2)
			return;
	}

	hdr = (dmdl_t *) e->model->extradata;

	if (e->flags & (RF_SHELL_GREEN | RF_SHELL_RED | RF_SHELL_BLUE))
	{
		VectorClear (gl_meshuboupdate.shadelight);

		if (e->flags & RF_SHELL_RED) gl_meshuboupdate.shadelight[0] = 1.0;
		if (e->flags & RF_SHELL_GREEN) gl_meshuboupdate.shadelight[1] = 1.0;
		if (e->flags & RF_SHELL_BLUE) gl_meshuboupdate.shadelight[2] = 1.0;
	}
	else if (e->flags & RF_FULLBRIGHT)
	{
		gl_meshuboupdate.shadelight[0] = 1.0;
		gl_meshuboupdate.shadelight[1] = 1.0;
		gl_meshuboupdate.shadelight[2] = 1.0;
	}
	else
	{
		R_LightPoint (e->currorigin, gl_meshuboupdate.shadelight, lightspot);

		// player lighting hack for communication back to server
		// big hack!
		if (e->flags & RF_WEAPONMODEL)
		{
			// pick the greatest component, which should be the same
			// as the mono value returned by software
			if (gl_meshuboupdate.shadelight[0] > gl_meshuboupdate.shadelight[1])
			{
				if (gl_meshuboupdate.shadelight[0] > gl_meshuboupdate.shadelight[2])
					r_lightlevel->value = 150 * gl_meshuboupdate.shadelight[0];
				else r_lightlevel->value = 150 * gl_meshuboupdate.shadelight[2];
			}
			else
			{
				if (gl_meshuboupdate.shadelight[1] > gl_meshuboupdate.shadelight[2])
					r_lightlevel->value = 150 * gl_meshuboupdate.shadelight[1];
				else r_lightlevel->value = 150 * gl_meshuboupdate.shadelight[2];
			}
		}
	}

	if (e->flags & RF_MINLIGHT)
	{
		for (i = 0; i < 3; i++)
			if (gl_meshuboupdate.shadelight[i] > 0.1)
				break;

		if (i == 3)
		{
			gl_meshuboupdate.shadelight[0] = 0.1;
			gl_meshuboupdate.shadelight[1] = 0.1;
			gl_meshuboupdate.shadelight[2] = 0.1;
		}
	}

	if (e->flags & RF_GLOW)
	{
		// bonus items will pulse with time
		float	scale;
		float	min;

		scale = 0.1 * sin (r_newrefdef.time * 7);

		for (i = 0; i < 3; i++)
		{
			min = gl_meshuboupdate.shadelight[i] * 0.8;
			gl_meshuboupdate.shadelight[i] += scale;

			if (gl_meshuboupdate.shadelight[i] < min)
				gl_meshuboupdate.shadelight[i] = min;
		}
	}

	an = e->angles[1] / 180 * M_PI;
	Q_sincos (-an, &gl_meshuboupdate.shadevector[1], &gl_meshuboupdate.shadevector[0]);
	gl_meshuboupdate.shadevector[2] = 1;
	VectorNormalize (gl_meshuboupdate.shadevector);

	// locate the proper data
	c_alias_polys += hdr->num_tris;

	// draw all the triangles
	if (e->flags & RF_DEPTHHACK) // hack the depth range to prevent view model from poking into walls
		glDepthRange (gldepthmin, gldepthmin + 0.3 * (gldepthmax - gldepthmin));

	if ((e->flags & RF_WEAPONMODEL) && (r_lefthand->value == 1.0F))
	{
		glmatrix gunmatrix;

		GL_LoadIdentity (&gunmatrix);
		GL_ScaleMatrix (&gunmatrix, -1, 1, 1);
		GL_PerspectiveMatrix (&gunmatrix, r_newrefdef.fov_y, (float) r_newrefdef.width / r_newrefdef.height, 4, 4096);

		// eval a new mvp for left-handedness
		GL_LoadMatrix (&gl_meshuboupdate.localMatrix, &r_worldmatrix);
		GL_MultMatrix (&gl_meshuboupdate.localMatrix, &gl_meshuboupdate.localMatrix, &gunmatrix);

		glCullFace (GL_BACK);
	}
	else
		GL_LoadMatrix (&gl_meshuboupdate.localMatrix, &r_mvpmatrix);

	GL_TranslateMatrix (&gl_meshuboupdate.localMatrix, e->currorigin[0], e->currorigin[1], e->currorigin[2]);
	GL_RotateMatrix (&gl_meshuboupdate.localMatrix, e->angles[1], 0, 0, 1);
	GL_RotateMatrix (&gl_meshuboupdate.localMatrix, e->angles[0], 0, 1, 0);
	GL_RotateMatrix (&gl_meshuboupdate.localMatrix, -e->angles[2], 1, 0, 0);

	// select skin
	if (e->skin)
		skin = e->skin;	// custom player skin
	else
	{
		if (e->skinnum >= MAX_MD2SKINS)
			skin = e->model->skins[0];
		else
		{
			skin = e->model->skins[e->skinnum];

			if (!skin)
				skin = e->model->skins[0];
		}
	}

	if (!skin)
		skin = r_notexture;	// fallback...

	GL_BindTexture (GL_TEXTURE0, GL_TEXTURE_2D, r_modelsampler, skin->texnum);

	if ((e->currframe >= hdr->num_frames) || (e->currframe < 0))
	{
		VID_Printf (PRINT_ALL, "R_DrawAliasModel %s: no such frame %d\n", e->model->name, e->currframe);
		e->currframe = 0;
		e->lastframe = 0;
	}

	if ((e->lastframe >= hdr->num_frames) || (e->lastframe < 0))
	{
		VID_Printf (PRINT_ALL, "R_DrawAliasModel %s: no such oldframe %d\n", e->model->name, e->lastframe);
		e->currframe = 0;
		e->lastframe = 0;
	}

	if (!r_lerpmodels->value)
		e->backlerp = 0;

	GL_DrawAliasFrameLerp (e, hdr, e->backlerp, false);

	if ((e->flags & RF_WEAPONMODEL) && (r_lefthand->value == 1.0F))
	{
		glCullFace (GL_FRONT);
	}

	if (e->flags & RF_DEPTHHACK)
		glDepthRange (gldepthmin, gldepthmax);

	if (gl_shadows->value && !(e->flags & (RF_TRANSLUCENT | RF_WEAPONMODEL | RF_NOSHADOW)))
	{
		glmatrix shadowmatrix;

		// recreate the local matrix for the model, this time appropriately for shadows
		GL_LoadIdentity(&gl_meshuboupdate.localMatrix);

		// run the scale first which is the same as using a depth hack in our shader
		if (e->flags & RF_DEPTHHACK)
			GL_ScaleMatrix(&gl_meshuboupdate.localMatrix, 1.0f, 1.0f, 0.3f);

		float lheight = e->currorigin[2] - lightspot[2];

		GL_MultMatrix(&gl_meshuboupdate.localMatrix, &gl_meshuboupdate.localMatrix, &r_worldmatrix);
		GL_TranslateMatrix(&gl_meshuboupdate.localMatrix, e->currorigin[0], e->currorigin[1], e->currorigin[2]);
		GL_TranslateMatrix(&gl_meshuboupdate.localMatrix, 0.0f, 0.0f, -lheight);

		shadowmatrix.m[0][0] = 1;
		shadowmatrix.m[0][1] = 0;
		shadowmatrix.m[0][2] = 0;
		shadowmatrix.m[0][3] = 0;

		shadowmatrix.m[1][0] = 0;
		shadowmatrix.m[1][1] = 1;
		shadowmatrix.m[1][2] = 0;
		shadowmatrix.m[1][3] = 0;

		shadowmatrix.m[2][0] = SHADOW_SKEW_X;
		shadowmatrix.m[2][1] = SHADOW_SKEW_Y;
		shadowmatrix.m[2][2] = SHADOW_VSCALE;
		shadowmatrix.m[2][3] = 0;

		shadowmatrix.m[3][0] = 0;
		shadowmatrix.m[3][1] = 0;
		shadowmatrix.m[3][2] = SHADOW_HEIGHT;
		shadowmatrix.m[3][3] = 1;

		GL_MultMatrix(&gl_meshuboupdate.localMatrix, &gl_meshuboupdate.localMatrix, &shadowmatrix);

		GL_TranslateMatrix(&gl_meshuboupdate.localMatrix, 0.0f, 0.0f, lheight);
		GL_RadianRotateMatrix(&gl_meshuboupdate.localMatrix, e->angles[YAW], e->angles[PITCH], -e->angles[ROLL]);

		// shadow pass
		GL_DrawAliasFrameLerp(e, hdr, e->backlerp, true);
	}
}


/*
==============================================================================

	BUFFER ALLOCATION

==============================================================================
*/

void RMesh_CreateFrames (model_t *mod, dmdl_t *hdr)
{
	int i, frame;
	posevert_t *verts = (posevert_t *) Scratch_Alloc ();
	int numverts = 0;

	glGenBuffers (1, &mod->meshvbo);

	for (frame = 0; frame < hdr->num_frames; frame++)
	{
		int *order = (int *) ((byte *) hdr + hdr->ofs_glcmds);
		dtrivertx_t *vert = ((daliasframe_t *) ((byte *) hdr + hdr->ofs_frames + frame * hdr->framesize))->verts;

		while (1)
		{
			// get the vertex count and primitive type
			int count = *order++;

			if (!count) break;
			if (count < 0) count = -count;

			for (i = 0; i < count; i++, verts++, order += 3)
			{
				verts->position[0] = vert[order[2]].v[0];
				verts->position[1] = vert[order[2]].v[1];
				verts->position[2] = vert[order[2]].v[2];
				verts->position[3] = vert[order[2]].lightnormalindex;
			}

			numverts += count;
		}
	}

	glNamedBufferDataEXT (mod->meshvbo, numverts * sizeof (posevert_t), Scratch_Alloc (), GL_STATIC_DRAW);
}


void RMesh_CreateTexCoords (model_t *mod, dmdl_t *hdr)
{
	mdlst_t *st = (mdlst_t *) Scratch_Alloc ();
	int *order = (int *) ((byte *) hdr + hdr->ofs_glcmds);

	mod->numframeverts = 0;

	glGenBuffers (1, &mod->texcoordvbo);

	while (1)
	{
		// get the vertex count and primitive type
		int count = *order++;
		int i;

		if (!count) break;
		if (count < 0) count = -count;

		for (i = 0; i < count; i++, st++, order += 3)
		{
			st->texcoords[0] = ((float *) order)[0];
			st->texcoords[1] = ((float *) order)[1];
		}

		mod->numframeverts += count;
	}

	glNamedBufferDataEXT (mod->texcoordvbo, mod->numframeverts * sizeof (mdlst_t), Scratch_Alloc (), GL_STATIC_DRAW);
}


void RMesh_CreateIndexes (model_t *mod, dmdl_t *hdr)
{
	unsigned short *ndx = (unsigned short *) Scratch_Alloc ();
	int *order = (int *) ((byte *) hdr + hdr->ofs_glcmds);
	int firstvertex = 0;

	mod->numindexes = 0;

	glGenBuffers (1, &mod->indexbuffer);

	while (1)
	{
		// get the vertex count and primitive type
		int count = *order++;
		int i;

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
		mod->numindexes += (count - 2) * 3;
	}

	glNamedBufferDataEXT (mod->indexbuffer, mod->numindexes * sizeof (unsigned short), Scratch_Alloc (), GL_STATIC_DRAW);
}


void RMesh_MakeVertexBuffers (model_t *mod)
{
	// because the buffers may have been used by a previous model
	if (mod->meshvbo) glDeleteBuffers (1, &mod->meshvbo);
	if (mod->texcoordvbo) glDeleteBuffers (1, &mod->texcoordvbo);
	if (mod->indexbuffer) glDeleteBuffers (1, &mod->indexbuffer);
	if (mod->meshvao) glDeleteVertexArrays (1, &mod->meshvao);

	RMesh_CreateFrames (mod, (dmdl_t *) mod->extradata);
	RMesh_CreateTexCoords (mod, (dmdl_t *) mod->extradata);
	RMesh_CreateIndexes (mod, (dmdl_t *) mod->extradata);

	glGenVertexArrays (1, &mod->meshvao);

	glEnableVertexArrayAttribEXT (mod->meshvao, 0);
	glVertexArrayVertexAttribOffsetEXT (mod->meshvao, mod->meshvbo, 0, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof (posevert_t), 0);

	glEnableVertexArrayAttribEXT (mod->meshvao, 1);
	glVertexArrayVertexAttribOffsetEXT (mod->meshvao, mod->meshvbo, 1, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof (posevert_t), 0);

	glEnableVertexArrayAttribEXT (mod->meshvao, 2);
	glVertexArrayVertexAttribOffsetEXT (mod->meshvao, mod->texcoordvbo, 2, 2, GL_FLOAT, GL_FALSE, 0, 0);

	mod->lastcurrframe = 0;
	mod->lastlastframe = 0;

	GL_BindVertexArray (mod->meshvao);
	glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, mod->indexbuffer);
}


