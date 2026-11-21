#version 460

// Lights a surface with the lights of the scene. The result is the light that reaches the camera, which may be
// far brighter than a display can show; Tonemap.frag fits it to the display afterwards.

#include "SceneData.glsl"
#include "Brdf.glsl"

layout(location = 0) in vec3 InRelativePosition;
layout(location = 1) in vec3 InNormal;
layout(location = 2) flat in int InDrawIndex;

layout(location = 0) out vec4 OutColor;

void main()
{
	// The camera is the origin of camera-relative space.
	vec3 ToCamera = normalize(-InRelativePosition);
	FSurface Surface = MakeSurface(Draws[InDrawIndex].BaseColor.rgb,
		Draws[InDrawIndex].MetallicRoughness.x,
		Draws[InDrawIndex].MetallicRoughness.y,
		normalize(InNormal),
		ToCamera);

	vec3 Light = Draws[InDrawIndex].EmissiveColor.rgb;

	for (int Index = 0; Index < View.LightCounts.x; ++Index)
	{
		Light += ReflectLight(
			Surface, View.DirectionalLights[Index].DirectionToLight.xyz, View.DirectionalLights[Index].Illuminance.rgb);
	}

	for (int Index = 0; Index < View.LightCounts.y; ++Index)
	{
		vec3 Offset = View.PointLights[Index].LocationAndRange.xyz - InRelativePosition;
		float DistanceSquared = max(dot(Offset, Offset), 1.0e-8);
		float Range = View.PointLights[Index].LocationAndRange.w;

		// Light spreads over a sphere that grows with the square of the distance.
		float Falloff = 1.0 / DistanceSquared;
		if (Range > 0.0)
		{
			// A light with a range fades to nothing at that distance, smoothly, as glTF recommends.
			float Ratio = DistanceSquared / (Range * Range);
			float Window = clamp(1.0 - Ratio * Ratio, 0.0, 1.0);
			Falloff *= Window * Window;
		}

		Light += ReflectLight(Surface, Offset * inversesqrt(DistanceSquared), View.PointLights[Index].Intensity.rgb * Falloff);
	}

	OutColor = vec4(min(Light, vec3(MaxSceneLight)), 1.0);
}