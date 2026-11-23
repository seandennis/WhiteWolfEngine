#include "Viewport/GizmoDrag.h"

#include "Math/Transform.h"
#include "Rendering/CameraView.h"
#include "Viewport/GizmoSettings.h"
#include "Viewport/TransformGizmo.h"
#include "Viewport/ViewportCamera.h"

#include <doctest/doctest.h>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <limits>
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

		// A step that is finer than the least a drag leaves of a size stops at the last whole step that leaves no
		// less: 247 steps of four thousandths down from one.
		const FGizmoSettings FineSteps = MakeSteps(0.5, 15.0f, 0.004f);
		Drag.MoveTo(FVector2f(40.0f, 100.0f), FineSteps);
		CHECK(IsNear(Drag.GetScale(), FVector3f(0.012f, 1.0f, 1.0f)));
	}

	TEST_CASE("With steps, it is the change of a size that goes in steps, whatever the step")
	{
		// Steps that one is no whole number of, up to the largest: a drag that has gone nowhere is no step.
		for (const float Step : {0.3f, 0.75f, 2.0f, FGizmoSettings::MaxScaleSnap})
		{
			CAPTURE(Step);
			const FGizmoSettings Steps = MakeSteps(0.5, 15.0f, Step);
			REQUIRE(Steps.IsValid());

			FGizmoDrag Box(MakeFrontGizmo(), EGizmoMode::Scale, EGizmoHandle::X, FVector2f(150.0f, 100.0f));
			Box.MoveTo(FVector2f(150.0f, 100.0f), Steps);
			CHECK(Box.GetScale() == FVector3f(1.0f));

			FGizmoDrag Middle(MakeFrontGizmo(), EGizmoMode::Scale, EGizmoHandle::All, FVector2f(100.0f, 100.0f));
			Middle.MoveTo(FVector2f(100.0f, 100.0f), Steps);
			CHECK(Middle.GetScale() == FVector3f(1.0f));
		}

		// In steps of three tenths, 1.26 times the size is nearer to one step more than to none.
		const FGizmoSettings Tenths = MakeSteps(0.5, 15.0f, 0.3f);
		FGizmoDrag Drag(MakeFrontGizmo(), EGizmoMode::Scale, EGizmoHandle::X, FVector2f(150.0f, 100.0f));
		Drag.MoveTo(FVector2f(163.0f, 100.0f), Tenths);
		CHECK(IsNear(Drag.GetScale(), FVector3f(1.3f, 1.0f, 1.0f)));
		// Three steps less leave a tenth; a fourth would leave less than nothing.
		Drag.MoveTo(FVector2f(40.0f, 100.0f), Tenths);
		CHECK(IsNear(Drag.GetScale(), FVector3f(0.1f, 1.0f, 1.0f)));

		// In steps of twice the size, a thing gets no smaller, and larger by two sizes at a time.
		const FGizmoSettings Doubles = MakeSteps(0.5, 15.0f, 2.0f);
		Drag.MoveTo(FVector2f(40.0f, 100.0f), Doubles);
		CHECK(IsNear(Drag.GetScale(), FVector3f(1.0f, 1.0f, 1.0f)));
		Drag.MoveTo(FVector2f(250.0f, 100.0f), Doubles);
		CHECK(IsNear(Drag.GetScale(), FVector3f(3.0f, 1.0f, 1.0f)));
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

	TEST_CASE("A drag that comes back to where it began leaves a thing exactly as it was")
	{
		FTransform Start;
		Start.Location = FVector(1.0e9 + 0.125, -3.0, 7.5);
		Start.Rotation = glm::angleAxis(glm::radians(33.0f), glm::normalize(FVector3f(1.0f, 2.0f, 3.0f)));
		Start.Scale = FVector3f(1.5f, 0.25f, 3.0f);

		const auto ComeBack =
			[&Start](EGizmoMode Mode, EGizmoHandle Handle, const FVector2f& Point, const FGizmoSettings& Settings)
		{
			CAPTURE(static_cast<int32>(Mode));
			CAPTURE(static_cast<int32>(Handle));
			FGizmoDrag Drag(MakeFrontGizmo(), Mode, Handle, Point);

			// Somewhere else first, where the drag has done something.
			Drag.MoveTo(Point + FVector2f(37.0f, -23.0f), Settings);
			CHECK_FALSE(Drag.Apply(Start) == Start);

			Drag.MoveTo(Point, Settings);
			CHECK(Drag.GetTranslation() == FVector(0.0));
			CHECK(Drag.GetAngle() == 0.0f);
			CHECK(Drag.GetScale() == FVector3f(1.0f));
			CHECK(Drag.Apply(Start) == Start);
		};
		// Without steps, in steps that one is a whole number of, and in steps that it is not.
		for (const FGizmoSettings& Settings :
			{FGizmoSettings(), MakeSteps(0.5, 15.0f, 0.25f), MakeSteps(0.3, 7.0f, 0.4f)})
		{
			ComeBack(EGizmoMode::Translate, EGizmoHandle::X, FVector2f(130.0f, 100.0f), Settings);
			ComeBack(EGizmoMode::Translate, EGizmoHandle::XY, FVector2f(118.0f, 82.0f), Settings);
			ComeBack(EGizmoMode::Rotate, EGizmoHandle::Z, FVector2f(150.0f, 100.0f), Settings);
			ComeBack(EGizmoMode::Scale, EGizmoHandle::X, FVector2f(150.0f, 100.0f), Settings);
			ComeBack(EGizmoMode::Scale, EGizmoHandle::All, FVector2f(100.0f, 100.0f), Settings);
		}
	}

	TEST_CASE("Where the mouse comes to say nothing, a drag stays what it had come to")
	{
		// The gizmo's X leans sixty degrees towards the camera, and so does the plane that the mouse points at for
		// its arrow. From the left of the picture the camera looks along that plane and past it.
		const FTransformGizmo Leaning =
			MakeFrontGizmo(glm::angleAxis(glm::radians(-60.0f), FVector3f(0.0f, 1.0f, 0.0f)));
		FGizmoDrag Arrow(Leaning, EGizmoMode::Translate, EGizmoHandle::X, FVector2f(130.0f, 100.0f));
		Arrow.MoveTo(FVector2f(140.0f, 100.0f), FGizmoSettings());
		const FVector Moved = Arrow.GetTranslation();
		REQUIRE(glm::length(Moved) > 0.5);
		Arrow.MoveTo(FVector2f(20.0f, 100.0f), FGizmoSettings());
		CHECK(Arrow.GetTranslation() == Moved);

		// A box is measured as an arrow is.
		FGizmoDrag Box(Leaning, EGizmoMode::Scale, EGizmoHandle::X, FVector2f(130.0f, 100.0f));
		Box.MoveTo(FVector2f(140.0f, 100.0f), FGizmoSettings());
		const FVector3f Sized = Box.GetScale();
		REQUIRE(Sized.x > 1.1f);
		Box.MoveTo(FVector2f(20.0f, 100.0f), FGizmoSettings());
		CHECK(Box.GetScale() == Sized);

		// As the editor's camera first looks, from above, the plane of X and Z ends at a horizon in the picture,
		// and above it the mouse points at the sky.
		const FViewportCamera Camera;
		const FTransformGizmo Seen(Camera.GetView(), PictureSize, Camera.GetFocusPoint(), NoRotation, AxisLength);
		const std::optional<FVector2f> OnSquare = Seen.FindHandlePoint(EGizmoMode::Translate, EGizmoHandle::XZ);
		REQUIRE(OnSquare.has_value());
		FGizmoDrag Square(Seen, EGizmoMode::Translate, EGizmoHandle::XZ, *OnSquare);
		Square.MoveTo(*OnSquare + FVector2f(20.0f, 0.0f), FGizmoSettings());
		const FVector Slid = Square.GetTranslation();
		REQUIRE(glm::length(Slid) > 0.1);
		CHECK(Slid.y == 0.0);
		Square.MoveTo(FVector2f(100.0f, 5.0f), FGizmoSettings());
		CHECK(Square.GetTranslation() == Slid);
	}

	TEST_CASE("Towards the horizon of the plane that a drag is taken in, the mouse says nothing and the thing stays")
	{
		// A gizmo five metres below a camera that looks level: the plane of X and Z is a floor whose horizon is the
		// middle row of the picture. Fifty rows below it the floor is ten metres ahead, where the gizmo is, and five
		// rows below it a hundred.
		FCameraView View;
		View.VerticalFieldOfView = 90.0f;
		const FTransformGizmo Gizmo(View, PictureSize, FVector(0.0, -5.0, -10.0), NoRotation, AxisLength);

		// The arrow along Z is measured on the same floor: of the planes that hold its axis, that one faces the
		// camera most squarely.
		for (const EGizmoHandle Handle : {EGizmoHandle::XZ, EGizmoHandle::Z})
		{
			CAPTURE(static_cast<int32>(Handle));
			FGizmoDrag Drag(Gizmo, EGizmoMode::Translate, Handle, FVector2f(100.0f, 150.0f));
			Drag.MoveTo(FVector2f(100.0f, 105.0f), FGizmoSettings());
			CHECK(IsNear(Drag.GetTranslation(), FVector(0.0, 0.0, -90.0), 1.0e-2));

			// A quarter of a row below the horizon the floor is two kilometres ahead, a hundredth of a row fifty;
			// on the horizon and above it there is no floor.
			for (const float Row : {100.25f, 100.01f, 100.0f, 99.0f, 50.0f})
			{
				CAPTURE(Row);
				Drag.MoveTo(FVector2f(100.0f, Row), FGizmoSettings());
				CHECK(IsNear(Drag.GetTranslation(), FVector(0.0, 0.0, -90.0), 1.0e-2));
			}

			// Back below it, the thing follows the mouse again.
			Drag.MoveTo(FVector2f(100.0f, 125.0f), FGizmoSettings());
			CHECK(IsNear(Drag.GetTranslation(), FVector(0.0, 0.0, -10.0), 1.0e-3));
		}

		// A box on an axis that points away from the camera is measured on that floor too. Taken where the floor
		// is twenty metres ahead and dragged to where it is a hundred, it is eighty metres along an axis of five.
		const FTransformGizmo TurnedAway(View,
			PictureSize,
			FVector(0.0, -5.0, -10.0),
			glm::angleAxis(glm::radians(180.0f), FVector3f(0.0f, 1.0f, 0.0f)),
			AxisLength);
		FGizmoDrag Box(TurnedAway, EGizmoMode::Scale, EGizmoHandle::Z, FVector2f(100.0f, 125.0f));
		Box.MoveTo(FVector2f(100.0f, 105.0f), FGizmoSettings());
		CHECK(IsNear(Box.GetScale(), FVector3f(1.0f, 1.0f, 17.0f), 1.0e-2f));
		Box.MoveTo(FVector2f(100.0f, 100.25f), FGizmoSettings());
		CHECK(IsNear(Box.GetScale(), FVector3f(1.0f, 1.0f, 17.0f), 1.0e-2f));
	}

	TEST_CASE("A point of the picture that is no number leaves a drag as it was")
	{
		const float NoNumber = std::numeric_limits<float>::quiet_NaN();
		const float Endless = std::numeric_limits<float>::infinity();
		const FTransform Start = MakeTransformAt(FVector(1.0, 2.0, 3.0));

		for (const EGizmoMode Mode : {EGizmoMode::Translate, EGizmoMode::Rotate, EGizmoMode::Scale})
		{
			CAPTURE(static_cast<int32>(Mode));
			FGizmoDrag Drag(MakeFrontGizmo(), Mode, EGizmoHandle::X, FVector2f(150.0f, 100.0f));
			Drag.MoveTo(FVector2f(160.0f, 90.0f), FGizmoSettings());
			const FTransform Before = Drag.Apply(Start);
			REQUIRE_FALSE(Before == Start);

			for (const FVector2f& Nowhere :
				{FVector2f(NoNumber, 100.0f), FVector2f(150.0f, NoNumber), FVector2f(Endless, 100.0f)})
			{
				Drag.MoveTo(Nowhere, FGizmoSettings());
				CHECK(Drag.Apply(Start) == Before);
			}
		}

		// The box in the middle is measured in the picture alone, and is left as it was all the same.
		FGizmoDrag Middle(MakeFrontGizmo(), EGizmoMode::Scale, EGizmoHandle::All, FVector2f(100.0f, 100.0f));
		Middle.MoveTo(FVector2f(125.0f, 100.0f), FGizmoSettings());
		Middle.MoveTo(FVector2f(NoNumber, NoNumber), FGizmoSettings());
		CHECK(IsNear(Middle.GetScale(), FVector3f(1.5f)));
	}

	TEST_CASE("A drag of a gizmo that is not visible does nothing")
	{
		// Behind a camera that looks along +X. The plane that the mouse would point at is in front of it all the
		// same.
		FCameraView View;
		View.VerticalFieldOfView = 90.0f;
		View.Rotation = glm::angleAxis(glm::radians(-90.0f), FVector3f(0.0f, 1.0f, 0.0f));
		const FTransformGizmo Gizmo(View, PictureSize, FVector(-5.0, 0.0, -3.0), NoRotation, AxisLength);
		REQUIRE_FALSE(Gizmo.IsVisible());
		FTransform Start = MakeTransformAt(FVector(1.0, 2.0, 3.0));
		Start.Scale = FVector3f(1.0f, 2.0f, 3.0f);

		for (const EGizmoMode Mode : {EGizmoMode::Translate, EGizmoMode::Rotate, EGizmoMode::Scale})
		{
			CAPTURE(static_cast<int32>(Mode));
			FGizmoDrag Drag(Gizmo, Mode, EGizmoHandle::X, FVector2f(50.0f, 100.0f));
			Drag.MoveTo(FVector2f(60.0f, 110.0f), FGizmoSettings());

			CHECK(Drag.GetHandle() == EGizmoHandle::None);
			CHECK(Drag.Apply(Start) == Start);
		}
	}

	TEST_CASE("An arrow of a gizmo whose rotation is not of unit length moves a thing as far as the mouse goes")
	{
		// A quarter turn around Y, written too long: the gizmo's Z axis points to the right.
		const FTransformGizmo Gizmo = MakeFrontGizmo(FQuat(1.0f, 0.0f, 1.0f, 0.0f));
		FGizmoDrag Drag(Gizmo, EGizmoMode::Translate, EGizmoHandle::Z, FVector2f(130.0f, 100.0f));

		// Twenty pixels to the right are two metres.
		Drag.MoveTo(FVector2f(150.0f, 100.0f), FGizmoSettings());

		CHECK(IsNear(Drag.GetTranslation(), FVector(2.0, 0.0, 0.0)));
	}

	TEST_CASE("In a picture that is wider than it is high, a pixel is as far in the world both ways")
	{
		// Half as wide again as it is high: the middle of the gizmo is at (150, 100), and a pixel is still ten
		// centimetres there.
		FCameraView View;
		View.VerticalFieldOfView = 90.0f;
		const FTransformGizmo Gizmo(View, FVector2f(300.0f, 200.0f), Pivot, NoRotation, AxisLength);
		CHECK(Gizmo.GetWorldAxisLength() == doctest::Approx(5.0));
		const std::optional<FVector2f> Beside = Gizmo.FindInPicture(Pivot + FVector(2.0, 1.0, 0.0));
		REQUIRE(Beside.has_value());
		CHECK(glm::length(*Beside - FVector2f(170.0f, 90.0f)) < 0.01f);

		FGizmoDrag AlongX(Gizmo, EGizmoMode::Translate, EGizmoHandle::X, FVector2f(180.0f, 100.0f));
		AlongX.MoveTo(FVector2f(200.0f, 100.0f), FGizmoSettings());
		CHECK(IsNear(AlongX.GetTranslation(), FVector(2.0, 0.0, 0.0)));
		FGizmoDrag AlongY(Gizmo, EGizmoMode::Translate, EGizmoHandle::Y, FVector2f(150.0f, 70.0f));
		AlongY.MoveTo(FVector2f(150.0f, 50.0f), FGizmoSettings());
		CHECK(IsNear(AlongY.GetTranslation(), FVector(0.0, 2.0, 0.0)));
	}

	TEST_CASE("Every arrow, square and box does to its own axes what the others do to theirs")
	{
		// As the editor's camera first looks, from the side and above.
		const FViewportCamera Camera;
		const FTransformGizmo Gizmo(Camera.GetView(), PictureSize, Camera.GetFocusPoint(), NoRotation, AxisLength);
		const double Length = Gizmo.GetWorldAxisLength();
		const auto FindPoint = [&Gizmo, Length](const FVector& AxisLengths)
		{
			const std::optional<FVector2f> Point = Gizmo.FindInPicture(Gizmo.GetPivot() + AxisLengths * Length);
			REQUIRE(Point.has_value());
			return *Point;
		};
		constexpr std::array<EGizmoHandle, 3> AxisHandles = {EGizmoHandle::X, EGizmoHandle::Y, EGizmoHandle::Z};
		constexpr std::array<EGizmoHandle, 3> PlaneHandles = {EGizmoHandle::YZ, EGizmoHandle::XZ, EGizmoHandle::XY};

		for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
		{
			CAPTURE(AxisIndex);
			FVector Along(0.0);
			Along[AxisIndex] = 1.0;
			FVector First(0.0);
			First[(AxisIndex + 1) % 3] = 1.0;
			FVector Second(0.0);
			Second[(AxisIndex + 2) % 3] = 1.0;

			// From the middle of the gizmo to the end of its axis is the length of the axis.
			const EGizmoHandle AxisHandle = AxisHandles[static_cast<std::size_t>(AxisIndex)];
			FGizmoDrag Arrow(Gizmo, EGizmoMode::Translate, AxisHandle, FindPoint(FVector(0.0)));
			Arrow.MoveTo(FindPoint(Along), FGizmoSettings());
			CHECK(IsNear(Arrow.GetTranslation(), Along * Length, 1.0e-3));

			// From one point of a square's plane to another is the way between the two.
			const EGizmoHandle PlaneHandle = PlaneHandles[static_cast<std::size_t>(AxisIndex)];
			FGizmoDrag Square(Gizmo, EGizmoMode::Translate, PlaneHandle, FindPoint((First + Second) * 0.375));
			Square.MoveTo(FindPoint(First * 0.75 + Second * 0.25), FGizmoSettings());
			CHECK(IsNear(Square.GetTranslation(), (First * 0.375 - Second * 0.125) * Length, 1.0e-3));

			// From the end of an axis to half as far again is half the size again, along that axis alone.
			FGizmoDrag Box(Gizmo, EGizmoMode::Scale, AxisHandle, FindPoint(Along));
			Box.MoveTo(FindPoint(Along * 1.5), FGizmoSettings());
			FVector3f Sized(1.0f);
			Sized[AxisIndex] = 1.5f;
			CHECK(IsNear(Box.GetScale(), Sized, 1.0e-3f));
		}
	}

	TEST_CASE("A ring that is seen from its edge is taken by its near side, wherever along it the mouse takes hold")
	{
		// The far half of the ring around Y, which is not there, lies on the near half in the picture, and its
		// points are closer together there: at 132, one of them is half a pixel away and the nearest of the near
		// half three and a half. The near half runs to the right all along the line.
		const FTransformGizmo Gizmo = MakeFrontGizmo();
		for (float Along = 51.0f; Along < 150.0f; Along += 1.0f)
		{
			// In the middle the ring around X crosses.
			if (Along == 100.0f)
			{
				continue;
			}
			CAPTURE(Along);
			const FVector2f Start(Along, 100.0f);
			REQUIRE(Gizmo.FindHandle(EGizmoMode::Rotate, Start, 0.5f) == EGizmoHandle::Y);
			FGizmoDrag Drag(Gizmo, EGizmoMode::Rotate, EGizmoHandle::Y, Start);

			Drag.MoveTo(Start + FVector2f(50.0f, 0.0f), FGizmoSettings());

			CHECK(Drag.GetAngle() == doctest::Approx(glm::degrees(1.0f)).epsilon(0.02));
		}
	}

	TEST_CASE("A ring that is seen nearly from its edge is taken by the half that is there, also from beside it")
	{
		// The ring around Y leans two degrees towards the camera: its near half is a little below the middle of the
		// picture and its far half, which is not there, a little above. A point of the far half is within reach of
		// the near half, and the mouse that presses there has the near half, which runs to the right.
		const FTransformGizmo Gizmo = MakeFrontGizmo(glm::angleAxis(glm::radians(2.0f), FVector3f(1.0f, 0.0f, 0.0f)));
		const float BehindAngle = glm::radians(157.5f);
		const FVector3f Behind = Gizmo.GetAxis(2) * std::cos(BehindAngle) + Gizmo.GetAxis(0) * std::sin(BehindAngle);
		REQUIRE(glm::dot(Behind, Gizmo.GetViewDirection()) > 0.5f);
		const std::optional<FVector2f> Start =
			Gizmo.FindInPicture(Gizmo.GetPivot() + FVector(Behind) * Gizmo.GetWorldAxisLength());
		REQUIRE(Start.has_value());
		constexpr float Reach = 5.0f;
		REQUIRE(Gizmo.FindHandle(EGizmoMode::Rotate, *Start, Reach) == EGizmoHandle::Y);
		FGizmoDrag Drag(Gizmo, EGizmoMode::Rotate, EGizmoHandle::Y, *Start);

		Drag.MoveTo(*Start + FVector2f(50.0f, 0.0f), FGizmoSettings());

		CHECK(Drag.GetAngle() == doctest::Approx(glm::degrees(1.0f)).epsilon(0.02));
	}

	TEST_CASE("The ring around X is taken by its near side too, which runs downwards in the picture")
	{
		FGizmoDrag Drag(MakeFrontGizmo(), EGizmoMode::Rotate, EGizmoHandle::X, FVector2f(100.0f, 70.0f));

		Drag.MoveTo(FVector2f(100.0f, 120.0f), FGizmoSettings());

		CHECK(Drag.GetAngle() == doctest::Approx(glm::degrees(1.0f)).epsilon(0.02));
		const FVector3f TowardsCamera = Drag.Apply(MakeTransformAt(Pivot)).Rotation * FVector3f(0.0f, 0.0f, 1.0f);
		CHECK(TowardsCamera.y < -0.5f);
	}

	TEST_CASE("A ring of a gizmo that is turned turns a thing around the gizmo's own axis")
	{
		// A quarter turn around X: the gizmo's Y points at the camera, and the ring around it is a circle in the
		// picture. Taken at its rightmost point and dragged a quarter of the way around, upwards.
		const FTransformGizmo Gizmo = MakeFrontGizmo(glm::angleAxis(glm::radians(90.0f), FVector3f(1.0f, 0.0f, 0.0f)));
		FGizmoDrag Drag(Gizmo, EGizmoMode::Rotate, EGizmoHandle::Y, FVector2f(150.0f, 100.0f));
		const float QuarterTurn = 50.0f * std::numbers::pi_v<float> * 0.5f;

		Drag.MoveTo(FVector2f(150.0f, 100.0f - QuarterTurn), FGizmoSettings());

		// Around the axis that points at the camera, which is the world's Z here and not its Y.
		CHECK(Drag.GetAngle() == doctest::Approx(90.0f).epsilon(1.0e-4));
		const FTransform Turned = Drag.Apply(MakeTransformAt(Pivot));
		CHECK(IsNear(Turned.Rotation * FVector3f(1.0f, 0.0f, 0.0f), FVector3f(0.0f, 1.0f, 0.0f)));
	}
}
