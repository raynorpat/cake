#version 430

layout (local_size_x = 1, local_size_y = 1) in;

uniform float deltaTime;

layout(binding = 0, rgba16f) uniform highp image2D currentImage;
layout(binding = 1, rgba16f) uniform highp image2D image0;
layout(binding = 2, rgba16f) uniform highp image2D image1;

void main()
{
	float currentLum = imageLoad(currentImage, ivec2(0, 0)).r;
	float lastLum = imageLoad(image0, ivec2(0, 0)).r;
	
	float currentMaxLum = imageLoad(currentImage, ivec2(0, 0)).g;
	float lastMaxLum = imageLoad(image0, ivec2(0, 0)).g;
	
	float currentMinLum = imageLoad(currentImage, ivec2(0, 0)).b;
	float lastMinLum = imageLoad(image0, ivec2(0, 0)).b;
	
	float newLum = lastLum + (currentLum - lastLum) * ( 1.0 - pow( 0.98f, 30.0 * deltaTime ) );
	float newMaxLum = lastMaxLum + (currentMaxLum - lastMaxLum) * ( 1.0 - pow( 0.98f, 30.0 * deltaTime ) );
	float newMinLum = lastMinLum + (currentMinLum - lastMinLum) * ( 1.0 - pow( 0.98f, 30.0 * deltaTime ) );

	imageStore(image1, ivec2(0, 0), vec4(newLum, newMaxLum, newMinLum, newLum));
}

