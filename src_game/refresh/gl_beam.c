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

// draw.c

#include "gl_local.h"

#define NUM_CYLINDER_VERTS	152

// this is a dump from a radius 1, 75 slices, 1 stacks gluCylinder call and it's rotated, positioned and scaled on the GPU
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

GLuint gl_beamprog = 0;
GLuint u_beammatrix = 0;
GLuint u_beamcolour = 0;
GLuint u_beamscaling = 0;

GLuint r_beamvbo = 0;
GLuint r_beamvao = 0;


void RBeam_CreatePrograms (void)
{
	if (sizeof (cylinderverts) != NUM_CYLINDER_VERTS * 3 * sizeof (float))
		VID_Error (ERR_FATAL, "sizeof (cylinderverts) != NUM_CYLINDER_VERTS * 3 * sizeof (float)");

	glGenBuffers (1, &r_beamvbo);
	glNamedBufferDataEXT (r_beamvbo, NUM_CYLINDER_VERTS * 3 * sizeof (float), cylinderverts, GL_STATIC_DRAW);

	gl_beamprog = GL_CreateShaderFromName("glsl/untextured.glsl", "UntexturedVS", "UntexturedFS");

	u_beammatrix = glGetUniformLocation (gl_beamprog, "localMatrix");
	u_beamcolour = glGetUniformLocation (gl_beamprog, "color");
	u_beamscaling = glGetUniformLocation (gl_beamprog, "scale");

	glGenVertexArrays (1, &r_beamvao);
	glEnableVertexArrayAttribEXT (r_beamvao, 0);
	glVertexArrayVertexAttribOffsetEXT (r_beamvao, r_beamvbo, 0, 3, GL_FLOAT, GL_FALSE, 0, 0);
}


/*
=============
R_DrawBeam
=============
*/
void R_DrawBeam (entity_t *e)
{
	float dir[3], len, ang, rad;
	const float beam_epsilon = 0.000001f;

	VectorSubtract (e->lastorigin, e->currorigin, dir);

	// catch 0 length beams
	if ((len = DotProduct (dir, dir)) < beam_epsilon) return;

	// handle the degenerate case of z1 == z2 with an approximation
	// (should we not just rotate around a different axis instead?)
	if (fabs (dir[2]) < beam_epsilon) dir[2] = beam_epsilon;

	// we already got the dotproduct so we just need to sqrt it here
	ang = 57.2957795f * acos (dir[2] / (len = sqrt (len))) * ((dir[2] < 0.0f) ? -1.0f : 1.0f);

	// get proper radius
	rad = (float) e->currframe / 2.0f;

	// we can't scale the matrix as scaling needs to be done before the matrix multiply so we do it separately in the vs instead
	GL_LoadMatrix (&e->matrix, &r_mvpmatrix);
	GL_TranslateMatrix (&e->matrix, e->currorigin[0], e->currorigin[1], e->currorigin[2]);
	GL_RotateMatrix (&e->matrix, ang, -dir[1] * dir[2], dir[0] * dir[2], 0.0f);

	glProgramUniformMatrix4fv (gl_beamprog, u_beammatrix, 1, GL_FALSE, e->matrix.m[0]);
	glProgramUniform3f (gl_beamprog, u_beamscaling, rad, rad, len);

	glProgramUniform4f (gl_beamprog,
		u_beamcolour,
		((byte *) &d_8to24table_rgba[e->skinnum & 0xff])[0] / 255.0f,
		((byte *) &d_8to24table_rgba[e->skinnum & 0xff])[1] / 255.0f,
		((byte *) &d_8to24table_rgba[e->skinnum & 0xff])[2] / 255.0f,
		e->alpha);

	// and now we can draw
	GL_Enable (BLEND_BIT | DEPTHTEST_BIT | (gl_cull->value ? CULLFACE_BIT : 0));
	GL_UseProgram (gl_beamprog);

	// the cylinder geometry came from gluCylinder which in my implementation used GL_QUAD_STRIP so this keeps it consistent
	GL_BindVertexArray (r_beamvao);
	glDrawArrays (GL_QUAD_STRIP, 0, NUM_CYLINDER_VERTS);
}

