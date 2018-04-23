
#include "gl_local.h"

static struct
{
	GLuint textures[16];
	GLenum texunit;

	GLuint program;

	GLuint drawFramebuffer;
	GLuint readFramebuffer;
	GLuint renderbuffer;
} glDsaState;

/*
glMapNamedBufferEXT
glMapNamedBufferRangeEXT
glUnmapNamedBufferEXT
glNamedBufferDataEXT
glNamedBufferSubDataEXT

glEnableVertexArrayAttribEXT
glVertexArrayVertexAttribOffsetEXT

glTextureStorage2DEXT
glTextureSubImage2DEXT
glTextureImage2DEXT
glTextureSubImage2DEXT
glTextureSubImage3DEXT

glBindMultiTextureEXT
*/

void GL_BindNullTextures(void)
{
	int i;

	if (gl_config.gl_ext_directstateaccess_support)
	{
		for (i = 0; i < 16; i++)
		{
			glBindMultiTextureEXT(GL_TEXTURE0 + i, GL_TEXTURE_2D, 0);
			glDsaState.textures[i] = 0;
		}
	}
	else
	{
		for (i = 0; i < 16; i++)
		{
			glActiveTexture(GL_TEXTURE0 + i);
			glBindTexture(GL_TEXTURE_2D, 0);
			glDsaState.textures[i] = 0;
		}

		glActiveTexture(GL_TEXTURE0);
		glDsaState.texunit = GL_TEXTURE0;
	}
}

int GL_BindMultiTexture(GLenum texunit, GLenum target, GLuint texture)
{
	GLuint tmu = texunit - GL_TEXTURE0;

	if (glDsaState.textures[tmu] == texture)
		return 0;

	if (target >= GL_TEXTURE_CUBE_MAP_POSITIVE_X && target <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z)
		target = GL_TEXTURE_CUBE_MAP;

	glBindMultiTextureEXT(texunit, target, texture);
	glDsaState.textures[tmu] = texture;
	return 1;
}

GLvoid APIENTRY GLDSA_BindMultiTextureEXT(GLenum texunit, GLenum target, GLuint texture)
{
	if (glDsaState.texunit != texunit)
	{
		glActiveTexture(texunit);
		glDsaState.texunit = texunit;
	}

	glBindTexture(target, texture);
}

GLvoid APIENTRY GLDSA_TextureParameterfEXT(GLuint texture, GLenum target, GLenum pname, GLfloat param)
{
	GL_BindMultiTexture(glDsaState.texunit, target, texture);
	glTexParameterf(target, pname, param);
}

GLvoid APIENTRY GLDSA_TextureParameteriEXT(GLuint texture, GLenum target, GLenum pname, GLint param)
{
	GL_BindMultiTexture(glDsaState.texunit, target, texture);
	glTexParameteri(target, pname, param);
}

GLvoid APIENTRY GLDSA_TextureImage2DEXT(GLuint texture, GLenum target, GLint level, GLint internalformat,
	GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
	GL_BindMultiTexture(glDsaState.texunit, target, texture);
	glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
}

GLvoid APIENTRY GLDSA_TextureSubImage2DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset,
	GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels)
{
	GL_BindMultiTexture(glDsaState.texunit, target, texture);
	glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

GLvoid APIENTRY GLDSA_CopyTextureSubImage2DEXT(GLuint texture, GLenum target, GLint level, GLint xoffset, GLint yoffset,
	GLint x, GLint y, GLsizei width, GLsizei height)
{
	GL_BindMultiTexture(glDsaState.texunit, target, texture);
	glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
}

GLvoid APIENTRY  GLDSA_CompressedTextureImage2DEXT(GLuint texture, GLenum target, GLint level, GLenum internalformat,
	GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid *data)
{
	GL_BindMultiTexture(glDsaState.texunit, target, texture);
	glCompressedTexImage2D(target, level, internalformat, width, height, border, imageSize, data);
}

GLvoid APIENTRY GLDSA_CompressedTextureSubImage2DEXT(GLuint texture, GLenum target, GLint level,
	GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format,
	GLsizei imageSize, const GLvoid *data)
{
	GL_BindMultiTexture(glDsaState.texunit, target, texture);
	glCompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, data);
}

GLvoid APIENTRY GLDSA_GenerateTextureMipmapEXT(GLuint texture, GLenum target)
{
	GL_BindMultiTexture(glDsaState.texunit, target, texture);
	glGenerateMipmap(target);
}

void GL_BindNullProgram()
{
	GL_UseProgramNoUBOs(0);
	glDsaState.program = 0;
}

int GL_UseProgram(GLuint program)
{
	if (glDsaState.program == program)
		return 0;

	GL_UseProgramNoUBOs(program);
	glDsaState.program = program;
	return 1;
}

GLvoid APIENTRY GLDSA_ProgramUniform1iEXT(GLuint program, GLint location, GLint v0)
{
	GL_UseProgram(program);
	glUniform1i(location, v0);
}

GLvoid APIENTRY GLDSA_ProgramUniform1fEXT(GLuint program, GLint location, GLfloat v0)
{
	GL_UseProgram(program);
	glUniform1f(location, v0);
}

GLvoid APIENTRY GLDSA_ProgramUniform2fEXT(GLuint program, GLint location,
	GLfloat v0, GLfloat v1)
{
	GL_UseProgram(program);
	glUniform2f(location, v0, v1);
}

GLvoid APIENTRY GLDSA_ProgramUniform3fEXT(GLuint program, GLint location,
	GLfloat v0, GLfloat v1, GLfloat v2)
{
	GL_UseProgram(program);
	glUniform3f(location, v0, v1, v2);
}

GLvoid APIENTRY GLDSA_ProgramUniform4fEXT(GLuint program, GLint location,
	GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)
{
	GL_UseProgram(program);
	glUniform4f(location, v0, v1, v2, v3);
}

GLvoid APIENTRY GLDSA_ProgramUniform1fvEXT(GLuint program, GLint location,
	GLsizei count, const GLfloat *value)
{
	GL_UseProgram(program);
	glUniform1fv(location, count, value);
}

GLvoid APIENTRY GLDSA_ProgramUniformMatrix4fvEXT(GLuint program, GLint location,
	GLsizei count, GLboolean transpose,
	const GLfloat *value)
{
	GL_UseProgram(program);
	glUniformMatrix4fv(location, count, transpose, value);
}

void GL_BindNullFramebuffers(void)
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDsaState.drawFramebuffer = glDsaState.readFramebuffer = 0;
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
	glDsaState.renderbuffer = 0;
}

void GL_BindFramebuffer(GLenum target, GLuint framebuffer)
{
	switch (target)
	{
		case GL_FRAMEBUFFER:
			if (framebuffer != glDsaState.drawFramebuffer || framebuffer != glDsaState.readFramebuffer)
			{
				glBindFramebuffer(target, framebuffer);
				glDsaState.drawFramebuffer = glDsaState.readFramebuffer = framebuffer;
			}
			break;

		case GL_DRAW_FRAMEBUFFER:
			if (framebuffer != glDsaState.drawFramebuffer)
			{
				glBindFramebuffer(target, framebuffer);
				glDsaState.drawFramebuffer = framebuffer;
			}
			break;

		case GL_READ_FRAMEBUFFER:
			if (framebuffer != glDsaState.readFramebuffer)
			{
				glBindFramebuffer(target, framebuffer);
				glDsaState.readFramebuffer = framebuffer;
			}
			break;
	}
}

void GL_BindRenderbuffer(GLuint renderbuffer)
{
	if (renderbuffer != glDsaState.renderbuffer)
	{
		glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
		glDsaState.renderbuffer = renderbuffer;
	}
}

GLvoid APIENTRY GLDSA_NamedRenderbufferStorageEXT(GLuint renderbuffer,
	GLenum internalformat, GLsizei width, GLsizei height)
{
	GL_BindRenderbuffer(renderbuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, internalformat, width, height);
}

GLvoid APIENTRY GLDSA_NamedRenderbufferStorageMultisampleEXT(GLuint renderbuffer,
	GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height)
{
	GL_BindRenderbuffer(renderbuffer);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, internalformat, width, height);
}

GLenum APIENTRY GLDSA_CheckNamedFramebufferStatusEXT(GLuint framebuffer, GLenum target)
{
	GL_BindFramebuffer(target, framebuffer);
	return glCheckFramebufferStatus(target);
}

GLvoid APIENTRY GLDSA_NamedFramebufferTexture2DEXT(GLuint framebuffer,
	GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
	GL_BindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, textarget, texture, level);
}

GLvoid APIENTRY GLDSA_NamedFramebufferRenderbufferEXT(GLuint framebuffer,
	GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
	GL_BindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment, renderbuffertarget, renderbuffer);
}
