
// common
INOUTTYPE vec4 texcoords;

#ifdef VERTEXSHADER
uniform mat4 orthomatrix;

layout(location = 0) in vec4 position;
layout(location = 2) in vec4 texcoord;

void BloomVS ()
{
	gl_Position = orthomatrix * position;
	texcoords = texcoord;
}
#endif


#ifdef FRAGMENTSHADER
uniform sampler2D bloomScene;
uniform sampler2D scene;

uniform float r_bloomIntensity;

out vec4 fragColor;

vec3 UpsampleFilter(vec2 uv)
{
#if HIGH_QUALITY_BLOOM_UPSAMPLE
    // 9-tap bilinear upsampler (tent filter)
    vec4 d = r_FBufScale.xyxy * vec4(1, 1, -1, 0) * 0.67;

    vec3 s;
    s  = texture(bloomScene, uv - d.xy).rgb;
    s += texture(bloomScene, uv - d.wy).rgb * 2;
    s += texture(bloomScene, uv - d.zy).rgb;

    s += texture(bloomScene, uv + d.zw).rgb * 2;
    s += texture(bloomScene, uv       ).rgb * 4;
    s += texture(bloomScene, uv + d.xw).rgb * 2;

    s += texture(bloomScene, uv + d.zy).rgb;
    s += texture(bloomScene, uv + d.wy).rgb * 2;
    s += texture(bloomScene, uv + d.xy).rgb;

    return s * (1.0 / 16);
#else
    // 4-tap bilinear upsampler
    vec4 d = r_FBufScale.xyxy * vec4(-1, -1, +1, +1) * (0.67 * 0.5);

    vec3 s;
    s  = texture(bloomScene, uv + d.xy).rgb;
    s += texture(bloomScene, uv + d.zy).rgb;
    s += texture(bloomScene, uv + d.xw).rgb;
    s += texture(bloomScene, uv + d.zw).rgb;

    return s * (1.0 / 4);
#endif
}

void BloomFS ()
{
	vec2 st = gl_FragCoord.st * r_FBufScale;

	// grab the base scene
	vec4 baseScene = texture(scene, st);

	// grab the blurred scene textures and add them together
    vec3 blur = UpsampleFilter(st);

	// convert to linear RGB space
	baseScene.rgb = sRGBToLinearRGB(baseScene.rgb);

	// add blurred scene to base scene and multiply
	vec3 blurredScene = baseScene.rgb + blur * r_bloomIntensity;

	// convert back to sRGB
	blurredScene = LinearRGBToSRGB(blurredScene);

	// output
	fragColor = vec4(blurredScene, baseScene.a);
}
#endif

