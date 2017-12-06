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


#include "d3d_local.h"


/*
====================================================================

IMAGE FLOOD FILLING

====================================================================
*/


/*
=================
Mod_FloodFillSkin

Fill background pixels so mipmapping doesn't have haloes
=================
*/

typedef struct
{
	short		x, y;
} floodfill_t;

// must be a power of 2
#define FLOODFILL_FIFO_SIZE 0x1000
#define FLOODFILL_FIFO_MASK (FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP( off, dx, dy ) \
{ \
	if (pos[off] == fillcolor) \
	{ \
		pos[off] = 255; \
		fifo[inpt].x = x + (dx), fifo[inpt].y = y + (dy); \
		inpt = (inpt + 1) & FLOODFILL_FIFO_MASK; \
	} \
	else if (pos[off] != 255) fdc = pos[off]; \
}

void R_FloodFillSkin (byte *skin, int skinwidth, int skinheight)
{
	byte				fillcolor = *skin; // assume this is the pixel to fill
	static floodfill_t	fifo[FLOODFILL_FIFO_SIZE];
	int					inpt = 0, outpt = 0;
	int					filledcolor = -1;
	int					i;

	if (filledcolor == -1)
	{
		filledcolor = 0;

		// attempt to find opaque black
		for (i = 0; i < 256; ++i)
		{
			if (d_8to24table[i].peRed == 0 &&
				d_8to24table[i].peGreen == 0 &&
				d_8to24table[i].peBlue == 0) // alpha 1.0
			{
				filledcolor = i;
				break;
			}
		}
	}

	// can't fill to filled color or to transparent color (used as visited marker)
	if ((fillcolor == filledcolor) || (fillcolor == 255))
	{
		//printf( "not filling skin from %d to %d\n", fillcolor, filledcolor );
		return;
	}

	fifo[inpt].x = 0, fifo[inpt].y = 0;
	inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;

	while (outpt != inpt)
	{
		int			x = fifo[outpt].x, y = fifo[outpt].y;
		int			fdc = filledcolor;
		byte		*pos = &skin[x + skinwidth * y];

		outpt = (outpt + 1) & FLOODFILL_FIFO_MASK;

		if (x > 0)				FLOODFILL_STEP (-1, -1, 0);
		if (x < skinwidth - 1)	FLOODFILL_STEP (1, 1, 0);
		if (y > 0)				FLOODFILL_STEP (-skinwidth, 0, -1);
		if (y < skinheight - 1)	FLOODFILL_STEP (skinwidth, 0, 1);

		skin[x + skinwidth *y] = fdc;
	}
}


void GL_Image8To32 (byte *data8, unsigned *data32, int size, unsigned *palette)
{
	int i;

	if (size & 3)
	{
		for (i = 0; i < size; i++)
		{
			data32[i] = palette[data8[i]];
		}
	}
	else
	{
		for (i = 0; i < size; i += 4, data32 += 4, data8 += 4)
		{
			data32[0] = palette[data8[0]];
			data32[1] = palette[data8[1]];
			data32[2] = palette[data8[2]];
			data32[3] = palette[data8[3]];
		}
	}
}


void D3D_GetScaledTextureDimensions (int width, int height, qboolean mipmap, texdimension_t *scaled)
{
	if (!(d3d_DeviceCaps.TextureCaps & D3DPTEXTURECAPS_POW2) && !(d3d_DeviceCaps.TextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL))
	{
		scaled->Width = width;
		scaled->Height = height;
	}
	else
	{
		for (scaled->Width = 1; scaled->Width < width; scaled->Width <<= 1);
		for (scaled->Height = 1; scaled->Height < height; scaled->Height <<= 1);
	}

	if (gl_round_down->value && scaled->Width > width) scaled->Width >>= 1;
	if (gl_round_down->value && scaled->Height > height) scaled->Height >>= 1;

	// should picmip be a sampler state option or does value remain in using it to reduce vram usage (esp as we're requiring sm3)???
	if (mipmap && gl_picmip->value > 0)
	{
		scaled->Width >>= (int) gl_picmip->value;
		scaled->Height >>= (int) gl_picmip->value;
	}

	// never go below 1
	if (scaled->Width < 1) scaled->Width = 1;
	if (scaled->Height < 1) scaled->Height = 1;

	// these are hardware limits too so screw you if you want them smaller
	while (scaled->Width / scaled->Height > d3d_DeviceCaps.MaxTextureAspectRatio) scaled->Height <<= 1;
	while (scaled->Height / scaled->Width > d3d_DeviceCaps.MaxTextureAspectRatio) scaled->Width <<= 1;

	// finally clamp to max implementation supported size
	if (scaled->Width > d3d_DeviceCaps.MaxTextureWidth) scaled->Width = d3d_DeviceCaps.MaxTextureWidth;
	if (scaled->Height > d3d_DeviceCaps.MaxTextureHeight) scaled->Height = d3d_DeviceCaps.MaxTextureHeight;
}


void D3D_CollapseRowPitch (unsigned *data, int width, int height, int pitch)
{
	if (width != pitch)
	{
		unsigned *out = data;

		// as a minor optimization we can skip the first row
		// since out and data point to the same this is OK
		out += width;
		data += pitch;

		for (int h = 1; h < height; h++)
		{
			for (int w = 0; w < width; w++)
				out[w] = data[w];

			out += width;
			data += pitch;
		}
	}
}


void D3D_Compress32To24 (byte *data, int width, int height)
{
	byte *out = data;

	for (int h = 0; h < height; h++)
	{
		for (int w = 0; w < width; w++, data += 4, out += 3)
		{
			out[0] = data[0];
			out[1] = data[1];
			out[2] = data[2];
		}
	}
}


void D3D_WriteDataToTGA (char *name, void *data, int width, int height, int bpp)
{
	if ((bpp == 24 || bpp == 8) && name && data && width > 0 && height > 0)
	{
		FILE *f = fopen (name, "wb");

		if (f)
		{
			byte header[18];

			memset (header, 0, 18);

			header[2] = 2;
			header[12] = width & 255;
			header[13] = width >> 8;
			header[14] = height & 255;
			header[15] = height >> 8;
			header[16] = bpp;
			header[17] = 0x20;

			fwrite (header, 18, 1, f);
			fwrite (data, (width * height * bpp) >> 3, 1, f);

			fclose (f);
		}
	}
}

