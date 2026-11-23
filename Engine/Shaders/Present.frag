#version 460
#extension GL_EXT_samplerless_texture_functions : require

// Copies a picture onto the surface of a window, pixel for pixel from the top left corner. Where the surface
// reaches beyond the picture, as it can for a moment while a window is being resized, it is black.

layout(set = 0, binding = 0) uniform texture2D Picture;

// Declared again in Engine/Private/Rendering/SwapChain.cpp as FPresentConstants; keep the two identical.
layout(push_constant) uniform FPresentConstants
{
	// Not zero if the surface stores what is written to it as it is. The colours are then encoded here; a
	// surface of an sRGB format does that itself.
	int EncodeSrgb;
} Constants;

layout(location = 0) out vec4 OutColor;

// The sRGB encoding of one colour channel, as IEC 61966-2-1 defines it.
float EncodeChannel(float Linear)
{
	return Linear <= 0.0031308 ? Linear * 12.92 : 1.055 * pow(Linear, 1.0 / 2.4) - 0.055;
}

void main()
{
	ivec2 Pixel = ivec2(gl_FragCoord.xy);
	bool bIsInPicture = all(lessThan(Pixel, textureSize(Picture, 0)));
	vec3 Color = bIsInPicture ? texelFetch(Picture, Pixel, 0).rgb : vec3(0.0);
	if (Constants.EncodeSrgb != 0)
	{
		Color = vec3(EncodeChannel(Color.r), EncodeChannel(Color.g), EncodeChannel(Color.b));
	}
	OutColor = vec4(Color, 1.0);
}