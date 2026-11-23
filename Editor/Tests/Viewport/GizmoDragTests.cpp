#include "Viewport/GizmoDrag.h"

#include "Math/Transform.h"
#include "Viewport/GizmoSettings.h"
#include "Viewport/TransformGizmo.h"
#include "Viewport/ViewportCamera.h"

#include <doctest/doctest.h>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>

#include <cmath>
#include <initializer_list>
#include <numbers>
#include <optional>
#include <ostream>

// These tests drag the handles of a gizmo that is seen from straight in front, as in the tests of FTransformGizmo:
// its middle is at (100, 100) of a picture of 200 pixels a side, ten metres ahead of the camera, a pixel is ten
// centimetres there, and its axes are 50 pixels and five metres long, X to the right, Y upwards and Z at the camera.

namespace
{
	const FVector2f PictureSize(200.0f, 200.0f);
	const FVector Pivot(0.0, 0.0, -10.0);
	constexpr float AxisLength = 50.0f;
	const FQuat NoRotation(1.0f, 0.0f, 0.0f, 0.0f);

	FTransformGizmo MakeFrontGizmo(const FQuat& Orientation = NoRotation)
	{
		FCameraView View;
		View.VerticalFieldOfView = 90.0f;
		return FTransformGizmo(View, PictureSize, Pivot, Orientation, AxisLength);
	}

	bool IsNear(const FVector& Actual, const FVector& Expected, double Tolerance = 1.0e-4)
	{
		return glm::length(Actual - Expected) <= Tolerance;
	}

	bool IsNear(const FVector3f& Actual, const FVector3f& Expected, float Tolerance = 1.0e-4f)
	{
		return glm::length(Actual - Expected) <= Tolerance;
	}

	/** Returns a thing's transform at a place, turned and sized as nothing is. */
	FTransform MakeTransformAt(const FVector& Location)
	{
		FTransform Transform;
		Transform.Location = Location;
		return Transform;
	}

	/** Returns the settings with which things move, turn and grow in the given steps. */
	FGizmoSettings MakeSteps(double LocationStep, float RotationStep, float ScaleStep)
	{
		FGizmoSettings Settings;
		Settings.bSnapLocation = true;
		Settings.LocationSnap = LocationStep;
		Settings.bSnapRotation = true;
		Settings.RotationSnap = RotationStep;
		Settings.bSnapScale = true;
		Settings.ScaleSnap = ScaleStep;
		return Settings;
	}
}

TEST_SUITE("FGizmoDrag")
{
	TEST_CASE("A drag that has gone nowhere leaves a thing exactly as it was")
	{
		FTransform Start;
		Start.Location = FVector(1.0e9 + 0.125, -3.0, 7.5);
		Start.Rotation = glm::angleAxis(glm::radians(33.0f), glm::normalize(FVector3f(1.0f, 2.0f, 3.0f)));
		Start.Scale = FVector3f(1.5f, 0.25f, 3.0f);

		for (const EGizmoMode Mode : {EGizmoMode::Translate, EGizmoMode::Rotate, EGizmoMode::Scale})
		{
			const FGizmoDrag Drag(MakeFrontGizmo(), Mode, EGizmoHandle::X, FVector2f(130.0f, 100.0f));

			CHECK(Drag.GetMode() == Mode);
			CHECK(Drag.GetHandle() == EGizmoHandle::X);
			CHECK(Drag.GetTranslation() == FVector(0.0));
			CHECK(Drag.GetAngle() == 0.0f);
			CHECK(Drag.GetScale() == FVector3f(1.0f));
			CHECK(Drag.Apply(Start) == Start);
		}
	}

	TEST_CASE("An arrow moves a thing along its axis, as far as the mouse goes along it in the picture")
	{
		FGizmoDrag Drag(MakeFrontGizmo(), EGizmoMode::Translate, EGizmoHandle::X, FVector2f(130.0f, 100.0f));

		// Twenty pixels to the right are two metres along X.
		Drag.MoveTo(FVector2f(150.0f, 100.0f), FGizmoSettings());
		CHECK(IsNear(Drag.GetTranslation(), FVector(2.0, 0.0, 0.0)));
		CHECK(IsNear(Drag.Apply(MakeTransformAt(FVector(3.0, 4.0, 5.0))).Location, FVector(5.0, 4.0, 5.0)));

		// What the mouse does across the axis does not count: thirty pixels down, the thing is where it was.
		Drag.MoveTo(FVector2f(150.0f, 130.0f), FGizmoSettings());
		CHECK(IsNear(Drag.GetTranslation(), FVector(2.0, 0.0, 0.0)));

		// Back past where it took hold, the thing goes the other way; the whole drag counts, not its last part.
		Drag.MoveTo(FVector2f(95.0f, 100.0f), FGizmoSettings());
		CHECK(IsNear(Drag.GetTranslation(), FVector(-3.5, 0.0, 0.0)));

		// Moving a thing neither turns nor sizes it.
		FTransform Start = MakeTransformAt(FVector(1.0, 1.0, 1.0));
		Start.Rotation = glm::angleAxis(glm::radians(45.0f), FVector3f(0.0f, 1.0f, 0.0f));
		Start.Scale = FVector3f(2.0f);
		const FTransform Moved = Drag.Apply(Start);
		CHECK(Moved.Rotation == Start.Rotation);
		CHECK(Moved.Scale == Start.Scale);
	}

	TEST_CASE("The arrow of another axis moves along that axis")
	{
		FGizmoDrag Drag(MakeFrontGizmo(), EGizmoMode::Translate, EGizmoHandle::Y, FVector2f(100.0f, 70.0f));

		// Thirty pixels up in the picture, which counts downwards, are three metres up.
		Drag.MoveTo(FVector2f(100.0f, 40.0f), FGizmoSettings());

		CHECK(IsNear(Drag.GetTranslation(), FVector(0.0, 3.0, 0.0)));
	}

	TEST_CASE("An arrow of a gizmo that is turned moves along the gizmo's own axis")
	{
		// A quarter turn around Z: the gizmo's X axis points up in the world.
		const FTransformGizmo Gizmo = MakeFrontGizmo(glm::angleAxis(glm::radians(90.0f), FVector3f(0.0f, 0.0f, 1.0f)));
		FGizmoDrag Drag(Gizmo, EGizmoMode::Translate, EGizmoHandle::X, FVector2f(100.0f, 70.0f));

		Drag.MoveTo(FVector2f(100.0f, 50.0f), FGizmoSettings());

		CHECK(IsNear(Drag.GetTranslation(), FVector(0.0, 2.0, 0.0)));
	}

	TEST_CASE("A square moves a thing in its plane, and the point that the mouse took stays under the mouse")
	{
		FGizmoDrag Drag(MakeFrontGizmo(), EGizmoMode::Translate, EGizmoHandle::XY, FVector2f(118.0f, 82.0f));

		// Ten pixels to the right and ten up are a metre along X and a metre along Y.
		Drag.MoveTo(FVector2f(128.0f, 72.0f), FGizmoSettings());
		CHECK(IsNear(Drag.GetTranslation(), FVector(1.0, 1.0, 0.0)));

		Drag.MoveTo(FVector2f(68.0f, 122.0f), FGizmoSettings());
		CHECK(IsNear(Drag.GetTranslation(), FVector(-5.0, -4.0, 0.0)));
	}

	TEST_CASE("With steps, a thing moves by whole steps from where it was")
	{
		const FGizmoSettings Steps = MakeSteps(0.5, 15.0f, 0.25f);

		FGizmoDrag Arrow(MakeFrontGizmo(), EGizmoMode::Translate, EGizmoHandle::X, FVector2f(130.0f, 100.0f));
		// 2.3 metres are nearer to two and a half than to two; 2.2 are nearer to two.
		Arrow.MoveTo(FVector2f(153.0f, 100.0f), Steps);
		CHECK(IsNear(Arrow.GetTranslation(), FVector(2.5, 0.0, 0.0)));
		Arrow.MoveTo(FVector2f(152.0f, 100.0f), Steps);
		CHECK(IsNear(Arrow.GetTranslation(), FVector(2.0, 0.0, 0.0)));
		// Less than half a step is no step.
		Arrow.MoveTo(FVector2f(132.0f, 100.0f), Steps);
		CHECK(Arrow.GetTranslation() == FVector(0.0));
		// It is the move that goes in steps: a thing that was between them stays between them.
		Arrow.MoveTo(FVector2f(140.0f, 100.0f), Steps);
		CHECK(IsNear(Arrow.Apply(MakeTransformAt(FVector(0.37, 0.0, 0.0))).Location, FVector(1.37, 0.0, 0.0)));

		// In a plane, each of its two axes goes in steps by itself.
		FGizmoDrag Square(MakeFrontGizmo(), EGizmoMode::Translate, EGizmoHandle::XY, FVector2f(118.0f, 82.0f));
		Square.MoveTo(FVector2f(131.0f, 75.0f), Steps);
		CHECK(IsNear(Square.GetTranslation(), FVector(1.5, 0.5, 0.0)));
	}

	TEST_CASE("An arrow along the line of sight cannot be dragged: the mouse says nothing about it")
	{
		FGizmoDrag Drag(MakeFrontGizmo(), EGizmoMode::Translate, EGizmoHandle::Z, FVector2f(100.0f, 100.0f));

		Drag.MoveTo(FVector2f(150.0f, 120.0f), FGizmoSettings());

		CHECK(Drag.GetTranslation() == FVector(0.0));
	}

	TEST_CASE("A ring turns a thing around its axis by as much as the mouse goes along the ring")
	{
		// The ring around Z is a circle in the picture. Taken at its rightmost point, it runs upwards there.
		FGizmoDrag Drag(MakeFrontGizmo(), EGizmoMode::Rotate, EGizmoHandle::Z, FVector2f(150.0f, 100.0f));

		// The length of an axis along the ring is one radian: a quarter turn is half of pi times fifty pixels.
		const float QuarterTurn = 50.0f * std::numbers::pi_v<float> * 0.5f;
		Drag.MoveTo(FVector2f(150.0f, 100.0f - QuarterTurn), FGizmoSettings());
		CHECK(Drag.GetAngle() == doctest::Approx(90.0f).epsilon(1.0e-4));

		// A thing at the gizmo's place turns where it is: counter-clockwise, seen from where the axis points.
		const FTransform Turned = Drag.Apply(MakeTransformAt(Pivot));
		CHECK(IsNear(Turned.Location, Pivot));
		CHECK(IsNear(Turned.Rotation * FVector3f(1.0f, 0.0f, 0.0f), FVector3f(0.0f, 1.0f, 0.0f)));

		// What the mouse does across the ring does not count.
		Drag.MoveTo(FVector2f(190.0f, 100.0f), FGizmoSettings());
		CHECK(Drag.GetAngle() == doctest::Approx(0.0f));

		// Downwards is the other way round.
		Drag.MoveTo(FVector2f(150.0f, 150.0f), FGizmoSettings());
		CHECK(Drag.GetAngle() == doctest::Approx(-glm::degrees(1.0f)).epsilon(1.0e-4));
	}

	TEST_CASE("A thing that is not at the gizmo's place goes around that place as it turns")
	{
		FGizmoDrag Drag(MakeFrontGizmo(), EGizmoMode::Rotate, EGizmoHandle::Z, FVector2f(150.0f, 100.0f));
		const float QuarterTurn = 50.0f * std::numbers::pi_v<float> * 0.5f;
		Drag.MoveTo(FVector2f(150.0f, 100.0f - QuarterTurn), FGizmoSettings());

		// Two metres to the right of the gizmo, and already turned a quarter around Y.
		FTransform Start = MakeTransformAt(Pivot + FVector(2.0, 0.0, 0.0));
		Start.Rotation = glm::angleAxis(glm::radians(90.0f), FVector3f(0.0f, 1.0f, 0.0f));
		Start.Scale = FVector3f(1.0f, 2.0f, 3.0f);
		const FTransform Turned = Drag.Apply(Start);

		// It is two metres above the gizmo now, and its own turn comes before the ring's.
		CHECK(IsNear(Turned.Location, Pivot + FVector(0.0, 2.0, 0.0)));
		const FQuat Expected = glm::angleAxis(glm::radians(90.0f), FVector3f(0.0f, 0.0f, 1.0f)) * Start.Rotation;
		CHECK(std::abs(glm::dot(Turned.Rotation, Expected)) > 1.0f - 1.0e-5f);
		CHECK(Turned.Scale == Start.Scale);
	}

	TEST_CASE("A ring that is seen from its edge is taken by its near side")
	{
		// The ring around Y is a level line in the picture: its near half and its far half lie on each other.
		// Taken thirty pixels to the right of the middle, it is the near half that the mouse has, and that runs
		// to the right there.
		FGizmoDrag Drag(MakeFrontGizmo(), EGizmoMode::Rotate, EGizmoHandle::Y, FVector2f(130.0f, 100.0f));

		Drag.MoveTo(FVector2f(180.0f, 100.0f), FGizmoSettings());

		// Fifty pixels are one radian, and the near side going right is counter-clockwise seen from above.
		CHECK(Drag.GetAngle() == doctest::Approx(glm::degrees(1.0f)).epsilon(0.02));
		const FTransform Turned = Drag.Apply(MakeTransformAt(Pivot));
		const FVector3f TowardsCamera = Turned.Rotation * FVector3f(0.0f, 0.0f, 1.0f);
		CHECK(TowardsCamera.x > 0.5f);
	}

	TEST_CASE("With steps, a thing turns by whole steps")
	{
		const FGizmoSettings Steps = MakeSteps(0.5, 15.0f, 0.25f);
		FGizmoDrag Drag(MakeFrontGizmo(), EGizmoMode::Rotate, EGizmoHandle::Z, FVector2f(150.0f, 100.0f));

		// Fifty degrees along the ring are nearer to forty-five than to sixty.
		const float FiftyDegrees = 50.0f * glm::radians(50.0f);
		Drag.MoveTo(FVector2f(150.0f, 100.0f - FiftyDegrees), Steps);
		CHECK(Drag.GetAngle() == doctest::Approx(45.0f));

		// Seven degrees are no step yet, and eight are one.
		Drag.MoveTo(FVector2f(150.0f, 100.0f - 50.0f * glm::radians(7.0f)), Steps);
		CHECK(Drag.GetAngle() == 0.0f);
		Drag.MoveTo(FVector2f(150.0f, 100.0f - 50.0f * glm::radians(8.0f)), Steps);
		CHECK(Drag.GetAngle() == doctest::Approx(15.0f));
	}

	TEST_CASE("The box of an axis sizes a thing along that axis, by the part of the axis that the mouse goes")
	{
		// Taken at the end of the X axis, five metres from the middle.
		FGizmoDrag Drag(MakeFrontGizmo(), EGizmoMode::Scale, EGizmoHandle::X, FVector2f(150.0f, 100.0f));

		// To twice as far from the middle is twice the size.
		Drag.MoveTo(FVector2f(200.0f, 100.0f), FGizmoSettings());
		CHECK(IsNear(Drag.GetScale(), FVector3f(2.0f, 1.0f, 1.0f)));
		FTransform Start = MakeTransformAt(FVector(1.0, 2.0, 3.0));
		Start.Scale = FVector3f(1.0f, 2.0f, 3.0f);
		const FTransform Sized = Drag.Apply(Start);
		CHECK(IsNear(Sized.Scale, FVector3f(2.0f, 2.0f, 3.0f)));
		CHECK(Sized.Location == Start.Location);
		CHECK(Sized.Rotation == Start.Rotation);

		// To half as far is half the size.
		Drag.MoveTo(FVector2f(125.0f, 100.0f), FGizmoSettings());
		CHECK(IsNear(Drag.GetScale(), FVector3f(0.5f, 1.0f, 1.0f)));

		// Through the middle and out the other side, the thing is not turned inside out: a sliver is left of it.
		Drag.MoveTo(FVector2f(40.0f, 100.0f), FGizmoSettings());
		CHECK(IsNear(Drag.GetScale(), FVector3f(FGizmoDrag::MinScaleFactor, 1.0f, 1.0f)));
	}

	TEST_CASE("The box in the middle sizes a thing along all its axes, larger to the right and upwards")
	{
		FGizmoDrag Drag(MakeFrontGizmo(), EGizmoMode::Scale, EGizmoHandle::All, FVector2f(100.0f, 100.0f));

		// Thirty pixels to the right and twenty up are the fifty of an axis: twice the size.
		Drag.MoveTo(FVector2f(130.0f, 80.0f), FGizmoSettings());
		CHECK(IsNear(Drag.GetScale(), FVector3f(2.0f)));

		// To the left and downwards is smaller, down to a sliver.
		Drag.MoveTo(FVector2f(90.0f, 115.0f), FGizmoSettings());
		CHECK(IsNear(Drag.GetScale(), FVector3f(0.5f)));
		Drag.MoveTo(FVector2f(0.0f, 200.0f), FGizmoSettings());
		CHECK(IsNear(Drag.GetScale(), FVector3f(FGizmoDrag::MinScaleFactor)));
	}

	TEST_CASE("With steps, a thing grows and shrinks by whole steps, and to no less than one")
	{
		const FGizmoSettings Steps = MakeSteps(0.5, 15.0f, 0.25f);
		FGizmoDrag Drag(MakeFrontGizmo(), EGizmoMode::Scale, EGizmoHandle::X, FVector2f(150.0f, 100.0f));

		// 1.26 times the size is nearer to one and a quarter than to one and a half.
		Drag.MoveTo(FVector2f(163.0f, 100.0f), Steps);
		CHECK(IsNear(Drag.GetScale(), FVector3f(1.25f, 1.0f, 1.0f)));

		Drag.MoveTo(FVector2f(40.0f, 100.0f), Steps);
		CHECK(IsNear(Drag.GetScale(), FVector3f(0.25f, 1.0f, 1.0f)));

		// A step that is finer than the least a drag leaves of a size does not go below that.
		const FGizmoSettings FineSteps = MakeSteps(0.5, 15.0f, 0.001f);
		Drag.MoveTo(FVector2f(40.0f, 100.0f), FineSteps);
		CHECK(IsNear(Drag.GetScale(), FVector3f(FGizmoDrag::MinScaleFactor, 1.0f, 1.0f)));
	}

	TEST_CASE("A handle that the gizmo has not in its mode does nothing when it is dragged")
	{
		const FTransform Start = MakeTransformAt(FVector(1.0, 2.0, 3.0));

		for (const EGizmoHandle Handle : {EGizmoHandle::None, EGizmoHandle::All})
		{
			FGizmoDrag Drag(MakeFrontGizmo(), EGizmoMode::Translate, Handle, FVector2f(118.0f, 82.0f));
			Drag.MoveTo(FVector2f(160.0f, 40.0f), FGizmoSettings());
			CHECK(Drag.Apply(Start) == Start);
		}
		for (const EGizmoHandle Handle : {EGizmoHandle::None, EGizmoHandle::XY, EGizmoHandle::All})
		{
			FGizmoDrag Drag(MakeFrontGizmo(), EGizmoMode::Rotate, Handle, FVector2f(118.0f, 82.0f));
			Drag.MoveTo(FVector2f(160.0f, 40.0f), FGizmoSettings());
			CHECK(Drag.Apply(Start) == Start);
		}
		for (const EGizmoHandle Handle : {EGizmoHandle::None, EGizmoHandle::XY})
		{
			FGizmoDrag Drag(MakeFrontGizmo(), EGizmoMode::Scale, Handle, FVector2f(118.0f, 82.0f));
			Drag.MoveTo(FVector2f(160.0f, 40.0f), FGizmoSettings());
			CHECK(Drag.Apply(Start) == Start);
		}
	}

	TEST_CASE("A drag is measured from the gizmo as it lay when the mouse took hold")
	{
		// The same drag of the same arrow, seen at a slant as the editor's camera first looks.
		const FViewportCamera Camera;
		const FTransformGizmo Gizmo(Camera.GetView(), PictureSize, Camera.GetFocusPoint(), NoRotation, AxisLength);
		const std::optional<FVector2f> Middle = Gizmo.FindInPicture(Gizmo.GetPivot());
		const std::optional<FVector2f> End =
			Gizmo.FindInPicture(Gizmo.GetPivot() + FVector(Gizmo.GetAxis(0)) * Gizmo.GetWorldAxisLength());
		REQUIRE(Middle.has_value());
		REQUIRE(End.has_value());
		FGizmoDrag Drag(Gizmo, EGizmoMode::Translate, EGizmoHandle::X, *Middle);

		// From the middle of the gizmo to the tip of its arrow is the length of its axis, whatever the angle.
		Drag.MoveTo(*End, FGizmoSettings());

		CHECK(IsNear(Drag.GetTranslation(), FVector(Gizmo.GetWorldAxisLength(), 0.0, 0.0), 1.0e-3));
	}
}
