#version 460

// Colours a surface with the base colour of its material, without lighting.

#include "DrawConstants.glsl"

layout(location = 0) out vec4 OutColor;

void main()
{
	// Surfaces are opaque, whatever the alpha of the material's colour.
	OutColor = vec4(Draw.BaseColor.rgb, 1.0);
}