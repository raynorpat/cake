
INOUTTYPE vec4 texcoords;

#ifdef VERTEXSHADER
layout(std140) uniform GlobalUniforms
{
    mat4 worldMatrix;
	vec3 forwardVec;
	vec3 upVec;
	vec3 rightVec;
	vec3 viewOrigin;
};


uniform vec3 entOrigin;

layout(location = 0) in vec4 position;
layout(location = 1) in vec4 texcoord;

void SpriteVS ()
{
	gl_Position = worldMatrix * vec4 (rightVec * position.y + (upVec * position.x + entOrigin), 1);
	texcoords = texcoord;
}
#endif


#ifdef FRAGMENTSHADER
uniform sampler2D diffuse;
uniform vec4 colour;
uniform float gamma;

out vec4 fragColor;


void SpriteFS ()
{
	fragColor = texture (diffuse, texcoords.st) * colour;
}
#endif

