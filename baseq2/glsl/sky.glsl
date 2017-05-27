
// common
INOUTTYPE vec4 texcoords;

#ifdef VERTEXSHADER
uniform mat4 localMatrix;
uniform mat4 skymatrix;

layout(location = 0) in vec4 position;

void SkyVS ()
{
	gl_Position = localMatrix * position;
	texcoords = skymatrix * position;
}
#endif


#ifdef FRAGMENTSHADER
uniform samplerCube skydiffuse;
uniform float gamma;

out vec4 fragColor;

void SkyFS ()
{
	//fragColor = texture (skydiffuse, texcoords.xzy);
	fragColor = pow(texture (skydiffuse, texcoords.xyz), vec4(gamma));
}
#endif

