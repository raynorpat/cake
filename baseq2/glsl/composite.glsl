
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

void CompositeFS ()
{
	vec4 color = vec4(0.0);
	vec2 st = gl_FragCoord.st * texScale;
		
	// brightpass
	if(compositeMode == 1)
	{
		// multiply with 4 because the FBO is only 1/4th of the screen resolution
		st *= vec2(4.0, 4.0);
		
		// grab scene
		color = texture (diffuse, st);

		// adjust contrast
		float hdrContrastThreshold = brightParam.x;
		float hdrContrastScaler = brightParam.y;

		fragColor = max((color - hdrContrastThreshold) * hdrContrastScaler, vec4(0.0,0.0,0.0,0.0));
		return;
	}
	// horizontal gaussian blur pass
	else if(compositeMode == 2)
	{
		float Usigma = 3.0;
		vec2 UblurMultiplyVec = vec2(1.0, 0.0);
		float UnumBlurPixelsPerSide = 4.0;
	
		// Incremental Gaussian Coefficient Calculation (See GPU Gems 3 pg. 877 - 889)
		vec3 incrementalGaussian;
		incrementalGaussian.x = 1.0 / (sqrt(2.0 * pi) * Usigma);
		incrementalGaussian.y = exp(-0.5 / (Usigma * Usigma));
		incrementalGaussian.z = incrementalGaussian.y * incrementalGaussian.y;

		vec4 avgValue = vec4(0.0, 0.0, 0.0, 0.0);
		float coefficientSum = 0.0;

		// take the central sample first
		avgValue += texture(diffuse, st) * incrementalGaussian.x;
		coefficientSum += incrementalGaussian.x;
		incrementalGaussian.xy *= incrementalGaussian.yz;

		vec2 offset = texScale * UblurMultiplyVec; // component-wise
		for (float i = 1.0; i <= UnumBlurPixelsPerSide; i++) {
			avgValue += texture(diffuse, st - i * offset) * incrementalGaussian.x;
			avgValue += texture(diffuse, st + i * offset) * incrementalGaussian.x;
			coefficientSum += 2 * incrementalGaussian.x;
			incrementalGaussian.xy *= incrementalGaussian.yz;
		}

		fragColor = avgValue / coefficientSum;
		return;
	}
	// vertical gaussian blur pass
	else if(compositeMode == 3)
	{
		float Usigma = 3.0;
		vec2 UblurMultiplyVec = vec2(0.0, 1.0);
		float UnumBlurPixelsPerSide = 4.0;
	
		// Incremental Gaussian Coefficient Calculation (See GPU Gems 3 pg. 877 - 889)
		vec3 incrementalGaussian;
		incrementalGaussian.x = 1.0 / (sqrt(2.0 * pi) * Usigma);
		incrementalGaussian.y = exp(-0.5 / (Usigma * Usigma));
		incrementalGaussian.z = incrementalGaussian.y * incrementalGaussian.y;

		vec4 avgValue = vec4(0.0, 0.0, 0.0, 0.0);
		float coefficientSum = 0.0;

		// take the central sample first
		avgValue += texture(diffuse, st) * incrementalGaussian.x;
		coefficientSum += incrementalGaussian.x;
		incrementalGaussian.xy *= incrementalGaussian.yz;

		vec2 offset = texScale * UblurMultiplyVec; // component-wise
		for (float i = 1.0; i <= UnumBlurPixelsPerSide; i++) {
			avgValue += texture(diffuse, st - i * offset) * incrementalGaussian.x;
			avgValue += texture(diffuse, st + i * offset) * incrementalGaussian.x;
			coefficientSum += 2 * incrementalGaussian.x;
			incrementalGaussian.xy *= incrementalGaussian.yz;
		}

		fragColor = avgValue / coefficientSum;
		return;
	}
	// sepia pass for menu and loading background
	else if(compositeMode == 4)
	{
		// grab scene
		color = texture (diffuse, st);

		// convert to greyscale using NTSC weightings
		float grey = dot(color.rgb, vec3(0.299, 0.587, 0.114));
		
		// multiply rgb weightings with grayscale value to give sepia look
		vec3 sepia = vec3(1.2, 1.0, 0.8);
		sepia *= grey;
		vec4 colorSepia = vec4(mix(color.rgb, sepia, 0.88), 1.0);
		
		// small vignette
		float OuterVignetting	= 1.4 - 0.75;
		float InnerVignetting	= 1.0 - 0.75;
		float d = distance(vec2(0.5, 0.5), st) * 1.0;
		float vignetting = clamp((OuterVignetting - d) / (OuterVignetting - InnerVignetting), 0.0, 1.0);

		// mix everything together
		color = mix(color, colorSepia, vignetting);

		// gamma correct
		fragColor = toLinear(color);
		return;
	}
}
#endif

