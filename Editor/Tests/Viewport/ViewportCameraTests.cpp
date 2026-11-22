#include "Viewport/ViewportCamera.h"

#include <doctest/doctest.h>
#include <glm/geometric.hpp>

#include <limits>
#include <ostream>

namespace
{
	/** How far two directions may be apart and still count as the same. */
	constexpr float DirectionTolerance = 1.0e-5f;

	bool IsSameDirection(const FVector3f& Actual, const FVector3f& Expected)
	{
		return glm::length(Actual - Expected) <= DirectionTolerance;
	}

	/** A camera at the origin that is not turned or tilted. */
	FViewportCamera MakeLevelCamera()
	{
		FViewportCamera Camera;
		Camera.Location = FVector(0.0);
		Camera.Yaw = 0.0f;
		Camera.Pitch = 0.0f;
		return Camera;
	}
}

TEST_SUITE("FViewportCamera")
{
	TEST_CASE("A new camera is valid and looks at the origin of the scene from a short way off")
	{
		const FViewportCamera Camera;

		CHECK(Camera.IsValid());

		// The line of sight passes the origin within a fraction of the distance to it.
		const FVector ToOrigin = FVector(0.0) - Camera.Location;
		const double Distance = glm::length(ToOrigin);
		CHECK(Distance > 5.0);
		CHECK(Distance < 15.0);
		const FVector Forward(Camera.GetForward());
		const double DistanceAlongSight = glm::dot(ToOrigin, Forward);
		const double DistanceFromSight = glm::length(ToOrigin - Forward * DistanceAlongSight);
		CHECK(DistanceAlongSight > 0.0);
		CHECK(DistanceFromSight < 0.05 * Distance);
	}

	TEST_CASE("A camera that is not turned looks along -Z with +X to its right and +Y up")
	{
		const FViewportCamera Camera = MakeLevelCamera();

		CHECK(IsSameDirection(Camera.GetForward(), FVector3f(0.0f, 0.0f, -1.0f)));
		CHECK(IsSameDirection(Camera.GetRight(), FVector3f(1.0f, 0.0f, 0.0f)));
		CHECK(IsSameDirection(Camera.GetUp(), FVector3f(0.0f, 1.0f, 0.0f)));
	}

	TEST_CASE("Yaw turns the camera counter-clockwise as seen from above")
	{
		FViewportCamera Camera = MakeLevelCamera();

		Camera.Yaw = 90.0f;
		CHECK(IsSameDirection(Camera.GetForward(), FVector3f(-1.0f, 0.0f, 0.0f)));
		CHECK(IsSameDirection(Camera.GetRight(), FVector3f(0.0f, 0.0f, -1.0f)));

		Camera.Yaw = 180.0f;
		CHECK(IsSameDirection(Camera.GetForward(), FVector3f(0.0f, 0.0f, 1.0f)));

		Camera.Yaw = -90.0f;
		CHECK(IsSameDirection(Camera.GetForward(), FVector3f(1.0f, 0.0f, 0.0f)));
	}

	TEST_CASE("Pitch tilts the camera up and down, and leaves its right level")
	{
		FViewportCamera Camera = MakeLevelCamera();

		Camera.Pitch = 30.0f;
		CHECK(IsSameDirection(Camera.GetForward(), FVector3f(0.0f, 0.5f, -0.8660254f)));
		CHECK(IsSameDirection(Camera.GetRight(), FVector3f(1.0f, 0.0f, 0.0f)));

		Camera.Pitch = -30.0f;
		CHECK(IsSameDirection(Camera.GetForward(), FVector3f(0.0f, -0.5f, -0.8660254f)));

		Camera.Yaw = 90.0f;
		CHECK(IsSameDirection(Camera.GetForward(), FVector3f(-0.8660254f, -0.5f, 0.0f)));
		CHECK(Camera.GetRight().y == doctest::Approx(0.0f).epsilon(DirectionTolerance));
	}

	TEST_CASE("LookAt turns the camera to face a point")
	{
		FViewportCamera Camera = MakeLevelCamera();
		Camera.Location = FVector(1.0, 2.0, 3.0);
		FVector Target(0.0);
		FVector3f ExpectedForward(0.0f);

		SUBCASE("Ahead")
		{
			Target = FVector(1.0, 2.0, -7.0);
			ExpectedForward = FVector3f(0.0f, 0.0f, -1.0f);
		}
		SUBCASE("To the left")
		{
			Target = FVector(-9.0, 2.0, 3.0);
			ExpectedForward = FVector3f(-1.0f, 0.0f, 0.0f);
		}
		SUBCASE("Behind")
		{
			Target = FVector(1.0, 2.0, 13.0);
			ExpectedForward = FVector3f(0.0f, 0.0f, 1.0f);
		}
		SUBCASE("Below and to the right")
		{
			Target = FVector(4.0, -2.0, 3.0);
			ExpectedForward = FVector3f(0.6f, -0.8f, 0.0f);
		}
		SUBCASE("Somewhere")
		{
			Target = FVector(3.0, 5.0, -3.0);
			ExpectedForward = glm::normalize(FVector3f(2.0f, 3.0f, -6.0f));
		}

		REQUIRE(Camera.LookAt(Target));

		CHECK(IsSameDirection(Camera.GetForward(), ExpectedForward));
		CHECK(Camera.IsValid());
		CHECK(Camera.Location == FVector(1.0, 2.0, 3.0));
	}

	TEST_CASE("LookAt faces a point far from the origin exactly")
	{
		// Ten billion kilometres out, a metre is far below what single precision can tell apart.
		FViewportCamera Camera = MakeLevelCamera();
		Camera.Location = FVector(1.0e13, 0.0, 0.0);

		REQUIRE(Camera.LookAt(FVector(1.0e13 + 3.0, 4.0, 0.0)));

		CHECK(IsSameDirection(Camera.GetForward(), FVector3f(0.6f, 0.8f, 0.0f)));
	}

	TEST_CASE("LookAt straight up or down goes as steep as the camera can, and keeps its heading")
	{
		FViewportCamera Camera = MakeLevelCamera();
		Camera.Yaw = 40.0f;

		REQUIRE(Camera.LookAt(FVector(0.0, 10.0, 0.0)));
		CHECK(Camera.Pitch == FViewportCamera::MaxPitch);
		CHECK(Camera.Yaw == 40.0f);

		REQUIRE(Camera.LookAt(FVector(0.0, -10.0, 0.0)));
		CHECK(Camera.Pitch == -FViewportCamera::MaxPitch);
		CHECK(Camera.Yaw == 40.0f);
		CHECK(Camera.IsValid());
	}

	TEST_CASE("LookAt refuses the point the camera is at, and points that are not positions")
	{
		FViewportCamera Camera = MakeLevelCamera();
		Camera.Location = FVector(1.0, 2.0, 3.0);
		Camera.Yaw = 12.0f;
		Camera.Pitch = -5.0f;
		const FViewportCamera Before = Camera;

		CHECK_FALSE(Camera.LookAt(FVector(1.0, 2.0, 3.0)));
		CHECK_FALSE(Camera.LookAt(FVector(std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0)));
		CHECK_FALSE(Camera.LookAt(FVector(std::numeric_limits<double>::infinity(), 0.0, 0.0)));

		CHECK(Camera == Before);
	}

	TEST_CASE("The view the renderer draws from is where the camera is, facing the way it faces")
	{
		FViewportCamera Camera;
		Camera.Location = FVector(1.0e12, 5.0, -3.0);
		Camera.Yaw = 120.0f;
		Camera.Pitch = 15.0f;
		Camera.VerticalFieldOfView = 75.0f;
		Camera.Exposure = 0.25f;

		const FCameraView View = Camera.GetView();

		CHECK(View.IsValid());
		CHECK(View.Location == Camera.Location);
		CHECK(IsSameDirection(View.Rotation * FVector3f(0.0f, 0.0f, -1.0f), Camera.GetForward()));
		CHECK(IsSameDirection(View.Rotation * FVector3f(0.0f, 1.0f, 0.0f), Camera.GetUp()));
		CHECK(View.VerticalFieldOfView == 75.0f);
		CHECK(View.Exposure == 0.25f);
	}

	TEST_CASE("A camera is not valid with a number that no picture can be drawn from")
	{
		FViewportCamera Camera;

		SUBCASE("A location that is not a number")
		{
			Camera.Location.y = std::numeric_limits<double>::quiet_NaN();
		}
		SUBCASE("A location at infinity")
		{
			Camera.Location.z = std::numeric_limits<double>::infinity();
		}
		SUBCASE("A yaw that is not a number")
		{
			Camera.Yaw = std::numeric_limits<float>::quiet_NaN();
		}
		SUBCASE("A pitch beyond straight up")
		{
			Camera.Pitch = 89.5f;
		}
		SUBCASE("A pitch beyond straight down")
		{
			Camera.Pitch = -90.0f;
		}
		SUBCASE("A field of view of nothing")
		{
			Camera.VerticalFieldOfView = 0.0f;
		}
		SUBCASE("A field of view of half a turn")
		{
			Camera.VerticalFieldOfView = 180.0f;
		}
		SUBCASE("No exposure")
		{
			Camera.Exposure = 0.0f;
		}
		SUBCASE("An infinite exposure")
		{
			Camera.Exposure = std::numeric_limits<float>::infinity();
		}

		CHECK_FALSE(Camera.IsValid());
	}

	TEST_CASE("The steepest pitch either way is valid")
	{
		FViewportCamera Camera;

		Camera.Pitch = FViewportCamera::MaxPitch;
		CHECK(Camera.IsValid());

		Camera.Pitch = -FViewportCamera::MaxPitch;
		CHECK(Camera.IsValid());
	}
}
