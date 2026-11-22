#version 460
#extension GL_EXT_samplerless_texture_functions : require

// Works out, for each pixel, how much of its surrounding the surface there can see: 1 in the open, less in
// creases, corners and under things. The light of the sky is dimmed by it, which is what makes objects look as
// if they stand on the ground instead of floating above it.
//
// This is the "scalable ambient obscurance" estimator of McGuire, Mara and Luebke (2012). Around each pixel it
// looks at other pixels within a radius that is fixed in the world, not on the screen, and counts how far the
// surfaces there rise above the plane of the pixel's own surface. A flat surface, at any angle, gets exactly 1.

#include "AmbientOcclusionData.glsl"

layout(location = 0) out float OutOcclusion;

const int SampleCount = 16;

// The samples lie on a spiral that winds this often, a number chosen so that 16 samples cover a disc evenly.
const float SpiralTurns = 7.0;

const float TwoPi = 6.28318530717959;

// Returns the angle at which a pixel's spiral starts. With every spiral in the same place, the few samples would
// show as bands, so each pixel turns its spiral by one of sixteen angles, a sixteenth of a turn apart. The angles
// are laid out in a pattern in which each row repeats every four pixels and holds four angles a quarter turn
// apart, and any four rows, one below the other, together hold all sixteen. The blur afterwards gives the four
// places in a row the same share and then the four rows the same share, so it averages the sixteen spirals and
// leaves no trace of the pattern.
float GetStartAngle(ivec2 Pixel)
{
	return float((4 * Pixel.x + 5 * Pixel.y) & 15) * (TwoPi / 16.0);
}

void main()
{
	ivec2 Pixel = ivec2(gl_FragCoord.xy);
	float Depth = ReadDepth(Pixel);
	if (Depth <= 0.0)
	{
		// Sky: nothing to occlude.
		OutOcclusion = 1.0;
		return;
	}

	vec3 Position = GetViewPosition(Pixel, Depth);
	vec3 Normal = normalize(mat3(Pass.RelativeToView) * (texelFetch(SceneNormals, Pixel, 0).xyz * 2.0 - 1.0));
	float Distance = -Position.z;

	float Radius = Pass.Settings.x;
	float RadiusOnScreen = Radius * Pass.Projection.w / Distance;
	if (RadiusOnScreen < 1.0)
	{
		// So far away that the radius is smaller than a pixel.
		OutOcclusion = 1.0;
		return;
	}

	// A surface must rise this far above the pixel's own plane before it counts. This is what keeps a flat
	// surface clean: the normal is stored in eight bits, and positions are only as fine as a pixel.
	float Bias = 0.02 * Radius + 0.002 * Distance;

	float RadiusSquared = Radius * Radius;
	float StartAngle = GetStartAngle(Pixel);
	ivec2 PictureSize = ivec2(Pass.Settings.zw);

	float Sum = 0.0;
	for (int Index = 0; Index < SampleCount; ++Index)
	{
		float Outwards = (float(Index) + 0.5) / float(SampleCount);
		float Angle = Outwards * SpiralTurns * TwoPi + StartAngle;
		ivec2 SamplePixel = Pixel + ivec2(RadiusOnScreen * Outwards * vec2(cos(Angle), sin(Angle)));
		if (any(lessThan(SamplePixel, ivec2(0))) || any(greaterThanEqual(SamplePixel, PictureSize)))
		{
			continue;
		}

		float SampleDepth = ReadDepth(SamplePixel);
		if (SampleDepth <= 0.0)
		{
			continue;
		}

		vec3 ToSample = GetViewPosition(SamplePixel, SampleDepth) - Position;
		float DistanceSquared = dot(ToSample, ToSample);
		float AbovePlane = dot(ToSample, Normal);

		// Surfaces count less the farther away they are and not at all beyond the radius.
		float Falloff = max(RadiusSquared - DistanceSquared, 0.0);
		Sum += Falloff * Falloff * Falloff * max((AbovePlane - Bias) / (DistanceSquared + 1.0e-4), 0.0);
	}

	float Strength = Pass.Settings.y;
	float Obscured = Sum * 5.0 * Strength / (RadiusSquared * RadiusSquared * RadiusSquared * float(SampleCount));
	OutOcclusion = clamp(1.0 - Obscured, 0.0, 1.0);
}