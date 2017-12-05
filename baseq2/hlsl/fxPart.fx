
struct VS_INPUT
{
	float3 Origin: POSITION0;
	float3 Velocity: TEXCOORD0;
	float3 Acceleration: TEXCOORD1;
	float Time: TEXCOORD2;
	float4 Color : COLOR0;
	float Alpha: TEXCOORD3;
	float2 TexCoord: TEXCOORD4;
};

struct PS_INPUT
{
	float4 Position: POSITION0;
	float4 Color : COLOR0;
	float2 TexCoord: TEXCOORD0;
	float Alpha: TEXCOORD1;
};


#ifdef VERTEXSHADER
float4x4 WorldMatrix : register(c0);
float3 r_origin : register(c4);
float3 vpn : register(c5);

float3 pup : register(c8);
float3 pright : register(c9);

PS_INPUT PartVS (VS_INPUT vs_in)
{
	PS_INPUT vs_out;

	// civilized people in 2012 do this on the GPU (reordered to more easily generate MAD instructions)
	//float3 NewPosition = vs_in.Origin + vs_in.Velocity * vs_in.Time + vs_in.Acceleration * (vs_in.Time * vs_in.Time);
	float3 NewPosition = vs_in.Origin + (vs_in.Velocity + (vs_in.Acceleration * vs_in.Time)) * vs_in.Time;

	// hack a scale up to keep particles from disapearing
	float2 pscale = vs_in.TexCoord * (1.0f + dot (NewPosition - r_origin, vpn) * 0.002f);

	// and billboard it
	vs_out.Position = mul (WorldMatrix, float4 (NewPosition + pright * pscale.x + pup * pscale.y, 1.0f));

	// because the palette is bgra we need to swizzle the components again
	vs_out.Color = vs_in.Color.bgra;
	vs_out.TexCoord = vs_in.TexCoord;
	vs_out.Alpha = vs_in.Alpha;

	return vs_out;
}
#endif


#ifdef PIXELSHADER
float4 PartPS (PS_INPUT ps_in) : COLOR0
{
	// procedurally generate the particle dot for good speed and per-pixel accuracy at any scale
	float4 Color = ps_in.Color;
	Color.a = ((1.0f - dot (ps_in.TexCoord, ps_in.TexCoord)) * 1.5f) * ps_in.Alpha;
	return Color;
}
#endif
