#include "Rendering/CameraView.h"

#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec4.hpp>

#include <cmath>
#include <initializer_list>
#include <limits>

namespace
{
	/** A position in clip space after the division by w. */
	struct FProjectedPoint
	{
		float X = 0.0f;
		float Y = 0.0f;
		float Depth = 0.0f;
	};

	/** Projects a position given in view space. */
	FProjectedPoint Project(const FCameraView& View, float AspectRatio, const FVector3f& ViewPosition)
	{
		const glm::vec4 Clip = View.GetProjectionMatrix(AspectRatio) * glm::vec4(ViewPosition, 1.0f);
		REQUIRE(Clip.w > 0.0f);
		return FProjectedPoint{Clip.x / Clip.w, Clip.y / Clip.w, Clip.z / Clip.w};
	}

	bool IsNear(const FVector3f& Actual, const FVector3f& Expected, float Tolerance = 1.0e-5f)
	{
		return std::abs(Actual.x - Expected.x) <= Tolerance && std::abs(Actual.y - Expected.y) <= Tolerance &&
			std::abs(Actual.z - Expected.z) <= Tolerance;
	}

	/** A rotation of Degrees about the Y axis, which turns a camera to its left for positive angles. */
	FQuat YawRotation(float Degrees)
	{
		return glm::angleAxis(glm::radians(Degrees), FVector3f(0.0f, 1.0f, 0.0f));
	}
}

TEST_SUITE("FCameraView")
{
	TEST_CASE("The default view is valid and looks along -Z from the origin")
	{
		const FCameraView View;

		CHECK(View.IsValid());
		CHECK(View.Location == FVector(0.0));
		// A point straight ahead of the camera lands in the middle of the picture.
		const FProjectedPoint Ahead = Project(View, 1.0f, FVector3f(0.0f, 0.0f, -5.0f));
		CHECK(Ahead.X == doctest::Approx(0.0f));
		CHECK(Ahead.Y == doctest::Approx(0.0f));
	}

	TEST_CASE("Depth is one at the near plane and falls towards zero with distance, without a far plane")
	{
		FCameraView View;
		View.NearPlane = 0.5f;

		CHECK(Project(View, 1.0f, FVector3f(0.0f, 0.0f, -0.5f)).Depth == doctest::Approx(1.0f));
		CHECK(Project(View, 1.0f, FVector3f(0.0f, 0.0f, -1.0f)).Depth == doctest::Approx(0.5f));
		CHECK(Project(View, 1.0f, FVector3f(0.0f, 0.0f, -100.0f)).Depth == doctest::Approx(0.005f));

		// Something a light-year away is still in front of the far end of the depth range.
		const float VeryFarDepth = Project(View, 1.0f, FVector3f(0.0f, 0.0f, -9.46e15f)).Depth;
		CHECK(VeryFarDepth > 0.0f);
		CHECK(VeryFarDepth < 1.0e-12f);
	}

	TEST_CASE("Two distant objects a small step apart still have different depths")
	{
		const FCameraView View;

		// With a conventional depth buffer these two, a kilometre away and a metre apart, would be far less
		// distinct than two objects near the camera. Reversed depth keeps them apart by a comfortable margin.
		const float Nearer = Project(View, 1.0f, FVector3f(0.0f, 0.0f, -1000.0f)).Depth;
		const float Farther = Project(View, 1.0f, FVector3f(0.0f, 0.0f, -1001.0f)).Depth;

		CHECK(Nearer > Farther);
		CHECK((Nearer - Farther) / Nearer > 1.0e-4f);
	}

	TEST_CASE("The field of view is the angle between the top and bottom edges of the picture")
	{
		FCameraView View;
		View.VerticalFieldOfView = 90.0f;

		// With a 90 degree field of view the edge is as far from the centre as the point is from the camera.
		CHECK(Project(View, 1.0f, FVector3f(0.0f, 10.0f, -10.0f)).Y == doctest::Approx(1.0f));
		CHECK(Project(View, 1.0f, FVector3f(0.0f, -10.0f, -10.0f)).Y == doctest::Approx(-1.0f));

		View.VerticalFieldOfView = 60.0f;
		const float HalfHeight = 10.0f * std::tan(glm::radians(30.0f));
		CHECK(Project(View, 1.0f, FVector3f(0.0f, HalfHeight, -10.0f)).Y == doctest::Approx(1.0f));
	}

	TEST_CASE("Up in the world is up in the picture and right is right")
	{
		const FCameraView View;

		const FProjectedPoint UpAndRight = Project(View, 1.0f, FVector3f(1.0f, 2.0f, -10.0f));

		CHECK(UpAndRight.X > 0.0f);
		CHECK(UpAndRight.Y > 0.0f);
		CHECK(UpAndRight.Y > UpAndRight.X);
	}

	TEST_CASE("A wider picture shows more to the sides and the same from top to bottom")
	{
		FCameraView View;
		View.VerticalFieldOfView = 90.0f;

		const FProjectedPoint Square = Project(View, 1.0f, FVector3f(10.0f, 10.0f, -10.0f));
		const FProjectedPoint Wide = Project(View, 2.0f, FVector3f(10.0f, 10.0f, -10.0f));

		CHECK(Square.X == doctest::Approx(1.0f));
		CHECK(Wide.X == doctest::Approx(0.5f));
		CHECK(Wide.Y == doctest::Approx(Square.Y));
	}

	TEST_CASE("A position behind the camera gets a negative w, so it is clipped away")
	{
		const FCameraView View;

		const glm::vec4 Clip = View.GetProjectionMatrix(1.0f) * glm::vec4(0.0f, 0.0f, 5.0f, 1.0f);

		CHECK(Clip.w < 0.0f);
	}

	TEST_CASE("The view matrix turns the world to face the camera and does not move it")
	{
		FCameraView View;
		View.Location = FVector(1000.0, 2000.0, 3000.0);
		// Turned a quarter turn to its left, the camera looks along the world's -X axis.
		View.Rotation = YawRotation(90.0f);

		const FMatrix4f Matrix = View.GetViewMatrix();

		const FVector3f Ahead = FVector3f(Matrix * glm::vec4(-1.0f, 0.0f, 0.0f, 1.0f));
		const FVector3f Up = FVector3f(Matrix * glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
		const FVector3f Right = FVector3f(Matrix * glm::vec4(0.0f, 0.0f, -1.0f, 1.0f));
		CHECK(IsNear(Ahead, FVector3f(0.0f, 0.0f, -1.0f)));
		CHECK(IsNear(Up, FVector3f(0.0f, 1.0f, 0.0f)));
		CHECK(IsNear(Right, FVector3f(1.0f, 0.0f, 0.0f)));
		// The camera's location is not in the view matrix: the origin stays the origin.
		CHECK(FVector3f(Matrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)) == FVector3f(0.0f));
	}

	TEST_CASE("An object is placed relative to the camera, with its rotation and scale")
	{
		FCameraView View;
		View.Location = FVector(10.0, 0.0, 0.0);
		FTransform Object;
		Object.Location = FVector(13.0, 1.0, -2.0);
		Object.Rotation = YawRotation(90.0f);
		Object.Scale = FVector3f(2.0f, 3.0f, 4.0f);

		const FMatrix4f Matrix = View.GetCameraRelativeMatrix(Object);

		// The object's origin is three metres to the side of the camera, whatever the camera's rotation.
		CHECK(IsNear(FVector3f(Matrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)), FVector3f(3.0f, 1.0f, -2.0f)));
		// A local point is scaled, then rotated, then moved: (1, 1, 0) becomes (2, 3, 0), turns to (0, 3, -2).
		CHECK(IsNear(FVector3f(Matrix * glm::vec4(1.0f, 1.0f, 0.0f, 1.0f)), FVector3f(3.0f, 4.0f, -4.0f)));
		// The same as the transform itself computes in double precision.
		const FVector Expected = Object.TransformPosition(FVector(0.5, -1.0, 2.0)) - View.Location;
		CHECK(IsNear(FVector3f(Matrix * glm::vec4(0.5f, -1.0f, 2.0f, 1.0f)), FVector3f(Expected), 1.0e-4f));
	}

	TEST_CASE("The offset from the camera is exact however far both are from the world origin")
	{
		// Ten billion kilometres from the origin, single precision cannot tell positions a kilometre apart from
		// each other. The offset is taken in double precision first, so it is exactly what it is at the origin.
		for (const double Distance : {0.0, 1.0e6, 1.0e9, 1.0e13})
		{
			FCameraView View;
			View.Location = FVector(Distance, -Distance, Distance);
			FTransform Object;
			Object.Location = View.Location + FVector(0.125, -2.5, 4.0);

			const FMatrix4f Matrix = View.GetCameraRelativeMatrix(Object);

			CAPTURE(Distance);
			CHECK(FVector3f(Matrix[3]) == FVector3f(0.125f, -2.5f, 4.0f));
		}
	}

	TEST_CASE("A view is invalid if a picture cannot be drawn from it")
	{
		const float Infinity = std::numeric_limits<float>::infinity();
		const float NotANumber = std::numeric_limits<float>::quiet_NaN();

		FCameraView View;
		View.VerticalFieldOfView = FCameraView::MinFieldOfView;
		CHECK(View.IsValid());
		View.VerticalFieldOfView = FCameraView::MaxFieldOfView;
		CHECK(View.IsValid());

		for (const float FieldOfView : {0.0f, -60.0f, 0.5f, 179.5f, 180.0f, 360.0f, Infinity, NotANumber})
		{
			View = FCameraView();
			View.VerticalFieldOfView = FieldOfView;
			CAPTURE(FieldOfView);
			CHECK_FALSE(View.IsValid());
		}

		for (const float NearPlane : {0.0f, -0.1f, Infinity, NotANumber})
		{
			View = FCameraView();
			View.NearPlane = NearPlane;
			CAPTURE(NearPlane);
			CHECK_FALSE(View.IsValid());
		}

		for (const float Exposure : {0.0f, -1.0f, Infinity, NotANumber})
		{
			View = FCameraView();
			View.Exposure = Exposure;
			CAPTURE(Exposure);
			CHECK_FALSE(View.IsValid());
		}

		View = FCameraView();
		View.Location.y = std::numeric_limits<double>::infinity();
		CHECK_FALSE(View.IsValid());

		View = FCameraView();
		View.Location.z = std::numeric_limits<double>::quiet_NaN();
		CHECK_FALSE(View.IsValid());

		View = FCameraView();
		View.Rotation = FQuat(0.0f, 0.0f, 0.0f, 0.0f);
		CHECK_FALSE(View.IsValid());

		View = FCameraView();
		View.Rotation = FQuat(2.0f, 0.0f, 0.0f, 0.0f);
		CHECK_FALSE(View.IsValid());

		View = FCameraView();
		View.Rotation = FQuat(NotANumber, 0.0f, 0.0f, 0.0f);
		CHECK_FALSE(View.IsValid());
	}

	TEST_CASE("Asking for a projection with an aspect ratio that is not positive is a programming error")
	{
		const FScopedLogCapture Capture;
		const FCameraView View;

		CHECK_THROWS_AS(View.GetProjectionMatrix(0.0f), FFatalErrorException);
		CHECK_THROWS_AS(View.GetProjectionMatrix(-1.0f), FFatalErrorException);
	}

	TEST_CASE("The ray through the middle of the picture goes straight ahead, from the near plane")
	{
		FCameraView View;
		View.NearPlane = 0.25f;

		const FRay Ray = View.GetRay(FVector2f(0.0f, 0.0f), 16.0f / 9.0f);

		CHECK(Ray.Direction == FVector3f(0.0f, 0.0f, -1.0f));
		CHECK(Ray.Origin == FVector(0.0, 0.0, -0.25));
	}

	TEST_CASE("The rays through the edges of the picture are half the field of view from straight ahead")
	{
		FCameraView View;
		View.VerticalFieldOfView = 90.0f;
		constexpr float AspectRatio = 2.0f;

		// Half of ninety degrees up, the ray rises as fast as it goes ahead; to the side, in a picture twice as
		// wide as high, twice as fast.
		const FRay Top = View.GetRay(FVector2f(0.0f, 1.0f), AspectRatio);
		const FRay Right = View.GetRay(FVector2f(1.0f, 0.0f), AspectRatio);
		const FRay BottomLeft = View.GetRay(FVector2f(-1.0f, -1.0f), AspectRatio);

		CHECK(IsNear(Top.Direction, glm::normalize(FVector3f(0.0f, 1.0f, -1.0f))));
		CHECK(IsNear(Right.Direction, glm::normalize(FVector3f(2.0f, 0.0f, -1.0f))));
		CHECK(IsNear(BottomLeft.Direction, glm::normalize(FVector3f(-2.0f, -1.0f, -1.0f))));
	}

	TEST_CASE("What lies along the ray through a point of the picture is drawn at that point")
	{
		FCameraView View;
		View.Location = FVector(120.0, -4.0, 37.5);
		View.Rotation = YawRotation(130.0f) * glm::angleAxis(glm::radians(-25.0f), FVector3f(1.0f, 0.0f, 0.0f));
		View.VerticalFieldOfView = 70.0f;
		View.NearPlane = 0.5f;
		constexpr float AspectRatio = 1.6f;

		// The last of the points is beyond the edge of the picture, where the picture would go on.
		for (const FVector2f& PicturePoint : {FVector2f(0.0f, 0.0f),
				 FVector2f(1.0f, 1.0f),
				 FVector2f(-1.0f, -1.0f),
				 FVector2f(0.3f, -0.7f),
				 FVector2f(-1.5f, 0.2f)})
		{
			const FRay Ray = View.GetRay(PicturePoint, AspectRatio);
			CHECK(glm::length(Ray.Direction) == doctest::Approx(1.0f));

			for (const double Distance : {0.0, 1.0, 250.0})
			{
				const FVector3f FromCamera = FVector3f(Ray.GetPoint(Distance) - View.Location);
				const FVector3f InViewSpace = FVector3f(View.GetViewMatrix() * glm::vec4(FromCamera, 1.0f));
				const FProjectedPoint Drawn = Project(View, AspectRatio, InViewSpace);

				CHECK(Drawn.X == doctest::Approx(PicturePoint.x).epsilon(1.0e-4));
				CHECK(Drawn.Y == doctest::Approx(PicturePoint.y).epsilon(1.0e-4));
				if (Distance == 0.0)
				{
					// Where the ray begins is on the near plane, whose depth is one.
					CHECK(Drawn.Depth == doctest::Approx(1.0f).epsilon(1.0e-4));
				}
			}
		}
	}

	TEST_CASE("A ray begins exactly where the camera is, however far that is from the origin of the world")
	{
		FCameraView View;
		View.Location = FVector(4.0e9, -2.0e9, 1.0e9);
		View.NearPlane = 0.125f;

		const FRay Ray = View.GetRay(FVector2f(0.0f, 0.0f), 1.0f);

		CHECK(Ray.Origin - View.Location == FVector(0.0, 0.0, -0.125));
	}

	TEST_CASE("Asking for a ray with an aspect ratio that is not positive is a programming error")
	{
		const FScopedLogCapture Capture;
		const FCameraView View;

		CHECK_THROWS_AS(View.GetRay(FVector2f(0.0f, 0.0f), 0.0f), FFatalErrorException);
		CHECK_THROWS_AS(View.GetRay(FVector2f(0.0f, 0.0f), -2.0f), FFatalErrorException);
	}
}
