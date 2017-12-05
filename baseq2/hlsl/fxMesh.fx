
struct VS_MESH
{
	float4 Position0: POSITION0;
	float3 Normal0: NORMAL0;
	float4 Position1: POSITION1;
	float3 Normal1: NORMAL1;
	float2 TexCoord: TEXCOORD0;
};

struct PS_MESH
{
	float4 Position: POSITION0;
	float3 Normal : TEXCOORD0;
	float2 TexCoord: TEXCOORD1;
};

struct VS_SHADOW
{
	float4 Position0: POSITION0;
	float4 Position1: POSITION1;
};

struct PS_SHADOW
{
	float4 Position: POSITION0;
};

struct VS_SHELL
{
	float4 Position0: POSITION0;
	float3 Normal0: NORMAL0;
	float4 Position1: POSITION1;
	float3 Normal1: NORMAL1;
};

struct PS_SHELL
{
	float4 Position: POSITION0;
	float3 Normal : TEXCOORD0;
};


#ifdef VERTEXSHADER
float4x4 LocalMatrix : register(c8);
float lerpblend : register(c12);
float3 move : register(c13);
float3 frontv : register(c14);
float3 backv : register(c15);

PS_MESH MeshVS (VS_MESH vs_in)
{
	PS_MESH vs_out;

	vs_out.Position = mul (
		LocalMatrix,
		float4 (move, 1.0f) + 
		(vs_in.Position0 * float4 (frontv, 0.0f)) + 
		(vs_in.Position1 * float4 (backv, 0.0f))
	);

	vs_out.Normal = lerp (vs_in.Normal0, vs_in.Normal1, lerpblend);
	vs_out.TexCoord = vs_in.TexCoord;

	return vs_out;
}

PS_SHADOW ShadowVS (VS_SHADOW vs_in)
{
	PS_SHADOW vs_out;

	vs_out.Position = mul (
		LocalMatrix,
		float4 (move, 1.0f) + 
		(vs_in.Position0 * float4 (frontv, 0.0f)) + 
		(vs_in.Position1 * float4 (backv, 0.0f))
	);

	return vs_out;
}

PS_SHELL ShellVS (VS_SHELL vs_in)
{
	PS_SHELL vs_out;

	float4 lerpnormal = float4 (lerp (vs_in.Normal0, vs_in.Normal1, lerpblend), 0.0f);
	float powersuit_scale = 4.0f;

	vs_out.Position = mul (
		LocalMatrix,
		float4 (move, 1.0f) + 
		(vs_in.Position0 * float4 (frontv, 0.0f)) + 
		(vs_in.Position1 * float4 (backv, 0.0f)) +
		(lerpnormal * float4 (powersuit_scale, powersuit_scale, powersuit_scale, 0.0))
	);

	vs_out.Normal = lerp (vs_in.Normal0, vs_in.Normal1, lerpblend);

	return vs_out;
}
#endif


#ifdef PIXELSHADER
Texture meshTexture : register(t0);
sampler meshSampler : register(s0) = sampler_state {Texture = <meshTexture>;};

float4 shadelight : register(c0);
float3 shadevector : register(c1);

float4 MeshPS (PS_MESH ps_in) : COLOR0
{
	float4 diff = tex2D (meshSampler, ps_in.TexCoord);
	float shadedot = dot (normalize (ps_in.Normal), shadevector);

	return float4 ((diff * (max (shadedot + 1.0f, (shadedot * 0.2954545f) + 1.0f) * shadelight)).rgb, shadelight.a);
}

float4 ShadowPS (PS_SHADOW ps_in) : COLOR0
{
	// co-opt the shadelight register for gl_shadows
	return shadelight;
}

float4 ShellPS (PS_SHELL ps_in) : COLOR0
{
	// note - we may just wish to return shadelight here...
	float shadedot = dot (normalize (ps_in.Normal), shadevector);
	return float4 ((max (shadedot + 1.0f, (shadedot * 0.2954545f) + 1.0f) * shadelight).rgb, shadelight.a);
}
#endif



