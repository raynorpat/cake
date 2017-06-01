// common
INOUTTYPE vec4 texcoords;

#ifdef VERTEXSHADER
uniform mat4 orthomatrix;

layout(location = 0) in vec4 position;
layout(location = 2) in vec4 texcoord;


void PostVS ()
{
	gl_Position = orthomatrix * position;
	texcoords = texcoord;
}
#endif


#ifdef FRAGMENTSHADER
uniform sampler2D diffuse;
uniform sampler2D precomposite;
uniform vec4 surfcolor;
uniform vec2 rescale;

uniform vec2 texScale;

out vec4 fragColor;

void PostFS ()
{
	vec2 st = gl_FragCoord.st * texScale;
	vec4 scene = texture (diffuse, st);
	vec4 prescene = texture (precomposite, st);
	
	vec4 color = scene + prescene;

	fragColor = mix(color, surfcolor, surfcolor.a);
}
#endif

