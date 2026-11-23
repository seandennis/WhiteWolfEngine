#version 460
#extension GL_EXT_samplerless_texture_functions : require

// Draws a line around the meshes that were marked in the mask: outside them, as wide as it is told, and soft over
// one pixel more.

// One where a marked mesh is and zero elsewhere, pixel for pixel the size of the picture.
layout(set = 0, binding = 0) uniform texture2D Mask;

// Must match FOutlines::FConstants.
layout(push_constant) uniform FOutlineConstants
{
	// The colour of the line; a is how much of the picture below it the line covers.
	vec4 Color;

	// x: the width of the line in pixels.
	vec4 Settings;
} Outline;

layout(location = 0) out vec4 OutColor;

bool IsMarked(ivec2 Pixel)
{
	return texelFetch(Mask, Pixel, 0).r > 0.5;
}

void main()
{
	ivec2 Pixel = ivec2(gl_FragCoord.xy);
	ivec2 LastPixel = textureSize(Mask, 0) - 1;

	// The line goes around what is marked, not over it.
	if (IsMarked(Pixel))
	{
		discard;
	}

	// How far it is to the nearest marked pixel, from centre to centre. Beyond the edge of the picture there is
	// what the edge has, which is never nearer than the edge itself.
	float Width = Outline.Settings.x;
	int Reach = int(ceil(Width));
	float Nearest = Width + 1.0;
	for (int Y = -Reach; Y <= Reach; ++Y)
	{
		for (int X = -Reach; X <= Reach; ++X)
		{
			if (IsMarked(clamp(Pixel + ivec2(X, Y), ivec2(0), LastPixel)))
			{
				Nearest = min(Nearest, length(vec2(X, Y)));
			}
		}
	}

	// What is marked ends half a pixel from the centre of its outermost pixel, and this pixel begins half a pixel
	// before its own centre. So the line has all of this pixel while that distance is no more than its width, and
	// less and less of it over the pixel after.
	float Coverage = clamp(Width + 1.0 - Nearest, 0.0, 1.0);
	if (Coverage <= 0.0)
	{
		discard;
	}
	OutColor = vec4(Outline.Color.rgb, Outline.Color.a * Coverage);
}
