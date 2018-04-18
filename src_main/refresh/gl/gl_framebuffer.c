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
// gl_framebuffer.c -- opengl framebuffer functionality

#include "gl_local.h"

FBO_t		*fbos[MAX_FBOS];
int			numFBOs;

FBO_t		*basicRenderFBO;
FBO_t		*hdrRenderFBO;
FBO_t		*hdrDownscale64;
FBO_t		*brightpassRenderFBO;
FBO_t		*bloomRenderFBO[MAX_BLOOM_BUFFERS];
FBO_t		*AORenderFBO;

/*
============
R_CreateFBO
============
*/
static FBO_t *R_CreateFBO(const char *name, int width, int height)
{
	FBO_t          *fbo;

	if (strlen(name) >= MAX_QPATH)
	{
		VID_Error(ERR_DROP, "R_CreateFBO: \"%s\" is too long\n", name);
	}

	if (numFBOs == MAX_FBOS)
	{
		VID_Error(ERR_DROP, "R_CreateFBO: MAX_FBOS hit");
	}

	fbo = fbos[numFBOs] = malloc(sizeof(*fbo));

	fbo->frameBuffer = 0;

	memset(fbo->colorBuffers, 0, sizeof(fbo->colorBuffers));
	fbo->colorFormat = 0;

	fbo->depthBuffer = 0;
	fbo->depthFormat = 0;

	fbo->stencilBuffer = 0;
	fbo->stencilFormat = 0;

	fbo->width = width;
	fbo->height = height;

	strncpy(fbo->name, name, sizeof(fbo->name));

	fbo->index = numFBOs++;

	glGenFramebuffers(1, &fbo->frameBuffer);

	GL_CheckError (__func__);

	return fbo;
}

/*
================
R_CreateFBOColorBuffer

Framebuffer must be bound
================
*/
static void R_CreateFBOColorBuffer(FBO_t * fbo, int format, int index)
{
	if (index < 0 || index >= 16)
	{
		VID_Printf(PRINT_ALL, S_COLOR_RED "R_CreateFBOColorBuffer: invalid attachment index %i\n", index);
		return;
	}
	
	fbo->colorFormat = format;

	qboolean notCreatedYet = fbo->colorBuffers[index] == 0;
	if (notCreatedYet)
		glGenRenderbuffers(1, &fbo->colorBuffers[index]);

	glBindRenderbuffer(GL_RENDERBUFFER, fbo->colorBuffers[index]);
	glRenderbufferStorage(GL_RENDERBUFFER, format, fbo->width, fbo->height);

	if (notCreatedYet)
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index, GL_RENDERBUFFER, fbo->colorBuffers[index]);

	GL_CheckError (__func__);
}

/*
================
R_CreateFBODepthBuffer
================
*/
static void R_CreateFBODepthBuffer(FBO_t * fbo, int format)
{
	fbo->depthFormat = format;

	qboolean notCreatedYet = fbo->depthBuffer == 0;
	if (notCreatedYet)
		glGenRenderbuffers(1, &fbo->depthBuffer);

	glBindRenderbuffer(GL_RENDERBUFFER, fbo->depthBuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, fbo->depthFormat, fbo->width, fbo->height);

	if (notCreatedYet)
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, fbo->depthBuffer);

	GL_CheckError (__func__);
}

/*
================
R_CreateFBOStencilBuffer
================
*/
static void R_CreateFBOStencilBuffer(FBO_t * fbo, int format)
{
	fbo->stencilFormat = format;

	qboolean notCreatedYet = fbo->stencilBuffer == 0;
	if (notCreatedYet)
		glGenRenderbuffers(1, &fbo->stencilBuffer);

	glBindRenderbuffer(GL_RENDERBUFFER, fbo->stencilBuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, fbo->stencilFormat, fbo->width, fbo->height);

	if (notCreatedYet)
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, fbo->stencilBuffer);

	GL_CheckError (__func__);
}

/*
=================
R_AttachFBOTexture2D
=================
*/
static void R_AttachFBOTexture2D(int target, int texId, int index)
{
	if (index < 0 || index >= 16)
	{
		VID_Printf(PRINT_ALL, S_COLOR_RED "R_AttachFBOTexture2D: invalid attachment index %i\n", index);
		return;
	}

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index, target, texId, 0);
}

/*
=================
R_AttachFBOTextureDepth
=================
*/
static void R_AttachFBOTextureDepth(int texId)
{
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, texId, 0);
}

/*
============
R_BindFBO
============
*/
void R_BindFBO(FBO_t * fbo)
{
	//if (gl_state.currentFBO != fbo)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, fbo->frameBuffer);
		gl_state.currentFBO = fbo;
	}
}

/*
============
R_BindNullFBO
============
*/
void R_BindNullFBO(void)
{
	//if (gl_state.currentFBO)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
		gl_state.currentFBO = NULL;
	}
}

/*
============
R_InitFBOs
============
*/
extern GLuint r_brightPassRenderImage;
extern GLuint r_bloomRenderImage[MAX_BLOOM_BUFFERS];
extern GLuint r_currentRenderHDRImage64;
extern GLuint r_currentRenderHDRImage;
extern GLuint r_currentRenderImage;
extern GLuint r_currentDepthRenderImage;
extern GLuint r_currentAORenderImage;

void R_InitFBOs(void)
{
	numFBOs = 0;
	gl_state.currentFBO = NULL;

	// BASIC RENDER

	basicRenderFBO = R_CreateFBO("_basicRender", vid.width, vid.height);
	R_BindFBO(basicRenderFBO);

	R_CreateFBOColorBuffer(basicRenderFBO, GL_RGBA8, 0);
	R_CreateFBODepthBuffer(basicRenderFBO, GL_DEPTH_COMPONENT24);

	R_AttachFBOTexture2D(GL_TEXTURE_2D, r_currentRenderImage, 0);
	R_AttachFBOTextureDepth(r_currentDepthRenderImage);

	R_CheckFBO(basicRenderFBO);

	// HDR RENDER

	hdrRenderFBO = R_CreateFBO("_hdrRender", vid.width, vid.height);
	R_BindFBO(hdrRenderFBO);

	R_CreateFBOColorBuffer(hdrRenderFBO, GL_RGBA16F, 0);
	R_CreateFBODepthBuffer(hdrRenderFBO, GL_DEPTH_COMPONENT24);

	R_AttachFBOTexture2D(GL_TEXTURE_2D, r_currentRenderHDRImage, 0);
	R_AttachFBOTextureDepth(r_currentDepthRenderImage);

	R_CheckFBO(hdrRenderFBO);

	// HDR DOWNSCALE

	hdrDownscale64 = R_CreateFBO("_hdrDownscale64", 64, 64);
	R_BindFBO(hdrDownscale64);

	R_CreateFBOColorBuffer(hdrDownscale64, GL_RGBA16F, 0);

	R_AttachFBOTexture2D(GL_TEXTURE_2D, r_currentRenderHDRImage64, 0);

	R_CheckFBO(hdrDownscale64);

	// BRIGHTPASS

	brightpassRenderFBO = R_CreateFBO("_brightpassRender", vid.width * 0.25f, vid.height * 0.25f);
	R_BindFBO(brightpassRenderFBO);

	R_CreateFBOColorBuffer(brightpassRenderFBO, GL_RGBA16F, 0);

	R_AttachFBOTexture2D(GL_TEXTURE_2D, r_brightPassRenderImage, 0);

	R_CheckFBO(brightpassRenderFBO);

	// BLOOM

	for (int i = 0; i < MAX_BLOOM_BUFFERS; i++)
	{
		bloomRenderFBO[i] = R_CreateFBO(va("_bloomRender%i", i), vid.width, vid.height);
		R_BindFBO(bloomRenderFBO[i]);

		R_CreateFBOColorBuffer(bloomRenderFBO[i], GL_RGBA8, 0);

		R_AttachFBOTexture2D(GL_TEXTURE_2D, r_bloomRenderImage[i], 0);

		R_CheckFBO(bloomRenderFBO[i]);
	}

	// AMBIENT OCCLUSION

	AORenderFBO = R_CreateFBO("_AORender", vid.width, vid.height);
	R_BindFBO(AORenderFBO);

	R_CreateFBOColorBuffer(AORenderFBO, GL_RGBA16F, 0);

	R_AttachFBOTexture2D(GL_TEXTURE_2D, r_currentAORenderImage, 0);

	R_CheckFBO(AORenderFBO);

	// do an unbind to clear state
	R_BindNullFBO();
}

/*
============
R_ShutdownFBOs
============
*/
void R_ShutdownFBOs(void)
{
	int             i, j;
	FBO_t          *fbo;

	R_BindNullFBO();

	for (i = 0; i < numFBOs; i++)
	{
		fbo = fbos[i];

		for (j = 0; j < 16; j++)
		{
			if (fbo->colorBuffers[j])
				glDeleteRenderbuffers(1, &fbo->colorBuffers[j]);
		}

		if (fbo->depthBuffer)
			glDeleteRenderbuffers(1, &fbo->depthBuffer);

		if (fbo->stencilBuffer)
			glDeleteRenderbuffers(1, &fbo->stencilBuffer);

		if (fbo->frameBuffer)
			glDeleteFramebuffers(1, &fbo->frameBuffer);
	}
}

/*
============
R_FBOList_f
============
*/
void R_FBOList_f(void)
{
	int             i;
	FBO_t          *fbo;

	VID_Printf(PRINT_ALL, "             size       name\n");
	VID_Printf(PRINT_ALL, "----------------------------------------------------------\n");

	for (i = 0; i < numFBOs; i++)
	{
		fbo = fbos[i];

		VID_Printf(PRINT_ALL, "  %4i: %4i %4i %s\n", i, fbo->width, fbo->height, fbo->name);
	}

	VID_Printf(PRINT_ALL, " %i FBOs\n", numFBOs);
}

/*
=============
R_CheckFBO
=============
*/
qboolean R_CheckFBO(const FBO_t * fbo)
{
	int             code;
	int             id;

	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &id);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo->frameBuffer);

	code = glCheckFramebufferStatus(GL_FRAMEBUFFER);

	if (code == GL_FRAMEBUFFER_COMPLETE)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, id);
		return true;
	}

	// an error occured
	switch (code)
	{
	case GL_FRAMEBUFFER_UNSUPPORTED:
		VID_Printf(PRINT_ALL, S_COLOR_RED "R_CheckFBO: (%s) Unsupported framebuffer format\n", fbo->name);
		break;
	case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
		VID_Printf(PRINT_ALL, S_COLOR_RED "R_CheckFBO: (%s) Framebuffer incomplete, incomplete attachment\n", fbo->name);
		break;
	case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
		VID_Printf(PRINT_ALL, S_COLOR_RED "R_CheckFBO: (%s) Framebuffer incomplete, missing multisample\n", fbo->name);
		break;
	case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
		VID_Printf(PRINT_ALL, S_COLOR_RED "R_CheckFBO: (%s) Framebuffer incomplete, missing layer targets\n", fbo->name);
		break;
	case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
		VID_Printf(PRINT_ALL, S_COLOR_RED "R_CheckFBO: (%s) Framebuffer incomplete, missing attachment\n", fbo->name);
		break;
	case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
		VID_Printf(PRINT_ALL, S_COLOR_RED "R_CheckFBO: (%s) Framebuffer incomplete, missing draw buffer\n", fbo->name);
		break;
	case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
		VID_Printf(PRINT_ALL, S_COLOR_RED "R_CheckFBO: (%s) Framebuffer incomplete, missing read buffer\n", fbo->name);
		break;
	default:
		VID_Printf(PRINT_ALL, S_COLOR_RED "R_CheckFBO: (%s) Unknown error 0x%X\n", fbo->name, code);
		assert(0);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, id);

	return false;
}
