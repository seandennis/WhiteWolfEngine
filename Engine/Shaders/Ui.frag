#version 460

// Colours a triangle of a user interface: the colour that comes from its corners, multiplied by its picture.

#include "UiData.glsl"

// The picture that covers the triangle; one white pixel for a triangle without a picture. Textures are bound from 0
// on and samplers from 128 on; see the binding offsets of NVRHI's Vulkan backend.
layout(set = 0, binding = 0) uniform texture2D Picture;
layout(set = 0, binding = 128) uniform sampler PictureSampler;

layout(location = 0) in vec2 InTextureCoordinate;
layout(location = 1) in vec4 InColor;

layout(location = 0) out vec4 OutColor;

void main()
{
	vec4 Texel = texture(sampler2D(Picture, PictureSampler), InTextureCoordinate);

	// The surface of a render target that is shown as a picture covers what lies behind it, whatever its alpha.
	float TexelAlpha = mix(1.0, Texel.a, Ui.TextureAlpha);
	OutColor = vec4(InColor.rgb * Texel.rgb, InColor.a * TexelAlpha);
}
