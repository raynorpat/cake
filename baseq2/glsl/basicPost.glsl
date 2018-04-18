
// common
uniform vec4 warpparams;

INOUTTYPE vec4 texcoords[2];

#ifdef VERTEXSHADER
uniform mat4 orthomatrix;

layout(location = 0) in vec4 position;
layout(location = 2) in vec4 texcoord;

void BasicPostVS ()
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
uniform vec2 brightnessContrastAmount;
uniform vec2 rescale;

out vec4 fragColor;

void BasicPostFS ()
{
	vec2 st = gl_FragCoord.st * r_FBufScale;
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
		scene = GammaToLinearSpace(texture(diffuse, st));
	}
	
	// tonemap using filmic tonemapping curve
	//scene.rgb = ToneMap_ACESFilmic (scene.rgb);
	
	// brightness
	scene.rgb = doBrightnessAndContrast(scene.rgb, brightnessContrastAmount.x, brightnessContrastAmount.y);

	// convert back out to gamma space
	vec4 finalColor = LinearToGammaSpace(scene);
	
	// mix scene with possible modulation (eg item pickups, getting shot, etc)
	finalColor = mix(finalColor, surfcolor, surfcolor.a);
	
	// send it out to the screen
	fragColor = finalColor;
}
#endif
