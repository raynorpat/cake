
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

uniform vec2 texScale;
uniform vec3 viewOrigin;
uniform vec4 fogColorDensity;

out vec4 fragColor;

void GlobalFogFS ()
{
	vec2 st = gl_FragCoord.st * texScale;
	
	// reconstruct vertex position in world space
	float zdepth = texture(depth, st).r;
	vec4 P = unprojectmatrix * vec4(gl_FragCoord.xy, zdepth, 1.0);
	P.xyz /= P.w;

	// calculate distance to camera
	float fogDistance = distance(P.xyz, viewOrigin);

	// calculate fog exponent
	float fogExponent = fogDistance * fogColorDensity.a;

	// calculate fog factor
	float fogFactor = exp2(-abs(fogExponent));

	// grab scene
	vec3 currentScene = texture(diffuse, st).rgb;
	
	// compute final color, lerp between fog color and current color by fog factor
	vec4 color;
	color.r = (1.0 - fogFactor) * fogColorDensity.r + currentScene.r * fogFactor;
	color.g = (1.0 - fogFactor) * fogColorDensity.g + currentScene.g * fogFactor;
	color.b = (1.0 - fogFactor) * fogColorDensity.b + currentScene.b * fogFactor;
	color.a = 1.0;
	fragColor = color;
}
#endif

