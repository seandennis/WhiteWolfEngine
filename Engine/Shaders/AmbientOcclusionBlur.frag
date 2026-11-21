#version 460
#extension GL_EXT_samplerless_texture_functions : require

// Smooths the ambient occlusion along one direction, without letting it spread from one surface onto another
// that lies at a different depth. Drawn twice, across and down, it removes the grain of the occlusion pass.

#include "AmbientOcclusionData.glsl"

// The ambient occlusion to smooth.
layout(set = 0, binding = 2) uniform texture2D Occlusion;

layout(location = 0) out float OutOcclusion;

// How many pixels to each side are taken in, and how much each counts: a bell curve. Every second pixel is
// taken, which doubles the reach of the blur for the same work; the two passes, across and down, and the pattern
// of the occlusion pass's grain leave no trace of the gaps.
const int BlurRadius = 4;
const int BlurStride = 2;
const float Weights[5] = float[5](0.20, 0.17, 0.12, 0.07, 0.04);

// A pixel stops counting when its distance from the camera differs from the centre's by this part of it.
const float DepthTolerance = 0.03;

void main()
{
	ivec2 Pixel = ivec2(gl_FragCoord.xy);
	float Depth = ReadDepth(Pixel);
	float Centre = texelFetch(Occlusion, Pixel, 0).r;
	if (Depth <= 0.0)
	{
		OutOcclusion = Centre;
		return;
	}

	ivec2 Direction = ivec2(Pass.Blur.xy);
	ivec2 LastPixel = ivec2(Pass.Settings.zw) - 1;

	float Sum = Centre * Weights[0];
	float WeightSum = Weights[0];
	for (int Step = -BlurRadius; Step <= BlurRadius; ++Step)
	{
		if (Step == 0)
		{
			continue;
		}

		ivec2 TapPixel = clamp(Pixel + Direction * (Step * BlurStride), ivec2(0), LastPixel);
		float TapDepth = ReadDepth(TapPixel);
		if (TapDepth <= 0.0)
		{
			continue;
		}

		// Depth is the near plane divided by the distance, so the ratio of two depths is the ratio of two distances.
		float Difference = abs(Depth / TapDepth - 1.0);
		float Weight = Weights[abs(Step)] * max(1.0 - Difference / DepthTolerance, 0.0);
		Sum += texelFetch(Occlusion, TapPixel, 0).r * Weight;
		WeightSum += Weight;
	}

	OutOcclusion = Sum / WeightSum;
}