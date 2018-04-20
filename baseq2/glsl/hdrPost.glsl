
uniform vec4 waterwarpParam;

// common
INOUTTYPE vec4 texcoords[2];

#ifdef VERTEXSHADER
uniform mat4 orthomatrix;

layout(location = 0) in vec4 position;
layout(location = 2) in vec4 texcoord;


void HDRPostVS ()
{
	gl_Position = orthomatrix * position;
	texcoords[0] = texcoord;
	texcoords[1] = ((texcoord * waterwarpParam.y) + waterwarpParam.x) * waterwarpParam.z * waterwarpParam.w;
}
#endif


#ifdef FRAGMENTSHADER
uniform sampler2D precomposite;
uniform sampler2D warpgradient;
uniform sampler2D lumTex;
uniform sampler2D AOTex;

uniform float exposure;
uniform int waterwarppost;

uniform vec4 surfcolor;
uniform vec4 brightnessContrastBlurSSAOAmount;
uniform vec2 rescale;

vec3 ToneMap(vec3 c, float avglum)
{
	float newExposure = 0.0;
	float exposureOffset = exposure;
	
	// calculation from: Perceptual Effects in Real-time Tone Mapping - Krawczyk et al.
	float hdrKey = 1.03 - (2.0 / (2.0 + (avglum + 1.0f)));

	// calculate exposure from geometric mean of luminance
	float avgLuminance = max( avglum, 0.00001 );
	float linearExposure = ( hdrKey / avgLuminance );
	newExposure = log2( max( linearExposure, 0.0001 ) );

	// add offset to exposure
	newExposure += exposureOffset;
	
	// exposure curves ranges from 0.0625 to 16.0
	vec3 exposedColor = exp2( newExposure ) * c.rgb;

	// tonemap
	return LinearRGBToSRGB( ToneMap_ACES(exposedColor) * 1.8 );
}

void FilmgrainPass( inout vec4 color )
{
	vec2 uv = gl_FragCoord.st * r_FBufScale;
    float strength = 16.0;
    
    float x = (uv.x + 4.0) * (uv.y + 4.0) * (waterwarpParam.x * 10.0);
	vec4 grain = vec4(mod((mod(x, 13.0) + 1.0) * (mod(x, 123.0) + 1.0), 0.01)-0.005) * strength;
    
   	grain = 1.0 - grain;
	color *= grain;
}

out vec4 fragColor;

void HDRPostFS ()
{
	vec2 st = gl_FragCoord.st * r_FBufScale;
	vec4 scene;

	// mix in water warp post effect
	if (waterwarppost == 1)
	{
		vec4 distort1 = texture (warpgradient, texcoords[1].yx);
		vec4 distort2 = texture (warpgradient, texcoords[1].xy);
		vec2 warpcoords = texcoords[0].xy + (distort1.ba + distort2.ab);

		scene = texture(precomposite, warpcoords * rescale);
	}
	else
	{
		scene = texture(precomposite, st);
	}
	
	// multiply scene with ambient occlusion
	//if (brightnessContrastBlurSSAOAmount.w > 0)
	//{
	//	vec4 AOScene = texture(AOTex, st);
	//	scene *= AOScene;
	//}

	// film grain effect
#if r_useFilmgrain
	FilmgrainPass(scene);
#endif	

	// brightness
	//scene.rgb = doBrightnessAndContrast(scene.rgb, brightnessContrastBlurSSAOAmount.x, brightnessContrastBlurSSAOAmount.y);

	// mix scene with possible modulation (eg item pickups, getting shot, etc)
	scene = mix(scene, surfcolor, surfcolor.a);
	
	// filmic vignette effect
#if r_useVignette
	vec2 vignetteST = st;
    vignetteST *= 1.0 - vignetteST.yx;
    float vig = vignetteST.x * vignetteST.y * 15.0;
    vig = pow(vig, 0.25);
	scene.rgb *= vig;
#endif
	
	// tonemap using filmic tonemapping curve
#if r_useTonemap
	vec3 luminance = texture(lumTex, vec2(0.0, 0.0)).rgb; // retrieves the log-average luminance texture 
	scene.rgb = ToneMap(scene.rgb, luminance.r);
#endif

	// send it out to the screen
	fragColor = scene;
}
#endif
