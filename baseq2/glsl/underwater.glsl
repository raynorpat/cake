
// common
uniform vec4 warpparams;

INOUTTYPE vec4 texcoords[2];

#ifdef VERTEXSHADER
uniform mat4 orthomatrix;

layout(location = 0) in vec4 position;
layout(location = 2) in vec4 texcoord;

void WaterWarpVS ()
{
	gl_Position = orthomatrix * position;
	texcoords[0] = texcoord;
	texcoords[1] = ((texcoord * warpparams.y) + warpparams.x) * warpparams.z * warpparams.w;
}
#endif


#ifdef FRAGMENTSHADER
uniform sampler2D diffuse;
uniform sampler2D gradient;
uniform vec4 surfcolor;
uniform vec2 rescale;
uniform float gamma;

out vec4 fragColor;

void WaterWarpFS ()
{
	vec4 distort1 = texture (gradient, texcoords[1].yx);
	vec4 distort2 = texture (gradient, texcoords[1].xy);
	vec2 warpcoords = texcoords[0].xy + (distort1.ba + distort2.ab);

	fragColor = mix (texture (diffuse, warpcoords * rescale), surfcolor, surfcolor.a);
}
#endif
