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

#include "gl_local.h"

unsigned	d_8to24table_rgba[256];
unsigned	d_8to24table_bgra[256];


int img_base = 0;

HANDLE ImageHeap = NULL;


void *Img_Alloc (int size)
{
	byte *data = (byte *) Scratch_Alloc ();

	data += img_base;
	img_base += size;

	return data;
}


void Img_Free (void)
{
	img_base = 0;
}


int Sys_LoadResourceData (int resourceid, void **resbuf);
void GL_ResampleTexture (unsigned *in, int inwidth, int inheight, unsigned *out, int outwidth, int outheight);


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


typedef struct glmode_s
{
	char *name;
	int	minimize, maximize;
} glmode_t;

glmode_t modes[] =
{
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};

#define NUM_GL_MODES (sizeof(modes) / sizeof (glmode_t))

int gl_texture_anisotropy = 1;


GLuint r_surfacesampler = 0;
GLuint r_modelsampler = 0;
GLuint r_lightmapsampler = 0;
GLuint r_drawclampsampler = 0;
GLuint r_drawwrapsampler = 0;
GLuint r_charsetsampler = 0;
GLuint r_skysampler = 0;


void RImage_SamplerMinMag (GLuint sampler, GLenum minfilter, GLenum magfilter, int anisotropy)
{
	glSamplerParameteri (sampler, GL_TEXTURE_MIN_FILTER, minfilter);
	glSamplerParameteri (sampler, GL_TEXTURE_MAG_FILTER, magfilter);
	glSamplerParameteri (sampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);
}


void RImage_SamplerClampMode (GLuint sampler, GLenum clampmode)
{
	glSamplerParameteri (sampler, GL_TEXTURE_WRAP_S, clampmode);
	glSamplerParameteri (sampler, GL_TEXTURE_WRAP_T, clampmode);
	glSamplerParameteri (sampler, GL_TEXTURE_WRAP_R, clampmode);
}


void RImage_UpdateSampler (GLuint sampler, GLenum clampmode)
{
	if (gl_texture_anisotropy > 1)
		RImage_SamplerMinMag (sampler, GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, gl_texture_anisotropy);
	else RImage_SamplerMinMag (sampler, gl_filter_min, gl_filter_max, 1);

	RImage_SamplerClampMode (sampler, clampmode);
}


void RImage_SetSampler (GLuint sampler, GLenum minmag, GLenum clampmode)
{
	RImage_SamplerMinMag (sampler, minmag, minmag, 1);
	RImage_SamplerClampMode (sampler, clampmode);
}


void RImage_CreateSamplers (void)
{
	glGenSamplers (1, &r_lightmapsampler);
	RImage_SetSampler (r_lightmapsampler, GL_LINEAR, GL_CLAMP_TO_EDGE);

	glGenSamplers (1, &r_drawclampsampler);
	RImage_SetSampler (r_drawclampsampler, GL_LINEAR, GL_CLAMP_TO_EDGE);

	glGenSamplers (1, &r_drawwrapsampler);
	RImage_SetSampler (r_drawwrapsampler, GL_LINEAR, GL_REPEAT);

	glGenSamplers (1, &r_charsetsampler);
	RImage_SetSampler (r_charsetsampler, GL_NEAREST, GL_CLAMP_TO_EDGE);

	glGenSamplers (1, &r_skysampler);
	RImage_SetSampler (r_skysampler, GL_LINEAR, GL_CLAMP_TO_EDGE);

	glGenSamplers (1, &r_surfacesampler);
	RImage_UpdateSampler (r_surfacesampler, GL_REPEAT);

	glGenSamplers (1, &r_modelsampler);
	RImage_UpdateSampler (r_modelsampler, GL_CLAMP_TO_EDGE);
}


image_t		gltextures[MAX_GLTEXTURES];
int			numgltextures;

static unsigned char gammatable[256];

int		gl_filter_min = GL_LINEAR_MIPMAP_LINEAR;
int		gl_filter_max = GL_LINEAR;


void GL_BindTexture (GLenum tmu, GLenum target, GLuint sampler, GLuint texnum)
{
	int tmunum = tmu - GL_TEXTURE0;

	if (tmu < GL_TEXTURE0) return;
	if (tmu > GL_TEXTURE31) return;

	if (gl_state.currenttextures[tmunum] != texnum)
	{
		glBindMultiTextureEXT (tmu, target, texnum);
		gl_state.currenttextures[tmunum] = texnum;
	}

	if (gl_state.currentsamplers[tmunum] != sampler)
	{
		glBindSampler (tmunum, sampler);
		gl_state.currentsamplers[tmunum] = sampler;
	}
}


/*
===============
GL_TextureMode
===============
*/
void GL_TextureMode (char *string, int anisotropy)
{
	int i;
	int af;
	float maxaf;

	for (i = 0; i < NUM_GL_MODES; i++)
	{
		if (!Q_stricmp (modes[i].name, string))
			break;
	}

	if (i == NUM_GL_MODES)
	{
		VID_Printf (PRINT_ALL, "bad filter name\n");
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	gl_texture_anisotropy = 1;

	if (GLEW_EXT_texture_filter_anisotropic)
	{
		glGetFloatv (GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxaf);

		if (maxaf > 1)
		{
			for (af = 1; af < anisotropy; af <<= 1);

			if (af < 1) af = 1;
			if (af > maxaf) af = maxaf;

			gl_texture_anisotropy = af;
		}
	}

	RImage_UpdateSampler (r_surfacesampler, GL_REPEAT);
	RImage_UpdateSampler (r_modelsampler, GL_CLAMP_TO_EDGE);

	for (i = 0; i < 32; i++)
	{
		gl_state.currentsamplers[i] = 0xffffffff;
		gl_state.currenttextures[i] = 0xffffffff;
	}
}


/*
===============
GL_ImageList_f
===============
*/
void GL_ImageList_f (void)
{
	int		i;
	image_t	*image;
	int		texels;
	const char *palstrings[2] =
	{
		"RGB",
		"PAL"
	};

	VID_Printf (PRINT_ALL, "------------------\n");
	texels = 0;

	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		if (image->texnum <= 0)
			continue;

		texels += image->upload_width * image->upload_height;

		switch (image->type)
		{
		case it_skin:
			VID_Printf (PRINT_ALL, "M");
			break;
		case it_sprite:
			VID_Printf (PRINT_ALL, "S");
			break;
		case it_wall:
			VID_Printf (PRINT_ALL, "W");
			break;
		case it_pic:
			VID_Printf (PRINT_ALL, "P");
			break;
		default:
			VID_Printf (PRINT_ALL, " ");
			break;
		}

		VID_Printf (PRINT_ALL, " %3i %3i: %s\n", image->upload_width, image->upload_height, image->name);
	}

	VID_Printf (PRINT_ALL, "Total texel count (not counting mipmaps): %i\n", texels);
}


/*
=============================================================================

 scrap allocation

 Allocate all the little status bar obejcts into a single texture
 to crutch up inefficient hardware / drivers

=============================================================================
*/

#define	MAX_SCRAPS		8
#define	BLOCK_WIDTH		256
#define	BLOCK_HEIGHT	256

typedef struct scrapblock_s
{
	int allocated[BLOCK_WIDTH];
	byte texels[BLOCK_WIDTH * BLOCK_HEIGHT];
	GLuint texnum;
	qboolean dirty;
} scrapblock_t;


scrapblock_t scrapBlocks[MAX_SCRAPS];
qboolean	scrap_check_dirty;

// returns a texture number and the position inside it
int Scrap_AllocBlock (int w, int h, int *x, int *y)
{
	int		i, j;
	int		best, best2;
	int		texnum;

	for (texnum = 0; texnum < MAX_SCRAPS; texnum++)
	{
		best = BLOCK_HEIGHT;

		for (i = 0; i < BLOCK_WIDTH - w; i++)
		{
			best2 = 0;

			for (j = 0; j < w; j++)
			{
				if (scrapBlocks[texnum].allocated[i + j] >= best)
					break;

				if (scrapBlocks[texnum].allocated[i + j] > best2)
					best2 = scrapBlocks[texnum].allocated[i + j];
			}

			if (j == w)
			{
				// this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > BLOCK_HEIGHT)
			continue;

		for (i = 0; i < w; i++)
			scrapBlocks[texnum].allocated[*x + i] = best + h;

		return texnum;
	}

	return -1;
	//	Sys_Error ("Scrap_AllocBlock: full");
}

int	scrap_uploads;


void Scrap_Init (void)
{
	int i;

	for (i = 0; i < MAX_SCRAPS; i++)
	{
		glDeleteTextures (1, &scrapBlocks[i].texnum);
		glGenTextures (1, &scrapBlocks[i].texnum);
		glTextureStorage2DEXT (scrapBlocks[i].texnum, GL_TEXTURE_2D, 1, GL_RGBA8, BLOCK_WIDTH, BLOCK_HEIGHT);

		// scrap is always dirty at the start
		scrapBlocks[i].dirty = true;
	}

	// scrap is always dirty at the start
	scrap_check_dirty = true;
}


void Scrap_Upload (void)
{
	int i;

	for (i = 0; i < MAX_SCRAPS; i++)
	{
		if (scrapBlocks[i].dirty)
		{
			GL_Image8To32 (scrapBlocks[i].texels, (unsigned *) Scratch_Alloc (), BLOCK_WIDTH * BLOCK_HEIGHT, d_8to24table_bgra);

			glTextureSubImage2DEXT (
				scrapBlocks[i].texnum,
				GL_TEXTURE_2D,
				0, 0, 0,
				BLOCK_WIDTH, BLOCK_HEIGHT,
				GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
				Scratch_Alloc ());

			scrapBlocks[i].dirty = false;
			scrap_uploads++;
		}
	}

	scrap_check_dirty = false;
}


void GL_CheckError (char *str)
{
	GLenum err = glGetError ();

	switch (err)
	{
	case GL_INVALID_ENUM: VID_Printf (PRINT_ALL, "GL_INVALID_ENUM"); break;
	case GL_INVALID_VALUE: VID_Printf (PRINT_ALL, "GL_INVALID_VALUE"); break;
	case GL_INVALID_OPERATION: VID_Printf (PRINT_ALL, "GL_INVALID_OPERATION"); break;
	case GL_STACK_OVERFLOW: VID_Printf (PRINT_ALL, "GL_STACK_OVERFLOW"); break;
	case GL_STACK_UNDERFLOW: VID_Printf (PRINT_ALL, "GL_STACK_UNDERFLOW"); break;
	case GL_OUT_OF_MEMORY: VID_Printf (PRINT_ALL, "GL_OUT_OF_MEMORY"); break;
	case GL_TABLE_TOO_LARGE: VID_Printf (PRINT_ALL, "GL_TABLE_TOO_LARGE"); break;
	default: return;
	}

	if (str)
		VID_Printf (PRINT_ALL, " with %s\n", str);
	else VID_Printf (PRINT_ALL, "\n");
}


GLuint GL_LoadCubeMap (cubeface_t *faces)
{
	int i;
	int size = 0;
	GLuint texnum = 0;
	qboolean allowStorage;

	glGenTextures (1, &texnum);

	// figure the size
	for (i = 0; i < 6; i++)
	{
		if (!faces[i].data) continue;
		if (!faces[i].width) continue;
		if (!faces[i].height) continue;

		if (faces[i].width > size) size = faces[i].width;
		if (faces[i].height > size) size = faces[i].height;
	}

	// select a default size
	if (!size) size = 1;

	// if this causes trouble on AMD we can revert it back to glTexImage (would really prefer not to though...)
	glGetError ();
	glTextureStorage2DEXT (texnum, GL_TEXTURE_CUBE_MAP, 1, GL_RGBA8, size, size);

	if (glGetError () != GL_NO_ERROR)
	{
		glDeleteTextures (1, &texnum);
		glGenTextures (1, &texnum);
		allowStorage = false;
	}
	else allowStorage = true;

	for (i = 0; i < 6; i++)
	{
		if (!faces[i].data) continue;

		if (faces[i].width != size || faces[i].height != size)
		{
			byte *newdata = (byte *) Img_Alloc (size * size * 4);

			GL_ResampleTexture ((unsigned *) faces[i].data, faces[i].width, faces[i].height, (unsigned *) newdata, size, size);
			faces[i].data = newdata;
		}

		if (allowStorage)
			glTextureSubImage2DEXT (texnum, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, 0, 0, size, size, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, faces[i].data);
		else glTextureImage2DEXT (texnum, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA8, size, size, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, faces[i].data);
	}

	// don't leak
	Img_Free ();

	return texnum;
}


/*
=================================================================

PCX LOADING

=================================================================
*/


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
		VID_Printf (PRINT_DEVELOPER, "Bad pcx file %s\n", filename);
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
		VID_Printf (PRINT_ALL, "Bad pcx file %s\n", filename);
		return;
	}

	out = Img_Alloc ((pcx->ymax + 1) * (pcx->xmax + 1));
	*pic = out;
	pix = out;

	if (palette)
	{
		*palette = Img_Alloc (768);
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
		VID_Printf (PRINT_DEVELOPER, "PCX file %s was malformed", filename);
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
		VID_Printf (PRINT_DEVELOPER, "Bad tga file\n");
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
		VID_Error (ERR_DROP, "LoadTGA: Only type 2 and 10 targa RGB images supported\n");

	if (targa_header.colormap_type != 0 || (targa_header.pixel_size != 32 && targa_header.pixel_size != 24))
		VID_Error (ERR_DROP, "LoadTGA: Only 32 or 24 bit images supported (no colormaps)\n");

	columns = targa_header.width;
	rows = targa_header.height;
	numPixels = columns * rows;

	if (width) *width = columns;
	if (height) *height = rows;

	targa_rgba = Img_Alloc (numPixels * 4);
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


void LoadTGAResource (int ResourceID, byte **pic, int *width, int *height)
{
	int length;
	byte *buffer;

	length = Sys_LoadResourceData (ResourceID, (void **) &buffer);

	LoadTGACommon (length, buffer, pic, width, height);
}


void LoadTGAFile (char *name, byte **pic, int *width, int *height)
{
	int length;
	byte *buffer;

	length = FS_LoadFile (name, (void **) &buffer);

	LoadTGACommon (length, buffer, pic, width, height);

	FS_FreeFile (buffer);
}


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
	floodfill_t			fifo[FLOODFILL_FIFO_SIZE];
	int					inpt = 0, outpt = 0;
	int					filledcolor = -1;
	int					i;

	if (filledcolor == -1)
	{
		filledcolor = 0;

		// attempt to find opaque black
		for (i = 0; i < 256; ++i)
		{
			if (d_8to24table_rgba[i] == (255 << 0)) // alpha 1.0
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

//=======================================================


int AverageMip (int _1, int _2, int _3, int _4)
{
	return (_1 + _2 + _3 + _4) >> 2;
}


int AverageMipGC (int _1, int _2, int _3, int _4)
{
	float a = (float) _1 / 255.0f;
	float b = (float) _2 / 255.0f;
	float c = (float) _3 / 255.0f;
	float d = (float) _4 / 255.0f;

	return (int) (255.0f * sqrt (((a * a) + (b * b) + (c * c) + (d * d)) / 4.0f));
}


/*
================
GL_ResampleTexture
================
*/
void GL_ResampleTexture (unsigned *in, int inwidth, int inheight, unsigned *out, int outwidth, int outheight)
{
	int		i, j;
	unsigned	*inrow, *inrow2;
	unsigned	frac, fracstep;
	unsigned	*p1, *p2;
	byte		*pix1, *pix2, *pix3, *pix4;

	if (outwidth < 1) outwidth = 1;
	if (outheight < 1) outheight = 1;

	p1 = (unsigned *) Img_Alloc (outwidth * 4);
	p2 = (unsigned *) Img_Alloc (outwidth * 4);

	fracstep = inwidth * 0x10000 / outwidth;
	frac = fracstep >> 2;

	for (i = 0; i < outwidth; i++)
	{
		p1[i] = 4 * (frac >> 16);
		frac += fracstep;
	}

	frac = 3 * (fracstep >> 2);

	for (i = 0; i < outwidth; i++)
	{
		p2[i] = 4 * (frac >> 16);
		frac += fracstep;
	}

	for (i = 0; i < outheight; i++, out += outwidth)
	{
		inrow = in + inwidth * (int) ((i + 0.25) * inheight / outheight);
		inrow2 = in + inwidth * (int) ((i + 0.75) * inheight / outheight);
		frac = fracstep >> 1;

		for (j = 0; j < outwidth; j++)
		{
			pix1 = (byte *) inrow + p1[j];
			pix2 = (byte *) inrow + p2[j];
			pix3 = (byte *) inrow2 + p1[j];
			pix4 = (byte *) inrow2 + p2[j];

			((byte *) (out + j))[0] = AverageMipGC (pix1[0], pix2[0], pix3[0], pix4[0]);
			((byte *) (out + j))[1] = AverageMipGC (pix1[1], pix2[1], pix3[1], pix4[1]);
			((byte *) (out + j))[2] = AverageMipGC (pix1[2], pix2[2], pix3[2], pix4[2]);
			((byte *) (out + j))[3] = AverageMipGC (pix1[3], pix2[3], pix3[3], pix4[3]);
		}
	}
}


/*
================
GL_MipMap

Operates in place, quartering the size of the texture
================
*/
void GL_MipMap (byte *in, int width, int height)
{
	int		i, j;
	byte	*out;

	width <<= 2;
	height >>= 1;
	out = in;

	for (i = 0; i < height; i++, in += width)
	{
		for (j = 0; j < width; j += 8, out += 4, in += 8)
		{
			out[0] = AverageMipGC (in[0], in[4], in[width, 0], in[width, 4]);
			out[1] = AverageMipGC (in[1], in[5], in[width, 1], in[width, 5]);
			out[2] = AverageMipGC (in[2], in[6], in[width, 2], in[width, 6]);
			out[3] = AverageMipGC (in[3], in[7], in[width, 3], in[width, 7]);
		}
	}
}


int	upload_width, upload_height;
qboolean uploaded_paletted;

static float Q_log2 (float i)
{
	return log10 (i) / log10 (2.0f);
}


/*
===============
GL_UploadTexture

===============
*/

GLuint GL_UploadTexture (byte *data, int width, int height, qboolean mipmap, int bits)
{
	int i;
	int nummips = mipmap ? (floor (Q_log2((float) max (width, height))) + 1) : 1;
	GLuint texnum = 0;
	unsigned *trans = NULL;

	if (bits == 8)
	{
		trans = (unsigned *) Img_Alloc (width * height * 4);
		GL_Image8To32 (data, trans, width * height, d_8to24table_bgra);
	}
	else trans = (unsigned *) data;

	// it's assumed that our hardware can handle Q2 texture sizes
	upload_width = width;
	upload_height = height;

	glGenTextures (1, &texnum);
	glTextureStorage2DEXT (texnum, GL_TEXTURE_2D, nummips, GL_RGBA8, width, height);
	glTextureSubImage2DEXT (texnum, GL_TEXTURE_2D, 0, 0, 0, width, height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, trans);

	for (i = 1; i < nummips; i++)
	{
		// do do - alternately resample if non-divisible-by-2
		if ((width & 1) || (height & 1))
		{
			unsigned *mipdata = (unsigned *) Img_Alloc (((width + 1) & ~1) * ((height + 1) & ~1));
			GL_ResampleTexture (trans, width, height, mipdata, width >> 1, height >> 1);
			trans = mipdata;
		}
		else GL_MipMap ((byte *) trans, width, height);

		if ((width = (width >> 1)) < 1) width = 1;
		if ((height = (height >> 1)) < 1) height = 1;

		glTextureSubImage2DEXT (texnum, GL_TEXTURE_2D, i, 0, 0, width, height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, trans);
	}

	// throw away all memory used for loading
	Img_Free ();

	return texnum;
}


/*
================
GL_LoadPic

This is also used as an entry point for the generated r_notexture
================
*/
image_t *GL_LoadPic (char *name, byte *pic, int width, int height, imagetype_t type, int bits)
{
	image_t		*image;
	int			i;

	// find a free image_t
	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		if (!image->texnum)
			break;
	}

	if (i == numgltextures)
	{
		if (numgltextures == MAX_GLTEXTURES)
			VID_Error (ERR_DROP, "MAX_GLTEXTURES");

		numgltextures++;
	}

	image = &gltextures[i];

	if (strlen (name) >= sizeof (image->name))
		VID_Error (ERR_DROP, "Draw_LoadPic: \"%s\" is too long", name);

	strcpy (image->name, name);
	image->registration_sequence = registration_sequence;

	image->width = width;
	image->height = height;
	image->type = type;

	if (type == it_skin && bits == 8)
		R_FloodFillSkin (pic, width, height);

	// load little pics into the scrap
	if (image->type == it_pic && bits == 8 && image->width < 128 && image->height < 128)
	{
		int		x, y;
		int		i, j, k;
		int		texnum;

		// offset the allocation by 1 in each direction so that bilerp filtering doesn't cause adjacent scrap images to bleed into each other
		texnum = Scrap_AllocBlock (image->width + 2, image->height + 2, &x, &y);

		if (texnum == -1) goto nonscrap;
		if (texnum >= MAX_SCRAPS) goto nonscrap;

		scrapBlocks[texnum].dirty = true;
		scrap_check_dirty = true;

		// copy the texels into the scrap block
		k = 0; x++; y++;

		for (i = 0; i < image->height; i++)
			for (j = 0; j < image->width; j++, k++)
				scrapBlocks[texnum].texels[(y + i) * BLOCK_WIDTH + x + j] = pic[k];

		image->texnum = scrapBlocks[texnum].texnum;
		image->scrap = true;
		image->mipmap = false;

		image->sl = (float) x / (float) BLOCK_WIDTH;
		image->sh = (float) (x + image->width) / (float) BLOCK_WIDTH;
		image->tl = (float) y / (float) BLOCK_HEIGHT;
		image->th = (float) (y + image->height) / (float) BLOCK_HEIGHT;
	}
	else
	{
nonscrap:
		image->scrap = false;
		image->mipmap = (image->type != it_pic && image->type != it_sky);

		image->texnum = GL_UploadTexture (pic, width, height, image->mipmap, bits);

		image->upload_width = upload_width;		// after power of 2 and scales
		image->upload_height = upload_height;

		image->sl = 0;
		image->sh = 1;
		image->tl = 0;
		image->th = 1;
	}

	return image;
}


/*
================
GL_LoadWal
================
*/
image_t *GL_LoadWal (char *name)
{
	miptex_t	*mt;
	int			width, height, ofs;
	image_t		*image;

	FS_LoadFile (name, (void **) &mt);

	if (!mt)
	{
		VID_Printf (PRINT_ALL, "GL_FindImage: can't load %s\n", name);
		return r_notexture;
	}

	width = LittleLong (mt->width);
	height = LittleLong (mt->height);
	ofs = LittleLong (mt->offsets[0]);

	image = GL_LoadPic (name, (byte *) mt + ofs, width, height, it_wall, 8);

	FS_FreeFile ((void *) mt);

	return image;
}

/*
===============
GL_FindImage

Finds or loads the given image
===============
*/
image_t	*GL_FindImage (char *name, imagetype_t type)
{
	image_t	*image;
	int		i, len;
	byte	*pic, *palette;
	int		width, height;

	if (!name)
		return NULL;	//	VID_Error (ERR_DROP, "GL_FindImage: NULL name");

	len = strlen (name);

	if (len < 5)
		return NULL;	//	VID_Error (ERR_DROP, "GL_FindImage: bad name: %s", name);

	// look for it
	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		if (!strcmp (name, image->name))
		{
			image->registration_sequence = registration_sequence;
			return image;
		}
	}

	// this is a hack to force drawing to flush if a new texture needs to be loaded, thanks to OpenGL bind-to-modify insanity
	Draw_End2D ();

	// load the pic from disk
	pic = NULL;
	palette = NULL;

	if (!strcmp (name + len - 4, ".pcx"))
	{
		LoadPCX (name, &pic, &palette, &width, &height);

		if (!pic)
			return NULL; // VID_Error (ERR_DROP, "GL_FindImage: can't load %s", name);

		image = GL_LoadPic (name, pic, width, height, type, 8);
	}
	else if (!strcmp (name + len - 4, ".wal"))
	{
		image = GL_LoadWal (name);
	}
	else if (!strcmp (name + len - 4, ".tga"))
	{
		LoadTGAFile (name, &pic, &width, &height);

		if (!pic)
			return NULL; // VID_Error (ERR_DROP, "GL_FindImage: can't load %s", name);

		image = GL_LoadPic (name, pic, width, height, type, 32);
	}
	else return NULL;	//	VID_Error (ERR_DROP, "GL_FindImage: bad extension on: %s", name);

	Img_Free ();

	return image;
}



/*
===============
R_RegisterSkin
===============
*/
struct image_s *R_RegisterSkin (char *name)
{
	return GL_FindImage (name, it_skin);
}


/*
================
GL_FreeUnusedImages

Any image that was not touched on this registration sequence
will be freed.
================
*/
void GL_FreeUnusedImages (void)
{
	int		i;
	image_t	*image;

	// never free r_notexture or particle texture
	r_notexture->registration_sequence = registration_sequence;

	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		if (image->registration_sequence == registration_sequence) continue;		// used this sequence
		if (!image->registration_sequence) continue;		// free image_t slot
		if (image->type == it_pic) continue;		// don't free pics

		// free it
		glDeleteTextures (1, &image->texnum);
		memset (image, 0, sizeof (*image));
	}
}


/*
===============
Draw_GetPalette
===============
*/
int Draw_GetPalette (void)
{
	int		i;
	int		r, g, b;
	unsigned	v;
	byte	*pic, *pal;
	int		width, height;

	// get the palette
	LoadPCX ("pics/colormap.pcx", &pic, &pal, &width, &height);

	if (!pal)
		VID_Error (ERR_FATAL, "Couldn't load pics/colormap.pcx");

	for (i = 0; i < 256; i++)
	{
		r = pal[i * 3 + 0];
		g = pal[i * 3 + 1];
		b = pal[i * 3 + 2];

		v = (255 << 24) + (r << 0) + (g << 8) + (b << 16);
		d_8to24table_rgba[i] = LittleLong (v);

		v = (255 << 24) + (r << 16) + (g << 8) + (b << 0);
		d_8to24table_bgra[i] = LittleLong (v);
	}

	// clear the alpha pixel to black to get some kind of fade off
	d_8to24table_rgba[255] = 0;
	d_8to24table_bgra[255] = 0;

	Img_Free ();

	return 0;
}


void R_LoadResourceTextures (void)
{
}


/*
===============
GL_InitImages
===============
*/
void GL_InitImages (void)
{
	int		i, j;
	float	g = vid_gamma->value;

	R_LoadResourceTextures ();

	Scrap_Init ();

	registration_sequence = 1;
	Draw_GetPalette ();

	for (i = 0; i < 256; i++)
	{
		if (g == 1)
		{
			gammatable[i] = i;
		}
		else
		{
			float inf;

			inf = 255 * pow ((i + 0.5) / 255.5, g) + 0.5;

			if (inf < 0)
				inf = 0;

			if (inf > 255)
				inf = 255;

			gammatable[i] = inf;
		}
	}
}

/*
===============
GL_ShutdownImages
===============
*/
void GL_ShutdownImages (void)
{
	int		i;
	image_t	*image;

	for (i = 0, image = gltextures; i < numgltextures; i++, image++)
	{
		if (!image->registration_sequence)
			continue;		// free image_t slot

		// free it
		glDeleteTextures (1, &image->texnum);
		memset (image, 0, sizeof (*image));
	}
}

