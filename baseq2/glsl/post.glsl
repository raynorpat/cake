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
uniform sampler2D diffuse;
uniform sampler2D precomposite;
uniform sampler2D warpgradient;

uniform int waterwarppost;

uniform vec4 surfcolor;
uniform vec2 rescale;
uniform vec2 texScale;

#define USE_TECHNICOLOR						1		// [0 or 1]
#define Technicolor_Amount					0.5		// [0.00 to 1.00]
#define Technicolor_Power					4.0		// [0.00 to 8.00]
#define Technicolor_RedNegativeAmount		0.88	// [0.00 to 1.00]
#define Technicolor_GreenNegativeAmount		0.88	// [0.00 to 1.00]
#define Technicolor_BlueNegativeAmount		0.88	// [0.00 to 1.00]

#define USE_VIBRANCE						1
#define Vibrance							0.75	// [-1.00 to 1.00]
#define	Vibrance_RGB_Balance				vec3( 1.0, 1.0, 1.0 )

#define USE_FILMGRAIN 						1

void TechnicolorPass( inout vec4 color )
{
	const vec3 cyanFilter = vec3( 0.0, 1.30, 1.0 );
	const vec3 magentaFilter = vec3( 1.0, 0.0, 1.05 );
	const vec3 yellowFilter = vec3( 1.6, 1.6, 0.05 );
	const vec3 redOrangeFilter = vec3( 1.05, 0.62, 0.0 );
	const vec3 greenFilter = vec3( 0.3, 1.0, 0.0 );
	
	vec2 redNegativeMul   = color.rg * ( 1.0 / ( Technicolor_RedNegativeAmount * Technicolor_Power ) );
	vec2 greenNegativeMul = color.rg * ( 1.0 / ( Technicolor_GreenNegativeAmount * Technicolor_Power ) );
	vec2 blueNegativeMul  = color.rb * ( 1.0 / ( Technicolor_BlueNegativeAmount * Technicolor_Power ) );
	
	float redNegative   = dot( redOrangeFilter.rg, redNegativeMul );
	float greenNegative = dot( greenFilter.rg, greenNegativeMul );
	float blueNegative  = dot( magentaFilter.rb, blueNegativeMul );
	
	vec3 redOutput   = vec3( redNegative ) + cyanFilter;
	vec3 greenOutput = vec3( greenNegative ) + magentaFilter;
	vec3 blueOutput  = vec3( blueNegative ) + yellowFilter;
	
	vec3 result = redOutput * greenOutput * blueOutput;
	color.rgb = mix( color.rgb, result, Technicolor_Amount );
}

void VibrancePass( inout vec4 color )
{
	vec3 vibrance = Vibrance_RGB_Balance * Vibrance;
	
	float Y = dot( vec4(0.2125, 0.7154, 0.0721, 0.0), color );
	
	float minColor = min( color.r, min( color.g, color.b ) );
	float maxColor = max( color.r, max( color.g, color.b ) );

	float colorSat = maxColor - minColor;
	
	color.rgb = mix( vec3( Y ), color.rgb, ( 1.0 + ( vibrance * ( 1.0 - ( sign( vibrance ) * colorSat ) ) ) ) );
}

void FilmgrainPass( inout vec4 color )
{
	vec2 uv = gl_FragCoord.st * texScale;
    float strength = 16.0;
    
    float x = (uv.x + 4.0 ) * (uv.y + 4.0 ) * (waterwarpParam.x * 10.0);
	vec4 grain = vec4(mod((mod(x, 13.0) + 1.0) * (mod(x, 123.0) + 1.0), 0.01)-0.005) * strength;
    
   	grain = 1.0 - grain;
	color *= grain;
}


vec4 cubic(float x)
{
    float x2 = x * x;
    float x3 = x2 * x;
    vec4 w;
    w.x =   -x3 + 3.0*x2 - 3.0*x + 1.0;
    w.y =  3.0*x3 - 6.0*x2       + 4.0;
    w.z = -3.0*x3 + 3.0*x2 + 3.0*x + 1.0;
    w.w =  x3;
    return w / 6.0;
}

vec4 BicubicTexture(in sampler2D tex, in vec2 coord)
{
	vec2 resolution = texScale.xy;

	coord *= resolution;

	float fx = fract(coord.x);
    float fy = fract(coord.y);
    coord.x -= fx;
    coord.y -= fy;

    fx -= 0.5;
    fy -= 0.5;

    vec4 xcubic = cubic(fx);
    vec4 ycubic = cubic(fy);

    vec4 c = vec4(coord.x - 0.5, coord.x + 1.5, coord.y - 0.5, coord.y + 1.5);
    vec4 s = vec4(xcubic.x + xcubic.y, xcubic.z + xcubic.w, ycubic.x + ycubic.y, ycubic.z + ycubic.w);
    vec4 offset = c + vec4(xcubic.y, xcubic.w, ycubic.y, ycubic.w) / s;

    vec4 sample0 = texture(tex, vec2(offset.x, offset.z) / resolution);
    vec4 sample1 = texture(tex, vec2(offset.y, offset.z) / resolution);
    vec4 sample2 = texture(tex, vec2(offset.x, offset.w) / resolution);
    vec4 sample3 = texture(tex, vec2(offset.y, offset.w) / resolution);

    float sx = s.x / (s.x + s.y);
    float sy = s.z / (s.z + s.w);

    return mix( mix(sample3, sample2, sx), mix(sample1, sample0, sx), sy);
}

vec3 BloomFetch(vec2 coord)
{
 	return BicubicTexture(diffuse, coord).rgb;   
}

vec3 Grab(vec2 coord, const float octave, const vec2 offset)
{
 	float scale = exp2(octave);
    
    coord /= scale;
    coord -= offset;

    return BloomFetch(coord);
}

vec2 CalcOffset(float octave)
{
    vec2 offset = vec2(0.0);    
    vec2 padding = vec2(10.0) / texScale.xy;
    
    offset.x = -min(1.0, floor(octave / 3.0)) * (0.25 + padding.x);    
    offset.y = -(1.0 - (1.0 / exp2(octave))) - padding.y * octave;
	offset.y += min(1.0, floor(octave / 3.0)) * 0.35;
    
 	return offset;   
}

vec3 GetBloom(vec2 coord)
{
 	vec3 bloom = vec3(0.0);
    
    //Reconstruct bloom from multiple blurred images
    bloom += Grab(coord, 1.0, vec2(CalcOffset(0.0))) * 1.0;
    bloom += Grab(coord, 2.0, vec2(CalcOffset(1.0))) * 1.5;
	bloom += Grab(coord, 3.0, vec2(CalcOffset(2.0))) * 1.0;
    bloom += Grab(coord, 4.0, vec2(CalcOffset(3.0))) * 1.5;
    bloom += Grab(coord, 5.0, vec2(CalcOffset(4.0))) * 1.8;
    bloom += Grab(coord, 6.0, vec2(CalcOffset(5.0))) * 1.0;
    bloom += Grab(coord, 7.0, vec2(CalcOffset(6.0))) * 1.0;
    bloom += Grab(coord, 8.0, vec2(CalcOffset(7.0))) * 1.0;

	return bloom;
}

out vec4 fragColor;

void PostFS ()
{
	vec2 st = gl_FragCoord.st * texScale;
	vec4 hdrScene = texture (diffuse, st);
	vec4 scene;

	// mix in water warp post effect first
	if (waterwarppost == 1)
	{
		vec4 distort1 = texture (warpgradient, texcoords[1].yx);
		vec4 distort2 = texture (warpgradient, texcoords[1].xy);
		vec2 warpcoords = texcoords[0].xy + (distort1.ba + distort2.ab);

		scene = texture (precomposite, warpcoords * rescale);
	}
	else
	{
		scene = texture (precomposite, st);
	}
	
	// then mix in the previously generated bloomed HDR scene
	vec4 color = scene + hdrScene;
	
	// technicolor post processing
#if USE_TECHNICOLOR
	TechnicolorPass( color );
#endif
	
	// RGB vibrance post processing
#if USE_VIBRANCE
	VibrancePass( color );
#endif

	// film grain effect
#if USE_FILMGRAIN
	FilmgrainPass( color );
#endif

	// mix scene with possible modulation (eg item pickups, getting shot, etc)
	fragColor = mix(color, surfcolor, surfcolor.a);
}
#endif

