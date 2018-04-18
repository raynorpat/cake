
// see FXAA
// http://developer.download.nvidia.com/assets/gamedev/files/sdk/11/FXAA_WhitePaper.pdf
// http://iryoku.com/aacourse/downloads/09-FXAA-3.11-in-15-Slides.pdf
// http://horde3d.org/wiki/index.php5?title=Shading_Technique_-_FXAA

// common
INOUTTYPE vec4 texcoords;

#ifdef VERTEXSHADER
uniform mat4 orthomatrix;

layout(location = 0) in vec4 position;
layout(location = 2) in vec4 texcoord;

void FXAAVS ()
{
	gl_Position = orthomatrix * position;
	texcoords = texcoord;
}
#endif


#ifdef FRAGMENTSHADER
uniform sampler2D diffuse;

out vec4 fragColor;

void FXAAFS ()
{
	vec2 st = gl_FragCoord.st * r_FBufScale;
	vec3 rgbM = texture(diffuse, st).rgb;
	
	float u_lumaThreshold = 0.5;
	float u_mulReduce = 1.0 / 8.0;
	float u_minReduce = 1.0 / 128.0;
	float u_maxSpan = 8.0;

	// sampling neighbour texels. offsets are adapted to OpenGL texture coordinates. 
	vec3 rgbNW = textureOffset(diffuse, st, ivec2(-1, 1)).rgb;
    vec3 rgbNE = textureOffset(diffuse, st, ivec2(1, 1)).rgb;
    vec3 rgbSW = textureOffset(diffuse, st, ivec2(-1, -1)).rgb;
    vec3 rgbSE = textureOffset(diffuse, st, ivec2(1, -1)).rgb;

	// see http://en.wikipedia.org/wiki/Grayscale
	const vec3 toLuma = vec3(0.299, 0.587, 0.114);

	// convert from RGB to luma.
	float lumaNW = dot(rgbNW, toLuma);
	float lumaNE = dot(rgbNE, toLuma);
	float lumaSW = dot(rgbSW, toLuma);
	float lumaSE = dot(rgbSE, toLuma);
	float lumaM = dot(rgbM, toLuma);

	// gather minimum and maximum luma.
	float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
	float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
	
	// if contrast is lower than a maximum threshold ...
	if (lumaMax - lumaMin < lumaMax * u_lumaThreshold)
	{
		// ... do no AA and return.
		fragColor = vec4(rgbM, 1.0);		
		return;
	}
	
	// sampling is done along the gradient.
	vec2 samplingDirection;	
	samplingDirection.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    samplingDirection.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));
    
    // sampling step distance depends on the luma: The brighter the sampled texels, the smaller the final sampling step direction.
    // this results, that brighter areas are less blurred/more sharper than dark areas.  
    float samplingDirectionReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.25 * u_mulReduce, u_minReduce);

	// factor for norming the sampling direction plus adding the brightness influence. 
	float minSamplingDirectionFactor = 1.0 / (min(abs(samplingDirection.x), abs(samplingDirection.y)) + samplingDirectionReduce);
    
    // calculate final sampling direction vector by reducing, clamping to a range and finally adapting to the texture size. 
    samplingDirection = clamp(samplingDirection * minSamplingDirectionFactor, vec2(-u_maxSpan, -u_maxSpan), vec2(u_maxSpan, u_maxSpan)) * r_FBufScale;
	
	// inner samples on the tab.
	vec3 rgbSampleNeg = texture(diffuse, st + samplingDirection * (1.0/3.0 - 0.5)).rgb;
	vec3 rgbSamplePos = texture(diffuse, st + samplingDirection * (2.0/3.0 - 0.5)).rgb;

	vec3 rgbTwoTab = (rgbSamplePos + rgbSampleNeg) * 0.5;  

	// outer samples on the tab.
	vec3 rgbSampleNegOuter = texture(diffuse, st + samplingDirection * (0.0/3.0 - 0.5)).rgb;
	vec3 rgbSamplePosOuter = texture(diffuse, st + samplingDirection * (3.0/3.0 - 0.5)).rgb;
	
	vec3 rgbFourTab = (rgbSamplePosOuter + rgbSampleNegOuter) * 0.25 + rgbTwoTab * 0.5;   
	
	// calculate luma for checking against the minimum and maximum value.
	float lumaFourTab = dot(rgbFourTab, toLuma);

	// are the outer samples of the tab beyond the edge ... 
	if (lumaFourTab < lumaMin || lumaFourTab > lumaMax)
	{
		// ... yes, so use only two samples.
		fragColor = vec4(rgbTwoTab, 1.0);
	}
	else
	{
		// ... no, so use four samples. 
		fragColor = vec4(rgbFourTab, 1.0);
	}
}
#endif

