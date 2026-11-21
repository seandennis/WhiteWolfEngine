#pragma once

#include "CoreMinimal.h"

#include "Math/Color.h"
#include "Rendering/DirectionalLightComponent.h"
#include "Rendering/Image.h"
#include "Rendering/PointLightComponent.h"
#include "Rendering/TestSceneRendering.h"

#include <doctest/doctest.h>
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <numbers>

// The engine's lighting model and tone mapper, written out a second time: in C++, in double precision and
// independently of the shaders, from the definition of the model in the glTF 2.0 specification (appendix B) and
// of the Khronos PBR Neutral tone mapper. Tests light a surface and compare what was drawn with this.

/** Red, green and blue amounts of light, in linear space. */
using FReferenceColor = glm::dvec3;

inline const FReferenceColor ReferenceWhite(1.0);

/** How far a channel of a drawn pixel may be from the reference: 16-bit floats and rounding on the way. */
inline constexpr int32 PixelTolerance = 2;

/** The material and geometry of the surface point in the middle of the picture. */
struct FReferenceSurface
{
	FReferenceColor BaseColor = FReferenceColor(0.5);
	double Metallic = 0.0;
	double Roughness = 1.0;
	FReferenceColor EmissiveColor = FReferenceColor(0.0);

	/** The way the surface faces. The test squares face the camera unless they are turned. */
	FVector Normal = FVector(0.0, 0.0, 1.0);

	/** From the surface to the camera. */
	FVector ToCamera = FVector(0.0, 0.0, 1.0);
};

/** The light a surface sends to the camera when Illuminance arrives from the direction ToLight. */
inline FReferenceColor ReferenceReflect(
	const FReferenceSurface& Surface, const FVector& ToLight, const FReferenceColor& Illuminance)
{
	const double NormalDotLight = glm::dot(Surface.Normal, ToLight);
	if (NormalDotLight <= 0.0)
	{
		return FReferenceColor(0.0);
	}

	const double Metallic = std::clamp(Surface.Metallic, 0.0, 1.0);
	const double Roughness = std::clamp(Surface.Roughness, 0.045, 1.0);
	const double Alpha = Roughness * Roughness;
	const double AlphaSquared = Alpha * Alpha;
	const FReferenceColor DiffuseColor = Surface.BaseColor * (1.0 - Metallic);
	const FReferenceColor SpecularColor = glm::mix(FReferenceColor(0.04), Surface.BaseColor, Metallic);

	const FVector Halfway = glm::normalize(ToLight + Surface.ToCamera);
	const double NormalDotCamera = std::max(glm::dot(Surface.Normal, Surface.ToCamera), 1.0e-4);
	const double NormalDotHalfway = std::clamp(glm::dot(Surface.Normal, Halfway), 0.0, 1.0);
	const double CameraDotHalfway = std::clamp(glm::dot(Surface.ToCamera, Halfway), 0.0, 1.0);

	const double Denominator = NormalDotHalfway * NormalDotHalfway * (AlphaSquared - 1.0) + 1.0;
	const double Distribution = AlphaSquared / (std::numbers::pi * Denominator * Denominator);
	const double Visibility = 0.5 /
		(NormalDotCamera * std::sqrt(NormalDotLight * NormalDotLight * (1.0 - AlphaSquared) + AlphaSquared) +
			NormalDotLight * std::sqrt(NormalDotCamera * NormalDotCamera * (1.0 - AlphaSquared) + AlphaSquared));
	const FReferenceColor Fresnel =
		SpecularColor + (ReferenceWhite - SpecularColor) * std::pow(1.0 - CameraDotHalfway, 5.0);

	const FReferenceColor Specular = Fresnel * (Distribution * Visibility);
	const FReferenceColor Diffuse = (ReferenceWhite - Fresnel) * DiffuseColor / std::numbers::pi;
	return (Diffuse + Specular) * Illuminance * NormalDotLight;
}

/**
 * How much of the light of its whole surrounding a surface mirrors towards the camera: Brian Karis' approximation
 * of the lighting model summed over a surrounding, as in AmbientLight.glsl.
 */
inline FReferenceColor ReferenceEnvironmentReflectance(
	const FReferenceColor& SpecularColor, double Roughness, double NormalDotCamera)
{
	const double TermX = 1.0 - Roughness;
	const double TermY = 0.0425 - 0.0275 * Roughness;
	const double TermZ = 1.04 - 0.572 * Roughness;
	const double TermW = 0.022 * Roughness - 0.04;
	const double Blend = std::min(TermX * TermX, std::pow(2.0, -9.28 * NormalDotCamera)) * TermX + TermY;
	return SpecularColor * (-1.04 * Blend + TermZ) + FReferenceColor(1.04 * Blend + TermW);
}

/**
 * The light a surface sends to the camera from a sky light.
 *
 * @param Gathered The light the surface gathers as a matte surface: the irradiance for the way it faces.
 * @param Mirrored The light it mirrors: what the environment shows in the mirrored direction, blurred by the
 * surface's roughness.
 */
inline FReferenceColor ReferenceReflectEnvironment(
	const FReferenceSurface& Surface, const FReferenceColor& Gathered, const FReferenceColor& Mirrored)
{
	const double Metallic = std::clamp(Surface.Metallic, 0.0, 1.0);
	const double Roughness = std::clamp(Surface.Roughness, 0.0, 1.0);
	const FReferenceColor DiffuseColor = Surface.BaseColor * (1.0 - Metallic);
	const FReferenceColor SpecularColor = glm::mix(FReferenceColor(0.04), Surface.BaseColor, Metallic);
	const double NormalDotCamera = std::clamp(glm::dot(Surface.Normal, Surface.ToCamera), 0.0, 1.0);
	const FReferenceColor Reflectance = ReferenceEnvironmentReflectance(SpecularColor, Roughness, NormalDotCamera);
	return DiffuseColor * (ReferenceWhite - Reflectance) * Gathered / std::numbers::pi + Mirrored * Reflectance;
}

/** The light a surface sends to the camera in a surrounding that has the same light in every direction. */
inline FReferenceColor ReferenceReflectEvenEnvironment(const FReferenceSurface& Surface, const FReferenceColor& Light)
{
	return ReferenceReflectEnvironment(Surface, Light * std::numbers::pi, Light);
}

/** The Khronos PBR Neutral tone mapper. */
inline FReferenceColor ReferenceToneMap(FReferenceColor Color)
{
	const double StartCompression = 0.8 - 0.04;
	const double Desaturation = 0.15;

	const double Smallest = std::min({Color.r, Color.g, Color.b});
	const double Offset = Smallest < 0.08 ? Smallest - 6.25 * Smallest * Smallest : 0.04;
	Color -= Offset;

	const double Peak = std::max({Color.r, Color.g, Color.b});
	if (Peak < StartCompression)
	{
		return Color;
	}

	const double Headroom = 1.0 - StartCompression;
	const double NewPeak = 1.0 - Headroom * Headroom / (Peak + Headroom - StartCompression);
	Color *= NewPeak / Peak;

	const double Whiteness = 1.0 - 1.0 / (Desaturation * (Peak - NewPeak) + 1.0);
	return glm::mix(Color, FReferenceColor(NewPeak), Whiteness);
}

inline uint8 EncodeSrgb(double Linear)
{
	const double Clamped = std::clamp(Linear, 0.0, 1.0);
	const double Encoded = Clamped <= 0.0031308 ? 12.92 * Clamped : 1.055 * std::pow(Clamped, 1.0 / 2.4) - 0.055;
	return static_cast<uint8>(std::lround(Encoded * 255.0));
}

/** The pixel that light of the given amount ends up as, after exposure, tone mapping and encoding. */
inline FColor ReferencePixel(const FReferenceColor& SceneLight, double Exposure = 1.0)
{
	const FReferenceColor Shown = ReferenceToneMap(SceneLight * Exposure);
	return FColor(EncodeSrgb(Shown.r), EncodeSrgb(Shown.g), EncodeSrgb(Shown.b), 255);
}

inline bool IsClose(FColor Actual, FColor Expected)
{
	return std::abs(Actual.R - Expected.R) <= PixelTolerance && std::abs(Actual.G - Expected.G) <= PixelTolerance &&
		std::abs(Actual.B - Expected.B) <= PixelTolerance && std::abs(Actual.A - Expected.A) <= PixelTolerance;
}

/** Checks one pixel against the reference and says both colours when they differ. */
inline void CheckPixel(const FImage& Image, int32 X, int32 Y, FColor Expected)
{
	const FColor Actual = Image.GetPixel(X, Y);
	INFO("Pixel (",
		X,
		", ",
		Y,
		") is (",
		static_cast<int32>(Actual.R),
		", ",
		static_cast<int32>(Actual.G),
		", ",
		static_cast<int32>(Actual.B),
		", ",
		static_cast<int32>(Actual.A),
		") and the reference is (",
		static_cast<int32>(Expected.R),
		", ",
		static_cast<int32>(Expected.G),
		", ",
		static_cast<int32>(Expected.B),
		", ",
		static_cast<int32>(Expected.A),
		")");
	CHECK(IsClose(Actual, Expected));
}

// A lit test scene: a square two metres in front of the camera, lights, and a picture with a pixel exactly in
// its middle.

/** The pictures are 65 pixels a side, so that the centre of pixel 32 is exactly the middle of the picture. */
inline constexpr int32 PictureSize = 65;
inline constexpr int32 Middle = 32;

/** Where the test square is: two metres ahead of a camera at the origin, facing it. */
inline const FVector SquareLocation(0.0, 0.0, -2.0);

/** Adds the test square with the reference surface's material. */
inline WStaticMeshComponent* AddSurface(
	WScene& Scene, const FReferenceSurface& Surface, const FTransform& Transform = MakeTransform(SquareLocation))
{
	WStaticMeshComponent* const Component = AddSquare(Scene, Transform, LinearBlack);
	const FReferenceColor& Base = Surface.BaseColor;
	const FReferenceColor& Emissive = Surface.EmissiveColor;
	Component->Material->BaseColor =
		FLinearColor(static_cast<float>(Base.r), static_cast<float>(Base.g), static_cast<float>(Base.b));
	Component->Material->EmissiveColor =
		FLinearColor(static_cast<float>(Emissive.r), static_cast<float>(Emissive.g), static_cast<float>(Emissive.b));
	Component->Material->Metallic = static_cast<float>(Surface.Metallic);
	Component->Material->Roughness = static_cast<float>(Surface.Roughness);
	return Component;
}

/** Adds a directional light. With no rotation it shines along -Z, straight at the test square. */
inline WDirectionalLightComponent* AddSun(WScene& Scene,
	const FQuat& Rotation = FQuat(1.0f, 0.0f, 0.0f, 0.0f),
	const FLinearColor& Color = FLinearColor(1.0f, 1.0f, 1.0f),
	float Intensity = std::numbers::pi_v<float>,
	WEntity* Parent = nullptr)
{
	WEntity* const Entity = Scene.CreateEntity("Sun", Parent);
	REQUIRE(Entity != nullptr);
	Entity->SetRelativeTransform(MakeTransform(FVector(0.0), Rotation));

	WDirectionalLightComponent* const Light = Entity->AddComponent<WDirectionalLightComponent>();
	REQUIRE(Light != nullptr);
	Light->Color = Color;
	Light->Intensity = Intensity;
	return Light;
}

inline WPointLightComponent* AddLamp(WScene& Scene, const FVector& Location, float Intensity, float Range = 0.0f)
{
	WEntity* const Entity = Scene.CreateEntity("Lamp");
	REQUIRE(Entity != nullptr);
	Entity->SetRelativeTransform(MakeTransform(Location));

	WPointLightComponent* const Light = Entity->AddComponent<WPointLightComponent>();
	REQUIRE(Light != nullptr);
	Light->Intensity = Intensity;
	Light->Range = Range;
	return Light;
}

inline FImage RenderPicture(FSceneRenderTest& Test, const FCameraView& View = MakeWideView())
{
	return Test.Render(View, PictureSize, PictureSize);
}
