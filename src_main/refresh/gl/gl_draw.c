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

// gl_draw.c

#include "gl_local.h"


int r_lastrawcols = -1;
int r_lastrawrows = -1;
GLuint r_rawtexture = 0;
static image_t *draw_chars;

GLuint gl_drawprog = 0;
GLuint u_drawBrightnessAmount = 0;
GLuint u_drawContrastAmount = 0;
GLuint u_drawtexturecolormix = 0;
GLuint u_drawcolorAdd = 0;

#define MAX_DRAW_QUADS	2048

typedef struct drawvert_s
{
	float position[2];

	union
	{
		unsigned color;
		byte rgba[4];
	};

	float texcoord[2];
} drawvert_t;

typedef struct drawstate_s
{
	qboolean drawing;
	GLuint currenttexture;
	GLuint currentsampler;
	float colorAdd[4];
	float texturecolormix;
	float brightness;
	float contrast;
	int firstquad;
	int numquads;
} drawstate_t;

drawstate_t gl_drawstate = {false, 0xffffffff, 0, 0, 0, 0, 0, 0};
drawvert_t gl_drawquads[MAX_DRAW_QUADS * 4];

GLuint gl_drawvbo = 0;
GLuint gl_drawvao = 0;
GLuint gl_drawibo = 0;

void RDraw_CreatePrograms (void)
{
	int i;
	unsigned short *ndx;

	glGenBuffers (1, &gl_drawvbo);
	glNamedBufferDataEXT (gl_drawvbo, MAX_DRAW_QUADS * 4 * sizeof (drawvert_t), NULL, GL_STREAM_DRAW);

	glGenBuffers (1, &gl_drawibo);
	glNamedBufferDataEXT (gl_drawibo, MAX_DRAW_QUADS * 6 * sizeof (unsigned short), NULL, GL_STATIC_DRAW);

	ndx = glMapNamedBufferEXT (gl_drawibo, GL_WRITE_ONLY);

	for (i = 0; i < MAX_DRAW_QUADS; i++, ndx += 6)
	{
		ndx[0] = (i << 2) + 0;
		ndx[1] = (i << 2) + 1;
		ndx[2] = (i << 2) + 2;
		ndx[3] = (i << 2) + 0;
		ndx[4] = (i << 2) + 2;
		ndx[5] = (i << 2) + 3;
	}

	glUnmapNamedBufferEXT (gl_drawibo);

	gl_drawstate.drawing = false;
	gl_drawstate.firstquad = 0;
	gl_drawstate.numquads = 0;
	gl_drawstate.currenttexture = 0xffffffff;
	gl_drawstate.currentsampler = 0xffffffff;
	Vector4Set (gl_drawstate.colorAdd, 1.0, 1.0, 1.0, 1.0);

	glDeleteTextures (1, &r_rawtexture);
	glGenTextures (1, &r_rawtexture);

	r_lastrawcols = -1;
	r_lastrawrows = -1;

	gl_drawprog = GL_CreateShaderFromName ("glsl/draw.glsl", "DrawVS", "DrawFS");

	glProgramUniformMatrix4fv (gl_drawprog, glGetUniformLocation (gl_drawprog, "orthomatrix"), 1, GL_FALSE, r_drawmatrix.m[0]);
	glProgramUniform1i (gl_drawprog, glGetUniformLocation (gl_drawprog, "diffuse"), 0);

	u_drawtexturecolormix = glGetUniformLocation (gl_drawprog, "texturecolormix");
	u_drawBrightnessAmount = glGetUniformLocation (gl_drawprog, "brightnessAmount");
	u_drawContrastAmount = glGetUniformLocation (gl_drawprog, "contrastAmount");
	u_drawcolorAdd = glGetUniformLocation (gl_drawprog, "colorAdd");

	glGenVertexArrays (1, &gl_drawvao);

	glEnableVertexArrayAttribEXT (gl_drawvao, 0);
	glVertexArrayVertexAttribOffsetEXT (gl_drawvao, gl_drawvbo, 0, 2, GL_FLOAT, GL_FALSE, sizeof (drawvert_t), 0);

	glEnableVertexArrayAttribEXT (gl_drawvao, 1);
	glVertexArrayVertexAttribOffsetEXT (gl_drawvao, gl_drawvbo, 1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof (drawvert_t), 8);

	glEnableVertexArrayAttribEXT (gl_drawvao, 2);
	glVertexArrayVertexAttribOffsetEXT (gl_drawvao, gl_drawvbo, 2, 2, GL_FLOAT, GL_FALSE, sizeof (drawvert_t), 12);

	GL_BindVertexArray (gl_drawvao);
	glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, gl_drawibo);
}


void Draw_Begin2D (void)
{
	if (!gl_drawstate.drawing)
	{
		drawvert_t *dv = NULL;

		// set 2D virtual screen size
		glViewport (0, 0, vid.width, vid.height);

		GL_Enable (BLEND_BIT);

		GL_BindVertexArray (gl_drawvao);

		gl_drawstate.currenttexture = 0xffffffff;
		gl_drawstate.currentsampler = 0xffffffff;
		gl_drawstate.numquads = 0;
		gl_drawstate.drawing = true;

		// force a recache on first hit
		gl_drawstate.texturecolormix = -1.0f;
		gl_drawstate.brightness = -1.0f;
		gl_drawstate.contrast = -1.0f;

		// program is always active
		GL_UseProgram (gl_drawprog);
	}
}


void Draw_Flush (void)
{
	if (gl_drawstate.numquads)
	{
		GLvoid *dst = NULL;
		drawvert_t *data = &gl_drawquads[gl_drawstate.firstquad * 4];

		int offset = gl_drawstate.firstquad * sizeof (drawvert_t) * 4;
		int size = gl_drawstate.numquads * sizeof (drawvert_t) * 4;

		if ((dst = glMapNamedBufferRangeEXT (gl_drawvbo, offset, size, BUFFER_NO_OVERWRITE)) != NULL)
		{
			memcpy (dst, data, size);
			glUnmapNamedBufferEXT (gl_drawvbo);

			glDrawElements (
				GL_TRIANGLES,
				gl_drawstate.numquads * 6,
				GL_UNSIGNED_SHORT,
				(void *) (gl_drawstate.firstquad * 6 * sizeof (unsigned short))
			);
		}

		gl_drawstate.firstquad += gl_drawstate.numquads;
		gl_drawstate.numquads = 0;
	}
}


void Draw_End2D (void)
{
	if (gl_drawstate.drawing)
	{
		Draw_Flush ();
		gl_drawstate.drawing = false;
	}
}


void Draw_GenericRect (GLuint texture, GLuint sampler, float texturecolormix, float brightness, float contrast, float x, float y, float w, float h, unsigned color, float sl, float tl, float sh, float th)
{
	drawvert_t *dv = NULL;

	Draw_Begin2D ();

	if (texturecolormix != gl_drawstate.texturecolormix)
	{
		Draw_Flush ();

		glProgramUniform1f (gl_drawprog, u_drawtexturecolormix, texturecolormix);

		gl_drawstate.texturecolormix = texturecolormix;
	}

	if (brightness != gl_drawstate.brightness)
	{
		Draw_Flush ();

		glProgramUniform1f(gl_drawprog, u_drawBrightnessAmount, brightness);

		gl_drawstate.brightness = brightness;
	}

	if (contrast != gl_drawstate.contrast)
	{
		Draw_Flush ();

		glProgramUniform1f(gl_drawprog, u_drawContrastAmount, contrast);

		gl_drawstate.contrast = contrast;
	}

	if (texture != gl_drawstate.currenttexture || sampler != gl_drawstate.currentsampler)
	{
		Draw_Flush ();

		GL_BindTexture (GL_TEXTURE0, GL_TEXTURE_2D, sampler, texture);

		gl_drawstate.currenttexture = texture;
		gl_drawstate.currentsampler = sampler;
	}

	if (gl_drawstate.firstquad + gl_drawstate.numquads + 1 >= MAX_DRAW_QUADS)
	{
		Draw_Flush ();

		glNamedBufferDataEXT (gl_drawvbo, MAX_DRAW_QUADS * 4 * sizeof (drawvert_t), NULL, GL_STREAM_DRAW);

		gl_drawstate.firstquad = 0;
	}

	if (!Vector4Compare(gl_drawstate.colorAdd, colorWhite))
	{
		glProgramUniform4f(gl_drawprog, u_drawcolorAdd, gl_drawstate.colorAdd[0], gl_drawstate.colorAdd[1], gl_drawstate.colorAdd[2], gl_drawstate.colorAdd[3]);
		
		Draw_Flush ();
	}
	else
	{
		glProgramUniform4f(gl_drawprog, u_drawcolorAdd, 1.0, 1.0, 1.0, 1.0);

		Draw_Flush();
	}

	dv = &gl_drawquads[(gl_drawstate.firstquad + gl_drawstate.numquads) * 4];

	Vector2Set (dv[0].position, x, y);
	dv[0].color = color;
	Vector2Set (dv[0].texcoord, sl, tl);

	Vector2Set (dv[1].position, x + w, y);
	dv[1].color = color;
	Vector2Set (dv[1].texcoord, sh, tl);

	Vector2Set (dv[2].position, x + w, y + h);
	dv[2].color = color;
	Vector2Set (dv[2].texcoord, sh, th);

	Vector2Set (dv[3].position, x, y + h);
	dv[3].color = color;
	Vector2Set (dv[3].texcoord, sl, th);

	gl_drawstate.numquads++;
}


void Draw_TexturedRect (GLuint texnum, GLuint sampler, float x, float y, float w, float h, float sl, float tl, float sh, float th)
{
	Draw_GenericRect (texnum, sampler, 0.0f, vid_gamma->value, vid_contrast->value, x, y, w, h, 0xffffffff, sl, tl, sh, th);
}


void Draw_ColouredRect (float x, float y, float w, float h, unsigned color)
{
	// prevent a texture change here
	Draw_GenericRect (gl_drawstate.currenttexture, gl_drawstate.currentsampler, 1.0f, vid_gamma->value, vid_contrast->value, x, y, w, h, color, 0, 0, 0, 0);
}


/*
===============
Draw_InitLocal
===============
*/
void Draw_InitLocal (void)
{
	// load console characters (don't bilerp characters)
	draw_chars = GL_FindImage ("pics/charset.png", it_pic);
	if (!draw_chars)
	{
		draw_chars = GL_FindImage("pics/conchars.pcx", it_pic);
		if (!draw_chars)
			Com_Error(ERR_FATAL, "Couldn't load pics/conchars.pcx");
	}
}

/*
=============
RE_Draw_SetColor

Passing NULL will set the color to white
=============
*/
void RE_GL_Draw_SetColor (float *rgba)
{
	if (!rgba)
	{
		// just send white
		Vector4Set(gl_drawstate.colorAdd, 1.0, 1.0, 1.0, 1.0);
		return;
	}

	gl_drawstate.colorAdd[0] = rgba[0];
	gl_drawstate.colorAdd[1] = rgba[1];
	gl_drawstate.colorAdd[2] = rgba[2];
	gl_drawstate.colorAdd[3] = rgba[3];
}

/*
================
RE_Draw_Char

Draws one 8*8 graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
*/
void RE_GL_Draw_Char (int x, int y, int num, float scale)
{
	float frow, fcol, size, scaledSize;

	num &= 255;

	if ((num & 127) == 32)
		return;			// space
	if (y <= -8)
		return;			// totally off screen

	frow = (num >> 4) * 0.0625;
	fcol = (num & 15) * 0.0625;
	size = 0.0625;

	scaledSize = 8 * scale;

	Draw_TexturedRect (draw_chars->texnum, r_drawnearestclampsampler, x, y, scaledSize, scaledSize, fcol, frow, fcol + size, frow + size);
}

/*
=============
RE_Draw_RegisterPic
=============
*/
image_t	*RE_GL_Draw_RegisterPic(char *name)
{
	image_t *gl;
	char	fullname[MAX_QPATH];

	if (name[0] != '/' && name[0] != '\\')
	{
		Com_sprintf (fullname, sizeof (fullname), "pics/%s.pcx", name);
		gl = GL_FindImage (fullname, it_pic);
		if (!gl)
		{
			Com_sprintf(fullname, sizeof(fullname), "%s", name);
			gl = GL_FindImage(fullname, it_pic);
		}
	}
	else gl = GL_FindImage (name + 1, it_pic);

	return gl;
}

/*
=============
RE_Draw_GetPicSize
=============
*/
void RE_GL_Draw_GetPicSize (int *w, int *h, char *pic)
{
	image_t *gl;

	gl = RE_GL_Draw_RegisterPic (pic);
	if (!gl)
	{
		*w = *h = -1;
		return;
	}

	*w = gl->width;
	*h = gl->height;
}

/*
=============
RE_Draw_StretchPic
=============
*/
void RE_GL_Draw_StretchPic (int x, int y, int w, int h, char *pic)
{
	image_t *gl;

	gl = RE_GL_Draw_RegisterPic (pic);
	if (!gl)
	{
		VID_Printf (PRINT_ALL, S_COLOR_RED "Can't find pic: %s\n", pic);
		return;
	}

	Draw_TexturedRect (gl->texnum, r_drawclampsampler, x, y, w, h, gl->sl, gl->tl, gl->sh, gl->th);
}

/*
=============
RE_Draw_StretchPicExt
=============
*/
void RE_GL_Draw_StretchPicExt (float x, float y, float w, float h, float sl, float tl, float sh, float th, char *pic)
{
	image_t *gl;

	gl = RE_GL_Draw_RegisterPic(pic);
	if (!gl)
	{
		VID_Printf(PRINT_ALL, S_COLOR_RED "Can't find pic: %s\n", pic);
		return;
	}

	Draw_TexturedRect (gl->texnum, r_drawclampsampler, x, y, w, h, sl, tl, sh, th);
}

/*
=============
RE_Draw_Pic
=============
*/
void RE_GL_Draw_Pic (int x, int y, char *pic, float scale)
{
	image_t *gl;

	gl = RE_GL_Draw_RegisterPic (pic);
	if (!gl)
	{
		VID_Printf(PRINT_ALL, S_COLOR_RED "Can't find pic: %s\n", pic);
		return;
	}

	GLfloat w = gl->width * scale;
	GLfloat h = gl->height * scale;

	Draw_TexturedRect(gl->texnum, r_drawclampsampler, x, y, w, h, gl->sl, gl->tl, gl->sh, gl->th);
}


/*
=============
RE_Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void RE_GL_Draw_TileClear (int x, int y, int w, int h, char *pic)
{
	image_t	*image;

	image = RE_GL_Draw_RegisterPic (pic);
	if (!image)
	{
		VID_Printf (PRINT_ALL, S_COLOR_RED "Can't find pic: %s\n", pic);
		return;
	}

	Draw_TexturedRect (image->texnum, r_drawwrapsampler, x, y, w, h, x / 64.0, y / 64.0, (x + w) / 64.0, (y + h) / 64.0);
}


/*
=============
RE_Draw_Fill

Fills a box of pixels with a single color
=============
*/
void RE_GL_Draw_Fill (int x, int y, int w, int h, int c)
{
	Draw_ColouredRect (x, y, w, h, d_8to24table_rgba[c & 255]);
}

//=============================================================================

/*
================
RE_Draw_FadeScreen
================
*/
void RE_GL_Draw_FadeScreen (void)
{
	if (!r_postprocessing->value)
	{
		Draw_ColouredRect (0, 0, vid.width, vid.height, 0xcc000000);
	}
	else
	{
		Draw_End2D ();
		RPostProcess_MenuBackground ();
	}
}

//====================================================================


/*
=============
RE_Draw_StretchRaw
=============
*/
void RE_GL_Draw_StretchRaw (int x, int y, int w, int h, int cols, int rows, byte *data)
{
	Draw_End2D ();

	// bind-to-modify madness here
	if (r_lastrawcols != cols || r_lastrawrows != rows)
	{
		// initial teximage
		glDeleteTextures (1, &r_rawtexture);
		glGenTextures (1, &r_rawtexture);
		glTextureStorage2DEXT (r_rawtexture, GL_TEXTURE_2D, 1, GL_RGBA8, cols, rows);

		// recache
		r_lastrawcols = cols;
		r_lastrawrows = rows;
	}

	// update the texture
	glTextureSubImage2DEXT (r_rawtexture, GL_TEXTURE_2D, 0, 0, 0, cols, rows, GL_RGBA, GL_UNSIGNED_BYTE, data);

	Draw_TexturedRect (r_rawtexture, r_drawclampsampler, x, y, w, h, 0, 0, 1, 1);

	// hack
	Draw_End2D ();
}
