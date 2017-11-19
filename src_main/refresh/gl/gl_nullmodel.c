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

#define NUM_NULL_VERTS		12

// these are the null model verts dumped to file
float nullverts[NUM_NULL_VERTS * 3] =
{
	0.000000, 0.000000, -16.000000, 16.000000, 0.000000, 0.000000, 0.000000, 16.000000, 0.000000, -16.000000, 0.000000, 0.000000, -0.000000, -16.000000, 0.000000, 16.000000,
	-0.000000, 0.000000, 0.000000, 0.000000, 16.000000, 16.000000, -0.000000, 0.000000, -0.000000, -16.000000, 0.000000, -16.000000, 0.000000, 0.000000, 0.000000, 16.000000,
	0.000000, 16.000000, 0.000000, 0.000000, 
};


GLuint gl_nullprog = 0;
GLuint u_nullmatrix = 0;
GLuint u_nullcolour = 0;
GLuint u_nullscaling = 0;

GLuint r_nullvbo = 0;
GLuint r_nullvao = 0;


void RNull_CreatePrograms (void)
{
	if (sizeof (nullverts) != NUM_NULL_VERTS * 3 * sizeof (float))
		VID_Error (ERR_FATAL, "sizeof (nullverts) != NUM_NULL_VERTS * 3 * sizeof (float)");

	glGenBuffers (1, &r_nullvbo);
	glNamedBufferDataEXT (r_nullvbo, NUM_NULL_VERTS * 3 * sizeof (float), nullverts, GL_STATIC_DRAW);

	gl_nullprog = GL_CreateShaderFromName ("glsl/untextured.glsl", "UntexturedVS", "UntexturedFS");

	u_nullmatrix = glGetUniformLocation (gl_nullprog, "localMatrix");
	u_nullcolour = glGetUniformLocation (gl_nullprog, "color");
	u_nullscaling = glGetUniformLocation (gl_nullprog, "scale");

	glGenVertexArrays (1, &r_nullvao);
	glEnableVertexArrayAttribEXT (r_nullvao, 0);
	glVertexArrayVertexAttribOffsetEXT (r_nullvao, r_nullvbo, 0, 3, GL_FLOAT, GL_FALSE, 0, 0);
}


/*
=============
R_DrawNullModel
=============
*/
void R_DrawNullModel (entity_t *e)
{
	vec3_t	shadelight;

	if (e->flags & RF_FULLBRIGHT)
		shadelight[0] = shadelight[1] = shadelight[2] = 1.0f;
	else R_LightPoint (e->currorigin, shadelight);

	GL_LoadMatrix (&e->matrix, &r_mvpmatrix);

	GL_TranslateMatrix (&e->matrix, e->currorigin[0], e->currorigin[1], e->currorigin[2]);
	GL_RotateMatrix (&e->matrix, e->angles[1], 0, 0, 1);
	GL_RotateMatrix (&e->matrix, -e->angles[0], 0, 1, 0);
	GL_RotateMatrix (&e->matrix, -e->angles[2], 1, 0, 0);

	glProgramUniformMatrix4fv (gl_nullprog, u_nullmatrix, 1, GL_FALSE, e->matrix.m[0]);
	glProgramUniform4f (gl_nullprog, u_nullcolour, shadelight[0], shadelight[1], shadelight[2], e->alpha);
	glProgramUniform3f (gl_nullprog, u_nullscaling, 1, 1, 1);

	GL_UseProgram (gl_nullprog);
	GL_BindVertexArray (r_nullvao);

	glDrawArrays (GL_TRIANGLE_FAN, 0, NUM_NULL_VERTS / 2);
	glDrawArrays (GL_TRIANGLE_FAN, 6, NUM_NULL_VERTS / 2);
}


