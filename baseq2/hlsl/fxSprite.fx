
struct VS_INPUT
{
	float2 Position: POSITION0;
	float2 TexCoord: TEXCOORD0;
};

struct PS_INPUT
{
	float4 Position: POSITION0;
	float2 TexCoord: TEXCOORD0;
};


#ifdef VERTEXSHADER
float4x4 WorldMatrix : register(c0);

float3 upVec : register(c6);
float3 rightVec : register(c7);
float3 entOrigin : register(c10);

PS_INPUT SpriteVS (VS_INPUT vs_in)
{
	PS_INPUT vs_out;

	vs_out.Position = mul (WorldMatrix, float4 (rightVec * vs_in.Position.y + (upVec * vs_in.Position.x + entOrigin), 1));
	vs_out.TexCoord = vs_in.TexCoord;

	return vs_out;
}
#endif


#ifdef PIXELSHADER
Texture spriteTexture : register(t0);
sampler spriteSampler : register(s0) = sampler_state {Texture = <spriteTexture>;};

float spriteAlpha : register(c0);

float4 SpritePS (PS_INPUT ps_in) : COLOR0
{
	return tex2D (spriteSampler, ps_in.TexCoord) * float4 (1.0f, 1.0f, 1.0f, spriteAlpha);
}
#endif
