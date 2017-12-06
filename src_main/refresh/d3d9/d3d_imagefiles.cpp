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
==============
LoadPCX
==============
*/
void LoadPCX (char *filename, byte **pic, byte **palette, int *width, int *height)
{
	byte	*raw;
	pcx_t	*pcx;
	int		x, y;
	int		len;
	int		dataByte, runLength;
	byte	*out, *pix;

	*pic = NULL;
	*palette = NULL;

	// load the file
	len = FS_LoadFile (filename, (void **) &raw);

	if (!raw)
	{
		Com_Printf ("Bad pcx file %s\n", filename);
		return;
	}

	// parse the PCX file
	pcx = (pcx_t *) raw;

	pcx->xmin = LittleShort (pcx->xmin);
	pcx->ymin = LittleShort (pcx->ymin);
	pcx->xmax = LittleShort (pcx->xmax);
	pcx->ymax = LittleShort (pcx->ymax);
	pcx->hres = LittleShort (pcx->hres);
	pcx->vres = LittleShort (pcx->vres);

	pcx->bytes_per_line = LittleShort (pcx->bytes_per_line);
	pcx->palette_type = LittleShort (pcx->palette_type);

	raw = &pcx->data;

	if (pcx->manufacturer != 0x0a ||
		pcx->version != 5 ||
		pcx->encoding != 1 ||
		pcx->bits_per_pixel != 8 ||
		pcx->xmax >= 640 ||
		pcx->ymax >= 480)
	{
		Com_Printf ("Bad pcx file %s\n", filename);
		return;
	}

	out = (byte *) Scratch_Alloc ((pcx->ymax + 1) * (pcx->xmax + 1));
	*pic = out;
	pix = out;

	if (palette)
	{
		*palette = (byte *) Scratch_Alloc (768);
		memcpy (*palette, (byte *) pcx + len - 768, 768);
	}

	if (width) *width = pcx->xmax + 1;
	if (height) *height = pcx->ymax + 1;

	for (y = 0; y <= pcx->ymax; y++, pix += pcx->xmax + 1)
	{
		for (x = 0; x <= pcx->xmax ;)
		{
			dataByte = *raw++;

			if ((dataByte & 0xC0) == 0xC0)
			{
				runLength = dataByte & 0x3F;
				dataByte = *raw++;
			}
			else runLength = 1;

			while (runLength-- > 0)
				pix[x++] = dataByte;
		}
	}

	if (raw - (byte *) pcx > len)
	{
		Com_Printf ("PCX file %s was malformed", filename);
		*pic = NULL;
	}

	FS_FreeFile (pcx);
}

/*
=========================================================

TARGA LOADING

=========================================================
*/

typedef struct _TargaHeader
{
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} TargaHeader;


/*
=============
LoadTGA
=============
*/

void LoadTGACommon (int length, byte *buffer, byte **pic, int *width, int *height)
{
	int		columns, rows, numPixels;
	byte	*pixbuf;
	int		row, column;
	byte	*buf_p;
	TargaHeader		targa_header;
	byte			*targa_rgba;
	byte tmp[2];

	if (!buffer)
	{
		Com_Printf ("Bad tga file\n");
		return;
	}

	*pic = NULL;

	buf_p = buffer;

	targa_header.id_length = *buf_p++;
	targa_header.colormap_type = *buf_p++;
	targa_header.image_type = *buf_p++;

	tmp[0] = buf_p[0];
	tmp[1] = buf_p[1];
	targa_header.colormap_index = LittleShort (*((short *) tmp));
	buf_p += 2;
	tmp[0] = buf_p[0];
	tmp[1] = buf_p[1];
	targa_header.colormap_length = LittleShort (*((short *) tmp));
	buf_p += 2;
	targa_header.colormap_size = *buf_p++;
	targa_header.x_origin = LittleShort (*((short *) buf_p));
	buf_p += 2;
	targa_header.y_origin = LittleShort (*((short *) buf_p));
	buf_p += 2;
	targa_header.width = LittleShort (*((short *) buf_p));
	buf_p += 2;
	targa_header.height = LittleShort (*((short *) buf_p));
	buf_p += 2;
	targa_header.pixel_size = *buf_p++;
	targa_header.attributes = *buf_p++;

	if (targa_header.image_type != 2 && targa_header.image_type != 10)
		Com_Error (ERR_DROP, "LoadTGA: Only type 2 and 10 targa RGB images supported\n");

	if (targa_header.colormap_type != 0 || (targa_header.pixel_size != 32 && targa_header.pixel_size != 24))
		Com_Error (ERR_DROP, "LoadTGA: Only 32 or 24 bit images supported (no colormaps)\n");

	columns = targa_header.width;
	rows = targa_header.height;
	numPixels = columns * rows;

	if (width) *width = columns;
	if (height) *height = rows;

	targa_rgba = (byte *) Scratch_Alloc (numPixels * 4);
	*pic = targa_rgba;

	if (targa_header.id_length != 0)
		buf_p += targa_header.id_length; // skip TARGA image comment

	if (targa_header.image_type == 2) // Uncompressed, RGB images
	{
		for (row = rows - 1; row >= 0; row--)
		{
			pixbuf = targa_rgba + row * columns * 4;

			for (column = 0; column < columns; column++)
			{
				switch (targa_header.pixel_size)
				{
				case 24:
					*pixbuf++ = *buf_p++;
					*pixbuf++ = *buf_p++;
					*pixbuf++ = *buf_p++;
					*pixbuf++ = 255;
					break;

				case 32:
					*pixbuf++ = *buf_p++;
					*pixbuf++ = *buf_p++;
					*pixbuf++ = *buf_p++;
					*pixbuf++ = *buf_p++;
					break;
				}
			}
		}
	}
	else if (targa_header.image_type == 10)  // Runlength encoded RGB images
	{
		unsigned char red, green, blue, alphabyte, packetHeader, packetSize, j;

		for (row = rows - 1; row >= 0; row--)
		{
			pixbuf = targa_rgba + row * columns * 4;

			for (column = 0; column < columns;)
			{
				packetHeader = *buf_p++;
				packetSize = 1 + (packetHeader & 0x7f);

				if (packetHeader & 0x80)     // run-length packet
				{
					switch (targa_header.pixel_size)
					{
					case 24:
						blue = *buf_p++;
						green = *buf_p++;
						red = *buf_p++;
						alphabyte = 255;
						break;

					case 32:
						blue = *buf_p++;
						green = *buf_p++;
						red = *buf_p++;
						alphabyte = *buf_p++;
						break;

					default:
						// this cannot be reached as the pixel size check above ensures that one of the above two
						// will always satisfy the switch; this just shuts up warning 4701.
						blue = green = red = alphabyte = 0;
						break;
					}

					for (j = 0; j < packetSize; j++)
					{
						*pixbuf++ = blue;
						*pixbuf++ = green;
						*pixbuf++ = red;
						*pixbuf++ = alphabyte;
						column++;

						if (column == columns) // run spans across rows
						{
							column = 0;

							if (row > 0)
								row--;
							else
								goto breakOut;

							pixbuf = targa_rgba + row * columns * 4;
						}
					}
				}
				else               // non run-length packet
				{
					for (j = 0; j < packetSize; j++)
					{
						switch (targa_header.pixel_size)
						{
						case 24:
							*pixbuf++ = *buf_p++;
							*pixbuf++ = *buf_p++;
							*pixbuf++ = *buf_p++;
							*pixbuf++ = 255;
							break;

						case 32:
							*pixbuf++ = *buf_p++;
							*pixbuf++ = *buf_p++;
							*pixbuf++ = *buf_p++;
							*pixbuf++ = *buf_p++;
							break;
						}

						column++;

						if (column == columns) // pixel packet run spans across rows
						{
							column = 0;

							if (row > 0)
								row--;
							else goto breakOut;

							pixbuf = targa_rgba + row * columns * 4;
						}
					}
				}
			}

breakOut:;
		}
	}
}


void LoadTGAFile (char *name, byte **pic, int *width, int *height)
{
	int length;
	byte *buffer;

	length = FS_LoadFile (name, (void **) &buffer);

	LoadTGACommon (length, buffer, pic, width, height);

	FS_FreeFile (buffer);
}



