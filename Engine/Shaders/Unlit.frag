#version 460

// Colours a surface with the base colour of its material, without lighting.

#include "SceneData.glsl"
#include "MaterialTextures.glsl"

layout(location = 2) in vec2 InTextureCoordinate;
layout(location = 3) flat in int InDrawIndex;

layout(location = 0) out vec4 OutColor;

void main()
{
	vec3 BaseColor = Draws[InDrawIndex].BaseColor.rgb *
		texture(sampler2D(BaseColorTexture, BaseColorSampler), InTextureCoordinate).rgb;

	// Surfaces are opaque, whatever the alpha of the material's colour.
	OutColor = vec4(BaseColor, 1.0);
}