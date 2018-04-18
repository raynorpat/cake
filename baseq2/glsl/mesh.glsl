
INOUTTYPE vec2 vtx_texcoords;
INOUTTYPE vec3 vtx_normals;
INOUTTYPE vec4 vtx_world;

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
uniform vec3 r_entOrig;

void MeshVS ()
{
	vec4 lerpnormal = vec4 (mix (lightnormal[uint (currvert.w)], lightnormal[uint (lastvert.w)], lerpfrac), 0.0);

	gl_Position = localMatrix * 
	(
		vec4 (move, 1.0) + 
		(currvert * vec4(frontv, 0.0)) + 
		(lastvert * vec4(backv, 0.0)) + 
		(lerpnormal * vec4(powersuit_scale, powersuit_scale, powersuit_scale, 0.0))
	);

	vtx_texcoords = texcoord.st;
	
	vtx_normals = lerpnormal.xyz;
	
	vtx_world = vec4(r_entOrig, 1.0);
}
#endif


#ifdef FRAGMENTSHADER
uniform sampler2D diffuse;

struct LightParameters {
	vec3 origin[32];
	vec3 color[32];
	float radius[32];
};
uniform LightParameters Lights;
uniform int r_maxLights;

out vec4 fragColor;

// lighting accumulator for diffuse
vec3 diffLight = vec3(1.0);

// iterate dynamic lights
void DynamicLighting(vec3 normal)
{
	const float fillStrength = 0.2;
	for (int i = 0; i < r_maxLights; ++i)
	{	
		// light has no radius, just skip it
		if(Lights.radius[i] == 0.0)
			continue;
			
		vec3 delta = Lights.origin[i] - vtx_world.xyz;
		float distance = length(delta);
			
		// don't shade points outside of the radius
		if (distance < Lights.radius[i])
		{		
			// get angle
			vec3 lightDir = normalize(delta);

			// make distance relative to radius
			distance = 1.0 - distance / Lights.radius[i];

			// calculate diffuse light
			diffLight.rgb += LambertFill(lightDir, normal, fillStrength, Lights.color[i]) * distance * distance;
		}
	}
}

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
	
	// get dynamic lights
	DynamicLighting(normal);
	
	// set diffuse
	vec4 diffuseTerm = vec4(diffAlbedo.rgb * diffLight.rgb, 1.0) * shade;

	// output final color
	fragColor = mix (diffuseTerm, shadelight, meshshellmix);
	
	// tonemap final color
	fragColor.rgb = ToneMap_ACESFilmic (fragColor.rgb);
}
#endif

