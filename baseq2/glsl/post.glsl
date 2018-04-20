
uniform vec4 waterwarpParam;

// common
INOUTTYPE vec4 texcoords[2];

#ifdef VERTEXSHADER
uniform mat4 orthomatrix;

layout(location = 0) in vec4 position;
layout(location = 2) in vec4 texcoord;


void PostVS ()
{
	gl_Position = orthomatrix * position;
	texcoords[0] = texcoord;
	texcoords[1] = ((texcoord * waterwarpParam.y) + waterwarpParam.x) * waterwarpParam.z * waterwarpParam.w;
}
#endif


#ifdef FRAGMENTSHADER
uniform sampler2D scene;
uniform sampler2D warpgradient;

uniform int waterwarppost;

uniform vec4 surfcolor;
uniform vec2 brightnessContrastAmount;
uniform vec2 rescale;

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

void PostFS ()
{
	vec2 st = gl_FragCoord.st * r_FBufScale;
	vec4 sceneColor;

	// mix in water warp post effect
	if (waterwarppost == 1)
	{
		vec4 distort1 = texture (warpgradient, texcoords[1].yx);
		vec4 distort2 = texture (warpgradient, texcoords[1].xy);
		vec2 warpcoords = texcoords[0].xy + (distort1.ba + distort2.ab);

		sceneColor = texture(scene, warpcoords * rescale);
	}
	else
	{
		sceneColor = texture(scene, st);
	}
	
	// film grain effect
#if r_useFilmgrain
	FilmgrainPass(sceneColor);
#endif	

	// mix scene with possible modulation (eg item pickups, getting shot, etc)
	vec4 screenBlend = LinearRGBToSRGB(surfcolor);
	sceneColor = mix(sceneColor, screenBlend, screenBlend.a);

	// filmic vignette effect
#if r_useVignette
	vec2 vignetteST = st;
    vignetteST *= 1.0 - vignetteST.yx;
    float vig = vignetteST.x * vignetteST.y * 15.0;
    vig = pow(vig, 0.25);
	sceneColor.rgb *= vig;
#endif

	// brightness
	sceneColor.rgb = doBrightnessAndContrast(sceneColor.rgb, brightnessContrastAmount.x, brightnessContrastAmount.y);
		
	// send it out to the screen
	fragColor = sceneColor;
}
#endif
