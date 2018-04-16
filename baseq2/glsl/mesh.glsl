
INOUTTYPE vec2 vtx_texcoords;
INOUTTYPE vec3 vtx_normals;

layout(std140) uniform MeshUniforms
{
	mat4 localMatrix;
	vec3 move;
	vec3 frontv;
	vec3 backv;
	vec4 shadelight;
	vec3 shadevector;
	float lerpfrac;
	float powersuit_scale;
	float meshshellmix;
};

#ifdef VERTEXSHADER
layout(location = 0) in vec4 currvert;
layout(location = 1) in vec4 lastvert;
layout(location = 2) in vec4 texcoord;

uniform vec3 lightnormal[162];

void MeshVS ()
{
	vec4 lerpnormal = vec4 (mix (lightnormal[uint (currvert.w)], lightnormal[uint (lastvert.w)], lerpfrac), 0.0);

	gl_Position = localMatrix * 
	(
		vec4 (move, 1.0) + 
		(currvert * vec4 (frontv, 0.0)) + 
		(lastvert * vec4 (backv, 0.0)) + 
		(lerpnormal * vec4 (powersuit_scale, powersuit_scale, powersuit_scale, 0.0))
	);

	vtx_texcoords = texcoord.st;
	
	vtx_normals = lerpnormal.xyz;
}
#endif


#ifdef FRAGMENTSHADER
uniform sampler2D diffuse;

out vec4 fragColor;

void MeshFS ()
{
	// get vertex normal
	vec3 normal = normalize (vtx_normals);
	
	// get flat diffuse color
	vec4 diffAlbedo = vec4(1.0);
	diffAlbedo = texture (diffuse, vtx_texcoords);
	
	// get shade dots for shading
	float shadedot = dot (normal, shadevector);
	vec4 shade = max (shadedot + 1.0, (shadedot * 0.2954545) + 1.0) * shadelight;

	// set diffuse
	vec4 diffuseTerm = vec4(diffAlbedo.rgb, 1.0) * shade;

	// output final color
	fragColor = mix (diffuseTerm, shadelight, meshshellmix);
	
	// tonemap final color
	fragColor.rgb = ACESFilmRec2020_Tonemap (fragColor.rgb);
}
#endif

