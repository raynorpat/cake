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
// r_misc.c

#include "gl_local.h"


void GL_BindVertexArray (GLuint vertexarray)
{
	if (gl_state.currentvertexarray != vertexarray)
	{
		glBindVertexArray (vertexarray);
		gl_state.currentvertexarray = vertexarray;
	}
}


void GL_BlendFunc (GLenum sfactor, GLenum dfactor)
{
	if (gl_state.srcblend != sfactor || gl_state.srcblend != dfactor)
	{
		glBlendFunc (sfactor, dfactor);
		gl_state.srcblend = sfactor;
		gl_state.dstblend = dfactor;
	}
}


void GL_Enable (int bits)
{
	if (gl_state.statebits == bits) return;

	if ((bits & CULLFACE_BIT) && !(gl_state.statebits & CULLFACE_BIT)) glEnable (GL_CULL_FACE);
	if (!(bits & CULLFACE_BIT) && (gl_state.statebits & CULLFACE_BIT)) glDisable (GL_CULL_FACE);

	if ((bits & BLEND_BIT) && !(gl_state.statebits & BLEND_BIT)) glEnable (GL_BLEND);
	if (!(bits & BLEND_BIT) && (gl_state.statebits & BLEND_BIT)) glDisable (GL_BLEND);

	if ((bits & DEPTHTEST_BIT) && !(gl_state.statebits & DEPTHTEST_BIT)) glEnable (GL_DEPTH_TEST);
	if (!(bits & DEPTHTEST_BIT) && (gl_state.statebits & DEPTHTEST_BIT)) glDisable (GL_DEPTH_TEST);

	if ((bits & SCISSOR_BIT) && !(gl_state.statebits & SCISSOR_BIT)) glEnable (GL_SCISSOR_TEST);
	if (!(bits & SCISSOR_BIT) && (gl_state.statebits & SCISSOR_BIT)) glDisable (GL_SCISSOR_TEST);

	if ((bits & DEPTHWRITE_BIT) && !(gl_state.statebits & DEPTHWRITE_BIT)) glDepthMask (GL_TRUE);
	if (!(bits & DEPTHWRITE_BIT) && (gl_state.statebits & DEPTHWRITE_BIT)) glDepthMask (GL_FALSE);

	if ((bits & GOURAUD_BIT) && !(gl_state.statebits & GOURAUD_BIT)) glShadeModel (GL_SMOOTH);
	if (!(bits & GOURAUD_BIT) && (gl_state.statebits & GOURAUD_BIT)) glShadeModel (GL_FLAT);

	gl_state.statebits = bits;
}


void GL_GetShaderInfoLog (GLuint s, char *src, qboolean isprog)
{
	static char infolog[4096] = {0};
	int outlen = 0;

	infolog[0] = 0;

	if (isprog)
		glGetProgramInfoLog (s, 4095, &outlen, infolog);
	else glGetShaderInfoLog (s, 4095, &outlen, infolog);

	if (outlen && infolog[0])
	{
		VID_Printf (PRINT_ALL, "%s", infolog);
		OutputDebugString (infolog);
	}
}


qboolean GL_CompileShader (GLuint sh, char *src, GLenum shadertype, char *entrypoint)
{
	char *glslversion = "#version 330 core\n\n";
	char *glslstrings[4];
	char entrydefine[256] = {0};
	char shaderdefine[256] = {0};
	int result = 0;

	if (!sh || !src) return false;

	glGetError ();

	if (entrypoint) sprintf (entrydefine, "#define %s main\n", entrypoint);

	switch (shadertype)
	{
	case GL_VERTEX_SHADER:
		sprintf (shaderdefine, "#define VERTEXSHADER\n#define INOUTTYPE out\n");
		break;

	case GL_FRAGMENT_SHADER:
		sprintf (shaderdefine, "#define FRAGMENTSHADER\n#define INOUTTYPE in\n");
		break;

	default: return false;
	}

	glslstrings[0] = glslversion;
	glslstrings[1] = entrydefine;
	glslstrings[2] = shaderdefine;
	glslstrings[3] = src;

	glShaderSource (sh, 4, glslstrings, NULL);
	glCompileShader (sh);
	glGetShaderiv (sh, GL_COMPILE_STATUS, &result);

	GL_GetShaderInfoLog (sh, src, false);

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
	
	glGetError();
	
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
		return 0;
	else
	{
		// make it active for any further work we may be doing
		glUseProgram(progid);
		gl_state.currentprogram = progid;

		return progid;
	}
}


void GL_UseProgram (GLuint progid)
{
	if (gl_state.currentprogram != progid)
	{
		glUseProgram (progid);
		gl_state.currentprogram = progid;
	}
}


void GL_UseProgramWithUBOs (GLuint progid, ubodef_t *ubodef, int numubos)
{
	if (gl_state.currentprogram != progid)
	{
		int i;

		glUseProgram (progid);

		// more fun with drivers - this isn't supposed to be needed but actually is
		for (i = 0; i < numubos; i++)
			glBindBufferBase (GL_UNIFORM_BUFFER, ubodef[i].binding, ubodef[i].ubo);

		gl_state.currentprogram = progid;
	}
}


/*
==================
R_InitParticleTexture
==================
*/
byte	dottexture[8][8] =
{
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 1, 1, 0, 0, 0, 0},
	{0, 1, 1, 1, 1, 0, 0, 0},
	{0, 1, 1, 1, 1, 0, 0, 0},
	{0, 0, 1, 1, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0},
};

void R_InitParticleTexture (void)
{
	int		x, y;
	byte	data[8][8][4];

	// (particle texture doesn't exist any more)
	// also use this for bad textures, but without alpha
	for (x = 0; x < 8; x++)
	{
		for (y = 0; y < 8; y++)
		{
			data[y][x][0] = dottexture[x&3][y&3] * 255;
			data[y][x][1] = 0; // dottexture[x&3][y&3]*255;
			data[y][x][2] = 0; //dottexture[x&3][y&3]*255;
			data[y][x][3] = 255;
		}
	}

	r_notexture = GL_LoadPic ("***r_notexture***", (byte *) data, 8, 8, it_wall, 32);
}


/*
==============================================================================

						SCREEN SHOTS

==============================================================================
*/

typedef struct _TargaHeader
{
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} TargaHeader;

void SCR_CalcFOV (refdef_t *rd);

void GL_MapShot_f (char *name)
{
	byte *buffer;
	byte *src;
	byte *dst;
	char picname[MAX_OSPATH];
	int i, c, temp;
	FILE *f;
	refdef_t refdef;

	Com_sprintf (picname, sizeof (picname), "%s/save/%s/mapshot.tga", FS_Gamedir (), name);
	FS_CreatePath (picname);

	buffer = malloc (256 * 256 * 4 + 18);
	memset (buffer, 0, 18);
	buffer[2] = 2;		// uncompressed type
	buffer[12] = 256 & 255;
	buffer[13] = 256 >> 8;
	buffer[14] = 256 & 255;
	buffer[15] = 256 >> 8;
	buffer[16] = 24;	// pixel size

	memcpy (&refdef, &r_newrefdef, sizeof (refdef));

	refdef.x = 0;
	refdef.y = 0;
	refdef.width = 256;
	refdef.height = 256;

	SCR_CalcFOV (&refdef);

	// read as bgra for a fast read
	R_RenderView (&refdef);
	glReadPixels (0, vid.height - 256, 256, 256, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, buffer + 18);

	// compress to bgr
	src = buffer + 18;
	dst = buffer + 18;

	for (i = 0; i < 256 * 256; i++, src += 4, dst += 3)
	{
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
	}

	f = fopen (picname, "wb");
	fwrite (buffer, 1, 18 + 256 * 256 * 3, f);
	fclose (f);

	free (buffer);
}


/*
==================
GL_ScreenShot_f
==================
*/
void GL_ScreenShot_f (void)
{
	byte		*buffer;
	char		picname[80];
	char		checkname[MAX_OSPATH];
	int			i, c, temp;
	FILE		*f;

	// create the scrnshots directory if it doesn't exist
	Com_sprintf (checkname, sizeof (checkname), "%s/scrnshot", FS_Gamedir());
	Sys_Mkdir (checkname);

	//
	// find a file name to save it to
	//
	strcpy (picname, "quake00.tga");

	for (i = 0; i <= 99; i++)
	{
		picname[5] = i / 10 + '0';
		picname[6] = i % 10 + '0';
		Com_sprintf (checkname, sizeof (checkname), "%s/scrnshot/%s", FS_Gamedir(), picname);
		f = fopen (checkname, "rb");

		if (!f)
			break;	// file doesn't exist

		fclose (f);
	}

	if (i == 100)
	{
		VID_Printf (PRINT_ALL, "SCR_ScreenShot_f: Couldn't create a file\n");
		return;
	}


	buffer = malloc (vid.width * vid.height * 3 + 18);
	memset (buffer, 0, 18);
	buffer[2] = 2;		// uncompressed type
	buffer[12] = vid.width & 255;
	buffer[13] = vid.width >> 8;
	buffer[14] = vid.height & 255;
	buffer[15] = vid.height >> 8;
	buffer[16] = 24;	// pixel size

	glReadPixels (0, 0, vid.width, vid.height, GL_BGR, GL_UNSIGNED_BYTE, buffer + 18);

	c = 18 + vid.width * vid.height * 3;

	f = fopen (checkname, "wb");
	fwrite (buffer, 1, c, f);
	fclose (f);

	free (buffer);
	VID_Printf (PRINT_ALL, "Wrote %s\n", picname);
}

/*
GL_Strings_f
*/
void GL_Strings_f (void)
{
	VID_Printf (PRINT_ALL, "GL_VENDOR: %s\n", gl_config.vendor_string);
	VID_Printf (PRINT_ALL, "GL_RENDERER: %s\n", gl_config.renderer_string);
	VID_Printf (PRINT_ALL, "GL_VERSION: %s\n", gl_config.version_string);
	// VID_Printf (PRINT_ALL, "GL_EXTENSIONS: %s\n", gl_config.extensions_string);
}

/*
GL_SetDefaultState
*/
void GL_SetDefaultState (void)
{
	glDrawBuffer (GL_BACK);
	glReadBuffer (GL_BACK);

	glClearColor (1, 0, 0.5, 0.5);
	glCullFace (GL_FRONT);

	// assume everything is enabled
	gl_state.statebits = 0xffffffff;

	// and switch everything off
	GL_Enable (0);

	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);

	GL_TextureMode (gl_texturemode->string, (int) gl_textureanisotropy->value);

	gl_state.srcblend = GL_INVALID_VALUE;
	gl_state.dstblend = GL_INVALID_VALUE;

	GL_BlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	RMain_InvalidateCachedState ();
	GL_UpdateSwapInterval ();

	gldepthmin = 0;
	gldepthmax = 1;
	glDepthFunc (GL_LEQUAL);

	glDepthRange (gldepthmin, gldepthmax);
}


void GL_UpdateSwapInterval (void)
{
	if (gl_swapinterval->modified)
	{
		gl_swapinterval->modified = false;
	}
}

