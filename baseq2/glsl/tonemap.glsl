
// common
INOUTTYPE vec4 texcoords;

#ifdef VERTEXSHADER
uniform mat4 orthomatrix;

layout(location = 0) in vec4 position;
layout(location = 2) in vec4 texcoord;


void TonemapVS ()
{
	gl_Position = orthomatrix * position;
	texcoords = texcoord;
}
#endif


#ifdef FRAGMENTSHADER
uniform sampler2D scene;
uniform sampler2D sceneLum;

uniform float r_hdrKey;

#if USE_COMPUTE_LUM
vec3 ToneMapWithLum(vec3 x, float avglum)
{
	float avgLuminance = max( avglum, 0.00001 );
	float hdrKey = r_hdrKey;

	// calculate exposure from geometric mean of luminance
	float linearExposure = hdrKey / avgLuminance;
	float newExposure = log2( max( linearExposure, 0.00001 ) );
	
	// exposure curves ranges from 0.0625 to 16.0
	vec3 exposedColor = exp2( newExposure ) * x.rgb;

	// tonemap
	return LinearRGBToSRGB( ToneMap_ACES(exposedColor) * 1.8 );
}
#endif

out vec4 fragColor;

void TonemapFS ()
{
	vec2 st = gl_FragCoord.st * r_FBufScale;
	vec4 sceneColor = texture(scene, st);
	
	// filmic vignette effect
#if r_useVignette
	vec2 vignetteST = st;
    vignetteST *= 1.0 - vignetteST.yx;
    float vig = vignetteST.x * vignetteST.y * 15.0;
    vig = pow(vig, 0.25);
	sceneColor.rgb *= vig;
#endif

	// tonemap using filmic tonemapping curve
#if r_useTonemap
	#if USE_COMPUTE_LUM
		float luminance = sRGBAToLinearRGBA(texture(sceneLum, vec2(0.0, 0.0))).r; // retrieves the log-average luminance texture 
		sceneColor.rgb = ToneMapWithLum(sceneColor.rgb, luminance);
	#else
		sceneColor.rgb = LinearRGBToSRGB(ToneMap_ACES(sceneColor.rgb) * 1.8);
	#endif
#endif

	// send it out to the screen
	fragColor = sceneColor;
}
#endif
