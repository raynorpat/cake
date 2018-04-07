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
			data[y][x][1] = 0;
			data[y][x][2] = 0;
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

void SCR_CalcFOV (refdef_t *rd);

void GL_MapShot_f (char *name)
{
	byte *buffer;
	byte *src;
	byte *dst;
	char picname[MAX_OSPATH];
	int i;
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
	int w = vid.width, h = vid.height;
	byte *buffer = malloc(w * h * 3);

	if (!buffer)
	{
		VID_Printf(PRINT_ALL, S_COLOR_RED "GL_ScreenShot_f: Couldn't malloc %d bytes\n", w * h * 3);
		return;
	}

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, buffer);

	// the pixels are now row-wise left to right, bottom to top,
	// but we need them row-wise left to right, top to bottom.
	// so swap bottom rows with top rows
	{
		size_t bytesPerRow = 3 * w;
		byte *rowBuffer = (byte *)Q_alloca(sizeof(byte) * bytesPerRow);
		byte *curRowL = buffer; // first byte of first row
		byte *curRowH = buffer + bytesPerRow*(h - 1); // first byte of last row
		while (curRowL < curRowH)
		{
			memcpy(rowBuffer, curRowL, bytesPerRow);
			memcpy(curRowL, curRowH, bytesPerRow);
			memcpy(curRowH, rowBuffer, bytesPerRow);
			
			curRowL += bytesPerRow;
			curRowH -= bytesPerRow;
		}
	}

	VID_WriteScreenshot(w, h, 3, buffer);
}
