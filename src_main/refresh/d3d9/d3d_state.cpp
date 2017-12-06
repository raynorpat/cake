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

#include "d3d_local.h"


samplerstate_t d3d_DrawSampler (D3DTEXF_LINEAR, D3DTEXF_NONE, D3DTEXF_LINEAR, D3DTADDRESS_WRAP, 1);
samplerstate_t d3d_MeshSampler (D3DTEXF_LINEAR, D3DTEXF_LINEAR, D3DTEXF_LINEAR, D3DTADDRESS_CLAMP, 1);
samplerstate_t d3d_SpriteSampler (D3DTEXF_LINEAR, D3DTEXF_LINEAR, D3DTEXF_LINEAR, D3DTADDRESS_CLAMP, 1);
samplerstate_t d3d_SurfDiffuseSampler (D3DTEXF_LINEAR, D3DTEXF_LINEAR, D3DTEXF_LINEAR, D3DTADDRESS_WRAP, 1);
samplerstate_t d3d_SkySampler (D3DTEXF_LINEAR, D3DTEXF_NONE, D3DTEXF_LINEAR, D3DTADDRESS_CLAMP, 1);

samplerstate_t::samplerstate_t (D3DTEXTUREFILTERTYPE magfilter, D3DTEXTUREFILTERTYPE mipfilter, D3DTEXTUREFILTERTYPE minfilter, D3DTEXTUREADDRESS addressmode, DWORD maxanisotropy)
{
	this->Update (magfilter, mipfilter, minfilter, maxanisotropy);
	this->AddressMode = addressmode;
}


void samplerstate_t::Update (D3DTEXTUREFILTERTYPE magfilter, D3DTEXTUREFILTERTYPE mipfilter, D3DTEXTUREFILTERTYPE minfilter, DWORD maxanisotropy)
{
	if (maxanisotropy > 1)
	{
		this->MagFilter = D3DTEXF_ANISOTROPIC;
		this->MipFilter = mipfilter;
		this->MinFilter = D3DTEXF_ANISOTROPIC;
	}
	else
	{
		this->MagFilter = magfilter;
		this->MipFilter = mipfilter;
		this->MinFilter = minfilter;
	}

	this->MaxAnisotropy = maxanisotropy;
}


struct d3d_sampler_t
{
	char *name;
	D3DTEXTUREFILTERTYPE magfilter;
	D3DTEXTUREFILTERTYPE mipfilter;
	D3DTEXTUREFILTERTYPE minfilter;
	DWORD maxanisotropy;
};


d3d_sampler_t modes[] =
{
	{"GL_NEAREST", D3DTEXF_POINT, D3DTEXF_NONE, D3DTEXF_POINT, 1},
	{"GL_LINEAR", D3DTEXF_LINEAR, D3DTEXF_NONE, D3DTEXF_LINEAR, 1},
	{"GL_NEAREST_MIPMAP_NEAREST", D3DTEXF_POINT, D3DTEXF_POINT, D3DTEXF_POINT, 1},
	{"GL_LINEAR_MIPMAP_NEAREST", D3DTEXF_LINEAR, D3DTEXF_POINT, D3DTEXF_LINEAR, 1},
	{"GL_NEAREST_MIPMAP_LINEAR", D3DTEXF_POINT, D3DTEXF_LINEAR, D3DTEXF_POINT, 1},
	{"GL_LINEAR_MIPMAP_LINEAR", D3DTEXF_LINEAR, D3DTEXF_LINEAR, D3DTEXF_LINEAR, 1}
};


/*
===============
GL_TextureMode
===============
*/
void GL_TextureMode (char *string, int afilter)
{
	DWORD anisotropy = 1;

	// anisotropy must be a power of two and bounded
	for (anisotropy = 1; anisotropy < afilter; anisotropy <<= 1);

	if (anisotropy < 1) anisotropy = 1;
	if (anisotropy > d3d_DeviceCaps.MaxAnisotropy) anisotropy = d3d_DeviceCaps.MaxAnisotropy;

	for (int i = 0; i < 6; i++)
	{
		if (!Q_strcasecmp (modes[i].name, string))
		{
			// update that which needs to be updated
			d3d_DrawSampler.Update (modes[i].magfilter, D3DTEXF_NONE, modes[i].minfilter, 1);
			d3d_MeshSampler.Update (modes[i].magfilter, modes[i].mipfilter, modes[i].minfilter, anisotropy);
			d3d_SpriteSampler.Update (modes[i].magfilter, modes[i].mipfilter, modes[i].minfilter, anisotropy);
			d3d_SurfDiffuseSampler.Update (modes[i].magfilter, modes[i].mipfilter, modes[i].minfilter, anisotropy);
			d3d_SkySampler.Update (modes[i].magfilter, D3DTEXF_NONE, modes[i].minfilter, anisotropy);
			return;
		}
	}

	Com_Printf ("GL_TextureMode : %s is not a valid texture filter\n", string);
}


IDirect3DStateBlock9 *d3d_EnableAlphaObjects = NULL;
IDirect3DStateBlock9 *d3d_DisableAlphaObjects = NULL;
IDirect3DStateBlock9 *d3d_MainSceneState = NULL;
IDirect3DStateBlock9 *d3d_2DGUIState = NULL;
IDirect3DStateBlock9 *d3d_RTTState = NULL;
IDirect3DStateBlock9 *d3d_PolyblendState = NULL;
IDirect3DStateBlock9 *d3d_EnableShadows = NULL;
IDirect3DStateBlock9 *d3d_DisableShadows = NULL;

CStateManager *d3d_State = NULL;


void State_LoadObjects (void)
{
	d3d_State = new CStateManager ();

	d3d_Device->BeginStateBlock ();
	d3d_Device->SetRenderState (D3DRS_ALPHABLENDENABLE, TRUE);
	d3d_Device->SetRenderState (D3DRS_BLENDOP, D3DBLENDOP_ADD);
	d3d_Device->SetRenderState (D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	d3d_Device->SetRenderState (D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	d3d_Device->SetRenderState (D3DRS_ZWRITEENABLE, FALSE);
	d3d_Device->EndStateBlock (&d3d_EnableAlphaObjects);

	d3d_Device->BeginStateBlock ();
	d3d_Device->SetRenderState (D3DRS_ZWRITEENABLE, TRUE);
	d3d_Device->SetRenderState (D3DRS_ALPHABLENDENABLE, FALSE);
	d3d_Device->EndStateBlock (&d3d_DisableAlphaObjects);

	d3d_Device->BeginStateBlock ();
	d3d_Device->SetRenderState (D3DRS_CULLMODE, D3DCULL_CCW);
	d3d_Device->SetRenderState (D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
	d3d_Device->SetRenderState (D3DRS_ZENABLE, D3DZB_TRUE);
	d3d_Device->SetRenderState (D3DRS_ZWRITEENABLE, TRUE);
	d3d_Device->SetRenderState (D3DRS_SHADEMODE, D3DSHADE_FLAT);
	d3d_Device->SetRenderState (D3DRS_ALPHATESTENABLE, FALSE);
	d3d_Device->SetRenderState (D3DRS_ALPHABLENDENABLE, FALSE);
	d3d_Device->EndStateBlock (&d3d_MainSceneState);

	d3d_Device->BeginStateBlock ();
	d3d_Device->SetRenderState (D3DRS_ZENABLE, D3DZB_FALSE);
	d3d_Device->SetRenderState (D3DRS_ZWRITEENABLE, FALSE);
	d3d_Device->SetRenderState (D3DRS_CULLMODE, D3DCULL_NONE);
	d3d_Device->SetRenderState (D3DRS_ALPHABLENDENABLE, TRUE);
	d3d_Device->SetRenderState (D3DRS_BLENDOP, D3DBLENDOP_ADD);
	d3d_Device->SetRenderState (D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	d3d_Device->SetRenderState (D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	d3d_Device->EndStateBlock (&d3d_2DGUIState);

	d3d_Device->BeginStateBlock ();
	d3d_Device->SetRenderState (D3DRS_ZENABLE, D3DZB_FALSE);
	d3d_Device->SetRenderState (D3DRS_ZWRITEENABLE, FALSE);
	d3d_Device->SetRenderState (D3DRS_CULLMODE, D3DCULL_NONE);
	d3d_Device->SetRenderState (D3DRS_ALPHABLENDENABLE, TRUE);
	d3d_Device->SetRenderState (D3DRS_BLENDOP, D3DBLENDOP_ADD);
	d3d_Device->SetRenderState (D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	d3d_Device->SetRenderState (D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	d3d_Device->EndStateBlock (&d3d_PolyblendState);

	d3d_Device->BeginStateBlock ();
	d3d_Device->SetRenderState (D3DRS_ZENABLE, D3DZB_FALSE);
	d3d_Device->SetRenderState (D3DRS_ZWRITEENABLE, FALSE);
	d3d_Device->SetRenderState (D3DRS_CULLMODE, D3DCULL_NONE);
	d3d_Device->SetRenderState (D3DRS_ALPHABLENDENABLE, FALSE);
	d3d_Device->SetRenderState (D3DRS_SHADEMODE, D3DSHADE_FLAT);
	d3d_Device->SetRenderState (D3DRS_ALPHATESTENABLE, FALSE);
	d3d_Device->SetRenderState (D3DRS_ALPHABLENDENABLE, FALSE);
	d3d_Device->EndStateBlock (&d3d_RTTState);

	d3d_Device->BeginStateBlock ();
	d3d_Device->SetRenderState (D3DRS_ZENABLE, D3DZB_TRUE);
	d3d_Device->SetRenderState (D3DRS_ZWRITEENABLE, FALSE);
	d3d_Device->SetRenderState (D3DRS_ALPHABLENDENABLE, TRUE);
	d3d_Device->SetRenderState (D3DRS_BLENDOP, D3DBLENDOP_ADD);
	d3d_Device->SetRenderState (D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	d3d_Device->SetRenderState (D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	d3d_Device->SetRenderState (D3DRS_STENCILENABLE, TRUE);
	d3d_Device->SetRenderState (D3DRS_STENCILFUNC, D3DCMP_EQUAL);
	d3d_Device->SetRenderState (D3DRS_STENCILREF, 0x00000001);
	d3d_Device->SetRenderState (D3DRS_STENCILMASK, 0x00000002);
	d3d_Device->SetRenderState (D3DRS_STENCILWRITEMASK, 0xFFFFFFFF);
	d3d_Device->SetRenderState (D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
	d3d_Device->SetRenderState (D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
	d3d_Device->SetRenderState (D3DRS_STENCILPASS, D3DSTENCILOP_INCRSAT);
	d3d_Device->EndStateBlock (&d3d_EnableShadows);

	d3d_Device->BeginStateBlock ();
	d3d_Device->SetRenderState (D3DRS_ZWRITEENABLE, TRUE);
	d3d_Device->SetRenderState (D3DRS_ALPHABLENDENABLE, FALSE);
	d3d_Device->SetRenderState (D3DRS_STENCILENABLE, FALSE);
	d3d_Device->EndStateBlock (&d3d_DisableShadows);
}


void State_Shutdown (void)
{
	if (d3d_State)
	{
		delete d3d_State;
		d3d_State = NULL;
	}

	SAFE_RELEASE (d3d_EnableAlphaObjects);
	SAFE_RELEASE (d3d_DisableAlphaObjects);
	SAFE_RELEASE (d3d_MainSceneState);
	SAFE_RELEASE (d3d_2DGUIState);
	SAFE_RELEASE (d3d_RTTState);
	SAFE_RELEASE (d3d_PolyblendState);
	SAFE_RELEASE (d3d_EnableShadows);
	SAFE_RELEASE (d3d_DisableShadows);
}


CD3DHandler State_Handler (
	NULL,
	NULL,
	State_Shutdown,
	State_LoadObjects
);


CStateManager::CStateManager (void)
{
	// clear all current bindings
	// these are forced to NULL irrespective of what the device currently has so that the next Set call will force a Set
	memset (this->currentTextures, 0, sizeof (this->currentTextures));
	memset (this->currentStreams, 0, sizeof (this->currentStreams));
	memset (this->currentSamplerState, (int) D3DSAMP_FORCE_DWORD, sizeof (this->currentSamplerState));

	// these are forced to NULL irrespective of what the device currently has so that the next Set call will force a Set
	this->currentVertexDeclaration = NULL;
	this->currentVertexShader = NULL;
	this->currentPixelShader = NULL;
	this->currentIndexBuffer = NULL;

	// this is just used for returning state so it gets stored out
	d3d_Device->GetViewport (&this->currentViewPort);
}


CStateManager::~CStateManager (void)
{
	samplerstate_t defaultSampler (D3DTEXF_POINT, D3DTEXF_NONE, D3DTEXF_POINT, D3DTADDRESS_WRAP, 1);

	// unbind everything
	for (int i = 0; i < 8; i++)
		this->SetTexture (i, &defaultSampler, NULL);

	for (int i = 0; i < 16; i++)
	{
		this->SetStreamSource (i, NULL, 0, 0);
		this->SetStreamSourceFreq (i, 1);
	}

	this->SetVertexDeclaration (NULL);
	this->SetVertexShader (NULL);
	this->SetPixelShader (NULL);
	this->SetIndices (NULL);
}


void CStateManager::SetTexture (DWORD Stage, samplerstate_t *SamplerState, IDirect3DBaseTexture9 *pTexture)
{
	if (Stage < 0) return;
	if (Stage > 7) return;

	if (this->currentTextures[Stage] != pTexture)
	{
		// update the sampler state
		this->SetSamplerState (Stage, D3DSAMP_MAGFILTER, SamplerState->MagFilter);
		this->SetSamplerState (Stage, D3DSAMP_MIPFILTER, SamplerState->MipFilter);
		this->SetSamplerState (Stage, D3DSAMP_MINFILTER, SamplerState->MinFilter);

		this->SetSamplerState (Stage, D3DSAMP_ADDRESSU, SamplerState->AddressMode);
		this->SetSamplerState (Stage, D3DSAMP_ADDRESSV, SamplerState->AddressMode);
		this->SetSamplerState (Stage, D3DSAMP_ADDRESSW, SamplerState->AddressMode);

		this->SetSamplerState (Stage, D3DSAMP_MAXANISOTROPY, SamplerState->MaxAnisotropy);

		d3d_Device->SetTexture (Stage, pTexture);
		this->currentTextures[Stage] = pTexture;
	}
}


void CStateManager::SetSamplerState (DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value)
{
	if (Sampler < 0) return;
	if (Sampler > 7) return;

	if (this->currentSamplerState[Sampler][(int) Type] != Value)
	{
		d3d_Device->SetSamplerState (Sampler, Type, Value);
		this->currentSamplerState[Sampler][(int) Type] = Value;
	}
}


void CStateManager::SetVertexDeclaration (IDirect3DVertexDeclaration9 *pDecl)
{
	if (this->currentVertexDeclaration != pDecl)
	{
		d3d_Device->SetVertexDeclaration (pDecl);
		this->currentVertexDeclaration = pDecl;
	}
}


void CStateManager::SetVertexShader (IDirect3DVertexShader9 *pShader)
{
	if (this->currentVertexShader != pShader)
	{
		d3d_Device->SetVertexShader (pShader);
		this->currentVertexShader = pShader;
	}
}


void CStateManager::SetPixelShader (IDirect3DPixelShader9 *pShader)
{
	if (this->currentPixelShader != pShader)
	{
		d3d_Device->SetPixelShader (pShader);
		this->currentPixelShader = pShader;
	}
}


void CStateManager::SetStreamSource (UINT StreamNumber, IDirect3DVertexBuffer9 *pStreamData, UINT OffsetInBytes, UINT Stride)
{
	if (StreamNumber < 0) return;
	if (StreamNumber > 15) return;

	if (this->currentStreams[StreamNumber].pStreamData != pStreamData ||
		this->currentStreams[StreamNumber].OffsetInBytes != OffsetInBytes ||
		this->currentStreams[StreamNumber].Stride != Stride)
	{
		d3d_Device->SetStreamSource (StreamNumber, pStreamData, OffsetInBytes, Stride);
		this->currentStreams[StreamNumber].pStreamData = pStreamData;
		this->currentStreams[StreamNumber].OffsetInBytes = OffsetInBytes;
		this->currentStreams[StreamNumber].Stride = Stride;
	}
}


void CStateManager::SetStreamSourceFreq (UINT StreamNumber, UINT FrequencyParameter)
{
	if (StreamNumber < 0) return;
	if (StreamNumber > 15) return;

	if (this->currentStreams[StreamNumber].FrequencyParameter != FrequencyParameter)
	{
		d3d_Device->SetStreamSourceFreq (StreamNumber, FrequencyParameter);
		this->currentStreams[StreamNumber].FrequencyParameter = FrequencyParameter;
	}
}


void CStateManager::SetIndices (IDirect3DIndexBuffer9 *pIndexData)
{
	if (this->currentIndexBuffer != pIndexData)
	{
		d3d_Device->SetIndices (pIndexData);
		this->currentIndexBuffer = pIndexData;
	}
}


void CStateManager::GetViewport (D3DVIEWPORT9 *vp)
{
	memcpy (vp, &this->currentViewPort, sizeof (D3DVIEWPORT9));
}


void CStateManager::SetViewport (D3DVIEWPORT9 *vp)
{
	memcpy (&this->currentViewPort, vp, sizeof (D3DVIEWPORT9));
	d3d_Device->SetViewport (vp);
}


void CStateManager::SetViewport (int x, int y, int w, int h, float zn, float zf)
{
	D3DVIEWPORT9 pViewport = {
		x,
		y,
		w,
		h,
		zn,
		zf
	};

	this->SetViewport (&pViewport);
}


// these are just convenience wrappers
void CStateManager::SetVertexShaderConstant1f (int reg, float a) {this->SetVertexShaderConstant4fv (reg, MakeFloat4 (a, 0, 0, 0), 1);}
void CStateManager::SetVertexShaderConstant2f (int reg, float a, float b) {this->SetVertexShaderConstant4fv (reg, MakeFloat4 (a, b, 0, 0), 1);}
void CStateManager::SetVertexShaderConstant3f (int reg, float a, float b, float c) {this->SetVertexShaderConstant4fv (reg, MakeFloat4 (a, b, c, 0), 1);}
void CStateManager::SetVertexShaderConstant4f (int reg, float a, float b, float c, float d) {this->SetVertexShaderConstant4fv (reg, MakeFloat4 (a, b, c, d), 1);}
void CStateManager::SetVertexShaderConstant2fv (int reg, float *a) {this->SetVertexShaderConstant4fv (reg, MakeFloat4 (a[0], a[1], 0, 0), 1);}
void CStateManager::SetVertexShaderConstant3fv (int reg, float *a) {this->SetVertexShaderConstant4fv (reg, MakeFloat4 (a[0], a[1], a[2], 0), 1);}
void CStateManager::SetPixelShaderConstant1f (int reg, float a) {this->SetPixelShaderConstant4fv (reg, MakeFloat4 (a, 0, 0, 0), 1);}
void CStateManager::SetPixelShaderConstant2f (int reg, float a, float b) {this->SetPixelShaderConstant4fv (reg, MakeFloat4 (a, b, 0, 0), 1);}
void CStateManager::SetPixelShaderConstant3f (int reg, float a, float b, float c) {this->SetPixelShaderConstant4fv (reg, MakeFloat4 (a, b, c, 0), 1);}
void CStateManager::SetPixelShaderConstant4f (int reg, float a, float b, float c, float d) {this->SetPixelShaderConstant4fv (reg, MakeFloat4 (a, b, c, d), 1);}
void CStateManager::SetPixelShaderConstant2fv (int reg, float *a) {this->SetPixelShaderConstant4fv (reg, MakeFloat4 (a[0], a[1], 0, 0), 1);}
void CStateManager::SetPixelShaderConstant3fv (int reg, float *a) {this->SetPixelShaderConstant4fv (reg, MakeFloat4 (a[0], a[1], a[2], 0), 1);}


// and these are the real thing
void CStateManager::SetVertexShaderConstant4fv (int reg, float *constant, int numfloat4)
{
	d3d_Device->SetVertexShaderConstantF (reg, constant, numfloat4);
}


void CStateManager::SetPixelShaderConstant4fv (int reg, float *constant, int numfloat4)
{
	d3d_Device->SetPixelShaderConstantF (reg, constant, numfloat4);
}

