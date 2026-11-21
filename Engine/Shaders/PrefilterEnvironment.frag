#version 460

// Makes one face of one level of an environment's reflection cube map. Level 0 is the environment as a mirror
// shows it; each further level is the environment blurred the way a rougher surface reflects it, so that a
// surface of any roughness can read its reflection with a single look-up.
//
// The blur is the GGX distribution of the lighting model (Brdf.glsl), for a surface seen head-on, summed up with
// importance sampling: sample directions are drawn where the distribution is large. Each sample reads the
// picture at a level of detail that matches how much of the sphere the sample stands for, which removes the
// sparkle that a few samples of a picture with a small, very bright sun would otherwise give.

#include "Environment.glsl"

// The environment's picture, with its smaller versions.
layout(set = 0, binding = 0) uniform texture2D EnvironmentPicture;
layout(set = 0, binding = 128) uniform sampler EnvironmentSampler;

layout(push_constant) uniform FPrefilterConstants
{
	// Which face of the cube is drawn, in Vulkan's order: +X, -X, +Y, -Y, +Z, -Z.
	int Face;

	// The roughness this level stands for, from 0 to 1.
	float Roughness;

	// The width of the face being drawn, in pixels.
	float FaceSize;

	// The number of pixels of the environment's picture.
	float PicturePixelCount;
} Prefilter;

layout(location = 0) out vec4 OutColor;

const int SampleCount = 64;

// Returns the direction that a place on a face of a cube map stands for. Coordinate runs from -1 to 1 across the
// face, to the right and downwards, as Vulkan defines the faces.
vec3 GetFaceDirection(int Face, vec2 Coordinate)
{
	float U = Coordinate.x;
	float V = Coordinate.y;
	switch (Face)
	{
		case 0:
			return vec3(1.0, -V, -U);
		case 1:
			return vec3(-1.0, -V, U);
		case 2:
			return vec3(U, 1.0, V);
		case 3:
			return vec3(U, -1.0, -V);
		case 4:
			return vec3(U, -V, 1.0);
		default:
			return vec3(-U, -V, -1.0);
	}
}

vec3 ReadEnvironment(vec3 Direction, float Level)
{
	return textureLod(sampler2D(EnvironmentPicture, EnvironmentSampler), GetEnvironmentCoordinate(Direction), Level).rgb;
}

// Returns the Index-th of Count points that cover the unit square evenly (the Hammersley set).
vec2 GetSamplePoint(uint Index, uint Count)
{
	uint Bits = Index;
	Bits = (Bits << 16u) | (Bits >> 16u);
	Bits = ((Bits & 0x55555555u) << 1u) | ((Bits & 0xAAAAAAAAu) >> 1u);
	Bits = ((Bits & 0x33333333u) << 2u) | ((Bits & 0xCCCCCCCCu) >> 2u);
	Bits = ((Bits & 0x0F0F0F0Fu) << 4u) | ((Bits & 0xF0F0F0F0u) >> 4u);
	Bits = ((Bits & 0x00FF00FFu) << 8u) | ((Bits & 0xFF00FF00u) >> 8u);
	return vec2(float(Index) / float(Count), float(Bits) * 2.3283064365386963e-10);
}

void main()
{
	vec2 Coordinate = gl_FragCoord.xy / Prefilter.FaceSize * 2.0 - 1.0;
	vec3 Normal = normalize(GetFaceDirection(Prefilter.Face, Coordinate));

	// How much of the sphere one pixel of the picture and one pixel of this face cover, on average.
	float PicturePixelAngle = 4.0 * EnvironmentPi / Prefilter.PicturePixelCount;
	float FacePixelAngle = 4.0 * EnvironmentPi / (6.0 * Prefilter.FaceSize * Prefilter.FaceSize);

	if (Prefilter.Roughness <= 0.0)
	{
		// A mirror: the environment itself, at the detail this face can show.
		float Level = max(0.5 * log2(FacePixelAngle / PicturePixelAngle), 0.0);
		OutColor = vec4(ReadEnvironment(Normal, Level), 1.0);
		return;
	}

	// Any two directions at right angles to the normal.
	vec3 Helper = abs(Normal.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
	vec3 Tangent = normalize(cross(Helper, Normal));
	vec3 Bitangent = cross(Normal, Tangent);

	float Alpha = Prefilter.Roughness * Prefilter.Roughness;
	float AlphaSquared = Alpha * Alpha;

	vec3 Sum = vec3(0.0);
	float WeightSum = 0.0;
	for (uint Index = 0u; Index < uint(SampleCount); ++Index)
	{
		// A microfacet direction drawn from the GGX distribution, and the light direction it reflects the
		// viewer's line of sight to. The viewer is taken to look straight down the normal.
		vec2 Point = GetSamplePoint(Index, uint(SampleCount));
		float Around = 2.0 * EnvironmentPi * Point.x;
		float CosAngle = sqrt((1.0 - Point.y) / (1.0 + (AlphaSquared - 1.0) * Point.y));
		float SinAngle = sqrt(max(1.0 - CosAngle * CosAngle, 0.0));
		vec3 Halfway = Tangent * (SinAngle * cos(Around)) + Bitangent * (SinAngle * sin(Around)) + Normal * CosAngle;
		vec3 ToLight = 2.0 * dot(Normal, Halfway) * Halfway - Normal;

		float NormalDotLight = dot(Normal, ToLight);
		if (NormalDotLight <= 0.0)
		{
			continue;
		}

		// The part of the sphere this sample stands for decides how blurred a version of the picture it reads.
		float Denominator = CosAngle * CosAngle * (AlphaSquared - 1.0) + 1.0;
		float Distribution = AlphaSquared / (EnvironmentPi * Denominator * Denominator);
		float SampleAngle = 4.0 / (float(SampleCount) * Distribution + 1.0e-6);
		float Level = max(0.5 * log2(SampleAngle / PicturePixelAngle) + 1.0, 0.0);

		Sum += ReadEnvironment(ToLight, Level) * NormalDotLight;
		WeightSum += NormalDotLight;
	}

	OutColor = vec4(Sum / max(WeightSum, 1.0e-6), 1.0);
}