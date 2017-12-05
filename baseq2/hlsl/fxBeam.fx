
struct BEAMVERTEX
{
	float4 Position : POSITION0;
	float4 Color : COLOR0;
};

#ifdef VERTEXSHADER
float4x4 LocalMatrix : register(c8);
float4 BeamColor : register(c12);

BEAMVERTEX BeamVS (float4 Position : POSITION0)
{
	BEAMVERTEX vs_out;

	vs_out.Position = mul (LocalMatrix, Position);
	vs_out.Color = BeamColor;

	return vs_out;
}
#endif


#ifdef PIXELSHADER
float4 BeamPS (BEAMVERTEX ps_in) : COLOR0
{
	return ps_in.Color;
}
#endif

