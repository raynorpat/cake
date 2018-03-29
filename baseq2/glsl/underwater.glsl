
// common
uniform vec4 warpparams;

INOUTTYPE vec4 texcoords[2];

#ifdef VERTEXSHADER
uniform mat4 orthomatrix;

layout(location = 0) in vec4 position;
layout(location = 2) in vec4 texcoord;

void WaterWarpVS ()
{
	gl_Position = orthomatrix * position;
	texcoords[0] = texcoord;
	texcoords[1] = ((texcoord * warpparams.y) + warpparams.x) * warpparams.z * warpparams.w;
}
#endif


#ifdef FRAGMENTSHADER
uniform sampler2D diffuse;
uniform sampler2D gradient;

uniform vec4 surfcolor;
uniform int waterwarppost;
uniform vec2 texScale;
uniform vec2 brightnessContrastAmount;
uniform vec2 rescale;

out vec4 fragColor;

void WaterWarpFS ()
{
	vec2 st = gl_FragCoord.st * texScale;
	vec4 scene;

	// mix in water warp post effect
	if (waterwarppost == 1)
	{
		vec4 distort1 = texture (gradient, texcoords[1].yx);
		vec4 distort2 = texture (gradient, texcoords[1].xy);
		vec2 warpcoords = texcoords[0].xy + (distort1.ba + distort2.ab);
		scene = texture(diffuse, warpcoords * rescale);
	}
	else
	{
		scene = texture(diffuse, st);
	}
	
	// mix scene with possible modulation (eg item pickups, getting shot, etc)
	scene = mix(scene, surfcolor, surfcolor.a);

	// brightness
	scene.rgb = doBrightnessAndContrast(scene.rgb, brightnessContrastAmount.x, brightnessContrastAmount.y);

	// output
	fragColor = scene;
}
#endif
