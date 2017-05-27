
INOUTTYPE vec4 iocolor;

#ifdef VERTEXSHADER
layout(location = 0) in vec4 position;

uniform mat4 localMatrix;
uniform vec4 color;
uniform vec3 scale;

void UntexturedVS ()
{
	gl_Position = localMatrix * (position * vec4 (scale, 1.0));
	iocolor = color;
}
#endif


#ifdef FRAGMENTSHADER
uniform float gamma;

out vec4 fragColor;

void UntexturedFS ()
{
	fragColor = pow(iocolor, vec4(gamma));
}
#endif

