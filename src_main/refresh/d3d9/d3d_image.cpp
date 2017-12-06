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

PALETTEENTRY d_8to24table[256];

std::vector<image_t *> gl_textures;

int scratch_base = 0;

extern "C" void *Scratch_Alloc(void);
void *Scratch_Alloc(int size)
{
	byte *data = (byte *)Scratch_Alloc();

	data += scratch_base;
	scratch_base += size;

	return data;
}

void Scratch_Free(void)
{
	scratch_base = 0;
}

int Scratch_LowMark(void)
{
	return scratch_base;
}

void Scratch_FreeToLowMark(int mark)
{
	scratch_base = mark;
}

/*
===============
GL_ImageList_f
===============
*/
void GL_ImageList_f (void)
{
	int texels;

	Com_Printf("------------------\n");
	texels = 0;

	for (int i = 0; i < gl_textures.size (); i++)
	{
		D3DSURFACE_DESC desc;

		if (!gl_textures[i]->d3d_Texture) continue;
		if (FAILED (gl_textures[i]->d3d_Texture->GetLevelDesc (0, &desc))) continue;

		texels += desc.Width * desc.Height;

		switch (gl_textures[i]->type)
		{
		case it_skin:	Com_Printf("it_skin  "); break;
		case it_sprite:	Com_Printf("it_sprite"); break;
		case it_wall:	Com_Printf("it_wall  "); break;
		case it_pic:	Com_Printf("it_pic   "); break;
		case it_sky:	Com_Printf("it_sky   "); break;
		default:		Com_Printf("         "); break;
		}

		Com_Printf(" %3i %3i: %s\n", desc.Width, desc.Height, gl_textures[i]->name);
	}

	Com_Printf("Total texel count (not counting mipmaps): %i\n", texels);
}


qboolean D3D_FillTexture (IDirect3DTexture9 *tex, void *data, int width, int height, qboolean mipmap, PALETTEENTRY *palette)
{
	IDirect3DSurface9 *surf = NULL;

	if (SUCCEEDED (tex->GetSurfaceLevel (0, &surf)))
	{
		RECT surfrect = {0, 0, width, height};
		D3DSURFACE_DESC desc;

		if (SUCCEEDED (surf->GetDesc (&desc)))
		{
			DWORD filter = (desc.Width == width && desc.Height == height) ? D3DX_FILTER_NONE : D3DX_FILTER_LINEAR;

			if (!palette)
				D3DXLoadSurfaceFromMemory (surf, NULL, NULL, data, D3DFMT_A8R8G8B8, width << 2, NULL, &surfrect, filter, 0);
			else
				D3DXLoadSurfaceFromMemory (surf, NULL, NULL, data, D3DFMT_P8, width, palette, &surfrect, filter, 0);

			surf->Release ();

			// only fill the miplevels if we successfully filled the main level
			if (mipmap) D3DXFilterTexture (tex, NULL, 0, D3DX_FILTER_LINEAR);

			return true;
		}

		surf->Release ();
	}

	return false;
}


IDirect3DTexture9 *GL_UploadTexture (byte *data, int width, int height, qboolean mipmap, int bits)
{
	IDirect3DTexture9 *tex = NULL;
	texdimension_t scaled = {width, height};

	D3D_GetScaledTextureDimensions (width, height, mipmap, &scaled);

	if (SUCCEEDED (d3d_Device->CreateTexture (scaled.Width, scaled.Height, mipmap ? 0 : 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, NULL)))
	{
		if (bits == 32)
			D3D_FillTexture (tex, data, width, height, mipmap, NULL);
		else
			D3D_FillTexture (tex, data, width, height, mipmap, d_8to24table);
	}

	// throw away all memory used for loading
	Scratch_Free ();

	tex->PreLoad ();

	return tex;
}


/*
================
GL_LoadPic

This is also used as an entry point for the generated r_notexture
================
*/
image_t *GL_LoadPic (char *name, byte *pic, int width, int height, imagetype_t type, int bits)
{
	image_t *image = NULL;

	// find a free image_t
	for (int i = 0; i < gl_textures.size (); i++)
	{
		if (!gl_textures[i]->d3d_Texture)
		{
			image = gl_textures[i];
			break;
		}
	}

	if (!image)
	{
		// didn't get one, alloc a new
		image_t *newImage = new image_t;

		memset (newImage, 0, sizeof (newImage));
		newImage->texturechain_tail = &newImage->texturechain;
		gl_textures.push_back (newImage);

		image = gl_textures[gl_textures.size () - 1];
	}

	if (strlen (name) >= sizeof (image->name))
		Com_Error (ERR_DROP, "Draw_LoadPic: \"%s\" is too long", name);

	strcpy (image->name, name);
	image->registration_sequence = registration_sequence;

	image->width = width;
	image->height = height;
	image->type = type;

	if (type == it_skin && bits == 8)
		R_FloodFillSkin (pic, width, height);

	// load little pics into the scrap (reverted to 64 because the backtile needs to tile)
	// we allow 32bpp images into the scrap as well now
	if (image->type == it_pic && image->width < 64 && image->height < 64)
	{
		ImageScrap *Scrap = NULL;
		int x, y;

		// pad the texels to prevent the images from blierp-bleeding into each other
		if ((Scrap = ImageScrap::Scrap_AllocBlock (image->width + 2, image->height + 2, &x, &y)) != NULL)
		{
			Scrap->LoadTexels (x + 1, y + 1, image, pic, bits);
			return image;
		}
	}

	image->scrap = false;
	image->mipmap = (image->type != it_pic && image->type != it_sky);

	image->d3d_Texture = GL_UploadTexture (pic, width, height, image->mipmap, bits);

	image->sl = 0;
	image->sh = 1;
	image->tl = 0;
	image->th = 1;

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
		Com_Printf ("D3D9_FindImage: can't load %s\n", name);
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
D3D9_FindImage

Finds or loads the given image
===============
*/
image_t	*D3D9_FindImage (char *name, imagetype_t type)
{
	image_t	*image;
	int		len;
	byte	*pic, *palette;
	int		width, height;

	if (!name)
		return NULL;	//	VID_Error (ERR_DROP, "D3D9_FindImage: NULL name");

	len = strlen (name);

	if (len < 5)
		return NULL;	//	VID_Error (ERR_DROP, "D3D9_FindImage: bad name: %s", name);

	// look for it
	for (int i = 0; i < gl_textures.size (); i++)
	{
		if (!strcmp (name, gl_textures[i]->name))
		{
			gl_textures[i]->registration_sequence = registration_sequence;
			return gl_textures[i];
		}
	}

	// load the pic from disk
	pic = NULL;
	palette = NULL;

	if (!strcmp (name + len - 4, ".pcx"))
	{
		LoadPCX (name, &pic, &palette, &width, &height);

		if (!pic)
			return NULL; // VID_Error (ERR_DROP, "D3D9_FindImage: can't load %s", name);

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
			return NULL; // VID_Error (ERR_DROP, "D3D9_FindImage: can't load %s", name);

		image = GL_LoadPic (name, pic, width, height, type, 32);
	}
	else return NULL;	//	VID_Error (ERR_DROP, "D3D9_FindImage: bad extension on: %s", name);

	Scratch_Free ();

	return image;
}



/*
===============
D3D9_RegisterSkin
===============
*/
struct image_s *D3D9_RegisterSkin (char *name)
{
	return D3D9_FindImage (name, it_skin);
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
	// never free r_notexture or particle texture
	r_notexture->registration_sequence = registration_sequence;
	r_greytexture->registration_sequence = registration_sequence;
	r_whitetexture->registration_sequence = registration_sequence;
	r_blacktexture->registration_sequence = registration_sequence;

	for (int i = 0, startpos = 0; i < gl_textures.size (); i++)
	{
		if (gl_textures[i]->registration_sequence == registration_sequence) continue;		// used this sequence
		if (!gl_textures[i]->registration_sequence) continue;		// free image_t slot
		if (gl_textures[i]->type == it_pic) continue;		// don't free pics

		// free it
		SAFE_RELEASE (gl_textures[i]->d3d_Texture);
		memset (gl_textures[i], 0, sizeof (image_t));
		gl_textures[i]->texturechain_tail = &gl_textures[i]->texturechain;

		// put all free textures at the start of the list so that we can more quickly locate and reuse them
		std::swap (gl_textures[i], gl_textures[startpos]);
		startpos++;
	}
}


/*
===============
Draw_GetPalette
===============
*/
int Draw_GetPalette (void)
{
	byte *pic, *pal;
	int width, height;

	// get the palette
	LoadPCX ("pics/colormap.pcx", &pic, &pal, &width, &height);

	if (!pal)
	{
		// shut up code analysis
		Com_Error (ERR_FATAL, "Couldn't load pics/colormap.pcx");
		return 0;
	}

	// index 255 is alpha
	for (int i = 0; i < 255; i++, pal += 3)
	{
		d_8to24table[i].peRed = pal[0];
		d_8to24table[i].peGreen = pal[1];
		d_8to24table[i].peBlue = pal[2];
		d_8to24table[i].peFlags = 255;
	}

	// clear the alpha pixel to black to get some kind of fade off
	d_8to24table[255].peRed = 0;
	d_8to24table[255].peGreen = 0;
	d_8to24table[255].peBlue = 0;
	d_8to24table[255].peFlags = 0;

	Scratch_Free ();

	return 0;
}


/*
===============
GL_InitImages
===============
*/
void GL_InitImages (void)
{
	registration_sequence++;
	Draw_GetPalette ();
	ImageScrap::Init ();
}


/*
===============
GL_ShutdownImages
===============
*/
void GL_ShutdownImages (void)
{
	for (int i = 0; i < gl_textures.size (); i++)
	{
		if (!gl_textures[i]) continue;

		SAFE_RELEASE (gl_textures[i]->d3d_Texture);
		memset (gl_textures[i], 0, sizeof (image_t));
		gl_textures[i]->texturechain_tail = &gl_textures[i]->texturechain;

		delete gl_textures[i];
		gl_textures[i] = NULL;
	}

	gl_textures.clear ();
	ImageScrap::Shutdown ();
}

