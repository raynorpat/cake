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

// image scrap, not images crap

#include "d3d_local.h"

std::vector<ImageScrap *> Scraps;


ImageScrap::ImageScrap (void)
{
	d3d_Device->CreateTexture (
		SCRAP_SIZE,
		SCRAP_SIZE,
		1,
		0,
		D3DFMT_A8R8G8B8,
		D3DPOOL_MANAGED,
		&this->Texture,
		NULL
	);

	memset (this->Allocated, 0, sizeof (this->Allocated));
}


ImageScrap::~ImageScrap (void)
{
	if (this->Texture)
	{
		this->Texture->Release ();
		this->Texture = NULL;
	}

	memset (this->Allocated, 0, sizeof (this->Allocated));
}


bool ImageScrap::AllocBlock (int w, int h, int *x, int *y)
{
	int best = SCRAP_SIZE;

	for (int i = 0; i < SCRAP_SIZE - w; i++)
	{
		int best2 = 0;
		int j = 0;

		for (j = 0; j < w; j++)
		{
			if (this->Allocated[i + j] >= best)
				break;

			if (this->Allocated[i + j] > best2)
				best2 = this->Allocated[i + j];
		}

		if (j == w)
		{
			// this is a valid spot
			*x = i;
			*y = best = best2;
		}
	}

	if (best + h > SCRAP_SIZE)
		return false;

	for (int i = 0; i < w; i++)
		this->Allocated[*x + i] = best + h;

	return true;
}


void ImageScrap::LoadTexels (int x, int y, image_t *image, byte *data, int bits)
{
	IDirect3DSurface9 *texsurf = NULL;

	if (SUCCEEDED (this->Texture->GetSurfaceLevel (0, &texsurf)))
	{
		RECT srcrect = {0, 0, image->width, image->height};
		RECT dstrect = {x, y, x + image->width, y + image->height};

		if (bits == 8)
			D3DXLoadSurfaceFromMemory (texsurf, NULL, &dstrect, data, D3DFMT_P8, image->width, d_8to24table, &srcrect, D3DX_FILTER_NONE, 0);
		else D3DXLoadSurfaceFromMemory (texsurf, NULL, &dstrect, data, D3DFMT_A8R8G8B8, image->width << 2, NULL, &srcrect, D3DX_FILTER_NONE, 0);

		texsurf->Release ();
	}

	// set the image; because this will be released in both Scrap_Shutdown
	// and GL_ShutdownImages it needs an AddRef for each image that uses it
	this->Texture->PreLoad ();
	this->Texture->AddRef ();

	image->d3d_Texture = this->Texture;

	image->scrap = true;
	image->mipmap = false;

	image->sl = (float) x / (float) SCRAP_SIZE;
	image->sh = (float) image->width / (float) SCRAP_SIZE;
	image->tl = (float) y / (float) SCRAP_SIZE;
	image->th = (float) image->height / (float) SCRAP_SIZE;
}


ImageScrap *ImageScrap::Scrap_AllocBlock (int w, int h, int *x, int *y)
{
	for (int texnum = 0; texnum < Scraps.size (); texnum++)
		if (Scraps[texnum]->AllocBlock (w, h, x, y))
			return Scraps[texnum];

	// alloc a new scrap object
	Scraps.push_back (new ImageScrap ());

	// call recursively to use the new scrap object
	return ImageScrap::Scrap_AllocBlock (w, h, x, y);
}


void ImageScrap::Init (void)
{
	for (int i = 0; i < Scraps.size (); i++)
	{
		if (Scraps[i])
		{
			delete Scraps[i];
			Scraps[i] = NULL;
		}
	}

	Scraps.clear ();
}


void ImageScrap::Shutdown (void)
{
	for (int i = 0; i < Scraps.size (); i++)
	{
		if (Scraps[i])
		{
			delete Scraps[i];
			Scraps[i] = NULL;
		}
	}

	Scraps.clear ();
}


