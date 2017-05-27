

INOUTTYPE vec4 texcoords;
INOUTTYPE vec4 iocolor;

#ifdef VERTEXSHADER
uniform mat4 orthomatrix;

layout(location = 0) in vec4 position;
layout(location = 1) in vec4 color;
layout(location = 2) in vec4 texcoord;

void DrawVS ()
{
	gl_Position = orthomatrix * position;
	iocolor = color;
	texcoords = texcoord;
}
#endif


#ifdef FRAGMENTSHADER
uniform sampler2D diffuse;
uniform float texturecolormix;
uniform float gamma;

out vec4 fragColor;

void DrawFS ()
{
	fragColor = pow( mix (texture (diffuse, texcoords.st), iocolor, texturecolormix), vec4(gamma) );
}
#endif

