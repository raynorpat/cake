#version 430

layout (local_size_x = 1, local_size_y = 1) in;

layout(binding = 0, rgba16f) uniform highp image2D inputImage;
layout(binding = 1, rgba16f) uniform highp image2D outputImage;

vec3 LUMINANCE_VECTOR  = vec3(0.2125, 0.7154, 0.0721);

void main()
{
		float logLumSum = 0.0f;
		int x, y;
		
		for (y = 0; y < 16; y++)
		{
			for (x = 0; x < 16; x++)
			{
				logLumSum += log(dot(imageLoad(inputImage, ivec2(x, y)).rgb, LUMINANCE_VECTOR) + 0.00001);
			}
		}
		
		logLumSum /= 256.0;
		float val = exp(logLumSum + 0.00001);
		
		imageStore(outputImage, ivec2(0, 0), vec4(val, val, val, val));
}

