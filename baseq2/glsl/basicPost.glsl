
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

void FilmgrainPass( inout vec4 color )
{
	vec2 uv = gl_FragCoord.st * r_FBufScale;
    float strength = 16.0;
    
    float x = (uv.x + 4.0) * (uv.y + 4.0) * (warpparams.x * 10.0);
	vec4 grain = vec4(mod((mod(x, 13.0) + 1.0) * (mod(x, 123.0) + 1.0), 0.01)-0.005) * strength;
    
   	grain = 1.0 - grain;
	color *= grain;
}

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
		scene = texture(diffuse, st);
	}
		
	// brightness
	//scene.rgb = doBrightnessAndContrast(scene.rgb, brightnessContrastAmount.x, brightnessContrastAmount.y);
	
	// tonemap using filmic tonemapping curve
#if r_useTonemap	
	scene.rgb = ToneMap_ACES (scene.rgb * 1.8);
#endif

	// filmic vignette effect
#if r_useVignette
	vec2 vignetteST = st;
    vignetteST *= 1.0 - vignetteST.yx;
    float vig = vignetteST.x * vignetteST.y * 15.0;
    vig = pow(vig, 0.25);
	scene.rgb *= vig;
#endif

	// film grain effect
#if r_useFilmgrain
	FilmgrainPass(scene);
#endif

	// mix scene with possible modulation (eg item pickups, getting shot, etc)
	scene = mix(scene, surfcolor, surfcolor.a);
	
	// and send it out to the screen
	fragColor = scene;
}
#endif
