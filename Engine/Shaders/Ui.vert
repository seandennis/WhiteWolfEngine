#version 460

// Places one corner of a triangle of a user interface on the surface, and hands its colour and its place in the
// texture on to Ui.frag. The corner is an FUiVertex: see Engine/Public/Rendering/UiVertex.h.

#include "UiData.glsl"

// In pixels from the top left corner of the surface.
layout(location = 0) in vec2 InPosition;
layout(location = 1) in vec2 InTextureCoordinate;
// sRGB-encoded, with alpha as it is.
layout(location = 2) in vec4 InColor;

layout(location = 0) out vec2 OutTextureCoordinate;
layout(location = 1) out vec4 OutColor;

// Returns the amount of light that an sRGB-encoded colour stands for.
vec3 DecodeSrgb(vec3 Encoded)
{
	vec3 Dark = Encoded / 12.92;
	vec3 Bright = pow((Encoded + 0.055) / 1.055, vec3(2.4));
	return mix(Dark, Bright, step(vec3(0.04045), Encoded));
}

void main()
{
	gl_Position = vec4(InPosition * Ui.Scale + Ui.Translate, 0.0, 1.0);
	OutTextureCoordinate = InTextureCoordinate;

	// The colours of a user interface are given as they are picked and stored, sRGB-encoded. The surface is drawn
	// to in linear light and encodes what it is given, so the colours are decoded here.
	OutColor = vec4(DecodeSrgb(InColor.rgb), InColor.a);
}
