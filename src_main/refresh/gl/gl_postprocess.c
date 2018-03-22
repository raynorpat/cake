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

GLuint gl_ssaoprog = 0;
GLuint u_ssaoTexScale = 0;
GLuint u_ssaoZFar = 0;

GLuint gl_fxaaprog = 0;
GLuint u_fxaaTexScale = 0;

GLuint gl_compositeprog = 0;
GLuint u_compositeTexScale = 0;
GLuint u_compositeMode = 0;
GLuint u_compositeBrightParam = 0;

GLuint gl_postprog = 0;
GLuint u_postsurfcolor = 0;
GLuint u_postExposure = 0;
GLuint u_postBrightnessContrastBlurSSAOAmount = 0;
GLuint u_postTexScale = 0;
GLuint u_postwaterwarpparam = 0;
GLuint u_postwaterwarp = 0;

GLuint gl_calcLumProg = 0;
GLuint gl_calcAdaptiveLumProg = 0;
GLuint u_deltaTime = 0;

float r_warpmaxs;
float r_warpmaxt;

typedef struct wwvert_s
{
	float position[2];
	float texcoord[2];
} wwvert_t;

GLuint r_postvbo = 0;
GLuint r_postvao = 0;

qboolean r_skippost = false;
qboolean r_dowaterwarppost = false;

GLuint r_warpGradientImage;
GLuint r_brightPassRenderImage;
GLuint r_bloomRenderImage[MAX_BLOOM_BUFFERS];
GLuint r_currentRenderHDRImage64;
GLuint r_currentRenderHDRImage;
GLuint r_currentRenderImage;
GLuint r_currentDepthRenderImage;
GLuint r_currentAORenderImage;
GLuint m_lum[2];
GLuint m_lumCurrent;

cvar_t *r_hdrContrastThreshold;
cvar_t *r_hdrContrastScale;
cvar_t *r_hdrExposureCompensation;
cvar_t *r_hdrExposureAdjust;
cvar_t *r_hdrBlurAmount;
cvar_t *r_hdrBlurPasses;
cvar_t *r_postprocessing;
cvar_t *r_ssao;
cvar_t *r_fxaa;

void RPostProcess_CreatePrograms(void)
{
	wwvert_t wwverts[4];
	byte *data = NULL;
	byte lumdata[1][1][4];
	int width, height, x, y;

	// create texture for underwater warp gradient
	LoadTGAFile("env/warpgradient.tga", &data, &width, &height);
	if (data)
		r_warpGradientImage = GL_UploadTexture (data, width, height, false, 32);

	// create 1x1 texture with a decent middle luminance value
	// This fixes an issue with the adaptation going nuts on the 
	// very first frame, since there isn't a previous frame to fallback on.
	for (x = 0; x < 1; x++)
	{
		for (y = 0; y < 1; y++)
		{
			lumdata[y][x][0] = 32;
			lumdata[y][x][1] = 32;
			lumdata[y][x][2] = 32;
			lumdata[y][x][3] = 255;
		}
	}

	// create textures for use by the HDR framebuffer objects
	glDeleteTextures(1, &r_currentRenderImage);
	glGenTextures(1, &r_currentRenderImage);
	glBindTexture(GL_TEXTURE_2D, r_currentRenderImage);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, vid.width, vid.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glDeleteTextures(1, &r_currentRenderHDRImage);
	glGenTextures(1, &r_currentRenderHDRImage);
	glBindTexture(GL_TEXTURE_2D, r_currentRenderHDRImage);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, vid.width, vid.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glDeleteTextures(1, &r_currentRenderHDRImage64);
	glGenTextures(1, &r_currentRenderHDRImage64);
	glBindTexture(GL_TEXTURE_2D, r_currentRenderHDRImage64);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glDeleteTextures(1, &r_currentDepthRenderImage);
	glGenTextures(1, &r_currentDepthRenderImage);
	glBindTexture(GL_TEXTURE_2D, r_currentDepthRenderImage);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, vid.width, vid.height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glDeleteTextures(1, &r_currentAORenderImage);
	glGenTextures(1, &r_currentAORenderImage);
	glBindTexture(GL_TEXTURE_2D, r_currentAORenderImage);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, vid.width, vid.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	for (int i = 0; i < MAX_BLOOM_BUFFERS; i++) {
		glDeleteTextures(1, &r_bloomRenderImage[i]);
		glGenTextures(1, &r_bloomRenderImage[i]);
		glBindTexture(GL_TEXTURE_2D, r_bloomRenderImage[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, vid.width, vid.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}

	for (int i = 0; i < 2; i++) {
		glDeleteTextures(1, &m_lum[i]);
		glGenTextures(1, &m_lum[i]);
		glBindTexture(GL_TEXTURE_2D, m_lum[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, lumdata);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}

	glDeleteTextures(1, &m_lumCurrent);
	glGenTextures(1, &m_lumCurrent);
	glBindTexture(GL_TEXTURE_2D, m_lumCurrent);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, lumdata);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glDeleteTextures(1, &r_brightPassRenderImage);
	glGenTextures(1, &r_brightPassRenderImage);
	glBindTexture(GL_TEXTURE_2D, r_brightPassRenderImage);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, vid.width * 0.25f, vid.height * 0.25f, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glBindTexture(GL_TEXTURE_2D, 0);

	// set up the screen vbo and vao
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

	// do functionality test, as we don't want to load shaders that we don't need
	if (!GLEW_ARB_texture_gather || !gl_config.gl_ext_GPUShader5_support || !gl_config.gl_ext_computeShader_support)
	{
		// these won't quite work out that great on lesser hardware,
		// so skip post-processing when the hardware doesn't support it
		r_skippost = true;
		return;
	}

	// create shaders
	gl_compositeprog = GL_CreateShaderFromName("glsl/composite.glsl", "CompositeVS", "CompositeFS");
	gl_postprog = GL_CreateShaderFromName("glsl/post.glsl", "PostVS", "PostFS");
	gl_ssaoprog = GL_CreateShaderFromName("glsl/ssao.glsl", "SSAOVS", "SSAOFS");
	gl_fxaaprog = GL_CreateShaderFromName("glsl/fxaa.glsl", "FXAAVS", "FXAAFS");

	// create compute shaders
	gl_calcLumProg = GL_CreateComputeShaderFromName("glsl/calcLum.cs");
	gl_calcAdaptiveLumProg = GL_CreateComputeShaderFromName("glsl/calcAdaptiveLum.cs");

	// set up shader uniforms
	u_compositeMode = glGetUniformLocation(gl_compositeprog, "compositeMode");
	u_compositeTexScale = glGetUniformLocation(gl_compositeprog, "texScale");
	u_compositeBrightParam = glGetUniformLocation(gl_compositeprog, "brightParam");

	u_postsurfcolor = glGetUniformLocation (gl_postprog, "surfcolor");
	u_postBrightnessContrastBlurSSAOAmount = glGetUniformLocation (gl_postprog, "brightnessContrastBlurSSAOAmount");
	u_postExposure = glGetUniformLocation (gl_postprog, "exposure");
	u_postTexScale = glGetUniformLocation (gl_postprog, "texScale");
	u_postwaterwarpparam = glGetUniformLocation (gl_postprog, "waterwarpParam");
	u_postwaterwarp = glGetUniformLocation (gl_postprog, "waterwarppost");

	u_ssaoZFar = glGetUniformLocation (gl_ssaoprog, "zFar");
	u_ssaoTexScale = glGetUniformLocation (gl_ssaoprog, "texScale");

	u_fxaaTexScale = glGetUniformLocation (gl_fxaaprog, "texScale");

	u_deltaTime = glGetUniformLocation (gl_calcAdaptiveLumProg, "deltaTime");

	glProgramUniform1i (gl_compositeprog, glGetUniformLocation (gl_compositeprog, "diffuse"), 0);
	glProgramUniform2f (gl_compositeprog, glGetUniformLocation (gl_compositeprog, "rescale"), 1.0 / r_warpmaxs, 1.0 / r_warpmaxt);
	glProgramUniformMatrix4fv (gl_compositeprog, glGetUniformLocation (gl_compositeprog, "orthomatrix"), 1, GL_FALSE, r_drawmatrix.m[0]);

	glProgramUniform1i (gl_postprog, glGetUniformLocation (gl_postprog, "diffuse"), 0);
	glProgramUniform1i (gl_postprog, glGetUniformLocation (gl_postprog, "precomposite"), 1);
	glProgramUniform1i (gl_postprog, glGetUniformLocation (gl_postprog, "warpgradient"), 2);
	glProgramUniform1i (gl_postprog, glGetUniformLocation (gl_postprog, "lumTex"), 3);
	glProgramUniform1i (gl_postprog, glGetUniformLocation (gl_postprog, "AOTex"), 4);
	glProgramUniform2f (gl_postprog, glGetUniformLocation (gl_postprog, "rescale"), 1.0 / r_warpmaxs, 1.0 / r_warpmaxt);
	glProgramUniformMatrix4fv (gl_postprog, glGetUniformLocation (gl_postprog, "orthomatrix"), 1, GL_FALSE, r_drawmatrix.m[0]);

	glProgramUniform1i (gl_ssaoprog, glGetUniformLocation (gl_ssaoprog, "depthmap"), 0);
	glProgramUniformMatrix4fv (gl_ssaoprog, glGetUniformLocation (gl_ssaoprog, "orthomatrix"), 1, GL_FALSE, r_drawmatrix.m[0]);

	glProgramUniform1i (gl_fxaaprog, glGetUniformLocation (gl_fxaaprog, "diffuse"), 0);
	glProgramUniformMatrix4fv (gl_fxaaprog, glGetUniformLocation (gl_fxaaprog, "orthomatrix"), 1, GL_FALSE, r_drawmatrix.m[0]);

	glProgramUniform1i (gl_calcLumProg, glGetUniformLocation (gl_calcLumProg, "inputImage"), 0);
	glProgramUniform1i (gl_calcLumProg, glGetUniformLocation (gl_calcLumProg, "outputImage"), 1);

	glProgramUniform1i (gl_calcAdaptiveLumProg, glGetUniformLocation (gl_calcAdaptiveLumProg, "currentImage"), 0);
	glProgramUniform1i (gl_calcAdaptiveLumProg, glGetUniformLocation (gl_calcAdaptiveLumProg, "image0"), 1);
	glProgramUniform1i (gl_calcAdaptiveLumProg, glGetUniformLocation (gl_calcAdaptiveLumProg, "image1"), 2);
}

void RPostProcess_Init(void)
{
	r_hdrContrastThreshold = Cvar_Get("r_hdrContrastThreshold", "1.0", 0);
	r_hdrContrastScale = Cvar_Get("r_hdrContrastScale", "300", 0);
	r_hdrExposureCompensation = Cvar_Get("r_hdrExposureCompensation", "3.0", 0);
	r_hdrExposureAdjust = Cvar_Get("r_hdrExposureAdjust", "1.4", 0);
	r_hdrBlurAmount = Cvar_Get("r_hdrBlurAmount", "0.33", 0);
	r_hdrBlurPasses = Cvar_Get("r_hdrBlurPasses", "8", 0);

	r_postprocessing = Cvar_Get("r_postprocessing", "1", CVAR_ARCHIVE);
	r_ssao = Cvar_Get("r_ssao", "1", CVAR_ARCHIVE);
	r_fxaa = Cvar_Get("r_fxaa", "0", CVAR_ARCHIVE);
}

void RPostProcess_ComputeShader_CalculateLuminance(void)
{
	// calculate current luminance level
	GL_UseProgram (gl_calcLumProg);
	glBindImageTexture (0, r_currentRenderHDRImage64, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
	glBindImageTexture (1, m_lumCurrent, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
	glDispatchCompute (1, 1, 1);
	//glMemoryBarrier (GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);  

	// adapt luminance based on current and previous frame
	GL_UseProgram (gl_calcAdaptiveLumProg);
	glBindImageTexture (0, m_lumCurrent, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
	glBindImageTexture (1, m_lum[0], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA16F);
	glBindImageTexture (2, m_lum[1], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
	glUniform1f (u_deltaTime, 1 / 60.0f); // simulates 60fps
	glDispatchCompute (1, 1, 1);
	glMemoryBarrier (GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}

static void RPostProcess_SetCurrentRender(void)
{
	GL_BindTexture(GL_TEXTURE0, GL_TEXTURE_2D, r_drawnearestclampsampler, r_currentRenderImage);
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, vid.width, vid.height);
}

static void RPostProcess_DownscaleTo64(void)
{
	// blit current hdr framebuffer into downscaled 64x64 texture
	glBindFramebuffer(GL_READ_FRAMEBUFFER_EXT, hdrRenderFBO->frameBuffer);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER_EXT, hdrDownscale64->frameBuffer);
	glBlitFramebuffer(0, 0, vid.width, vid.height, 0, 0, 64, 64, GL_COLOR_BUFFER_BIT, GL_LINEAR);
}

static void RPostProcess_DownscaleBrightpass(void)
{
	vec2_t texScale;
	vec4_t brightParam;

	// set screen scale
	texScale[0] = 1.0f / vid.width;
	texScale[1] = 1.0f / vid.height;

	R_BindFBO(brightpassRenderFBO);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	GL_Clear(GL_COLOR_BUFFER_BIT);

	GL_Enable(!BLEND_BIT | !CULLFACE_BIT);

	// do downscaled brightpass
	GL_UseProgram(gl_compositeprog);

	brightParam[0] = r_hdrContrastThreshold->value;
	brightParam[1] = r_hdrContrastScale->value;
	brightParam[2] = 0;
	brightParam[3] = 0;
	glProgramUniform4f(gl_compositeprog, u_compositeBrightParam, brightParam[0], brightParam[1], brightParam[2], brightParam[3]);
	glProgramUniform2f(gl_compositeprog, u_compositeTexScale, texScale[0], texScale[1]);
	glProgramUniform1i(gl_compositeprog, u_compositeMode, 1);

	GL_BindTexture(GL_TEXTURE0, GL_TEXTURE_2D, r_drawnearestclampsampler, r_currentRenderHDRImage);

	GL_BindVertexArray(r_postvao);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	R_BindNullFBO();
}

static void RPostProcess_DoBloomAndTonemap(void)
{
	int i, j, flip = 0;
	vec2_t texScale;
	vec4_t waterwarpParam;

	// set screen scale
	texScale[0] = 1.0f / vid.width;
	texScale[1] = 1.0f / vid.height;

	// seperable blur passes
	GL_BindTexture(GL_TEXTURE0, GL_TEXTURE_2D, r_drawclampsampler, r_brightPassRenderImage);

	for (i = 0; i < 2; i++)
	{
		for (j = 0; j < r_hdrBlurPasses->value; j++)
		{
			texScale[0] = 1.0f / bloomRenderFBO[flip]->width;
			texScale[1] = 1.0f / bloomRenderFBO[flip]->height;

			R_BindFBO(bloomRenderFBO[flip]);

			GL_Clear(GL_COLOR_BUFFER_BIT);

			GL_Enable(BLEND_BIT);

			GL_UseProgram(gl_compositeprog);
			if (i == 0)
				glProgramUniform1i(gl_compositeprog, u_compositeMode, 2);
			else
				glProgramUniform1i(gl_compositeprog, u_compositeMode, 3);
			glProgramUniform2f(gl_compositeprog, u_compositeTexScale, texScale[0], texScale[1]);

			GL_BindVertexArray(r_postvao);
			glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

			GL_BindTexture(GL_TEXTURE0, GL_TEXTURE_2D, r_drawclampsampler, r_bloomRenderImage[flip]);
			flip ^= 1;
		}
	}

	R_BindNullFBO();

	// then we can do a final post-process pass
	if (v_blend[3])
		glProgramUniform4f(gl_postprog, u_postsurfcolor, v_blend[0], v_blend[1], v_blend[2], v_blend[3] * 0.5);
	else
		glProgramUniform4f(gl_postprog, u_postsurfcolor, 0, 0, 0, 0);

	texScale[0] = 1.0f / vid.width;
	texScale[1] = 1.0f / vid.height;
	glProgramUniform2f(gl_postprog, u_postTexScale, texScale[0], texScale[1]);

	// adaptive exposure adjustment in log space
	float newExp = r_hdrExposureCompensation->value * log(r_hdrExposureAdjust->value + 0.0001f);
	glProgramUniform1f(gl_postprog, u_postExposure, newExp);

	// water-warp parameters
	waterwarpParam[0] = r_newrefdef.time * (128.0 / M_PI);
	waterwarpParam[1] = 2.0f;
	waterwarpParam[2] = M_PI / 128.0;
	waterwarpParam[3] = 0.125f;
	glProgramUniform4f(gl_postprog, u_postwaterwarpparam, waterwarpParam[0], waterwarpParam[1], waterwarpParam[2], waterwarpParam[3]);
	if (r_dowaterwarppost)
	{
		glProgramUniform1i(gl_postprog, u_postwaterwarp, 1);
	}
	else
	{
		glProgramUniform1i(gl_postprog, u_postwaterwarp, 0);
	}

	// set brightness, contrast, and blur levels along with SSAO value
	glProgramUniform4f(gl_postprog, u_postBrightnessContrastBlurSSAOAmount, vid_gamma->value, vid_contrast->value, r_hdrBlurAmount->value, r_ssao->value);

	GL_Enable(BLEND_BIT);

	GL_UseProgram(gl_postprog);

	GL_BindTexture(GL_TEXTURE1, GL_TEXTURE_2D, r_drawnearestclampsampler, r_currentRenderHDRImage);
	GL_BindTexture(GL_TEXTURE2, GL_TEXTURE_2D, r_drawwrapsampler, r_warpGradientImage);
	GL_BindTexture(GL_TEXTURE3, GL_TEXTURE_2D, r_drawclampsampler, m_lum[1]);
	GL_BindTexture(GL_TEXTURE4, GL_TEXTURE_2D, r_drawnearestclampsampler, r_currentAORenderImage);

	GL_BindVertexArray(r_postvao);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void RPostProcess_SSAO(void)
{
	vec2_t texScale;
	vec3_t zFarParam;

	if (!r_ssao->value)
		return;

	// blit current framebuffer into ambient occlusion buffer
	glBindFramebuffer(GL_READ_FRAMEBUFFER_EXT, hdrRenderFBO->frameBuffer);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER_EXT, AORenderFBO->frameBuffer);
	glBlitFramebuffer(0, 0, vid.width, vid.height, 0, 0, vid.width, vid.height, GL_COLOR_BUFFER_BIT, GL_LINEAR);

	// bind ambient occlusion buffer and clear to white
	R_BindFBO(AORenderFBO);
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	// set screen scale
	texScale[0] = 1.0f / vid.width;
	texScale[1] = 1.0f / vid.height;

	GL_Enable(!DEPTHTEST_BIT | !CULLFACE_BIT | BLEND_BIT);

	GL_UseProgram(gl_ssaoprog);

	zFarParam[0] = 2.0f * tanf(DEG2RAD(r_newrefdef.fov_x * 0.5f)) / vid.width;
	zFarParam[1] = 2.0f * tanf(DEG2RAD(r_newrefdef.fov_y * 0.5f)) / vid.height;
	zFarParam[2] = 4096;
	glProgramUniform3f(gl_ssaoprog, u_ssaoZFar, zFarParam[0], zFarParam[1], zFarParam[2]);
	glProgramUniform2f(gl_ssaoprog, u_ssaoTexScale, texScale[0], texScale[1]);

	GL_BindTexture(GL_TEXTURE0, GL_TEXTURE_2D, r_drawnearestclampsampler, r_currentDepthRenderImage);

	GL_BindVertexArray(r_postvao);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	R_BindNullFBO();
}

void RPostProcess_FXAA(void)
{
	vec2_t texScale;

	if (!r_fxaa->value)
		return;

	// set screen scale
	texScale[0] = 1.0f / vid.width;
	texScale[1] = 1.0f / vid.height;
	
	GL_Enable(!DEPTHTEST_BIT | !CULLFACE_BIT | BLEND_BIT);
	GL_BlendFunc(GL_DST_COLOR, GL_ONE); // multiplicative blend

	GL_UseProgram(gl_fxaaprog);

	glProgramUniform2f(gl_fxaaprog, u_ssaoTexScale, texScale[0], texScale[1]);

	GL_BindTexture(GL_TEXTURE0, GL_TEXTURE_2D, r_drawnearestclampsampler, r_currentRenderImage);

	GL_BindVertexArray(r_postvao);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	// reset blend mode
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void RPostProcess_MenuBackground(void)
{
	vec2_t texScale;

	// set screen scale
	texScale[0] = 1.0f / vid.width;
	texScale[1] = 1.0f / vid.height;

	GL_Enable(!DEPTHTEST_BIT | !CULLFACE_BIT | !BLEND_BIT);

	GL_UseProgram(gl_compositeprog);

	glProgramUniform2f(gl_compositeprog, u_compositeTexScale, texScale[0], texScale[1]);
	glProgramUniform1i(gl_compositeprog, u_compositeMode, 4);

	GL_BindTexture(GL_TEXTURE0, GL_TEXTURE_2D, r_drawnearestclampsampler, r_currentRenderImage);

	GL_BindVertexArray(r_postvao);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void RPostProcess_Begin(void)
{
	mleaf_t *leaf;

	if (r_skippost || !r_postprocessing->value) return;
	if (!r_worldmodel) return;
	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL) return;
	if (!hdrRenderFBO) return;
	
	// bind HDR framebuffer object
	R_BindFBO(hdrRenderFBO);

	// see if we are underwater 
	leaf = Mod_PointInLeaf(r_origin, r_worldmodel);
	if ((leaf->contents & CONTENTS_WATER) || (leaf->contents & CONTENTS_LAVA) || (leaf->contents & CONTENTS_SLIME))
	{
		r_dowaterwarppost = true;
	}
	else
	{
		r_dowaterwarppost = false;
	}

	// clear out color in framebuffer object before we start drawing to it
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	GL_Clear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
}

void RPostProcess_FinishToScreen(void)
{
	if (r_skippost || !r_postprocessing->value) return;
	if (!r_worldmodel) return;
	if (r_newrefdef.rdflags & RDF_NOWORLDMODEL) return;
	if (!hdrRenderFBO) return;

	R_BindNullFBO ();

	// perform screenspace ambient occlusion
	RPostProcess_SSAO ();

	// downscale to 64x64
	RPostProcess_DownscaleTo64 ();

	// calculate eye adaptation by compute shader
	RPostProcess_ComputeShader_CalculateLuminance ();

	// downscale with bright pass
	RPostProcess_DownscaleBrightpass ();

	// perform bloom and tonemap
	RPostProcess_DoBloomAndTonemap ();

	// set currentrender image for other effects
	RPostProcess_SetCurrentRender ();

	// perform FXAA pass
	RPostProcess_FXAA ();

	// exchange lunimance texture for next frame
	GLuint temp = m_lum[0];
	m_lum[0] = m_lum[1];
	m_lum[1] = temp;

	// reset blending color for next frame
	v_blend[3] = 0;
}
