
struct WATERVERT
{
	float4 Position: POSITION0;
	float2 TexCoord0: TEXCOORD0;
	float2 TexCoord1: TEXCOORD1;
};


#ifdef VERTEXSHADER
float4x4 LocalMatrix : register(c8);
float warptime : register(c12);
float scroll : register(c13);

WATERVERT WarpVS (WATERVERT vs_in)
{
	WATERVERT vs_out;

	vs_out.Position = mul (LocalMatrix, vs_in.Position);
	vs_out.TexCoord0 = (vs_in.TexCoord0 * 64.0f + float2 (scroll, 0.0f)) * 0.015625f;
	vs_out.TexCoord1 = vs_in.TexCoord1 + warptime;

	return vs_out;
}
#endif


#ifdef PIXELSHADER
Texture warpTexture : register(t0);
sampler warpSampler : register(s0) = sampler_state {Texture = <warpTexture>;};

float wateralpha : register(c0);

float4 WarpPS (WATERVERT ps_in) : COLOR0
{
	// quake used 0.125f but GLQuake 2 halved the warp strength
	return tex2D (warpSampler, ps_in.TexCoord0 + sin (ps_in.TexCoord1) * 0.0625f) * float4 (1, 1, 1, wateralpha);
}
#endif
