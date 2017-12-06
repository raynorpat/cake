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
// r_main.c
#include "d3d_local.h"

struct spritebuffer_t
{
	IDirect3DVertexBuffer9 *VertexBuffer;

	char name[MAX_PATH];
	int registration_sequence;
};

spritebuffer_t d3d_SpriteBuffers[MAX_MODELS];

IDirect3DVertexShader9 *d3d_SpriteVS = NULL;
IDirect3DPixelShader9 *d3d_SpritePS = NULL;
IDirect3DVertexDeclaration9 *d3d_SpriteDecl = NULL;

struct spritevert_t
{
	// the vertex buffer filling code relies on this struct being 4 consecutive floats - CHANGE IT AND YOU DIE!!!!
	float position[2];
	float texcoord[2];
};

void Sprite_LoadObjects (void)
{
	D3DVERTEXELEMENT9 decllayout[] =
	{
		VDECL (0, offsetof (spritevert_t, position), D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_POSITION, 0),
		VDECL (0, offsetof (spritevert_t, texcoord), D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 0),
		D3DDECL_END ()
	};

	d3d_Device->CreateVertexDeclaration (decllayout, &d3d_SpriteDecl);

	d3d_SpriteVS = D3D_CreateVertexShader ("hlsl/fxSprite.fx", "SpriteVS");
	d3d_SpritePS = D3D_CreatePixelShader ("hlsl/fxSprite.fx", "SpritePS");
}


void Sprite_Shutdown (void)
{
	for (int i = 0; i < MAX_MODELS; i++)
	{
		SAFE_RELEASE (d3d_SpriteBuffers[i].VertexBuffer);
		memset (&d3d_SpriteBuffers[i], 0, sizeof (spritebuffer_t));
	}

	SAFE_RELEASE (d3d_SpriteDecl);
	SAFE_RELEASE (d3d_SpriteVS);
	SAFE_RELEASE (d3d_SpritePS);
}


CD3DHandler Sprite_Handler (
	Sprite_LoadObjects,
	Sprite_Shutdown
);


void R_DrawSpriteModel (entity_t *e)
{
	// don't even bother culling, because it's just a single polygon without a surface cache
	dsprite_t *psprite = (dsprite_t *) e->model->extradata;
	int framenum = e->currframe % psprite->numframes;
	dsprframe_t *frame = &psprite->frames[framenum];

	d3d_State->SetTexture (0, &d3d_SpriteSampler, e->model->skins[framenum]->d3d_Texture);

	d3d_State->SetVertexShader (d3d_SpriteVS);
	d3d_State->SetPixelShader (d3d_SpritePS);

	d3d_State->SetVertexDeclaration (d3d_SpriteDecl);
	d3d_State->SetStreamSource (0, d3d_SpriteBuffers[e->model->bufferset].VertexBuffer, 0, sizeof (spritevert_t));

	d3d_State->SetVertexShaderConstant3fv (10, e->currorigin);
	d3d_State->SetPixelShaderConstant1f (0, (e->flags & RF_TRANSLUCENT) ? e->alpha : 1.0f);

	d3d_Device->DrawPrimitive (D3DPT_TRIANGLESTRIP, framenum << 2, 2);
}


void R_CreateSpriteBuffers (model_t *mod, dsprite_t *hdr)
{
	spritebuffer_t *freeslot = NULL;

	// try to find a buffer with the same model
	for (int i = 0; i < MAX_MODELS; i++)
	{
		// find the first free buffer for potential use
		if (!freeslot && !d3d_SpriteBuffers[i].name[0])
		{
			freeslot = &d3d_SpriteBuffers[i];
			mod->bufferset = i;
			continue;
		}

		if (!strcmp (d3d_SpriteBuffers[i].name, mod->name))
		{
			d3d_SpriteBuffers[i].registration_sequence = registration_sequence;
			mod->bufferset = i;
			return;
		}
	}

	// out of buffers!!!
	if (!freeslot)
		Com_Error (ERR_FATAL, "too many sprite buffers");

	d3d_Device->CreateVertexBuffer (
		hdr->numframes * sizeof (spritevert_t) * 4,
		D3DUSAGE_WRITEONLY,
		0,
		D3DPOOL_MANAGED,
		&d3d_SpriteBuffers[mod->bufferset].VertexBuffer,
		NULL
	);

	for (int i = 0; i < hdr->numframes; i++)
	{
		spritevert_t *verts = NULL;
		dsprframe_t	*frame = &hdr->frames[i];

		d3d_SpriteBuffers[mod->bufferset].VertexBuffer->Lock (i * 4 * sizeof (spritevert_t), sizeof (spritevert_t) * 4, (void **) &verts, 0);

		// this works because the sprite vert is 4 consecutive floats
		Vector4Set (verts[0].position, -frame->origin_y, -frame->origin_x, 0, 1);
		Vector4Set (verts[1].position, frame->height - frame->origin_y, -frame->origin_x, 0, 0);
		Vector4Set (verts[2].position, -frame->origin_y, frame->width - frame->origin_x, 1, 1);
		Vector4Set (verts[3].position, frame->height - frame->origin_y, frame->width - frame->origin_x, 1, 0);

		d3d_SpriteBuffers[mod->bufferset].VertexBuffer->Unlock ();
		d3d_SpriteBuffers[mod->bufferset].VertexBuffer->PreLoad ();
	}

	// copy this off so that we know to use it
	strcpy (freeslot->name, mod->name);
	freeslot->registration_sequence = registration_sequence;
}


void R_FreeUnusedSpriteBuffers (void)
{
	for (int i = 0; i < MAX_MODELS; i++)
	{
		if (d3d_SpriteBuffers[i].registration_sequence == registration_sequence) continue;

		SAFE_RELEASE (d3d_SpriteBuffers[i].VertexBuffer);
		memset (&d3d_SpriteBuffers[i], 0, sizeof (spritebuffer_t));
	}
}

