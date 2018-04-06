
// common
uniform float scroll;

INOUTTYPE vec4 texcoords[2];
INOUTTYPE vec4 point;
INOUTTYPE vec4 normal;

#ifdef VERTEXSHADER
uniform mat4 localMatrix;
uniform mat4 modelViewMatrix;

layout(location = 0) in vec4 position;
layout(location = 1) in vec4 diffuse;
layout(location = 2) in vec4 lightmap;
layout(location = 3) in vec4 normals;

void LightmappedVS ()
{
	gl_Position = localMatrix * position;
	point = modelViewMatrix * position;
	texcoords[0] = diffuse + (vec4 (scroll, 0, 0, 0) * diffuse.z);
	texcoords[1] = lightmap;
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
uniform float maxLights;

out vec4 fragColor;

void LightmappedFS ()
{
	vec4 lmap = texture2DArray(lightmap, texcoords[1].xyz);
	vec4 albedo = texture(diffuse, texcoords[0].st);
	
	// calculate dynamic light sources
	vec4 light = vec4(0.0);
	for (int i = 0; i < 8; i++)
	{
		if (Lights.radius[i] == 0.0)
			break;
			
		vec3 delta = Lights.origin[i] - point.xyz;
		float len = length(delta);
		if (len < Lights.radius[i])
		{
			//float lambert = dot(normal, normalize(delta));
			//if (lambert > 0.0)
			{
				// windowed inverse square falloff
				float dist = len / Lights.radius[i];
				float falloff = clamp(1.0 - dist * dist * dist * dist, 0.0, 1.0);
				falloff = falloff * falloff;
				falloff = falloff / (dist * dist + 1.0);
				
				light += vec4(1.0, 1.0, 0.0, 1.0); // TESTING!
				light += vec4(Lights.color[i] * falloff, 1.0); // * lambert
			}
		}		
	}

	vec4 final = albedo * (colormatrix * ((lmap / lmap.a) + light));
	
	fragColor = vec4(final.rgb, surfalpha);
}
#endif

