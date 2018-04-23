
// common.glsl

const float pi = 3.14159265;

// saturate function from HLSL
float saturate( float v ) { return clamp( v, 0.0, 1.0 ); }
vec2 saturate( vec2 v ) { return clamp( v, 0.0, 1.0 ); }
vec3 saturate( vec3 v ) { return clamp( v, 0.0, 1.0 ); }
vec4 saturate( vec4 v ) { return clamp( v, 0.0, 1.0 ); }

// brightness function
float Brightness(vec3 c) { return max(max(c.r, c.g), c.b); }

// sRGB <-> Linear RGB Color Conversions
vec3 sRGBToLinearRGB( vec3 rgb )
{
#if !defined( USE_SRGB )
	return max( pow( rgb, vec3( 2.2 ) ), vec3( 0.0 ) );
#else
	return rgb;
#endif
}
vec4 sRGBAToLinearRGBA( vec4 rgba )
{
#if !defined( USE_SRGB )
	return vec4( max( pow( rgba.rgb, vec3( 2.2 ) ), vec3( 0.0 ) ), rgba.a );
#else
	return rgba;
#endif
}

vec3 LinearRGBToSRGB( vec3 rgb )
{
#if !defined( USE_SRGB )
	return pow( rgb, vec3( 1.0 ) / vec3( 2.2 ) );
#else
	return rgb;
#endif
}
vec4 LinearRGBToSRGB( vec4 rgba )
{
#if !defined( USE_SRGB )
	rgba.rgb = pow( rgba.rgb, vec3( 1.0 ) / vec3( 2.2 ) );
	return rgba;
#else
	return rgba;
#endif
}

// brightness and contrast control
vec3 doBrightnessAndContrast(vec3 value, float brightnessValue, float contrastValue)
{
	vec3 color = ((value.rgb - 0.5f) * max(contrastValue, 0)) + 0.5f;
	color = vec3(pow(abs(color.r), brightnessValue), pow(abs(color.g), brightnessValue), pow(abs(color.b), brightnessValue));
	return color;
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
	// get into ACES color space
    x = x * ACESInputMat;

    // apply RRT and ODT
    x = RRTAndODTFit(x);
	
	// output back into sRGB
    x = x * ACESOutputMat;

    // clamp to [0, 1]
    x = saturate(x);
	
	return x;
}
