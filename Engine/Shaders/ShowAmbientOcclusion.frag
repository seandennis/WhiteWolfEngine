#version 460
#extension GL_EXT_samplerless_texture_functions : require

// Shows the ambient occlusion by itself, as shades of grey: white in the open, darker where a surface is hidden
// from its surroundings. For the view mode that inspects it.

#include "AmbientOcclusionData.glsl"

layout(set = 0, binding = 2) uniform texture2D Occlusion;

layout(location = 0) out vec4 OutColor;

void main()
{
	float Value = texelFetch(Occlusion, ivec2(gl_FragCoord.xy), 0).r;
	OutColor = vec4(vec3(Value), 1.0);
}