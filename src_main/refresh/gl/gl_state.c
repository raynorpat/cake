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
// gl_state.c
#include "gl_local.h"

void GL_BindVertexArray(GLuint vertexarray)
{
	if (gl_state.currentvertexarray != vertexarray)
	{
		glBindVertexArray(vertexarray);
		gl_state.currentvertexarray = vertexarray;
	}
}


void GL_BlendFunc(GLenum sfactor, GLenum dfactor)
{
	if (gl_state.srcblend != sfactor || gl_state.srcblend != dfactor)
	{
		glBlendFunc(sfactor, dfactor);
		gl_state.srcblend = sfactor;
		gl_state.dstblend = dfactor;
	}
}


void GL_Enable(int bits)
{
	if (gl_state.statebits == bits) return;

	if ((bits & CULLFACE_BIT) && !(gl_state.statebits & CULLFACE_BIT)) glEnable(GL_CULL_FACE);
	if (!(bits & CULLFACE_BIT) && (gl_state.statebits & CULLFACE_BIT)) glDisable(GL_CULL_FACE);

	if ((bits & BLEND_BIT) && !(gl_state.statebits & BLEND_BIT)) glEnable(GL_BLEND);
	if (!(bits & BLEND_BIT) && (gl_state.statebits & BLEND_BIT)) glDisable(GL_BLEND);

	if ((bits & DEPTHTEST_BIT) && !(gl_state.statebits & DEPTHTEST_BIT)) glEnable(GL_DEPTH_TEST);
	if (!(bits & DEPTHTEST_BIT) && (gl_state.statebits & DEPTHTEST_BIT)) glDisable(GL_DEPTH_TEST);

	if ((bits & SCISSOR_BIT) && !(gl_state.statebits & SCISSOR_BIT)) glEnable(GL_SCISSOR_TEST);
	if (!(bits & SCISSOR_BIT) && (gl_state.statebits & SCISSOR_BIT)) glDisable(GL_SCISSOR_TEST);

	if ((bits & DEPTHWRITE_BIT) && !(gl_state.statebits & DEPTHWRITE_BIT)) glDepthMask(GL_TRUE);
	if (!(bits & DEPTHWRITE_BIT) && (gl_state.statebits & DEPTHWRITE_BIT)) glDepthMask(GL_FALSE);

	if ((bits & GOURAUD_BIT) && !(gl_state.statebits & GOURAUD_BIT)) glShadeModel(GL_SMOOTH);
	if (!(bits & GOURAUD_BIT) && (gl_state.statebits & GOURAUD_BIT)) glShadeModel(GL_FLAT);

	gl_state.statebits = bits;
}


void GL_Clear(GLbitfield mask)
{
	// because depth/stencil are interleaved, if we're clearing depth we must also clear stencil
	if (mask & GL_DEPTH_BUFFER_BIT)
		mask |= GL_STENCIL_BUFFER_BIT;

	glClear(mask);
}


void GL_CheckError (char *str)
{
	GLenum err = glGetError();

	switch (err)
	{
	case GL_INVALID_ENUM: VID_Printf(PRINT_ALL, S_COLOR_RED "GL_INVALID_ENUM"); break;
	case GL_INVALID_VALUE: VID_Printf(PRINT_ALL, S_COLOR_RED "GL_INVALID_VALUE"); break;
	case GL_INVALID_OPERATION: VID_Printf(PRINT_ALL, S_COLOR_RED "GL_INVALID_OPERATION"); break;
	case GL_STACK_OVERFLOW: VID_Printf(PRINT_ALL, S_COLOR_RED "GL_STACK_OVERFLOW"); break;
	case GL_STACK_UNDERFLOW: VID_Printf(PRINT_ALL, S_COLOR_RED "GL_STACK_UNDERFLOW"); break;
	case GL_OUT_OF_MEMORY: VID_Printf(PRINT_ALL, S_COLOR_RED  "GL_OUT_OF_MEMORY"); break;
	case GL_TABLE_TOO_LARGE: VID_Printf(PRINT_ALL, S_COLOR_RED "GL_TABLE_TOO_LARGE"); break;
	default: return;
	}

	if (str)
		VID_Printf(PRINT_ALL, S_COLOR_RED " with %s\n", str);
	else VID_Printf(PRINT_ALL, S_COLOR_RED "\n");
}


void GL_GetShaderInfoLog(GLuint s, char *src, qboolean isprog)
{
	static char infolog[4096] = { 0 };
	int outlen = 0;

	infolog[0] = 0;

	if (isprog)
		glGetProgramInfoLog(s, 4095, &outlen, infolog);
	else glGetShaderInfoLog(s, 4095, &outlen, infolog);

	if (outlen && infolog[0])
		VID_Printf(PRINT_ALL, S_COLOR_RED "%s", infolog);
}

static void GL_GetShaderHeader (GLenum shadertype, GLchar *entrypoint, char *dest, int size)
{
	float fbufWidthScale, fbufHeightScale;

	memset (dest, 0, sizeof(dest));

	// shader version string
	Q_strlcat (dest, "#version 330 core\n", size);

	// extension strings
	Q_strlcat (dest, "#extension GL_EXT_texture_array : enable\n", size);
	Q_strlcat (dest, "#extension GL_ARB_texture_gather : require\n", size);
	if (gl_config.gl_ext_GPUShader5_support)
		Q_strlcat (dest, "#extension GL_ARB_gpu_shader5 : require\n", size);

	// define entry point
	if (entrypoint)
		Q_strlcat (dest, va("#define %s main\n", entrypoint), size);

	// define shader type
	switch (shadertype)
	{
		case GL_VERTEX_SHADER:
			Q_strlcat (dest, "#define VERTEXSHADER\n#define INOUTTYPE out\n", size);
			break;

		case GL_FRAGMENT_SHADER:
			Q_strlcat (dest, "#define FRAGMENTSHADER\n#define INOUTTYPE in\n", size);
			break;
	}

	// framebuffer scale
	fbufWidthScale = 1.0f / ((float)vid.width);
	fbufHeightScale = 1.0f / ((float)vid.height);
	Q_strlcat (dest, va("#ifndef r_FBufScale\n#define r_FBufScale vec2(%f, %f)\n#endif\n", fbufWidthScale, fbufHeightScale), size);

	// load up common.glsl
	char *commonbuf = NULL;
	char *commonsrc;
	int commonsize = FS_LoadFile("glsl/common.glsl", (void **)&commonbuf);

	// the file doesn't have a trailing 0, so we need to copy it off
	commonsrc = malloc (commonsize + 1);
	memcpy (commonsrc, commonbuf, commonsize);
	commonsrc[commonsize] = 0;

	// prepend common.glsl to the header
	strncat (dest, commonsrc, commonsize);
	free (commonsrc);

	// OK we added a lot of stuff but if we do something bad in the GLSL shaders then we want the proper line
	// so we have to reset the line counting
	Q_strlcat (dest, "#line 0\n", size);
}

qboolean GL_CompileShader(GLuint sh, char *src, GLenum shadertype, char *entrypoint)
{
	char shaderHeader[32000];
	char *shaderFinal = NULL;
	int sizeHeader, sizeFinal;
	int result = 0;

	if (!sh || !src) return false;

	GL_CheckError (__func__);

	// generate GLSL header
	GL_GetShaderHeader (shadertype, entrypoint, shaderHeader, sizeof(shaderHeader));
	sizeHeader = strlen (shaderHeader);
	sizeFinal = sizeHeader + strlen (src);

	// put everything together
	shaderFinal = malloc (sizeFinal);
	Q_strlcpy (shaderFinal, shaderHeader, sizeFinal);
	Q_strlcat (shaderFinal, src, sizeFinal);

	// compile into shader program
	glShaderSource(sh, 1, &shaderFinal, &sizeFinal);
	glCompileShader(sh);
	glGetShaderiv(sh, GL_COMPILE_STATUS, &result);

	GL_GetShaderInfoLog(sh, src, false);

	free (shaderFinal);

	if (result != GL_TRUE)
		return false;
	else return true;
}

GLuint GL_CreateShaderFromName(char *name, char *vsentry, char *fsentry)
{
	GLuint progid = 0;
	char *resbuf = NULL;
	int reslen = FS_LoadFile(name, (void **)&resbuf);
	char *ressrc;
	GLuint vs, fs;
	int result = 0;

	if (!reslen)
		return 0;

	// the file doesn't have a trailing 0, so we need to copy it off
	ressrc = malloc(reslen + 1);
	memcpy(ressrc, resbuf, reslen);
	ressrc[reslen] = 0;

	vs = glCreateShader(GL_VERTEX_SHADER);
	fs = glCreateShader(GL_FRAGMENT_SHADER);

	GL_CheckError (__func__);

	// this crap really should have been built-in to GLSL...
	if (!GL_CompileShader(vs, ressrc, GL_VERTEX_SHADER, vsentry))
		return 0;
	if (!GL_CompileShader(fs, ressrc, GL_FRAGMENT_SHADER, fsentry))
		return 0;

	progid = glCreateProgram();

	glAttachShader(progid, vs);
	glAttachShader(progid, fs);

	glLinkProgram(progid);
	glGetProgramiv(progid, GL_LINK_STATUS, &result);

	// the shaders are compiled, attached and linked so this just marks them for deletion
	glDeleteShader(vs);
	glDeleteShader(fs);

	GL_GetShaderInfoLog(progid, "", true);

	free(ressrc);
	FS_FreeFile(resbuf);

	if (result != GL_TRUE)
	{
		return 0;
	}
	else
	{
		// make it active for any further work we may be doing
		glUseProgram(progid);
		gl_state.currentprogram = progid;
		return progid;
	}
}

GLuint GL_CreateComputeShaderFromName(char *name)
{
	GLuint progid = 0;
	char *resbuf = NULL;
	int reslen = FS_LoadFile(name, (void **)&resbuf);
	char *ressrc;
	GLuint cs;
	int result = 0;

	if (!reslen)
		return 0;

	// the file doesn't have a trailing 0, so we need to copy it off
	ressrc = malloc(reslen + 1);
	memcpy(ressrc, resbuf, reslen);
	ressrc[reslen] = 0;

	cs = glCreateShader(GL_COMPUTE_SHADER);

	GL_CheckError (__func__);

	// this crap really should have been built-in to GLSL...
	glShaderSource(cs, 1, &(ressrc), 0);
	glCompileShader(cs);
	GLint compiled = 0;
	glGetShaderiv(cs, GL_COMPILE_STATUS, &compiled);
	if (!compiled)
		return 0;

	progid = glCreateProgram();

	glAttachShader(progid, cs);

	glLinkProgram(progid);
	glGetProgramiv(progid, GL_LINK_STATUS, &result);

	// the shader are compiled, attached and linked so this just marks them for deletion
	glDeleteShader(cs);

	GL_GetShaderInfoLog(progid, "", true);

	free(ressrc);
	FS_FreeFile(resbuf);

	if (result != GL_TRUE)
	{
		return 0;
	}
	else
	{
		// make it active for any further work we may be doing
		glUseProgram(progid);
		gl_state.currentprogram = progid;
		return progid;
	}
}

void GL_UseProgram(GLuint progid)
{
	if (gl_state.currentprogram != progid)
	{
		glUseProgram(progid);
		gl_state.currentprogram = progid;
	}
}


void GL_UseProgramWithUBOs(GLuint progid, ubodef_t *ubodef, int numubos)
{
	if (gl_state.currentprogram != progid)
	{
		int i;

		glUseProgram(progid);

		// more fun with drivers - this isn't supposed to be needed but actually is
		for (i = 0; i < numubos; i++)
			glBindBufferBase(GL_UNIFORM_BUFFER, ubodef[i].binding, ubodef[i].ubo);

		gl_state.currentprogram = progid;
	}
}


/*
GL_SetDefaultState
*/
void GL_SetDefaultState(void)
{
	glDrawBuffer(GL_BACK);
	glReadBuffer(GL_BACK);

	glClearColor(1, 0, 0.5, 0.5);
	glCullFace(GL_FRONT);

	// assume everything is enabled
	gl_state.statebits = 0xffffffff;

	// and switch everything off
	GL_Enable(0);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	GL_TextureMode(gl_texturemode->string, (int)gl_textureanisotropy->value);

	gl_state.srcblend = GL_INVALID_VALUE;
	gl_state.dstblend = GL_INVALID_VALUE;

	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	RMain_InvalidateCachedState();
	GL_UpdateSwapInterval();

	gldepthmin = 0;
	gldepthmax = 1;
	glDepthFunc(GL_LEQUAL);

	glDepthRange(gldepthmin, gldepthmax);

	R_BindNullFBO();
}


void GL_UpdateSwapInterval(void)
{
	if (gl_swapinterval->modified)
	{
		gl_swapinterval->modified = false;
	}
}
