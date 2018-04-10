
// common
INOUTTYPE vec4 texcoords;

#ifdef VERTEXSHADER
uniform mat4 orthomatrix;

layout(location = 0) in vec4 position;
layout(location = 2) in vec4 texcoord;


void CompositeVS ()
{
	gl_Position = orthomatrix * position;
	texcoords = texcoord;
}
#endif


#ifdef FRAGMENTSHADER
uniform sampler2D diffuse;
uniform vec2 rescale;

uniform int compositeMode;
uniform vec2 texScale;
uniform vec4 brightParam;

out vec4 fragColor;

const float strength = 0.9;
const float ditherGamma = 0.79;
const float brightness = 0.6;

const float orig = 1.0 - strength;

float luma(vec4 rgba)
{
	const vec3 W = vec3(0.2125, 0.7154, 0.0721);
    return dot(rgba.xyz, W);
}

float dither4x4(vec2 position, float lum)
{
  int x = int(mod(position.x, 4.0));
  int y = int(mod(position.y, 4.0));
  int index = x + y * 1;
  float limit = 0.0;

  if (x < 4)
  {
    if (index == 0) limit = 0.015625;
    if (index == 1) limit = 0.515625;
    if (index == 2) limit = 0.140625;
    if (index == 3) limit = 0.640625;
    if (index == 4) limit = 0.046875;
    if (index == 5) limit = 0.546875;
    if (index == 6) limit = 0.171875;
    if (index == 7) limit = 0.671875;
    if (index == 8) limit = 0.765625;
    if (index == 9) limit = 0.265625;
    if (index == 10) limit = 0.890625;
    if (index == 11) limit = 0.390625;
    if (index == 12) limit = 0.796875;
    if (index == 13) limit = 0.296875;
    if (index == 14) limit = 0.921875;
    if (index == 15) limit = 0.421875;
    if (index == 16) limit = 0.203125;
    if (index == 17) limit = 0.703125;
    if (index == 18) limit = 0.078125;
    if (index == 19) limit = 0.578125;
    if (index == 20) limit = 0.234375;
    if (index == 21) limit = 0.734375;
    if (index == 22) limit = 0.109375;
    if (index == 23) limit = 0.609375;
    if (index == 24) limit = 0.953125;
    if (index == 25) limit = 0.453125;
    if (index == 26) limit = 0.828125;
    if (index == 27) limit = 0.328125;
    if (index == 28) limit = 0.984375;
    if (index == 29) limit = 0.484375;
    if (index == 30) limit = 0.859375;
    if (index == 31) limit = 0.359375;
    if (index == 32) limit = 0.0625;
    if (index == 33) limit = 0.5625;
    if (index == 34) limit = 0.1875;
    if (index == 35) limit = 0.6875;
    if (index == 36) limit = 0.03125;
    if (index == 37) limit = 0.53125;
    if (index == 38) limit = 0.15625;
    if (index == 39) limit = 0.65625;
    if (index == 40) limit = 0.8125;
    if (index == 41) limit = 0.3125;
    if (index == 42) limit = 0.9375;
    if (index == 43) limit = 0.4375;
    if (index == 44) limit = 0.78125;
    if (index == 45) limit = 0.28125;
    if (index == 46) limit = 0.90625;
    if (index == 47) limit = 0.40625;
    if (index == 48) limit = 0.25;
    if (index == 49) limit = 0.75;
    if (index == 50) limit = 0.125;
    if (index == 51) limit = 0.625;
    if (index == 52) limit = 0.21875;
    if (index == 53) limit = 0.71875;
    if (index == 54) limit = 0.09375;
    if (index == 55) limit = 0.59375;
    if (index == 56) limit = 1.0;
    if (index == 57) limit = 0.5;
    if (index == 58) limit = 0.875;
    if (index == 59) limit = 0.375;
    if (index == 60) limit = 0.96875;
    if (index == 61) limit = 0.46875;
    if (index == 62) limit = 0.84375;
    if (index == 63) limit = 0.34375;
  }

  return lum < limit ? 0.0 : brightness;
}

vec4 dither8x8(vec2 position, vec4 color)
{
    float l = luma(color);
	l = pow(l, ditherGamma);
	l -= 1.0/255.0;
    
    return vec4(color.rgb * dither4x4(position, l), 1.0);
}

void CompositeFS ()
{
	vec4 color = vec4(0.0);
	vec2 st = gl_FragCoord.st * texScale;
		
	// downsampled brightness-pass
	if(compositeMode == 1)
	{
		// set constants
		float hdrLinearThreshold = brightParam.x;
		float hdrKnee = hdrLinearThreshold * brightParam.y + 1e-5;
		vec3 hdrCurve = vec3(hdrLinearThreshold - hdrKnee, hdrKnee * 2.0, 0.25f / hdrKnee);
		
		// multiply with 4 because the FBO is only 1/4th of the screen resolution
		st *= vec2(4.0, 4.0);
		
		// grab scene
		color = texture(diffuse, st);
		vec3 linearColor = GammaToLinearSpace(color.rgb);
		
		// grab brightness
		float br = Brightness(linearColor);
		
		// set under-threshold quadratic curve
		float rq = clamp(br - hdrCurve.x, 0, hdrCurve.y);
		rq = hdrCurve.z * rq * rq;

		// combine and apply the brightness response curve.
		linearColor *= max(rq, br - hdrLinearThreshold) / max(br, 1e-5);
	
		fragColor = vec4(linearColor.rgb, 1.0);
		return;
	}
	// horizontal gaussian blur pass
	else if(compositeMode == 2)
	{
		float gaussFact[7] = float[7](1.0, 6.0, 15.0, 20.0, 15.0, 6.0, 1.0);
		float gaussSum = 64.0;
		const int tap = 3;
		
		// do a full gaussian blur
		for(int t = -tap; t <= tap; t++)
		{
			float weight = gaussFact[t + tap];
			color += texture(diffuse, st + vec2(t, 0) * texScale) * weight;
		}

		fragColor = color * (1.0 / gaussSum);
		return;
	}
	// vertical gaussian blur pass
	else if(compositeMode == 3)
	{
		float gaussFact[7] = float[7](1.0, 6.0, 15.0, 20.0, 15.0, 6.0, 1.0);
		float gaussSum = 64.0;
		const int tap = 3;
	
		// do a full gaussian blur
		for(int t = -tap; t <= tap; t++)
		{
			float weight = gaussFact[t + tap];
			color += texture(diffuse, st + vec2(0, t) * texScale) * weight;
		}

		fragColor = color * (1.0 / gaussSum);
		return;
	}
	// dither pass for menu, loading, and paused
	else if(compositeMode == 4)
	{	
		// grab scene
		color = texture (diffuse, st);
		
		// dither scene
		vec4 colorDither = strength * dither8x8(gl_FragCoord.st, color);
		
		// small vignette
		float OuterVignetting = 1.4 - 0.75;
		float InnerVignetting = 1.0 - 0.75;
		float d = distance(vec2(0.5, 0.5), st) * 1.0;
		float vignetting = clamp((OuterVignetting - d) / (OuterVignetting - InnerVignetting), 0.0, 1.0);

		// output
		fragColor = colorDither * vignetting;
		return;
	}
}
#endif

