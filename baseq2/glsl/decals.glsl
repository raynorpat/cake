
INOUTTYPE vec4 iocolor;
INOUTTYPE vec2 texcoord;

#ifdef VERTEXSHADER
uniform mat4 worldMatrix;

layout(location = 0) in vec4 position;
layout(location = 1) in vec2 texcoords;
layout(location = 2) in vec4 color;

void DecalVS ()
{
	gl_Position = worldMatrix * vec4(position.xyz, 1.0);
	iocolor = LinearRGBToSRGB(color);
	texcoord = texcoords;
}
#endif


#ifdef FRAGMENTSHADER
out vec4 fragColor;

void DecalFS ()
{
	vec4 color = iocolor;
	fragColor = color * iocolor.a;
}
#endif

