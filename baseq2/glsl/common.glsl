
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

float TonemapWhitePoint = 2.0; // linear white point

// ACES color system tonemapper
vec3 ToneMap_ACESFilmic (vec3 x)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;

    return (x * (a * x + b)) / (x * (c * x + d) + e);
}

vec3 Normalized_ToneMap_ACESFilmic (vec3 x)
{
	return ToneMap_ACESFilmic(x) / ToneMap_ACESFilmic(vec3(TonemapWhitePoint));
}

// John Hable's Uncharted 2 tonemapper
vec3 ToneMap_Hable (vec3 x)
{
    const float a = 0.22f;
    const float b = 0.30f;
    const float c = 0.10f;
    const float d = 0.20f;
    const float e = 0.01f;
    const float f = 0.30f;

    return ((x * (a * x + b * c) + d * e) / (x * (a * x + b) + d * f)) - e / f;
}

vec3 Normalized_ToneMap_Hable (vec3 x)
{
	return ToneMap_Hable(x) / ToneMap_Hable(vec3(TonemapWhitePoint));
}

// https://twitter.com/jimhejl/status/633777619998130176?
vec3 ToneMap_Hejl2015 (vec3 x, float whitePt)
{
	vec4 vh = vec4(x, whitePt);
	vec4 va = (1.425 * vh) + 0.05;
	vec4 vf = ((vh * va + 0.004) / ((vh * (va + 0.55) + 0.0491))) - 0.0821;

	return vf.rgb / vf.www;
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
