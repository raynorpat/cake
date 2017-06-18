
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
}
#endif

