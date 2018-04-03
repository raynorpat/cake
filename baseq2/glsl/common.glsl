
// common.glsl

#extension GL_EXT_texture_array : enable
#extension GL_ARB_texture_gather : require

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

vec3 LinearToSRGBSpace(vec3 v) { return max(1.055 * pow(v, 0.416666667) - 0.055, 0.0); }
vec4 LinearToSRGBSpace(vec4 v) { return vec4(LinearToSRGBSpace(v.rgb), v.a); }
	
// brightness and contrast control
vec3 doBrightnessAndContrast(vec3 value, float brightnessValue, float contrastValue)
{
	vec3 color = value.rgb * contrastValue;
	return vec3(pow(abs(color.rgb), vec3(brightnessValue)));
}
