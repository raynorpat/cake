
// common.glsl

float saturate( float v ) { return clamp( v, 0.0, 1.0 ); }
vec3 saturate( vec3 v ) { return clamp( v, 0.0, 1.0 ); }

float log10( in float n )
{
	const float logBase10 = 1.0 / log2( 10.0 );
	return log2( n ) * logBase10;
}

const vec4 LUMINANCE_VECTOR = vec4(0.2125, 0.7154, 0.0721, 0.0);

// https://en.wikipedia.org/wiki/SRGB
vec3 srgb_to_linear(vec3 srgb)
{
    float a = 0.055;
    float b = 0.04045;
    vec3 linear_lo = srgb / 12.92;
    vec3 linear_hi = pow((srgb + vec3(a)) / (1.0 + a), vec3(2.4));
    return vec3(
        srgb.r > b ? linear_hi.r : linear_lo.r,
        srgb.g > b ? linear_hi.g : linear_lo.g,
        srgb.b > b ? linear_hi.b : linear_lo.b);
}

vec3 linear_to_srgb(vec3 linear)
{
    float a = 0.055;
    float b = 0.0031308;
    vec3 srgb_lo = 12.92 * linear;
    vec3 srgb_hi = (1.0 + a) * pow(linear, vec3(1.0/2.4)) - vec3(a);
    return vec3(
        linear.r > b ? srgb_hi.r : srgb_lo.r,
        linear.g > b ? srgb_hi.g : srgb_lo.g,
        linear.b > b ? srgb_hi.b : srgb_lo.b);
}
