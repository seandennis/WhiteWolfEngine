#version 460
#extension GL_EXT_samplerless_texture_functions : require

// Turns the lit scene into the picture that is shown: applies the camera's exposure and fits the result to the
// range of a display.

#include "ToneMapping.glsl"

// The lit scene, pixel for pixel the size of the picture.
layout(set = 0, binding = 0) uniform texture2D SceneColor;

layout(push_constant) uniform FTonemapConstants
{
	float Exposure;
} Tonemap;

layout(location = 0) out vec4 OutColor;

void main()
{
	vec4 Scene = texelFetch(SceneColor, ivec2(gl_FragCoord.xy), 0);
	OutColor = vec4(ToneMap(Scene.rgb * Tonemap.Exposure), Scene.a);
}