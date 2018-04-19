
// common.glsl

const float pi = 3.14159265;
const float gamma = 2.2;
const vec4 LUMINANCE_VECTOR = vec4(0.2125, 0.7154, 0.0721, 0.0);

// saturate function from HLSL
float saturate( float v ) { return clamp( v, 0.0, 1.0 ); }
vec3 saturate( vec3 v ) { return clamp( v, 0.0, 1.0 ); }

// brightness function
float Brightness(vec3 c) { return max(max(c.r, c.g), c.b); }

// gamma to linear space conversion
vec3 GammaToLinearSpace(vec3 v) { return pow(v, vec3(gamma)); }
vec4 GammaToLinearSpace(vec4 v) { return vec4(GammaToLinearSpace(v.rgb), v.a); }

// linear to gamma space conversion
vec3 LinearToGammaSpace(vec3 v) { return pow(v, vec3(1.0 / gamma)); }
vec4 LinearToGammaSpace(vec4 v) { return vec4(LinearToGammaSpace(v.rgb), v.a); }

// sRGB space conversions - http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
vec3 SRGBToLinearSpace(vec3 v) { return v * (v * (v * 0.305306011 + 0.682171111) + 0.012522878); }
vec4 SRGBToLinearSpace(vec4 v) { return vec4(SRGBToLinearSpace(v.rgb), v.a); }

vec3 LinearToSRGBSpace(vec3 v) { return max(1.055 * pow(v, vec3(0.416666667)) - 0.055, 0.0); }
vec4 LinearToSRGBSpace(vec4 v) { return vec4(LinearToSRGBSpace(v.rgb), v.a); }
	
// brightness and contrast control
vec3 doBrightnessAndContrast(vec3 value, float brightnessValue, float contrastValue)
{
	vec3 color = value.rgb * contrastValue;
	return vec3(pow(abs(color.rgb), vec3(brightnessValue)));
}

// generic filmic tonemap by Timothy Lottes
//  - http://32ipi028l5q82yhj72224m8j-wpengine.netdna-ssl.com/wp-content/uploads/2016/03/GdcVdrLottes.pdf
vec3 ToneMap_Generic (vec3 x, float hdrMax)
{
	float a = 1.25; // contrast
    float d = 0.97; // shoulder
    float mid_in = 0.3;
	float mid_out = 0.18;
	
	vec3 crosstalk = vec3(64.0, 32.0, 128.0); // controls amount of channel crosstalk
	vec3 saturation = vec3(0.0, 0.0, 0.0); // full tonal range saturation control
	vec3 crossSaturation = vec3(4.0, 1.0, 16.0); // crosstalk saturation

	float ad = a * d; // contrast * shoulder

	float midi_pow_a  = pow(mid_in, a);
	float midi_pow_ad = pow(mid_in, ad);
	float hdrm_pow_a  = pow(hdrMax, a);
	float hdrm_pow_ad = pow(hdrMax, ad);
	float u = hdrm_pow_ad * mid_out - midi_pow_ad * mid_out;
	float v = midi_pow_ad * mid_out;

	float b = -((-midi_pow_a + (mid_out * (hdrm_pow_ad * midi_pow_a - hdrm_pow_a * v)) / u) / v); // power, adjusts compression
    float c = (hdrm_pow_ad * midi_pow_a - hdrm_pow_a * v) / u; // speed of compression
		
	// saturation base is contrast
	saturation += a;
	
	// peak of all color channels
	float peakColor = max(max(x.r, x.g), x.b);
	peakColor = max(peakColor, 1.0 / (256.0 * 65536.0)); // protect against / 0
	
	// color ratio
	vec3 peakRatio = x / peakColor;
	
	// contrast adjustment
	peakColor = pow(peakColor, a);
	
	// highlight compression
	peakColor = peakColor / (pow(peakColor, d) * b + c);
	
	// convert to non-linear space and saturate
	// saturation is folded into first transform
	peakRatio = pow(peakRatio, (saturation / crossSaturation));
  
	// move towards white on overexposure
	vec3 white = vec3(1.0, 1.0, 1.0);
	peakRatio = mix(peakRatio, white, pow(vec3(peakColor), crosstalk));
	
	// convert back to linear
	peakRatio = pow(peakRatio, crossSaturation);
    return peakRatio * peakColor;
}

// phong BRDF for specular highlights
vec3 Phong(vec3 viewDir, vec3 lightDir, vec3 normal, vec3 lightColor, float specIntensity, float specPower)
{
	vec3 reflection = reflect(lightDir, normal);
	float RdotV = max(-dot(viewDir, reflection), 0.0);
	return lightColor * specIntensity * pow(RdotV, 16.0 * specPower);
}

// blinn-phong BRDF for specular highlights
vec3 Blinn(vec3 viewDir, vec3 lightDir, vec3 normal, vec3 lightColor, float specIntensity, float specPower)
{
	const float exponent = 16.0 * 4.0; // try to roughly match phong highlight
	vec3 halfAngle = normalize(lightDir + viewDir);
	float NdotH = max(dot(normal, halfAngle), 0.0);
	return lightColor * specIntensity * pow(NdotH, exponent * specPower);
}

// lambert BRDF for diffuse lighting
vec3 Lambert(vec3 lightDir, vec3 normal, vec3 lightColor)
{
	return lightColor * max(dot(lightDir, normal), 0.0);
}

// lambert BRDF with fill light for diffuse lighting
vec3 LambertFill(vec3 lightDir, vec3 normal, float fillStrength, vec3 lightColor)
{
	// base
	float NdotL = dot(normal, lightDir);

	// half-lambert soft lighting
	float fill = 1.0 - (NdotL * 0.5 + 0.5);

	// lambert hard lighting
	float direct = max(NdotL, 0.0);

	// hard + soft lighting
	return lightColor * (fill * fillStrength + direct);
}
