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

#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_NO_BMP
#define STBI_NO_PSD
#define STBI_NO_GIF
#define STBI_NO_PIC
#define STBI_NO_PNM

// make sure STB_image uses standard malloc(), as we'll use standard free() to deallocate
#define STBI_MALLOC(sz)    malloc(sz)
#define STBI_REALLOC(p,sz) realloc(p,sz)
#define STBI_FREE(p)       free(p)

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

unsigned	d_8to24table_rgba[256];
unsigned	d_8to24table_bgra[256];

int img_base = 0;

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

void GL_ResampleTexture (unsigned *in, int inwidth, int inheight, unsigned *out, int outwidth, int outheight);

/*
=============
GL_Image8To32
=============
*/
static void GL_Image8To32 (byte *data8, unsigned *data32, int size, unsigned *palette)
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

/*
=============
GL_SwapBlueRed
=============
*/
static void GL_SwapBlueRed (byte *data, int width, int height, int samples)
{
	int i, j, size;

	size = width * height;
	for (i = 0; i < size; i++, data += samples)
	{
		j = data[0];
		data[0] = data[2];
		data[2] = j;
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
GLuint r_drawnearestclampsampler = 0;
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

	glGenSamplers (1, &r_drawnearestclampsampler);
	RImage_SetSampler (r_drawnearestclampsampler, GL_NEAREST, GL_CLAMP_TO_EDGE);

	glGenSamplers (1, &r_skysampler);
	RImage_SetSampler (r_skysampler, GL_LINEAR, GL_CLAMP_TO_EDGE);

	glGenSamplers (1, &r_surfacesampler);
	RImage_UpdateSampler (r_surfacesampler, GL_REPEAT);

	glGenSamplers (1, &r_modelsampler);
	RImage_UpdateSampler (r_modelsampler, GL_CLAMP_TO_EDGE);
}


image_t		gltextures[MAX_GLTEXTURES];
int			numgltextures;

int			gl_filter_min = GL_LINEAR_MIPMAP_LINEAR;
int			gl_filter_max = GL_LINEAR;

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
		VID_Printf (PRINT_ALL, S_COLOR_RED "bad filter name\n");
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	gl_texture_anisotropy = 1;

	if (GLEW_EXT_texture_filter_anisotropic & (anisotropy > 1))
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
	GL_CheckError( __func__ );
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
		else
			glTextureImage2DEXT (texnum, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA8, size, size, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, faces[i].data);
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
static void LoadPCX (char *filename, byte **pic, byte **palette, int *width, int *height)
{
	byte	*raw;
	pcx_t	*pcx;
	int		x, y;
	int		len, full_size;
	int		pcx_width, pcx_height;
	qboolean image_issues = false;
	int		dataByte, runLength;
	byte	*out, *pix;

	*pic = NULL;
	*palette = NULL;

	// load the file
	len = FS_LoadFile (filename, (void **) &raw);
	if (!raw || len < sizeof(pcx_t))
	{
		VID_Printf (PRINT_DEVELOPER, S_COLOR_RED "Bad pcx file %s\n", filename);
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

	pcx_width = pcx->xmax - pcx->xmin;
	pcx_height = pcx->ymax - pcx->ymin;

	if (pcx->manufacturer != 0x0a ||
		pcx->version != 5 ||
		pcx->encoding != 1 ||
		pcx->bits_per_pixel != 8 ||
		(pcx_width >= 4096) || (pcx_height >= 4096))
	{
		VID_Printf (PRINT_ALL, S_COLOR_RED "Bad pcx file %s\n", filename);
		return;
	}

	full_size = (pcx_height + 1) * (pcx_width + 1);
	out = Img_Alloc (full_size);

	*pic = out;
	pix = out;

	if (palette)
	{
		*palette = Img_Alloc (768);
		if (len > 768)
			memcpy (*palette, (byte *) pcx + len - 768, 768);
		else
			image_issues = true;
	}

	if (width) *width = pcx_width + 1;
	if (height) *height = pcx_height + 1;

	for (y = 0; y <= pcx_height; y++, pix += pcx_width + 1)
	{
		for (x = 0; x <= pcx_width;)
		{
			if (raw - (byte *)pcx > len)
			{
				// no place for read
				image_issues = true;
				x = pcx_width;
				break;
			}
			dataByte = *raw++;

			if ((dataByte & 0xC0) == 0xC0)
			{
				runLength = dataByte & 0x3F;
				if (raw - (byte *)pcx > len)
				{
					// no place for read
					image_issues = true;
					x = pcx_width;
					break;
				}
				dataByte = *raw++;
			}
			else runLength = 1;

			while (runLength-- > 0)
			{
				if ((*pic + full_size) <= (pix + x))
				{
					// no place for write
					image_issues = true;
					x += runLength;
					runLength = 0;
				}
				else
				{
					pix[x++] = dataByte;
				}
			}
		}
	}

	if (raw - (byte *) pcx > len)
	{
		VID_Printf (PRINT_DEVELOPER, S_COLOR_RED "PCX file %s was malformed.\n", filename);
		*pic = NULL;
	}

	if (image_issues)
		VID_Printf (PRINT_ALL, S_COLOR_YELLOW "PCX file %s has possible size issues.\n", filename);

	FS_FreeFile (pcx);
}

/*
==============
GetPCXInfo
==============
*/
static void GetPCXInfo (char *filename, int *width, int *height)
{
	pcx_t *pcx;
	byte *raw;

	FS_LoadFile (filename, (void **)&raw);
	if (!raw)
		return;

	pcx = (pcx_t *)raw;

	*width = pcx->xmax + 1;
	*height = pcx->ymax + 1;

	FS_FreeFile (raw);

	return;
}


/*
=========================================================

WAL LOADING

=========================================================
*/

/*
================
GL_LoadWal
================
*/
static image_t *GL_LoadWal(char *name)
{
	miptex_t	*mt;
	int			width, height, ofs;
	image_t		*image;

	FS_LoadFile(name, (void **)&mt);
	if (!mt)
	{
		VID_Printf(PRINT_ALL, S_COLOR_RED "GL_FindImage: can't load %s\n", name);
		return r_notexture;
	}

	width = LittleLong(mt->width);
	height = LittleLong(mt->height);
	ofs = LittleLong(mt->offsets[0]);

	image = GL_LoadPic(name, (byte *)mt + ofs, width, height, it_wall, 8);

	FS_FreeFile((void *)mt);

	return image;
}

/*
================
GetWalInfo
================
*/
static void GetWalInfo(char *name, int *width, int *height)
{
	miptex_t *mt;

	FS_LoadFile(name, (void **)&mt);
	if (!mt)
		return;

	*width = LittleLong(mt->width);
	*height = LittleLong(mt->height);

	FS_FreeFile((void *)mt);

	return;
}


/*
=========================================================

GENERIC RGBA PICTURE LOADING

=========================================================
*/

/*
=============
LoadImageThruSTB
=============
*/
qboolean LoadImageThruSTB (char *origname, char* type, byte **pic, int *width, int *height)
{
	char filename[256];

	Q_strlcpy (filename, origname, sizeof(filename));

	// add the extension
	if (strcmp(COM_FileExtension(filename), type) != 0)
	{
		Q_strlcat (filename, ".", sizeof(filename));
		Q_strlcat (filename, type, sizeof(filename));
	}

	*pic = NULL;
	*width = *height = 0;

	byte* rawdata = NULL;
	int rawsize = FS_LoadFile (filename, (void **)&rawdata);
	if (rawdata == NULL)
		return false;

	// load file into memory
	int w, h, bytesPerPixel;
	byte* data = NULL;
	data = stbi_load_from_memory (rawdata, rawsize, &w, &h, &bytesPerPixel, STBI_rgb_alpha);
	if (data == NULL)
	{
		VID_Printf (PRINT_ALL, S_COLOR_RED "stb_image couldn't load data from %s: %s!\n", filename, stbi_failure_reason());
		FS_FreeFile (rawdata);
		return false;
	}
	
	FS_FreeFile (rawdata);

	*pic = data;
	*width = w;
	*height = h;

	return true;
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
	else
	{
		trans = (unsigned *) data;
		GL_SwapBlueRed (data, width, height, 4);
	}

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
		else
			GL_MipMap ((byte *) trans, width, height);

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

	image->mipmap = (image->type != it_pic && image->type != it_sky);

	image->texnum = GL_UploadTexture (pic, width, height, image->mipmap, bits);

	image->upload_width = upload_width;		// after power of 2 and scales
	image->upload_height = upload_height;

	image->sl = 0;
	image->sh = 1;
	image->tl = 0;
	image->th = 1;

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
	char	*ext, *ptr;
	char	namewe[256];
	int		realwidth = 0, realheight = 0;

	if (!name)
		return NULL;

	ext = COM_FileExtension(name);
	if (!ext[0])
	{
		// file has no extension
		return NULL;
	}

	// remove the extension
	len = strlen (name);
	if (len < 5)
		return NULL;
	memset(namewe, 0, 256);
	memcpy(namewe, name, len - 4);

	// fix backslashes
	while ((ptr = strchr(name, '\\')))
		*ptr = '/';

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

	if (strcmp(ext, "pcx") == 0)
	{
		GetPCXInfo (name, &realwidth, &realheight);
		if (realwidth == 0)
		{
			// no texture found
			return NULL;
		}

		// check for retexture
		if (LoadImageThruSTB(namewe, "tga", &pic, &width, &height)
			|| LoadImageThruSTB(namewe, "png", &pic, &width, &height)
			|| LoadImageThruSTB(namewe, "jpg", &pic, &width, &height))
		{
			// upload tga or png or jpg
			image = GL_LoadPic (name, pic, realwidth, realheight, type, 32);
		}
		else
		{
			// upload PCX
			LoadPCX (name, &pic, &palette, &width, &height);
			if (!pic)
				return NULL;

			image = GL_LoadPic (name, pic, width, height, type, 8);
		}
	}
	else if (strcmp(ext, "wal") == 0)
	{
		GetWalInfo (name, &realwidth, &realheight);
		if (realwidth == 0)
		{
			// no texture found
			return NULL;
		}

		// check for retexture
		if (LoadImageThruSTB(namewe, "tga", &pic, &width, &height)
			|| LoadImageThruSTB(namewe, "png", &pic, &width, &height)
			|| LoadImageThruSTB(namewe, "jpg", &pic, &width, &height))
		{
			// upload tga or png or jpg
			image = GL_LoadPic (name, pic, realwidth, realheight, type, 32);
		}
		else
		{
			// upload wal
			image = GL_LoadWal (name);
		}

		if (!image)
			return NULL;
	}
	else if (strcmp(ext, "tga") == 0 || strcmp(ext, "png") == 0 || strcmp(ext, "jpg") == 0)
	{
		// load tga, png, or jpg
		LoadImageThruSTB (name, ext, &pic, &width, &height);
		if (!pic)
			return NULL;

		image = GL_LoadPic (name, pic, width, height, type, 32);
	}
	else
	{
		return NULL;
	}

	Img_Free ();

	return image;
}



/*
===============
RE_RegisterSkin
===============
*/
struct image_s *RE_GL_RegisterSkin (char *name)
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

/*
===============
GL_InitImages
===============
*/
void GL_InitImages (void)
{
	registration_sequence = 1;
	Draw_GetPalette ();
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

