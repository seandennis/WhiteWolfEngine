#include "Rendering/EnvironmentMap.h"

#include "Math/LinearColor.h"
#include "Object/ObjectGlobals.h"
#include "Object/TestObjects.h"
#include "Rendering/HdrImage.h"
#include "Rendering/SkyLightComponent.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/TestSceneObjects.h"

#include <doctest/doctest.h>
#include <glm/geometric.hpp>

#include <cmath>
#include <limits>
#include <numbers>
#include <string>

namespace
{
	bool IsNear(const FVector3f& Actual, const FVector3f& Expected, float Tolerance = 1.0e-5f)
	{
		return glm::length(Actual - Expected) <= Tolerance;
	}

	bool IsNear(const FVector2f& Actual, const FVector2f& Expected, float Tolerance = 1.0e-5f)
	{
		return glm::length(Actual - Expected) <= Tolerance;
	}
}

TEST_SUITE("WEnvironmentMap")
{
	TEST_CASE("A new environment has no picture and gives no light")
	{
		const FScopedObjectTest ObjectTest;

		const WEnvironmentMap* const Environment = NewObject<WEnvironmentMap>();

		CHECK_FALSE(Environment->HasImage());
		CHECK(Environment->GetImage().IsEmpty());
		CHECK(Environment->GetImageVersion() == 0);
		CHECK(Environment->GetIrradiance() == FSphericalHarmonics());
	}

	TEST_CASE("The picture is a map of directions: its middle is ahead, its right half to the right, its top up")
	{
		CHECK(IsNear(WEnvironmentMap::GetDirectionOfCoordinate(FVector2f(0.5f, 0.5f)), FVector3f(0.0f, 0.0f, -1.0f)));
		CHECK(IsNear(WEnvironmentMap::GetDirectionOfCoordinate(FVector2f(0.75f, 0.5f)), FVector3f(1.0f, 0.0f, 0.0f)));
		CHECK(IsNear(WEnvironmentMap::GetDirectionOfCoordinate(FVector2f(0.25f, 0.5f)), FVector3f(-1.0f, 0.0f, 0.0f)));
		// The left and right edges meet behind the viewer.
		CHECK(IsNear(WEnvironmentMap::GetDirectionOfCoordinate(FVector2f(0.0f, 0.5f)), FVector3f(0.0f, 0.0f, 1.0f)));
		CHECK(IsNear(WEnvironmentMap::GetDirectionOfCoordinate(FVector2f(1.0f, 0.5f)), FVector3f(0.0f, 0.0f, 1.0f)));
		// The whole top row is straight up and the whole bottom row straight down.
		CHECK(IsNear(WEnvironmentMap::GetDirectionOfCoordinate(FVector2f(0.3f, 0.0f)), FVector3f(0.0f, 1.0f, 0.0f)));
		CHECK(IsNear(WEnvironmentMap::GetDirectionOfCoordinate(FVector2f(0.8f, 1.0f)), FVector3f(0.0f, -1.0f, 0.0f)));
		// A quarter of the way down is half way between up and the horizon.
		CHECK(IsNear(WEnvironmentMap::GetDirectionOfCoordinate(FVector2f(0.5f, 0.25f)),
			glm::normalize(FVector3f(0.0f, 1.0f, -1.0f))));

		CHECK(IsNear(WEnvironmentMap::GetCoordinateOfDirection(FVector3f(0.0f, 0.0f, -1.0f)), FVector2f(0.5f, 0.5f)));
		CHECK(IsNear(WEnvironmentMap::GetCoordinateOfDirection(FVector3f(1.0f, 0.0f, 0.0f)), FVector2f(0.75f, 0.5f)));
		CHECK(WEnvironmentMap::GetCoordinateOfDirection(FVector3f(0.0f, 1.0f, 0.0f)).y == doctest::Approx(0.0f));
		CHECK(WEnvironmentMap::GetCoordinateOfDirection(FVector3f(0.0f, -1.0f, 0.0f)).y == doctest::Approx(1.0f));
	}

	TEST_CASE("A direction and its place in the picture convert into each other")
	{
		for (int32 Step = 0; Step < 200; ++Step)
		{
			// Directions spread over the whole sphere.
			const float Height = 1.0f - 2.0f * (static_cast<float>(Step) + 0.5f) / 200.0f;
			const float Around = static_cast<float>(Step) * 2.399963f;
			const float Level = std::sqrt(1.0f - Height * Height);
			const FVector3f Direction(Level * std::cos(Around), Height, Level * std::sin(Around));

			const FVector2f Coordinate = WEnvironmentMap::GetCoordinateOfDirection(Direction);

			CHECK(Coordinate.x >= 0.0f);
			CHECK(Coordinate.x <= 1.0f);
			CHECK(Coordinate.y >= 0.0f);
			CHECK(Coordinate.y <= 1.0f);
			CHECK(IsNear(WEnvironmentMap::GetDirectionOfCoordinate(Coordinate), Direction, 1.0e-4f));
		}
	}

	TEST_CASE("An environment keeps its picture and knows the light a surface gathers from it")
	{
		const FScopedObjectTest ObjectTest;
		WEnvironmentMap* const Environment = NewObject<WEnvironmentMap>();
		const FHdrImage Image(32, 16, FLinearColor(1.0f, 2.0f, 4.0f));
		std::string Error;

		REQUIRE(Environment->SetImage(Image, Error));

		CHECK(Environment->HasImage());
		CHECK(Environment->GetImage() == Image);
		CHECK(Environment->GetImageVersion() != 0);
		const FVector3f Gathered = Environment->GetIrradiance().Evaluate(FVector3f(0.0f, 1.0f, 0.0f));
		CHECK(IsNear(Gathered, FVector3f(1.0f, 2.0f, 4.0f) * std::numbers::pi_v<float>, 0.1f));
	}

	TEST_CASE("Values that are not amounts of light are taken as no light")
	{
		const FScopedObjectTest ObjectTest;
		WEnvironmentMap* const Environment = NewObject<WEnvironmentMap>();
		FHdrImage Image(2, 1, FLinearColor(1.0f, 1.0f, 1.0f));
		Image.SetPixel(0,
			0,
			FLinearColor(-3.0f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), 0.5f));
		std::string Error;

		REQUIRE(Environment->SetImage(Image, Error));

		CHECK(Environment->GetImage().GetPixel(0, 0) == FLinearColor(0.0f, 0.0f, 0.0f));
		CHECK(Environment->GetImage().GetPixel(1, 0) == FLinearColor(1.0f, 1.0f, 1.0f));
	}

	TEST_CASE("The image version changes with every new picture and differs between environments")
	{
		const FScopedObjectTest ObjectTest;
		WEnvironmentMap* const First = NewObject<WEnvironmentMap>();
		WEnvironmentMap* const Second = NewObject<WEnvironmentMap>();
		const FHdrImage Image(4, 2);
		std::string Error;

		REQUIRE(First->SetImage(Image, Error));
		const uint64 FirstVersion = First->GetImageVersion();
		REQUIRE(Second->SetImage(Image, Error));
		REQUIRE(First->SetImage(Image, Error));

		CHECK(FirstVersion != Second->GetImageVersion());
		CHECK(First->GetImageVersion() != FirstVersion);
		CHECK(First->GetImageVersion() != Second->GetImageVersion());
	}

	TEST_CASE("An empty picture or one that is too large is refused, and the environment is left as it was")
	{
		const FScopedObjectTest ObjectTest;
		WEnvironmentMap* const Environment = NewObject<WEnvironmentMap>();
		const FHdrImage Image(4, 2, FLinearColor(1.0f, 1.0f, 1.0f));
		std::string Error;
		REQUIRE(Environment->SetImage(Image, Error));
		const uint64 Version = Environment->GetImageVersion();
		const FSphericalHarmonics Irradiance = Environment->GetIrradiance();

		CHECK_FALSE(Environment->SetImage(FHdrImage(), Error));
		CHECK(Error == "an environment map cannot be 0 by 0 pixels; each side must be between 1 and 16384");
		CHECK_FALSE(Environment->SetImage(FHdrImage(WEnvironmentMap::MaxDimension + 1, 1), Error));

		CHECK(Environment->GetImage() == Image);
		CHECK(Environment->GetImageVersion() == Version);
		CHECK(Environment->GetIrradiance() == Irradiance);
	}
}

TEST_SUITE("WSkyLightComponent")
{
	TEST_CASE("A new sky light is visible, has full intensity and no environment yet")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;

		const WSkyLightComponent* const Light = Scene->CreateEntity("Sky")->AddComponent<WSkyLightComponent>();

		REQUIRE(Light != nullptr);
		CHECK(Light->Environment == nullptr);
		CHECK(Light->Intensity == 1.0f);
		CHECK(Light->bVisible);
	}

	TEST_CASE("A sky light keeps its environment alive")
	{
		const FScopedSceneTest SceneTest;
		const FScopedTestScene Scene;
		WSkyLightComponent* const Light = Scene->CreateEntity("Sky")->AddComponent<WSkyLightComponent>();
		Light->Environment = NewObject<WEnvironmentMap>();
		const TWeakObjectPtr<WEnvironmentMap> Environment(Light->Environment.Get());

		CollectGarbage();
		CHECK(Environment.Get() != nullptr);

		Light->GetOwner()->Destroy();
		CollectGarbage();
		CHECK(Environment.Get() == nullptr);
	}
}
