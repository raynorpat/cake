
struct VS_DRAW
{
	float4 Position: POSITION0;
	float4 Color : COLOR0;
	float4 TexCoord: TEXCOORD0;
	float2 Offsets: TEXCOORD1;
};


struct PS_DRAW
{
	float4 Position: POSITION0;
	float4 Color : COLOR0;
	float2 TexCoord: TEXCOORD0;
};


#ifdef VERTEXSHADER
float4x4 LocalMatrix : register(c0);
float4 warpParams : register(c4);
float4 halfPixel : register(c5);
float2 screenDimensions : register(c6);

float4 Draw_GetInstance (float4 inst, float2 offsets)
{
	return float4 (
		inst.x + inst.y * offsets.x,
		inst.z + inst.w * offsets.y,
		0.0f,
		1.0f
	);
}

PS_DRAW DrawVS (VS_DRAW vs_in)
{
	PS_DRAW vs_out;

	// correct the half-pixel offset
	vs_out.Position = mul (LocalMatrix, Draw_GetInstance (vs_in.Position, vs_in.Offsets) - halfPixel);
	vs_out.Color = vs_in.Color;
	vs_out.TexCoord = (Draw_GetInstance (vs_in.TexCoord, vs_in.Offsets)).xy;

	return vs_out;
}

float4 PolyblendVS (float2 Position : POSITION0) : POSITION0
{
	return float4 (Position, 0, 1) - halfPixel;
}

PS_DRAW BrightpassVS (float2 Position : POSITION0, float2 TexCoord: TEXCOORD0)
{
	PS_DRAW vs_out;

	vs_out.Position = float4 (((Position - halfPixel.xy) * screenDimensions) - float2 (1, 1), 0, 1);
	vs_out.Color = float4 (1, 1, 1, 1);
	vs_out.TexCoord = TexCoord;

	return vs_out;
}

PS_DRAW WaterwarpVS (float2 Position : POSITION0, float2 TexCoord: TEXCOORD0)
{
	PS_DRAW vs_out;

	vs_out.Position = float4 (((Position - halfPixel.xy) * screenDimensions) - float2 (1, 1), 0, 1);
	vs_out.Color = float4 (1, 1, 1, 1);
	vs_out.TexCoord = TexCoord * warpParams.xy;

	return vs_out;
}
#endif


#ifdef PIXELSHADER
Texture drawTexture : register(t0);
sampler drawSampler : register(s0) = sampler_state {Texture = <drawTexture>;};

float4 PolyblendColor : register(c0);
float vid_gamma : register(c1);
float vid_contrast : register(c7);
float4 warpParams : register(c2);

float4 DrawTexturedPS (PS_DRAW ps_in) : COLOR0
{
	return tex2D (drawSampler, ps_in.TexCoord) * ps_in.Color;
}

float4 DrawColouredPS (PS_DRAW ps_in) : COLOR0
{
	return ps_in.Color;
}

float4 PolyblendPS (float4 Position : POSITION0) : COLOR0
{
	return PolyblendColor;
}

float4 BrightpassPS (PS_DRAW ps_in) : COLOR0
{
	float4 color = tex2D (drawSampler, ps_in.TexCoord) * vid_contrast;
	return pow (abs (color), vid_gamma);
}

float4 WaterwarpPS (PS_DRAW ps_in) : COLOR0
{
	float2 st = (ps_in.TexCoord.xy + sin (ps_in.TexCoord.yx + warpParams.w) * warpParams.z) * warpParams.xy;
	float4 color = tex2D (drawSampler, st);

	return lerp (color, PolyblendColor, PolyblendColor.a);
}
#endif
