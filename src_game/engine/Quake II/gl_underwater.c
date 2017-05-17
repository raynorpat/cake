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


GLuint r_waterwarptexture;
GLuint r_warpgradient;

GLuint gl_underwaterprog = 0;
GLuint u_uwwarpparams = 0;
GLuint u_uwsurfcolor = 0;


float r_warpmaxs;
float r_warpmaxt;

typedef struct wwvert_s
{
	float position[2];
	float texcoord[2];
} wwvert_t;

GLuint r_underwatervbo = 0;
GLuint r_underwatervao = 0;
GLuint r_underwaterfbo = 0;

void RUnderwater_CreatePrograms (void)
{
	GLuint rboId;
	wwvert_t wwverts[4];
	byte *data = NULL;
	int width, height;

	LoadTGAFile("env/warpgradient.tga", &data, &width, &height);
	if (data)
		r_warpgradient = GL_UploadTexture (data, width, height, false, 32);

	gl_underwaterprog = GL_CreateShaderFromResource (IDR_UNDERWATER, "WaterWarpVS", "WaterWarpFS");

	// create a texture for use by the FBO
	glDeleteTextures (1, &r_waterwarptexture);
	glGenTextures (1, &r_waterwarptexture);

	glTextureStorage2DEXT (r_waterwarptexture, GL_TEXTURE_2D, 1, GL_RGBA8, vid.width, vid.height);

	// create a renderbuffer object to store depth info
	// whoever on the ARB thought it was a good idea to not allow using the default system-provided depth buffer needs to be shot
	glDeleteRenderbuffers (1, &rboId);
	glGenRenderbuffers (1, &rboId);
	glBindRenderbuffer (GL_RENDERBUFFER, rboId);
	glRenderbufferStorage (GL_RENDERBUFFER, GL_DEPTH_COMPONENT, vid.width, vid.height);
	glBindRenderbuffer (GL_RENDERBUFFER, 0);

	// create a framebuffer object
	glDeleteFramebuffers (1, &r_underwaterfbo);
	glGenFramebuffers (1, &r_underwaterfbo);
	glBindFramebuffer (GL_DRAW_FRAMEBUFFER, r_underwaterfbo);

	// attach the texture to FBO color attachment point
	glFramebufferTexture2D (GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, r_waterwarptexture, 0);

	// fixme
	//if (glw_state.pfd.cStencilBits)
	//	;
	// attach the renderbuffer to depth attachment point
	glFramebufferRenderbuffer (GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboId);

	// check FBO status
	if (glCheckFramebufferStatus (GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		glDeleteFramebuffers (1, &r_underwaterfbo);
		glDeleteTextures (1, &r_waterwarptexture);
		glDeleteRenderbuffers (1, &rboId);
	}

	// switch back to window-system-provided framebuffer
	glBindFramebuffer (GL_DRAW_FRAMEBUFFER, 0);

	// set up everything else we need
	r_warpmaxs = 400;
	r_warpmaxt = (r_warpmaxs * vid.height) / vid.width;

	wwverts[0].position[0] = 0;
	wwverts[0].position[1] = 0;
	wwverts[0].texcoord[0] = 0;
	wwverts[0].texcoord[1] = r_warpmaxt;

	wwverts[1].position[0] = vid.width;
	wwverts[1].position[1] = 0;
	wwverts[1].texcoord[0] = r_warpmaxs;
	wwverts[1].texcoord[1] = r_warpmaxt;

	wwverts[2].position[0] = vid.width;
	wwverts[2].position[1] = vid.height;
	wwverts[2].texcoord[0] = r_warpmaxs;
	wwverts[2].texcoord[1] = 0;

	wwverts[3].position[0] = 0;
	wwverts[3].position[1] = vid.height;
	wwverts[3].texcoord[0] = 0;
	wwverts[3].texcoord[1] = 0;

	glGenBuffers (1, &r_underwatervbo);
	glNamedBufferDataEXT (r_underwatervbo, sizeof (wwvert_t) * 4, wwverts, GL_STATIC_DRAW);

	glGenVertexArrays (1, &r_underwatervao);

	// 0 and 2 so that it can share layout with 2d drawing shaders
	glEnableVertexArrayAttribEXT (r_underwatervao, 0);
	glVertexArrayVertexAttribOffsetEXT (r_underwatervao, r_underwatervbo, 0, 2, GL_FLOAT, GL_FALSE, sizeof (wwvert_t), 0);

	glEnableVertexArrayAttribEXT (r_underwatervao, 2);
	glVertexArrayVertexAttribOffsetEXT (r_underwatervao, r_underwatervbo, 2, 2, GL_FLOAT, GL_FALSE, sizeof (wwvert_t), 8);

	u_uwwarpparams = glGetUniformLocation (gl_underwaterprog, "warpparams");
	u_uwsurfcolor = glGetUniformLocation (gl_underwaterprog, "surfcolor");

	glProgramUniform1i (gl_underwaterprog, glGetUniformLocation (gl_underwaterprog, "diffuse"), 0);
	glProgramUniform1i (gl_underwaterprog, glGetUniformLocation (gl_underwaterprog, "gradient"), 1);
	glProgramUniform2f (gl_underwaterprog, glGetUniformLocation (gl_underwaterprog, "rescale"), 1.0 / r_warpmaxs, 1.0 / r_warpmaxt);
	glProgramUniformMatrix4fv (gl_underwaterprog, glGetUniformLocation (gl_underwaterprog, "orthomatrix"), 1, GL_FALSE, r_drawmatrix.m[0]);
}


qboolean r_dowaterwarp = false;

void RWarp_BeginWaterWarp (void)
{
	mleaf_t *leaf;

	r_dowaterwarp = false;

	if (!r_worldmodel) return;
	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL) return;
	if (!r_underwaterfbo) return;

	leaf = Mod_PointInLeaf (r_origin, r_worldmodel);

	if ((leaf->contents & CONTENTS_WATER) || (leaf->contents & CONTENTS_LAVA) || (leaf->contents & CONTENTS_SLIME))
	{
		glBindFramebuffer (GL_DRAW_FRAMEBUFFER, r_underwaterfbo);
		r_dowaterwarp = true;
	}
}


void RWarp_DoWaterWarp (void)
{
	float warpparams[4];

	if (!r_worldmodel) return;
	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL) return;
	if (!r_underwaterfbo) return;
	if (!r_dowaterwarp) return;

	glBindFramebuffer (GL_DRAW_FRAMEBUFFER, 0);

	warpparams[0] = r_newrefdef.time * (128.0 / M_PI);
	warpparams[1] = 2.0f;
	warpparams[2] = M_PI / 128.0;
	warpparams[3] = 0.125f;

	glProgramUniform4fv (gl_underwaterprog, u_uwwarpparams, 1, warpparams);

	if (v_blend[3] && gl_polyblend->value)
		glProgramUniform4f (gl_underwaterprog, u_uwsurfcolor, v_blend[0], v_blend[1], v_blend[2], v_blend[3] * gl_polyblend->value);
	else glProgramUniform4f (gl_underwaterprog, u_uwsurfcolor, 0, 0, 0, 0);

	GL_Enable (0);
	GL_BindTexture (GL_TEXTURE0, GL_TEXTURE_2D, r_drawclampsampler, r_waterwarptexture);
	GL_BindTexture (GL_TEXTURE1, GL_TEXTURE_2D, r_drawwrapsampler, r_warpgradient);
	GL_BindVertexArray (r_underwatervao);
	GL_UseProgram (gl_underwaterprog);

	glDrawArrays (GL_TRIANGLE_FAN, 0, 4);

	// disable polyblend because we get it for free here
	v_blend[3] = 0;
	r_dowaterwarp = false;
}


