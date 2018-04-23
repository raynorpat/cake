
uniform vec4 waterwarpParam;

// common
INOUTTYPE vec4 texcoords[2];

#ifdef VERTEXSHADER
uniform mat4 orthomatrix;

layout(location = 0) in vec4 position;
layout(location = 2) in vec4 texcoord;


void PostVS ()
{
	gl_Position = orthomatrix * position;
	texcoords[0] = texcoord;
	texcoords[1] = ((texcoord * waterwarpParam.y) + waterwarpParam.x) * waterwarpParam.z * waterwarpParam.w;
}
#endif


#ifdef FRAGMENTSHADER
uniform sampler2D scene;
uniform sampler2D warpgradient;
uniform sampler2D colorLUT;

uniform int waterwarppost;

uniform vec4 surfcolor;
uniform vec2 brightnessContrastAmount;
uniform vec2 rescale;

void FilmgrainPass( inout vec4 color )
{
	vec2 uv = gl_FragCoord.st * r_FBufScale;
    float strength = 16.0;
    
    float x = (uv.x + 4.0) * (uv.y + 4.0) * (waterwarpParam.x * 10.0);
	vec4 grain = vec4(mod((mod(x, 13.0) + 1.0) * (mod(x, 123.0) + 1.0), 0.01)-0.005) * strength;
    
   	grain = 1.0 - grain;
	color *= grain;
}

#define LUT_WIDTH 256.0
#define LUT_HEIGHT 16.0

// this takes a 2D LUT and turns it into a 3D LUT
// with a fix to the blue channel to prevent banding artifacts
vec3 ColorMap (vec3 x)
{
	float cell = x.b * 16.0;

	// calculate the two adjacent cells to read from
    float cell_l = floor(cell); 
    float cell_h = ceil(cell);
	
	float half_px_x = 0.5 / LUT_WIDTH;
    float half_px_y = 0.5 / LUT_HEIGHT;
    float r_offset = half_px_x + x.r / 16.0 * 0.9375;
    float g_offset = half_px_y + x.g * 0.9375;
	
	// calculate two separate lookup positions, one for each cell
	vec2 lut_pos_l = vec2(cell_l / 16.0 + r_offset, g_offset); 
    vec2 lut_pos_h = vec2(cell_h / 16.0 + r_offset, g_offset);
	
	// sample the two colors from the cell positions
	vec3 graded_color_l = texture (colorLUT, lut_pos_l).rgb;
    vec3 graded_color_h = texture (colorLUT, lut_pos_h).rgb;
	
	// mix the colors linearly according to the fraction of cell, which is the scaled blue color value
	return mix(graded_color_l, graded_color_h, fract(cell));
}

out vec4 fragColor;

void PostFS ()
{
	vec2 st = gl_FragCoord.st * r_FBufScale;
	vec4 sceneColor;

	// mix in water warp post effect
	if (waterwarppost == 1)
	{
		vec4 distort1 = texture (warpgradient, texcoords[1].yx);
		vec4 distort2 = texture (warpgradient, texcoords[1].xy);
		vec2 warpcoords = texcoords[0].xy + (distort1.ba + distort2.ab);

		sceneColor = texture(scene, warpcoords * rescale);
	}
	else
	{
		sceneColor = texture(scene, st);
	}
	
	// convert to linear RGB
	sceneColor.rgb = sRGBToLinearRGB(sceneColor.rgb);
	
	// color grade via 2D LUT
	sceneColor.rgb = clamp(sceneColor.rgb, 0.0, 1.0);
	sceneColor.rgb = ColorMap(sceneColor.rgb);
	
	// film grain effect
#if r_useFilmgrain
	FilmgrainPass(sceneColor);
#endif	

	// mix scene with possible modulation (eg item pickups, getting shot, etc)
	vec4 screenBlend = LinearRGBToSRGB(surfcolor);
	sceneColor = mix(sceneColor, screenBlend, screenBlend.a);

	// brightness
	sceneColor.rgb = doBrightnessAndContrast(sceneColor.rgb, brightnessContrastAmount.x, brightnessContrastAmount.y);

	// dither screen to prevent banding artifacts
	Dither(sceneColor.rgb);
	
	// convert back to sRGB
	sceneColor.rgb = LinearRGBToSRGB(sceneColor.rgb);
	
	// send it out to the screen
	fragColor = sceneColor;
}
#endif
