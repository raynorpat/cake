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

GLuint gl_particleprog = 0;
GLuint gl_particlevbo = 0;
GLuint gl_particlevao = 0;
GLuint gl_particleubobinding = 0;

int r_firstparticle = 0;


void RPart_CreatePrograms (void)
{
	glGenBuffers (1, &gl_particlevbo);
	glNamedBufferDataEXT (gl_particlevbo, MAX_PARTICLES * sizeof (particle_t), NULL, GL_STREAM_DRAW);

	gl_particleprog = GL_CreateShaderFromName ("glsl/particles.glsl", "ParticleVS", "ParticleFS");
	r_firstparticle = 0;

	glUniformBlockBinding (gl_particleprog, glGetUniformBlockIndex (gl_particleprog, "GlobalUniforms"), gl_particleubobinding);

	glGenVertexArrays (1, &gl_particlevao);

	glEnableVertexArrayAttribEXT (gl_particlevao, 0);
	glVertexArrayVertexAttribOffsetEXT (gl_particlevao, gl_particlevbo, 0, 3, GL_FLOAT, GL_FALSE, sizeof (particle_t), 0);

	glEnableVertexArrayAttribEXT (gl_particlevao, 1);
	glVertexArrayVertexAttribOffsetEXT (gl_particlevao, gl_particlevbo, 1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof (particle_t), 12);

	GL_BindVertexArray (gl_particlevao);
	glVertexAttribDivisor (0, 1);
	glVertexAttribDivisor (1, 1);
}


/*
===============
R_DrawParticles
===============
*/
void R_DrawParticles (void)
{
	GLvoid *dst = NULL;
	int offset, size;
	ubodef_t ubodef = {gl_sharedubo, gl_particleubobinding};

	if (!r_newrefdef.num_particles) return;

	if (r_firstparticle + r_newrefdef.num_particles >= MAX_PARTICLES)
	{
		glNamedBufferDataEXT (gl_particlevbo, MAX_PARTICLES * sizeof (particle_t), NULL, GL_STREAM_DRAW);
		r_firstparticle = 0;
	}

	offset = r_firstparticle * sizeof (particle_t);
	size = r_newrefdef.num_particles * sizeof (particle_t);

	if ((dst = glMapNamedBufferRangeEXT (gl_particlevbo, offset, size, BUFFER_NO_OVERWRITE)) != NULL)
	{
		memcpy (dst, r_newrefdef.particles, size);
		glUnmapNamedBufferEXT (gl_particlevbo);
	}
	else glNamedBufferSubDataEXT (gl_particlevbo, offset, size, r_newrefdef.particles);

	GL_UseProgramWithUBOs (gl_particleprog, &ubodef, 1);
	GL_Enable ((BLEND_BIT | DEPTHTEST_BIT) | (gl_cull->value ? CULLFACE_BIT : 0));
	GL_BindVertexArray (gl_particlevao);

	glDrawArraysInstancedBaseInstance (GL_TRIANGLE_FAN, 0, 4, r_newrefdef.num_particles, r_firstparticle);

	r_firstparticle += r_newrefdef.num_particles;
}
