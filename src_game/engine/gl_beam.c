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
	0.000000, 1.000000, 1.000000, 0.000000, 1.000000, 0.000000, 0.083678, 0.996493, 1.000000, 0.083678, 0.996493, 0.000000, 0.166769, 0.985996, 1.000000, 0.166769, 0.985996, 0.000000,
	0.248690, 0.968583, 1.000000, 0.248690, 0.968583, 0.000000, 0.328867, 0.944376, 1.000000, 0.328867, 0.944376, 0.000000, 0.406737, 0.913545, 1.000000, 0.406737, 0.913545, 0.000000,
	0.481754, 0.876307, 1.000000, 0.481754, 0.876307, 0.000000, 0.553392, 0.832921, 1.000000, 0.553392, 0.832921, 0.000000, 0.621148, 0.783693, 1.000000, 0.621148, 0.783693, 0.000000,
	0.684547, 0.728969, 1.000000, 0.684547, 0.728969, 0.000000, 0.743145, 0.669131, 1.000000, 0.743145, 0.669131, 0.000000, 0.796530, 0.604599, 1.000000, 0.796530, 0.604599, 0.000000,
	0.844328, 0.535827, 1.000000, 0.844328, 0.535827, 0.000000, 0.886204, 0.463296, 1.000000, 0.886204, 0.463296, 0.000000, 0.921863, 0.387516, 1.000000, 0.921863, 0.387516, 0.000000,
	0.951057, 0.309017, 1.000000, 0.951057, 0.309017, 0.000000, 0.973579, 0.228351, 1.000000, 0.973579, 0.228351, 0.000000, 0.989272, 0.146083, 1.000000, 0.989272, 0.146083, 0.000000,
	0.998027, 0.062790, 1.000000, 0.998027, 0.062790, 0.000000, 0.999781, -0.020942, 1.000000, 0.999781, -0.020942, 0.000000, 0.994522, -0.104529, 1.000000, 0.994522, -0.104529,
	0.000000, 0.982287, -0.187381, 1.000000, 0.982287, -0.187381, 0.000000, 0.963163, -0.268920, 1.000000, 0.963163, -0.268920, 0.000000, 0.937282, -0.348572, 1.000000, 0.937282,
	-0.348572, 0.000000, 0.904827, -0.425779, 1.000000, 0.904827, -0.425779, 0.000000, 0.866025, -0.500000, 1.000000, 0.866025, -0.500000, 0.000000, 0.821149, -0.570714, 1.000000,
	0.821149, -0.570714, 0.000000, 0.770513, -0.637424, 1.000000, 0.770513, -0.637424, 0.000000, 0.714473, -0.699663, 1.000000, 0.714473, -0.699663, 0.000000, 0.653421, -0.756995,
	1.000000, 0.653421, -0.756995, 0.000000, 0.587785, -0.809017, 1.000000, 0.587785, -0.809017, 0.000000, 0.518027, -0.855364, 1.000000, 0.518027, -0.855364, 0.000000, 0.444635,
	-0.895712, 1.000000, 0.444635, -0.895712, 0.000000, 0.368125, -0.929776, 1.000000, 0.368125, -0.929776, 0.000000, 0.289032, -0.957319, 1.000000, 0.289032, -0.957319, 0.000000,
	0.207912, -0.978148, 1.000000, 0.207912, -0.978148, 0.000000, 0.125333, -0.992115, 1.000000, 0.125333, -0.992115, 0.000000, 0.041876, -0.999123, 1.000000, 0.041876, -0.999123,
	0.000000, -0.041876, -0.999123, 1.000000, -0.041876, -0.999123, 0.000000, -0.125333, -0.992115, 1.000000, -0.125333, -0.992115, 0.000000, -0.207912, -0.978148, 1.000000, -0.207912,
	-0.978148, 0.000000, -0.289032, -0.957319, 1.000000, -0.289032, -0.957319, 0.000000, -0.368125, -0.929776, 1.000000, -0.368125, -0.929776, 0.000000, -0.444635, -0.895712, 1.000000,
	-0.444635, -0.895712, 0.000000, -0.518027, -0.855364, 1.000000, -0.518027, -0.855364, 0.000000, -0.587785, -0.809017, 1.000000, -0.587785, -0.809017, 0.000000, -0.653421, -0.756995,
	1.000000, -0.653421, -0.756995, 0.000000, -0.714473, -0.699663, 1.000000, -0.714473, -0.699663, 0.000000, -0.770513, -0.637424, 1.000000, -0.770513, -0.637424, 0.000000, -0.821149,
	-0.570714, 1.000000, -0.821149, -0.570714, 0.000000, -0.866025, -0.500000, 1.000000, -0.866025, -0.500000, 0.000000, -0.904827, -0.425779, 1.000000, -0.904827, -0.425779, 0.000000,
	-0.937282, -0.348572, 1.000000, -0.937282, -0.348572, 0.000000, -0.963163, -0.268920, 1.000000, -0.963163, -0.268920, 0.000000, -0.982287, -0.187381, 1.000000, -0.982287, -0.187381,
	0.000000, -0.994522, -0.104528, 1.000000, -0.994522, -0.104528, 0.000000, -0.999781, -0.020943, 1.000000, -0.999781, -0.020943, 0.000000, -0.998027, 0.062791, 1.000000, -0.998027,
	0.062791, 0.000000, -0.989272, 0.146083, 1.000000, -0.989272, 0.146083, 0.000000, -0.973579, 0.228351, 1.000000, -0.973579, 0.228351, 0.000000, -0.951056, 0.309017, 1.000000,
	-0.951056, 0.309017, 0.000000, -0.921863, 0.387515, 1.000000, -0.921863, 0.387515, 0.000000, -0.886204, 0.463296, 1.000000, -0.886204, 0.463296, 0.000000, -0.844328, 0.535827,
	1.000000, -0.844328, 0.535827, 0.000000, -0.796530, 0.604599, 1.000000, -0.796530, 0.604599, 0.000000, -0.743145, 0.669131, 1.000000, -0.743145, 0.669131, 0.000000, -0.684547,
	0.728969, 1.000000, -0.684547, 0.728969, 0.000000, -0.621148, 0.783693, 1.000000, -0.621148, 0.783693, 0.000000, -0.553392, 0.832921, 1.000000, -0.553392, 0.832921, 0.000000,
	-0.481754, 0.876307, 1.000000, -0.481754, 0.876307, 0.000000, -0.406736, 0.913546, 1.000000, -0.406736, 0.913546, 0.000000, -0.328867, 0.944376, 1.000000, -0.328867, 0.944376,
	0.000000, -0.248690, 0.968583, 1.000000, -0.248690, 0.968583, 0.000000, -0.166769, 0.985996, 1.000000, -0.166769, 0.985996, 0.000000, -0.083678, 0.996493, 1.000000, -0.083678,
	0.996493, 0.000000, 0.000000, 1.000000, 1.000000, 0.000000, 1.000000, 0.000000
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

