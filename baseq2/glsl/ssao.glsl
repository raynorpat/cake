
// common
INOUTTYPE vec4 texcoords;
INOUTTYPE vec3 unprojectionParams;

#ifdef VERTEXSHADER
uniform mat4 orthomatrix;
uniform vec3 zFar;

layout(location = 0) in vec4 position;
layout(location = 2) in vec4 texcoord;

void SSAOVS ()
{
	gl_Position = orthomatrix * position;
	texcoords = texcoord;
	
	unprojectionParams.x = - 4.0 * zFar.z;
	unprojectionParams.y = 2.0 * (zFar.z - 4.0);
	unprojectionParams.z = 2.0 * zFar.z - 4.0;
}
#endif


#ifdef FRAGMENTSHADER
uniform sampler2D depthmap;

uniform vec3 zFar;

const float haloCutoff = 15.0;

float depthToZ(float depth) {
	return unprojectionParams.x / ( unprojectionParams.y * depth - unprojectionParams.z );
}
vec4 depthToZ(vec4 depth) {
	return unprojectionParams.x / ( unprojectionParams.y * depth - unprojectionParams.z );
}

vec4 rangeTest(vec4 diff1, vec4 diff2, vec4 deltaX, vec4 deltaY) {
	vec4 mask = step( 0.0, diff1 + diff2 ) * step( -haloCutoff, -diff1 ) * step( -haloCutoff, -diff2 );
	vec4 deltaXY = deltaX * deltaX + deltaY * deltaY;
	vec4 scale = inversesqrt( diff1 * diff1 + deltaXY );
	scale *= inversesqrt( diff2 * diff2 + deltaXY );
	vec4 cosAngle = scale * ( diff1 * diff2 - deltaXY );
	return mask * ( 0.5 * cosAngle + 0.5 );
}

// Based on AMD HDAO, adapted for lack of normals
void computeOcclusionForQuad( in vec2 centerTC, in float centerDepth, in vec2 quadOffset, inout vec4 occlusion, inout vec4 total ) {
	vec2 tc1 = centerTC + quadOffset * r_FBufScale;
	vec2 tc2 = centerTC - quadOffset * r_FBufScale;

	vec4 x = vec4( -0.5, 0.5, 0.5, -0.5 ) + quadOffset.x;
	vec4 y = vec4( 0.5, 0.5, -0.5, -0.5 ) + quadOffset.y;
	vec4 weights = inversesqrt( x * x + y * y );

	// depths1 and depth2 pixels are swizzled to be in opposite position
	vec4 depths1 = depthToZ( textureGather( depthmap, tc1 ) ).xyzw;
	vec4 depths2 = depthToZ( textureGather( depthmap, tc2 ) ).zwxy;

	depths1 = centerDepth - depths1;
	depths2 = centerDepth - depths2;

	total += weights;
	occlusion += weights * rangeTest( depths1, depths2, centerDepth * x * zFar.x, centerDepth * y * zFar.y );
}

const vec4 offsets[] = vec4[6](
	vec4( 0.5,  1.5, 1.5, -0.5 ),
	vec4( 0.5,  4.5, 3.5,  2.5 ),
	vec4( 4.5, -0.5, 2.5, -3.5 ),
	vec4( 0.5,  7.5, 3.5,  5.5 ),
	vec4( 6.5,  2.5, 7.5, -0.5 ),
	vec4( 5.5, -3.5, 2.5, -6.5 )
);

out vec4 fragColor;

void SSAOFS ()
{
	vec2 st = gl_FragCoord.st * r_FBufScale;

	vec4 color = vec4( 0.0 );
	vec4 occlusion = vec4( 0.0 );
	vec4 total = vec4( 0.0 );

	float center = texture( depthmap, st ).r;

	if( center >= 1.0 )
	{
		// no SSAO on the skybox !
		discard;
		return;
	}
	center = depthToZ( center );
	float spread = max( 1.0, 100.0 / center );

	for(int i = 0; i < offsets.length(); i++) {
		vec2 of;
		int checker = int(mod(gl_FragCoord.x - gl_FragCoord.y, 2.0));
		if ( checker != 0 )
			of = offsets[i].xy;
		else
			of = offsets[i].zw;
		computeOcclusionForQuad( st, center, spread * of, occlusion, total );
	}

	float summedOcclusion = dot( occlusion, vec4( 1.0 ) );
	float summedTotal = dot( total, vec4( 1.0 ) );

	if ( summedTotal > 0.0 ) {
		summedOcclusion /= summedTotal;
	}
	
	fragColor = vec4( clamp(1.0 - summedOcclusion, 0.0, 1.0) );
}
#endif

