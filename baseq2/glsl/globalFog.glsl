
// common
INOUTTYPE vec4 texcoords;

#ifdef VERTEXSHADER
uniform mat4 orthomatrix;

layout(location = 0) in vec4 position;
layout(location = 2) in vec4 texcoord;


void GlobalFogVS ()
{
	gl_Position = orthomatrix * position;
	texcoords = texcoord;
}
#endif


#ifdef FRAGMENTSHADER
uniform sampler2D diffuse;
uniform sampler2D depth;

uniform mat4 unprojectmatrix;

uniform vec3 viewOrigin;
uniform vec4 fogColorDensity;

out vec4 fragColor;

void GlobalFogFS ()
{
	vec2 st = gl_FragCoord.st * r_FBufScale;
	
	// reconstruct vertex position in world space
	float zdepth = texture(depth, st).r;
	vec4 P = unprojectmatrix * vec4(gl_FragCoord.xy, zdepth, 1.0);
	P.xyz /= P.w;

	// calculate distance to camera
	float fogDistance = distance(P.xyz, viewOrigin);

	// calculate fog exponent
	float fogExponent = fogDistance * fogColorDensity.a;

	// calculate fog factor
	float fogFactor = exp2(-fogExponent * fogExponent);

	// don't fog the skybox, since it mangles with depth, fog doesn't work
	if (zdepth >= 0.999999)
		fogFactor = 1.0;

	// grab scene and skybox
	vec4 sceneColor = texture(diffuse, st);

	// lerp between scene color and fog color by fog factor
	fragColor = mix(vec4(fogColorDensity.rgb, 1.0), sceneColor, fogFactor);
}
#endif

