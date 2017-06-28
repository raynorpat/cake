
// common
uniform float scroll;

INOUTTYPE vec4 texcoords[2];

#ifdef VERTEXSHADER
uniform mat4 localMatrix;

layout(location = 0) in vec4 position;
layout(location = 1) in vec4 diffuse;
layout(location = 2) in vec4 lightmap;


void LightmappedVS ()
{
	gl_Position = localMatrix * position;
	texcoords[0] = diffuse + (vec4 (scroll, 0, 0, 0) * diffuse.z);
	texcoords[1] = lightmap;
}
#endif


#ifdef FRAGMENTSHADER
#extension GL_EXT_texture_array : enable

uniform sampler2D diffuse;
uniform sampler2DArray lightmap;
uniform float surfalpha;
uniform mat4 colormatrix;

out vec4 fragColor;

void LightmappedFS ()
{
	vec4 lmap = texture2DArray(lightmap, texcoords[1].xyz);
	vec4 albedo = texture(diffuse, texcoords[0].st);
		
	vec4 final = albedo * (colormatrix * (lmap / lmap.a));
	
	fragColor = vec4(final.rgb, surfalpha);
}
#endif

