
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
uniform vec4 colorAdd;
uniform float brightnessAmount;
uniform float contrastAmount;
uniform float texturecolormix;

out vec4 fragColor;

void DrawFS ()
{
	vec4 colorTex = texture (diffuse, texcoords.st);
	colorTex *= colorAdd;
	
	fragColor = mix (colorTex, iocolor, texturecolormix);

	fragColor.rgb = doBrightnessAndContrast(fragColor.rgb, brightnessAmount, contrastAmount);
}
#endif

