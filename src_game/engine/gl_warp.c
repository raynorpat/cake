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
// gl_warp.c -- sky and water polygons

#include "gl_local.h"


GLuint gl_warpsurfprog = 0;
GLuint u_warpmatrix = 0;
GLuint u_warpparams = 0;
GLuint u_warpalpha = 0;
GLuint u_warpscroll = 0;

void RWarp_CreatePrograms (void)
{
	gl_warpsurfprog = GL_CreateShaderFromName ("glsl/liquid.glsl", "LiquidVS", "LiquidFS");

	u_warpmatrix = glGetUniformLocation (gl_warpsurfprog, "localMatrix");
	u_warpparams = glGetUniformLocation (gl_warpsurfprog, "warpparams");
	u_warpalpha = glGetUniformLocation (gl_warpsurfprog, "surfalpha");
	u_warpscroll = glGetUniformLocation (gl_warpsurfprog, "scroll");

	glProgramUniform1i (gl_warpsurfprog, glGetUniformLocation (gl_warpsurfprog, "diffuse"), 0);
}


void R_BeginWaterPolys (glmatrix *matrix, float alpha)
{
	glProgramUniformMatrix4fv (gl_warpsurfprog, u_warpmatrix, 1, GL_FALSE, matrix->m[0]);
	glProgramUniform1f (gl_warpsurfprog, u_warpalpha, alpha);

	GL_UseProgram (gl_warpsurfprog);
}


void RWarp_BeginFrame (void)
{
	float warpparams[4];
	float waterflow = 0;

	warpparams[0] = r_newrefdef.time * (128.0 / M_PI);
	warpparams[1] = 2.0f;
	warpparams[2] = M_PI / 128.0;
	warpparams[3] = 8.0f;

	glProgramUniform4fv (gl_warpsurfprog, u_warpparams, 1, warpparams);

	// approximate match to the speed of the current when you're in water
	waterflow = -64 * ((r_newrefdef.time / 32.0) - (int) (r_newrefdef.time / 32.0));

	while (waterflow >= 0.0)
		waterflow -= 64.0;

	glProgramUniform1f (gl_warpsurfprog, u_warpscroll, waterflow);
}

