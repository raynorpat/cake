
INOUTTYPE vec4 offsets;
INOUTTYPE vec4 iocolor;

#ifdef VERTEXSHADER
layout(std140) uniform GlobalUniforms
{
    mat4 worldMatrix;
	vec3 forwardVec;
	vec3 upVec;
	vec3 rightVec;
	vec3 viewOrigin;
};


layout(location = 0) in vec4 position;
layout(location = 1) in vec4 color;

const vec2 texcoords[4] = vec2[4] (vec2 (-1.0, -1.0), vec2 (1.0, -1.0), vec2 (1.0, 1.0), vec2 (-1.0, 1.0));

void ParticleVS ()
{
	float scale = (1.0 + dot (position.xyz - viewOrigin, forwardVec) * 0.004) * 0.5;
	vec4 texcoord = vec4 (texcoords[gl_VertexID], 0, 0);

	gl_Position = worldMatrix * vec4 (position.xyz + (rightVec * scale * texcoord.t) + (upVec * scale * texcoord.s), 1.0);

	iocolor = color;
	offsets = texcoord;
}
#endif


#ifdef FRAGMENTSHADER
uniform float gamma;

out vec4 fragColor;

void ParticleFS ()
{
	vec4 color = iocolor;
	color.a = (1.0 - dot (offsets.st, offsets.st)) * 1.5;

	fragColor = pow(color, vec4(gamma)) * iocolor.a;
}
#endif

