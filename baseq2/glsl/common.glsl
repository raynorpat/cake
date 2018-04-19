
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

// ACES color system tonemap
// - originally written by Stephen Hill (@self_shadow), who deserves all credit for coming up with this fit and implementing it. 

// sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
const mat3 ACESInputMat = mat3(
    0.59719, 0.35458, 0.04823,
    0.07600, 0.90834, 0.01566,
    0.02840, 0.13383, 0.83777
);

// ODT_SAT => XYZ => D60_2_D65 => sRGB
const mat3 ACESOutputMat = mat3(
     1.60475, -0.53108, -0.07367,
    -0.10208,  1.10813, -0.00605,
    -0.00327, -0.07276,  1.07602
);

vec3 RRTAndODTFit(vec3 v)
{
    vec3 a = v * (v + 0.0245786f) - 0.000090537f;
    vec3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
    return a / b;
}

vec3 ToneMap_ACES(vec3 x)
{
	// convert to sRGB
	x = LinearToSRGBSpace(x);
	
	// get into ACES color space
    x = x * ACESInputMat;

    // apply RRT and ODT
    x = RRTAndODTFit(x);
	
	// output back into sRGB
    x = x * ACESOutputMat;

    // clamp to [0, 1]
    x = saturate(x);
	
	// convert to linear RGB
	return SRGBToLinearSpace(x);
}
