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


GLuint gl_spriteprog = 0;
GLuint u_spritecolour = 0;
GLuint u_spriteorigin = 0;
GLuint gl_spriteubobinding = 0;


typedef struct spritevert_s
{
	float position[2];
	float texcoord[2];
} spritevert_t;


void Sprite_CreatePrograms (void)
{
	gl_spriteprog = GL_CreateShaderFromResource (IDR_SPRITE, "SpriteVS", "SpriteFS");

	u_spritecolour = glGetUniformLocation (gl_spriteprog, "colour");
	u_spriteorigin = glGetUniformLocation (gl_spriteprog, "entOrigin");
	glProgramUniform1i (gl_spriteprog, glGetUniformLocation (gl_spriteprog, "diffuse"), 0);
	glUniformBlockBinding (gl_spriteprog, glGetUniformBlockIndex (gl_spriteprog, "GlobalUniforms"), gl_spriteubobinding);
}


/*
=================
R_DrawSpriteModel

=================
*/
void R_DrawSpriteModel (entity_t *e)
{
	model_t *mod = e->model;
	dsprite_t *psprite = (dsprite_t *) mod->extradata;
	spritevert_t *sv = NULL;
	ubodef_t ubodef = {gl_sharedubo, gl_spriteubobinding};

	// don't draw invisible sprites
	if (e->alpha <= 0.0f) return;

	// don't even bother culling, because it's just a single
	// polygon without a surface cache
	e->currframe %= psprite->numframes;

	GL_UseProgramWithUBOs (gl_spriteprog, &ubodef, 1);

	glProgramUniform3fv (gl_spriteprog, u_spriteorigin, 1, e->currorigin);
	glProgramUniform4f (gl_spriteprog, u_spritecolour, 1, 1, 1, ((e->flags & RF_TRANSLUCENT) && e->alpha < 1.0f) ? e->alpha : 1);

	GL_Enable (BLEND_BIT | DEPTHTEST_BIT | (gl_cull->value ? CULLFACE_BIT : 0));
	GL_BindTexture (GL_TEXTURE0, GL_TEXTURE_2D, r_modelsampler, mod->skins[e->currframe]->texnum);

	GL_BindVertexArray (mod->spritevao);
	glDrawArrays (GL_TRIANGLE_STRIP, e->currframe * 4, 4);
}


void Sprite_MakeVertexBuffers (model_t *mod)
{
	int i;
	dsprite_t *psprite = (dsprite_t *) mod->extradata;

	glDeleteBuffers (1, &mod->spritevbo);
	glGenBuffers (1, &mod->spritevbo);
	glNamedBufferDataEXT (mod->spritevbo, psprite->numframes * 4 * sizeof (spritevert_t), NULL, GL_STATIC_DRAW);

	for (i = 0; i < psprite->numframes; i++)
	{
		spritevert_t verts[4];
		dsprframe_t	*frame = &psprite->frames[i];

		verts[0].position[0] = -frame->origin_y;
		verts[0].position[1] = -frame->origin_x;
		verts[0].texcoord[0] = 0;
		verts[0].texcoord[1] = 1;

		verts[1].position[0] = frame->height - frame->origin_y;
		verts[1].position[1] = -frame->origin_x;
		verts[1].texcoord[0] = 0;
		verts[1].texcoord[1] = 0;

		verts[2].position[0] = -frame->origin_y;
		verts[2].position[1] = frame->width - frame->origin_x;
		verts[2].texcoord[0] = 1;
		verts[2].texcoord[1] = 1;

		verts[3].position[0] = frame->height - frame->origin_y;
		verts[3].position[1] = frame->width - frame->origin_x;
		verts[3].texcoord[0] = 1;
		verts[3].texcoord[1] = 0;

		glNamedBufferSubDataEXT (mod->spritevbo, i * 4 * sizeof (spritevert_t), sizeof (spritevert_t) * 4, verts);
	}

	glDeleteVertexArrays (1, &mod->spritevao);
	glGenVertexArrays (1, &mod->spritevao);

	glEnableVertexArrayAttribEXT (mod->spritevao, 0);
	glVertexArrayVertexAttribOffsetEXT (mod->spritevao, mod->spritevbo, 0, 2, GL_FLOAT, GL_FALSE, sizeof (spritevert_t), 0);

	glEnableVertexArrayAttribEXT (mod->spritevao, 1);
	glVertexArrayVertexAttribOffsetEXT (mod->spritevao, mod->spritevbo, 1, 2, GL_FLOAT, GL_FALSE, sizeof (spritevert_t), 8);
}


