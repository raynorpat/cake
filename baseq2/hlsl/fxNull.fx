
// null models get fake light by deriving a normal from the position and using that for lighting
struct NULLVERTEX
{
	float4 Position : POSITION0;
	float3 Normal : NORMAL0;
};

#ifdef VERTEXSHADER
float4x4 LocalMatrix : register(c8);

NULLVERTEX NullVS (float4 Position : POSITION0)
{
	NULLVERTEX vs_out;

	vs_out.Position = mul (LocalMatrix, Position);
	vs_out.Normal = normalize (Position.xyz);

	return vs_out;
}
#endif


#ifdef PIXELSHADER
float4 shadelight : register(c0);
float3 shadevector : register(c1);

float4 NullPS (NULLVERTEX ps_in) : COLOR0
{
	float shadedot = dot (normalize (ps_in.Normal), shadevector);
	return float4 ((max (shadedot + 1.0f, (shadedot * 0.2954545f) + 1.0f) * shadelight).rgb, shadelight.a);
}
#endif

