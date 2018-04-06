
// common
uniform float scroll;

INOUTTYPE vec4 texcoords[2];
INOUTTYPE vec4 world;
INOUTTYPE vec4 normal;

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
	texcoords[0] = diffuse + (vec4 (scroll, 0, 0, 0) * diffuse.z);
	texcoords[1] = lightmap;
	
	vec4 worldCoord = lightMatrix * vec4(position.xyz, 1.0);
	world = worldCoord;
	
	vec4 worldNormal = lightMatrix * vec4(normals.xyz, 0.0f);
	normal = normalize(worldNormal);
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
uniform int lightBit;

out vec4 fragColor;

void LightmappedFS ()
{
	vec4 lmap = texture2DArray(lightmap, texcoords[1].xyz);
	vec4 albedo = texture(diffuse, texcoords[0].st);
	
	// calculate dynamic light sources
	vec4 light = vec4(0.0);
	if(lightBit != 0)
	{
		for (int i = 0; i < maxLights; ++i)
		{
			// light does not affect this plane, just skip it
			// FIXME: this kills a ton of otherwise correct lights
			//if((lightBit & (1 << i)) == 0)
			//	continue;
			
			// light has no radius, just skip it
			if(Lights.radius[i] == 0.0)
				continue;
			
			vec3 lightToPos = Lights.origin[i] - world.xyz;
			float distLightToPos = length(lightToPos);
			if (distLightToPos < Lights.radius[i])
			{
				float lambert = dot(normal.xyz, normalize(lightToPos));
				if (lambert > 0.0)
				{
					// clamped inverse square falloff
					float dist = distLightToPos / Lights.radius[i];
					float falloff = clamp(1.0 - dist * dist * dist * dist, 0.0, 1.0);
					falloff *= falloff;
					falloff /= (dist * dist + 1.0);
					
					light += vec4(Lights.color[i] * falloff * lambert, 1.0);
				}
			}
		}
	}

	// calculate final color = albedo * (gl_monolightmap values * ((lightmap rgb + dyn light) / lightmap whitepoint))
	vec4 final = albedo * (colormatrix * ((lmap + light) / lmap.a));
	
	fragColor = vec4(final.rgb, surfalpha);
}
#endif

