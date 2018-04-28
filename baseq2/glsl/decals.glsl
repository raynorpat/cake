
INOUTTYPE vec4 iocolor;
INOUTTYPE vec2 texcoord;

#ifdef VERTEXSHADER
uniform mat4 mvpMatrix;

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texcoords;
layout(location = 2) in vec4 color;

void DecalVS ()
{
	gl_Position = mvpMatrix * vec4(position.xyz, 1.0);
	iocolor = LinearRGBToSRGB(color);
	texcoord = texcoords;
}
#endif


#ifdef FRAGMENTSHADER
uniform sampler2D decalTex;

out vec4 fragColor;

void DecalFS ()
{
	vec4 decalColor = texture(decalTex, texcoord.st);

	// multiply by color
	decalColor *= iocolor;
	
	// output
	fragColor = decalColor;
}
#endif

