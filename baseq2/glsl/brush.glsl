
// common
uniform float scroll;

INOUTTYPE vec4 vtx_texcoords[2];
INOUTTYPE vec4 vtx_world;
INOUTTYPE vec4 vtx_normal;

#ifdef VERTEXSHADER
uniform mat4 localMatrix;
uniform mat4 lightMatrix;

layout(location = 0) in vec4 position;
layout(location = 1) in vec4 diffuse;
layout(location = 2) in vec4 lightmap;
layout(location = 3) in vec4 normals;

void LightmappedVS ()
{
	gl_Position = localMatrix * position;
	vtx_texcoords[0] = diffuse + (vec4 (scroll, 0, 0, 0) * diffuse.z);
	vtx_texcoords[1] = lightmap;
	
	vec4 worldCoord = lightMatrix * vec4(position.xyz, 1.0);
	vtx_world = worldCoord;
	
	vec4 worldNormal = lightMatrix * vec4(normals.xyz, 0.0f);
	vtx_normal = normalize(worldNormal);
}
#endif


#ifdef FRAGMENTSHADER
uniform sampler2D diffuse;
uniform sampler2DArray lightmap;
uniform float surfalpha;
uniform mat4 colormatrix;

struct LightParameters {
	vec3 origin[32];
	vec3 color[32];
	float radius[32];
};
uniform LightParameters Lights;
uniform int maxLights;
uniform vec4 debugParams; // x = gl_showlightmaps, y = gl_shownormals, z = , w =

out vec4 fragColor;

// lighting accumulator for diffuse
vec3 diffLight = vec3(0.0);

// iterate dynamic lights
void DynamicLighting(vec3 normal)
{
	for (int i = 0; i < maxLights; ++i)
	{	
		// light has no radius, just skip it
		if(Lights.radius[i] == 0.0)
			continue;
			
		vec3 delta = Lights.origin[i] - vtx_world.xyz;
		float distance = length(delta);
			
		// don't shade points outside of the radius
		if (distance > Lights.radius[i])
			continue;
				
		// get angle
		vec3 lightDir = normalize(delta);
			
		// make distance relative to radius
		distance = 1.0 - distance / Lights.radius[i];
		
		// calculate diffuse light
		diffLight.rgb += Lambert(lightDir, normal, Lights.color[i]) * distance * distance;
	}
}

void LightmappedFS ()
{
	// get alpha
	float alpha = surfalpha;
	
	// get vertex normal
	vec3 normal = normalize (vtx_normal.xyz);

	// get flat diffuse color
	vec4 diffAlbedo = vec4(1.0);
	diffAlbedo = texture (diffuse, vtx_texcoords[0].st);

	// get lightmap
	vec4 lightmap = texture2DArray (lightmap, vtx_texcoords[1].xyz);
	lightmap /= lightmap.a;
	lightmap *= colormatrix;

	// add static lighting
	float lightMask = 1.0;
	diffLight += lightmap.rgb * (lightMask * 0.5 + 0.5);

	// add dynamic lighting
	if (maxLights > 0)
		DynamicLighting (normal);
	
	// set diffuse
	vec3 diffuseTerm = diffAlbedo.rgb * diffLight;
	
	// debug parameters
	if (debugParams.x == 1)
	{
		// lightmaps only
		fragColor.rgb = lightmap.rgb;
	}
	else if (debugParams.y == 1)
	{
		// normals only
		fragColor.rgb = normal.xyz;
	}
	else
	{
		// diffuse term
		fragColor.rgb = diffuseTerm.rgb;
	}
	
	// output tonemapped colors
	fragColor.rgb = ToneMap_ACESFilmic (fragColor.rgb);
	fragColor.a = alpha;
}
#endif

