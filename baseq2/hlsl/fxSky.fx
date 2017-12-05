
struct SKYVERTEX
{
	float4 Position: POSITION0;
	float4 TexCoord: TEXCOORD0;
};

#ifdef VERTEXSHADER
float3 r_origin : register(c4);
float4x4 LocalMatrix : register(c8);
float4x4 SkyMatrix : register(c12);

SKYVERTEX SkyVS (SKYVERTEX vs_in)
{
	SKYVERTEX vs_out;

	vs_out.Position = mul (LocalMatrix, vs_in.Position);
	vs_out.TexCoord = mul (SkyMatrix, vs_in.TexCoord);

	return vs_out;
}
#endif


#ifdef PIXELSHADER
struct SKYPIXEL
{
	float4 Color : COLOR0;
	float Depth : DEPTH0;
};

Texture SkyTexture : register(t0);
sampler SkySampler : register(s0) = sampler_state {Texture = <SkyTexture>;};

SKYPIXEL SkyPS (SKYVERTEX ps_in)
{
	SKYPIXEL ps_out;

	ps_out.Color = texCUBE (SkySampler, ps_in.TexCoord.xyz);
	ps_out.Depth = 1.0f;

	return ps_out;
}
#endif
