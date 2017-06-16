
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
uniform vec4 hdrParam;
uniform vec4 brightParam;

out vec4 fragColor;

// https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
vec3 ACESFilm( vec3 x )
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return saturate( ( x * ( a * x + b ) ) / ( x * ( c * x + d ) + e ) );
}

void CompositeFS ()
{
	vec4 color = vec4(0.0);
	vec2 st = gl_FragCoord.st * texScale;
		
	// brightpass + tonemap
	if(compositeMode == 1)
	{
		// multiply with 4 because the FBO is only 1/4th of the screen resolution
		st *= vec2(4.0, 4.0);
		
		color = texture (diffuse, st);

		// get the luminance of the current pixel
		float Y = dot( LUMINANCE_VECTOR, color );
		if(Y < 0.1)
		{
			discard;
			return;
		}

		float hdrKey = hdrParam.x;
		float hdrAverageLuminance = hdrParam.y;
		float hdrMaxLuminance = hdrParam.z;
		
		// calculate the relative luminance
		float Yr = ( hdrKey * Y ) / hdrAverageLuminance;
		float Ymax = hdrMaxLuminance;

		// use geometric mean to figure out color based on exposure
		float avgLuminance = max( hdrAverageLuminance, 0.001 );
		float linearExposure = ( hdrKey / avgLuminance );
		float exposure = log2( max( linearExposure, 0.0001 ) );

		// tonemap using ACES filmic tonemapping curve
		vec3 exposedColor = exp2( exposure ) * color.rgb;
		vec3 curr = ACESFilm( exposedColor );		
		vec3 whiteScale = 1.0 / ACESFilm( vec3( Ymax ) );
		
		color.rgb = curr * whiteScale;

		// adjust contrast
		float hdrContrastThreshold = brightParam.x;
		float hdrContrastOffset = brightParam.y;

		float T = max( ( Yr * ( 1.0 + Yr / ( Ymax * Ymax * 2.0 ) ) ) - hdrContrastThreshold, 0.0 );
		float B = T > 0.0 ? T / ( hdrContrastOffset + T ) : T;

		// clamp to 0...1
		color.rgb *= clamp( B, 0.0, 1.0 );
	}
	// tonemap
	else if(compositeMode == 2)
	{
		color = texture (diffuse, st);
		
		// get the luminance of the current pixel
		float Y = dot( LUMINANCE_VECTOR, color );
		
		float hdrKey = hdrParam.x;
		float hdrAverageLuminance = hdrParam.y;
		float hdrMaxLuminance = hdrParam.z;

		// calculate the relative luminance
		float Yr = ( hdrKey * Y ) / hdrAverageLuminance;
		float Ymax = hdrMaxLuminance;

		// use geometric mean to figure out color based on exposure
		float avgLuminance = max( hdrAverageLuminance, 0.001 );
		float linearExposure = ( hdrKey / avgLuminance );
		float exposure = log2( max( linearExposure, 0.0001 ) );

		// tonemap using ACES filmic tonemapping curve		
		vec3 exposedColor = exp2( exposure ) * color.rgb;
		vec3 curr = ACESFilm( exposedColor );
		vec3 whiteScale = 1.0 / ACESFilm( vec3( Ymax ) );

		color.rgb = curr * whiteScale;
	}
	// horizontal hdr chromatic glare
	else if(compositeMode == 3)
	{
		const float gaussFact[9] = float[9](0.13298076, 0.12579441, 0.10648267, 0.08065691, 0.05467002, 0.03315905, 0.01799699, 0.00874063, 0.00379866);
		
		const vec3 chromaticOffsets[9] = vec3[](
			vec3(0.5, 0.5, 0.5), // w 
			vec3(0.8, 0.3, 0.3),
			vec3(0.2, 0.2, 0.2), // r
			vec3(0.5, 0.2, 0.8),
			vec3(0.2, 0.2, 1.0), // b
			vec3(0.2, 0.3, 0.9),
			vec3(0.2, 1.0, 0.2), // g
			vec3(0.3, 0.5, 0.3),
			vec3(0.3, 0.5, 0.3)
		);
	
		vec3 sumColor = vec3( 0.0 );

		const int tap = 4;
		const int samples = 9;
		
		float scale = 11.0; // bloom width
		const float weightScale = 2.3; // bloom strength
		
		#define so mix( chromaticOffsets[ i ], vec3(0.5, 0.5, 0.5), 0.5 )
	
		for( int i = 0; i < samples; i++ )
		{
			vec4 color = texture( diffuse, st + vec2( float( i ), 0 ) * texScale * scale);

			float weight = gaussFact[ i ];
			sumColor += color.rgb * ( so.rgb * weight * weightScale );
		}	

		for( int i = 1; i < samples; i++ )
		{
			vec4 color = texture( diffuse, st + vec2( float( -i ), 0 ) * texScale * scale );

			float weight = gaussFact[ i ];
			sumColor += color.rgb * ( so.rgb * weight * weightScale );
		}
	
		fragColor = vec4( sumColor, 1.0 );
		return;
	}
	
	// convert from linear RGB to sRGB
	float gamma = 1.0 / 2.2;
	color.r = pow( color.r, gamma );
	color.g = pow( color.g, gamma );
	color.b = pow( color.b, gamma );
	
	fragColor = color;
}
#endif

