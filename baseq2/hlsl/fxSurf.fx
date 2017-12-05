
struct SURFVERTEX
{
	float4 Position: POSITION0;
	float2 TexCoord0: TEXCOORD0;
	float2 TexCoord1: TEXCOORD1;
};

#ifdef VERTEXSHADER
float4x4 LocalMatrix : register(c8);
float scroll : register(c13);

SURFVERTEX SurfVS (SURFVERTEX vs_in)
{
	SURFVERTEX vs_out;

	vs_out.Position = mul (LocalMatrix, vs_in.Position);
	vs_out.TexCoord0 = vs_in.TexCoord0 + float2 (scroll, 0.0f);
	vs_out.TexCoord1 = vs_in.TexCoord1;

	return vs_out;
}
#endif


#ifdef PIXELSHADER
Texture diffTexture : register(t0);
Texture lmapTexture : register(t1);

sampler diffSampler : register(s0) = sampler_state {Texture = <diffTexture>;};
sampler lmapSampler : register(s1) = sampler_state {Texture = <lmapTexture>;};

float surfalpha : register(c0);
float monolightmap : register(c1);

float4 SurfPS (SURFVERTEX ps_in) : COLOR0
{
	float4 albedo = tex2D (diffSampler, ps_in.TexCoord0);
	float4 lmap = tex2D (lmapSampler, ps_in.TexCoord1);
	float4 grey = dot (lmap.rgb, float3 (0.3f, 0.59f, 0.11f));

	float4 final = albedo * lerp(lmap, grey, monolightmap);

	return float4 (final.rgb, surfalpha);
}

float4 AlphaPS (SURFVERTEX ps_in) : COLOR0
{
	return float4 ((tex2D (diffSampler, ps_in.TexCoord0)).rgb, surfalpha);
}
#endif

