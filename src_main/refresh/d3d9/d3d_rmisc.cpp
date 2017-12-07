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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// r_misc.c

#include "d3d_local.h"

/*
==================
R_InitTextures
==================
*/
void R_InitTextures (void)
{
	byte data[8][8][4];

	byte dottexture[8][8] =
	{
		{0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 1, 1, 0, 0, 0},
		{0, 0, 1, 1, 1, 1, 0, 0},
		{0, 0, 1, 1, 1, 1, 0, 0},
		{0, 0, 0, 1, 1, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0, 0, 0},
	};

	for (int x = 0; x < 8; x++)
	{
		for (int y = 0; y < 8; y++)
		{
			data[y][x][0] = dottexture[x & 3][y & 3] * 255;
			data[y][x][1] = 0;
			data[y][x][2] = 0;
			data[y][x][3] = 255;
		}
	}

	r_notexture = GL_LoadPic ("***r_notexture***", (byte *) data, 8, 8, it_wall, 32);

	for (int x = 0; x < 8; x++)
	{
		for (int y = 0; y < 8; y++)
		{
			data[y][x][0] = 128;
			data[y][x][1] = 128;
			data[y][x][2] = 128;
			data[y][x][3] = 255;
		}
	}

	r_greytexture = GL_LoadPic ("***r_greytexture***", (byte *) data, 8, 8, it_wall, 32);

	for (int x = 0; x < 8; x++)
	{
		for (int y = 0; y < 8; y++)
		{
			data[y][x][0] = 255;
			data[y][x][1] = 255;
			data[y][x][2] = 255;
			data[y][x][3] = 255;
		}
	}

	r_whitetexture = GL_LoadPic ("***r_whitetexture***", (byte *) data, 8, 8, it_wall, 32);

	for (int x = 0; x < 8; x++)
	{
		for (int y = 0; y < 8; y++)
		{
			data[y][x][0] = 0;
			data[y][x][1] = 0;
			data[y][x][2] = 0;
			data[y][x][3] = 255;
		}
	}

	r_blacktexture = GL_LoadPic ("***r_blacktexture***", (byte *) data, 8, 8, it_wall, 32);
}


/*
==============================================================================

						SCREEN SHOTS

==============================================================================
*/

/*
==================
GL_ScreenShot_f
==================
*/
void GL_ScreenShot_f (void)
{
	int i;
	char checkname[MAX_OSPATH];

	Sys_Mkdir (va ("%s/scrnshot", FS_Gamedir ()));

	// find a file name to save it to
	for (i = 0; i < 100; i++)
	{
		FILE *f = NULL;

		sprintf (checkname, "%s/scrnshot/quake%02i.tga", FS_Gamedir (), i);

		if ((f = fopen (checkname, "rb")) == NULL)
			break;
		else fclose (f);
	}

	if (i == 100)
	{
		Com_Printf ("SCR_ScreenShot_f: Couldn't create a TGA file\n");
		return;
	}

	IDirect3DSurface9 *backbuff = NULL;
	IDirect3DSurface9 *copysurf = NULL;

	if (SUCCEEDED (d3d_Device->GetBackBuffer (0, 0, D3DBACKBUFFER_TYPE_MONO, &backbuff)))
	{
		D3DSURFACE_DESC desc;

		if (SUCCEEDED (backbuff->GetDesc (&desc)))
		{
			if (SUCCEEDED (d3d_Device->CreateOffscreenPlainSurface (desc.Width, desc.Height, D3DFMT_X8R8G8B8, D3DPOOL_SCRATCH, &copysurf, NULL)))
			{
				if (SUCCEEDED (D3DXLoadSurfaceFromSurface (copysurf, NULL, NULL, backbuff, NULL, NULL, D3DX_FILTER_LINEAR, 0)))
				{
					D3DLOCKED_RECT lockrect;

					if (SUCCEEDED (copysurf->LockRect (&lockrect, NULL, 0)))
					{
						D3D_CollapseRowPitch ((unsigned *) lockrect.pBits, desc.Width, desc.Height, lockrect.Pitch >> 2);
						D3D_Compress32To24 ((byte *) lockrect.pBits, desc.Width, desc.Height);
						D3D_WriteDataToTGA (checkname, lockrect.pBits, desc.Width, desc.Height, 24);

						copysurf->UnlockRect ();

						Com_Printf ("Wrote %s\n", checkname);
					}
				}
			}
		}
	}

	SAFE_RELEASE (copysurf);
	SAFE_RELEASE (backbuff);
}





