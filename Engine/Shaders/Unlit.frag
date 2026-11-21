#version 460

// Colours a surface with the base colour of its material, without lighting.

#include "SceneData.glsl"

layout(location = 2) flat in int InDrawIndex;

layout(location = 0) out vec4 OutColor;

void main()
{
	// Surfaces are opaque, whatever the alpha of the material's colour.
	OutColor = vec4(Draws[InDrawIndex].BaseColor.rgb, 1.0);
}