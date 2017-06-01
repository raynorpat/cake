
// common
uniform float scroll;
uniform vec4 warpparams;

INOUTTYPE vec4 texcoords[2];

#ifdef VERTEXSHADER
uniform mat4 localMatrix;

layout(location = 0) in vec4 position;
layout(location = 1) in vec4 diffuse;

void LiquidVS ()
{
	gl_Position = localMatrix * position;
	texcoords[0] = diffuse;
	texcoords[1] = ((diffuse * warpparams.y) + warpparams.x) * warpparams.z;
}
#endif


#ifdef FRAGMENTSHADER
uniform sampler2D diffuse;
uniform float surfalpha;
uniform float gamma;

out vec4 fragColor;

void LiquidFS ()
{
	fragColor = vec4 (texture (diffuse, ((texcoords[0].xy + sin (texcoords[1].yx) * warpparams.w) * 0.015625) + (scroll * texcoords[0].z)).rgb, surfalpha);
}
#endif

