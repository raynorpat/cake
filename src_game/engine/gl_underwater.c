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
// gl_postprocess.c -- screen polygons and effects

#include "gl_local.h"


GLuint r_scenetexture;
GLuint r_warpgradient;

GLuint gl_underwaterprog = 0;
GLuint u_uwwarpparams = 0;
GLuint u_uwsurfcolor = 0;
GLuint u_uwgamma = 0;

GLuint gl_compositeprog = 0;
GLuint u_compositeTexScale = 0;
GLuint u_compositeMode = 0;

GLuint gl_postprog = 0;
GLuint u_postsurfcolor = 0;
GLuint u_postTexScale = 0;

float r_warpmaxs;
float r_warpmaxt;

typedef struct wwvert_s
{
	float position[2];
	float texcoord[2];
} wwvert_t;

GLuint r_postvbo = 0;
GLuint r_postvao = 0;
GLuint r_scenefbo = 0;

GLuint r_brightPassRenderImage;
GLuint r_bloomRenderImage[MAX_BLOOM_BUFFERS];
GLuint r_currentRenderHDRImage64;
GLuint r_currentRenderHDRImage;
GLuint r_currentDepthRenderImage;

void RPostProcess_CreatePrograms(void)
{
//	GLuint scenerboId;
	wwvert_t wwverts[4];
	byte *data = NULL;
	int width, height;

	LoadTGAFile("env/warpgradient.tga", &data, &width, &height);
	if (data)
		r_warpgradient = GL_UploadTexture (data, width, height, false, 32);

	gl_underwaterprog = GL_CreateShaderFromName ("glsl/underwater.glsl", "WaterWarpVS", "WaterWarpFS");
	gl_compositeprog = GL_CreateShaderFromName("glsl/composite.glsl", "CompositeVS", "CompositeFS");
	gl_postprog = GL_CreateShaderFromName("glsl/post.glsl", "PostVS", "PostFS");

	/*
	// create a texture for use by the FBO
	glDeleteTextures (1, &r_scenetexture);
	glGenTextures (1, &r_scenetexture);

	glTextureStorage2DEXT (r_scenetexture, GL_TEXTURE_2D, 1, GL_RGBA8, vid.width, vid.height);

	// create a renderbuffer object to store depth info
	// whoever on the ARB thought it was a good idea to not allow using the default system-provided depth buffer needs to be shot
	glDeleteRenderbuffers (1, &scenerboId);
	glGenRenderbuffers (1, &scenerboId);
	glBindRenderbuffer (GL_RENDERBUFFER, scenerboId);
	glRenderbufferStorage (GL_RENDERBUFFER, GL_DEPTH_COMPONENT, vid.width, vid.height);
	glBindRenderbuffer (GL_RENDERBUFFER, 0);

	// create a framebuffer object
	glDeleteFramebuffers (1, &r_scenefbo);
	glGenFramebuffers (1, &r_scenefbo);
	glBindFramebuffer (GL_DRAW_FRAMEBUFFER, r_scenefbo);

	// attach the texture to FBO color attachment point
	glFramebufferTexture2D (GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, r_scenetexture, 0);

	// attach the renderbuffer to depth attachment point
	glFramebufferRenderbuffer (GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, scenerboId);

	// check FBO status
	if (glCheckFramebufferStatus (GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		glDeleteFramebuffers (1, &r_scenefbo);
		glDeleteTextures (1, &r_scenetexture);
		glDeleteRenderbuffers (1, &scenerboId);
	}

	// switch back to window-system-provided framebuffer
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	*/

	// create textures for use by the HDR FBO
	glDeleteTextures(1, &r_currentRenderHDRImage);
	glGenTextures(1, &r_currentRenderHDRImage);
	glBindTexture(GL_TEXTURE_2D, r_currentRenderHDRImage);
	//glTextureStorage2DEXT(r_currentRenderHDRImage, GL_TEXTURE_2D, 1, GL_RGBA16F, vid.width, vid.height);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, vid.width, vid.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	glDeleteTextures(1, &r_currentRenderHDRImage64);
	glGenTextures(1, &r_currentRenderHDRImage64);
	glBindTexture(GL_TEXTURE_2D, r_currentRenderHDRImage64);
	//glTextureStorage2DEXT(r_currentRenderHDRImage64, GL_TEXTURE_2D, 1, GL_RGBA16F, 64, 64);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	glDeleteTextures(1, &r_currentDepthRenderImage);
	glGenTextures(1, &r_currentDepthRenderImage);
	glBindTexture(GL_TEXTURE_2D, r_currentDepthRenderImage);
	//glTextureStorage2DEXT(r_currentDepthRenderImage, GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT24, vid.width, vid.height);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, vid.width, vid.height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	glDeleteTextures(1, &r_bloomRenderImage[0]);
	glGenTextures(1, &r_bloomRenderImage[0]);
	glBindTexture(GL_TEXTURE_2D, r_bloomRenderImage[0]);
	//glTextureStorage2DEXT(r_bloomRenderImage[0], GL_TEXTURE_2D, 1, GL_RGBA8, vid.width / 4, vid.height / 4);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, vid.width, vid.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	glDeleteTextures(1, &r_bloomRenderImage[1]);
	glGenTextures(1, &r_bloomRenderImage[1]);
	glBindTexture(GL_TEXTURE_2D, r_bloomRenderImage[1]);
	//glTextureStorage2DEXT(r_bloomRenderImage[1], GL_TEXTURE_2D, 1, GL_RGBA8, vid.width / 4, vid.height / 4);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, vid.width, vid.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	glDeleteTextures(1, &r_brightPassRenderImage);
	glGenTextures(1, &r_brightPassRenderImage);
	glBindTexture(GL_TEXTURE_2D, r_brightPassRenderImage);
	//glTextureStorage2DEXT(r_brightPassRenderImage, GL_TEXTURE_2D, 1, GL_RGBA8, vid.width * 0.25f, vid.height * 0.25f);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, vid.width * 0.25f, vid.height * 0.25f, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

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

	glGenBuffers (1, &r_postvbo);
	glNamedBufferDataEXT (r_postvbo, sizeof (wwvert_t) * 4, wwverts, GL_STATIC_DRAW);

	glGenVertexArrays (1, &r_postvao);

	// 0 and 2 so that it can share layout with 2d drawing shaders
	glEnableVertexArrayAttribEXT (r_postvao, 0);
	glVertexArrayVertexAttribOffsetEXT (r_postvao, r_postvbo, 0, 2, GL_FLOAT, GL_FALSE, sizeof (wwvert_t), 0);

	glEnableVertexArrayAttribEXT (r_postvao, 2);
	glVertexArrayVertexAttribOffsetEXT (r_postvao, r_postvbo, 2, 2, GL_FLOAT, GL_FALSE, sizeof (wwvert_t), 8);

	u_uwwarpparams = glGetUniformLocation (gl_underwaterprog, "warpparams");
	u_uwsurfcolor = glGetUniformLocation (gl_underwaterprog, "surfcolor");
	u_uwgamma = glGetUniformLocation (gl_underwaterprog, "gamma");

	u_compositeMode = glGetUniformLocation(gl_compositeprog, "compositeMode");
	u_compositeTexScale = glGetUniformLocation(gl_compositeprog, "texScale");

	u_postsurfcolor = glGetUniformLocation (gl_postprog, "surfcolor");
	u_postTexScale = glGetUniformLocation (gl_postprog, "texScale");

	glProgramUniform1i (gl_underwaterprog, glGetUniformLocation (gl_underwaterprog, "diffuse"), 0);
	glProgramUniform1i (gl_underwaterprog, glGetUniformLocation (gl_underwaterprog, "gradient"), 1);
	glProgramUniform2f (gl_underwaterprog, glGetUniformLocation (gl_underwaterprog, "rescale"), 1.0 / r_warpmaxs, 1.0 / r_warpmaxt);
	glProgramUniformMatrix4fv (gl_underwaterprog, glGetUniformLocation (gl_underwaterprog, "orthomatrix"), 1, GL_FALSE, r_drawmatrix.m[0]);

	glProgramUniform1i (gl_compositeprog, glGetUniformLocation (gl_compositeprog, "diffuse"), 0);
	glProgramUniform2f (gl_compositeprog, glGetUniformLocation (gl_compositeprog, "rescale"), 1.0 / r_warpmaxs, 1.0 / r_warpmaxt);
	glProgramUniformMatrix4fv (gl_compositeprog, glGetUniformLocation (gl_compositeprog, "orthomatrix"), 1, GL_FALSE, r_drawmatrix.m[0]);

	glProgramUniform1i (gl_postprog, glGetUniformLocation (gl_postprog, "diffuse"), 0);
	glProgramUniform1i (gl_postprog, glGetUniformLocation (gl_postprog, "precomposite"), 1);
	glProgramUniform2f (gl_postprog, glGetUniformLocation (gl_postprog, "rescale"), 1.0 / r_warpmaxs, 1.0 / r_warpmaxt);
	glProgramUniformMatrix4fv (gl_postprog, glGetUniformLocation (gl_postprog, "orthomatrix"), 1, GL_FALSE, r_drawmatrix.m[0]);
}


qboolean r_dopostprocessing = false;
qboolean r_dowaterwarppost = false;

void RPostProcess_Begin(void)
{
	mleaf_t *leaf;

	r_dopostprocessing = false;
	r_dowaterwarppost = false;

	if (!r_worldmodel) return;
	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL) return;
//	if (!r_scenefbo) return;

	leaf = Mod_PointInLeaf (r_origin, r_worldmodel);

	if ((leaf->contents & CONTENTS_WATER) || (leaf->contents & CONTENTS_LAVA) || (leaf->contents & CONTENTS_SLIME))
	{
	//	glBindFramebuffer (GL_DRAW_FRAMEBUFFER, r_scenefbo);
	//	r_dowaterwarppost = true;
		r_dopostprocessing = true;
	}
	else
	{
	//	glBindFramebuffer (GL_DRAW_FRAMEBUFFER, r_scenefbo);
		R_BindFBO(hdrRenderFBO);
		r_dopostprocessing = true;
	}

	GL_Clear(GL_COLOR_BUFFER_BIT);
}


void RPostProcess_FinishToScreen(void)
{
	if (!r_worldmodel) return;
	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL) return;
//	if (!r_scenefbo) return;
	if (!r_dopostprocessing) return;

	//glBindFramebuffer (GL_DRAW_FRAMEBUFFER, 0);
	R_BindNullFBO();

	if (r_dowaterwarppost)
	{
		float warpparams[4];

		warpparams[0] = r_newrefdef.time * (128.0 / M_PI);
		warpparams[1] = 2.0f;
		warpparams[2] = M_PI / 128.0;
		warpparams[3] = 0.125f;

		glProgramUniform4fv(gl_underwaterprog, u_uwwarpparams, 1, warpparams);

		if (v_blend[3] && gl_polyblend->value)
			glProgramUniform4f(gl_underwaterprog, u_uwsurfcolor, v_blend[0], v_blend[1], v_blend[2], v_blend[3] * gl_polyblend->value);
		else
			glProgramUniform4f(gl_underwaterprog, u_uwsurfcolor, 0, 0, 0, 0);

		glProgramUniform1f(gl_underwaterprog, u_uwgamma, vid_gamma->value);

		GL_Enable(0);
		GL_BindTexture(GL_TEXTURE0, GL_TEXTURE_2D, r_drawclampsampler, r_scenetexture);
		GL_BindTexture(GL_TEXTURE1, GL_TEXTURE_2D, r_drawwrapsampler, r_warpgradient);
		GL_BindVertexArray(r_postvao);
		GL_UseProgram(gl_underwaterprog);

		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	}
	else
	{
		int i, flip = 0;
		vec2_t texScale;

		//GL_Enable(DEPTHTEST_BIT);
		GL_Enable(0);

		// do downscaled brightpass and tonemap
		{
			texScale[0] = 1.0f / vid.width;
			texScale[1] = 1.0f / vid.height;

			GL_UseProgram(gl_compositeprog);

			glProgramUniform1i(gl_compositeprog, u_compositeMode, 1);
			glProgramUniform2f(gl_compositeprog, u_compositeTexScale, texScale[0], texScale[1]);
			GL_BindTexture(GL_TEXTURE0, GL_TEXTURE_2D, r_drawclampsampler, r_currentRenderHDRImage);

			R_BindFBO(brightpassRenderFBO);
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);

			GL_BindVertexArray(r_postvao);
			glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
		}

		// hdr chromatic glare
		GL_BindTexture(GL_TEXTURE0, GL_TEXTURE_2D, r_drawclampsampler, r_brightPassRenderImage);
		for (i = 0; i < 8; i++)
		{
			texScale[0] = 1.0f / bloomRenderFBO[flip]->width;
			texScale[1] = 1.0f / bloomRenderFBO[flip]->height;

			R_BindFBO(bloomRenderFBO[flip]);

			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);

			GL_Enable(0);
			//GL_Enable(DEPTHTEST_BIT);

			GL_UseProgram(gl_compositeprog);
			glProgramUniform1i(gl_compositeprog, u_compositeMode, 2);
			glProgramUniform2f(gl_compositeprog, u_compositeTexScale, texScale[0], texScale[1]);
			
			GL_BindVertexArray(r_postvao);
			glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

			GL_BindTexture(GL_TEXTURE0, GL_TEXTURE_2D, r_drawclampsampler, r_bloomRenderImage[flip]);
			flip ^= 1;
		}

		R_BindNullFBO();
	
		// then we can do a final post-process pass
		texScale[0] = 1.0f / vid.width;
		texScale[1] = 1.0f / vid.height;

		if (v_blend[3] && gl_polyblend->value)
			glProgramUniform4f (gl_postprog, u_postsurfcolor, v_blend[0], v_blend[1], v_blend[2], v_blend[3] * gl_polyblend->value);
		else
			glProgramUniform4f (gl_postprog, u_postsurfcolor, 0, 0, 0, 0);
		glProgramUniform2f(gl_postprog, u_postTexScale, texScale[0], texScale[1]);

		//GL_Enable(DEPTHTEST_BIT);
		GL_Enable(0);
		GL_BindTexture(GL_TEXTURE1, GL_TEXTURE_2D, r_drawclampsampler, r_currentRenderHDRImage);
		GL_BindVertexArray(r_postvao);
		GL_UseProgram(gl_postprog);

		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	}

	// disable polyblend because we get it for free here
	v_blend[3] = 0;
	r_dopostprocessing = false;
}
