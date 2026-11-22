#version 460
#extension GL_EXT_samplerless_texture_functions : require

// Smooths the ambient occlusion along one direction, without letting it spread from one surface onto another
// that lies at a different depth. Drawn twice, across and down, it removes the pattern in which the occlusion
// pass turns its samples.

#include "AmbientOcclusionData.glsl"

// The ambient occlusion to smooth.
layout(set = 0, binding = 2) uniform texture2D Occlusion;

layout(location = 0) out float OutOcclusion;

// How many pixels to each side are taken in, and how much each counts: a bell curve. The occlusion pass turns its
// samples in a pattern whose rows repeat every four pixels, and these weights give each of the four places in a
// row the same share, a quarter: 14 + 1 + 1, 12 + 4, 8 + 8 and 4 + 12 are each 16 of 64. What the pass across
// leaves is the same in every fourth row, and the pass down gives those four rows the same share. That cancels
// the pattern exactly wherever every pixel counts in full, which is everywhere but next to an edge and on a
// surface that falls away from the camera steeply.
const int BlurRadius = 4;
const float Weights[5] = float[5](14.0 / 64.0, 12.0 / 64.0, 8.0 / 64.0, 4.0 / 64.0, 1.0 / 64.0);

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

		ivec2 TapPixel = clamp(Pixel + Direction * Step, ivec2(0), LastPixel);
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