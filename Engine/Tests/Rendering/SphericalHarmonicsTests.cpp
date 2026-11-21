#include "Rendering/SphericalHarmonics.h"

#include "Math/LinearColor.h"
#include "Rendering/EnvironmentMap.h"
#include "Rendering/HdrImage.h"

#include <doctest/doctest.h>
#include <glm/geometric.hpp>

#include <cmath>
#include <numbers>

namespace
{
	constexpr float Pi = std::numbers::pi_v<float>;

	const FVector3f Up(0.0f, 1.0f, 0.0f);
	const FVector3f Down(0.0f, -1.0f, 0.0f);
	const FVector3f Ahead(0.0f, 0.0f, -1.0f);
	const FVector3f Right(1.0f, 0.0f, 0.0f);

	bool IsNear(const FVector3f& Actual, const FVector3f& Expected, float Tolerance)
	{
		return std::abs(Actual.x - Expected.x) <= Tolerance && std::abs(Actual.y - Expected.y) <= Tolerance &&
			std::abs(Actual.z - Expected.z) <= Tolerance;
	}

	/** An environment that is one colour above the horizon and another below it. */
	FHdrImage MakeSkyAndGround(const FLinearColor& Sky, const FLinearColor& Ground)
	{
		FHdrImage Image(128, 64, Ground);
		for (int32 Y = 0; Y < 32; ++Y)
		{
			for (int32 X = 0; X < 128; ++X)
			{
				Image.SetPixel(X, Y, Sky);
			}
		}
		return Image;
	}

	FSphericalHarmonics GetIrradiance(const FHdrImage& Image)
	{
		return FSphericalHarmonics::ProjectEnvironment(Image).ToIrradiance();
	}
}

TEST_SUITE("FSphericalHarmonics")
{
	TEST_CASE("An empty picture has no light")
	{
		CHECK(FSphericalHarmonics::ProjectEnvironment(FHdrImage()) == FSphericalHarmonics());
		CHECK(FSphericalHarmonics().Evaluate(Up) == FVector3f(0.0f));
	}

	TEST_CASE("The harmonics of an even light give that light back in every direction")
	{
		const FSphericalHarmonics Light =
			FSphericalHarmonics::ProjectEnvironment(FHdrImage(64, 32, FLinearColor(2.0f, 1.0f, 0.5f)));

		for (const FVector3f& Direction : {Up, Down, Ahead, Right, glm::normalize(FVector3f(1.0f, -2.0f, 3.0f))})
		{
			CHECK(IsNear(Light.Evaluate(Direction), FVector3f(2.0f, 1.0f, 0.5f), 0.01f));
		}
	}

	TEST_CASE("A surface in an even light gathers pi times that light, whichever way it faces")
	{
		const FSphericalHarmonics Irradiance = GetIrradiance(FHdrImage(64, 32, FLinearColor(2.0f, 1.0f, 0.5f)));

		for (const FVector3f& Direction : {Up, Down, Ahead, Right, glm::normalize(FVector3f(-3.0f, 1.0f, 2.0f))})
		{
			CHECK(IsNear(Irradiance.Evaluate(Direction), FVector3f(2.0f, 1.0f, 0.5f) * Pi, 0.03f));
		}
	}

	TEST_CASE("Under a bright sky over a black ground, what faces up is lit fully, the side half and below not at all")
	{
		const FSphericalHarmonics Irradiance =
			GetIrradiance(MakeSkyAndGround(FLinearColor(1.0f, 1.0f, 1.0f), FLinearColor(0.0f, 0.0f, 0.0f)));

		CHECK(IsNear(Irradiance.Evaluate(Up), FVector3f(Pi), 0.05f));
		CHECK(IsNear(Irradiance.Evaluate(Ahead), FVector3f(Pi / 2.0f), 0.05f));
		CHECK(IsNear(Irradiance.Evaluate(Right), FVector3f(Pi / 2.0f), 0.05f));
		CHECK(IsNear(Irradiance.Evaluate(Down), FVector3f(0.0f), 0.05f));
	}

	TEST_CASE("Each colour is summed up on its own")
	{
		// A blue sky over a red ground.
		const FSphericalHarmonics Irradiance =
			GetIrradiance(MakeSkyAndGround(FLinearColor(0.0f, 0.0f, 1.0f), FLinearColor(1.0f, 0.0f, 0.0f)));

		CHECK(IsNear(Irradiance.Evaluate(Up), FVector3f(0.0f, 0.0f, Pi), 0.05f));
		CHECK(IsNear(Irradiance.Evaluate(Down), FVector3f(Pi, 0.0f, 0.0f), 0.05f));
		CHECK(IsNear(Irradiance.Evaluate(Ahead), FVector3f(Pi / 2.0f, 0.0f, Pi / 2.0f), 0.05f));
	}

	TEST_CASE("A single light in the surrounding lights most what faces it")
	{
		// A patch of light to the right, a little above the horizon, in an otherwise dark surrounding.
		FHdrImage Image(128, 64, FLinearColor(0.0f, 0.0f, 0.0f));
		const FVector3f ToLight = glm::normalize(FVector3f(1.0f, 0.5f, 0.0f));
		const FVector2f Coordinate = WEnvironmentMap::GetCoordinateOfDirection(ToLight);
		const int32 CentreX = static_cast<int32>(Coordinate.x * 128.0f);
		const int32 CentreY = static_cast<int32>(Coordinate.y * 64.0f);
		for (int32 Y = CentreY - 2; Y <= CentreY + 2; ++Y)
		{
			for (int32 X = CentreX - 2; X <= CentreX + 2; ++X)
			{
				Image.SetPixel(X, Y, FLinearColor(100.0f, 100.0f, 100.0f));
			}
		}

		const FSphericalHarmonics Irradiance = GetIrradiance(Image);

		const float Facing = Irradiance.Evaluate(ToLight).x;
		CHECK(Facing > 0.5f);
		CHECK(Facing > Irradiance.Evaluate(Up).x);
		CHECK(Facing > Irradiance.Evaluate(Ahead).x);
		CHECK(Facing > 4.0f * std::abs(Irradiance.Evaluate(-ToLight).x));
	}

	TEST_CASE("The size of the picture does not change the light it stands for")
	{
		const FLinearColor Sky(1.0f, 2.0f, 3.0f);
		const FLinearColor Ground(0.5f, 0.25f, 0.0f);
		FHdrImage Small(16, 8, Ground);
		for (int32 Y = 0; Y < 4; ++Y)
		{
			for (int32 X = 0; X < 16; ++X)
			{
				Small.SetPixel(X, Y, Sky);
			}
		}

		const FSphericalHarmonics FromSmall = GetIrradiance(Small);
		const FSphericalHarmonics FromLarge = GetIrradiance(MakeSkyAndGround(Sky, Ground));

		for (const FVector3f& Direction : {Up, Down, Ahead, glm::normalize(FVector3f(1.0f, 1.0f, 1.0f))})
		{
			// To a few percent of the brightest value, about ten: sixteen by eight pixels are a coarse sum.
			CHECK(IsNear(FromSmall.Evaluate(Direction), FromLarge.Evaluate(Direction), 0.3f));
		}
	}
}
