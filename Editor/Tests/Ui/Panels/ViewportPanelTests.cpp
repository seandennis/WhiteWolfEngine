#include "Assets/TestAssets.h"
#include "Object/ObjectGlobals.h"
#include "Rendering/MeshShapes.h"
#include "Rendering/StaticMesh.h"
#include "Rendering/StaticMeshComponent.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Ui/TestEditorUi.h"
#include "Viewport/ViewportCamera.h"

#include <doctest/doctest.h>
#include <glm/geometric.hpp>

#include <cmath>
#include <ostream>
#include <string>

// The viewport's camera is steered here as a person steers it: with the mouse on the picture of the scene and the
// keyboard, through FEditorUi. What the camera then does with what it is told is tested with FViewportCamera.

namespace
{
	/** How far two positions may be apart, in metres, and still count as the same. */
	constexpr double PositionTolerance = 1.0e-4;

	bool IsSamePosition(const FVector& Actual, const FVector& Expected)
	{
		return glm::length(Actual - Expected) <= PositionTolerance;
	}

	/** Puts the mouse on the middle of the viewport and holds a button down there. */
	void PressOnViewport(FEditorUiTest& Test, EMouseButton Button)
	{
		Test.MoveMouse(Test.RequireItem("Viewport").GetCenter());
		Test.Input.HandleMouseButtonEvent(Button, EInputEvent::Pressed);
		Test.DrawFrame();
	}

	void Release(FEditorUiTest& Test, EMouseButton Button)
	{
		Test.Input.HandleMouseButtonEvent(Button, EInputEvent::Released);
		Test.DrawFrame();
	}

	/** Moves the mouse from where it is, by a distance in pixels. */
	void MoveBy(FEditorUiTest& Test, const FVector2D& Distance)
	{
		Test.MoveMouse(Test.Input.GetMousePosition() + Distance);
	}

	/** Turns the wheel of the mouse by a number of notches, away from the user if positive. */
	void TurnWheel(FEditorUiTest& Test, double Notches)
	{
		Test.Input.HandleScroll(FVector2D(0.0, Notches));
		Test.DrawFrame();
	}

	/** Adds an entity that shows a box of one metre, at a place in the world, and returns its identifier. */
	int64 AddBox(FEditorUiTest& Test, const char* Name, const FVector& Location)
	{
		WEntity* const Entity = Test.Editor.GetScene().CreateEntity(Name);
		REQUIRE(Entity != nullptr);
		Entity->SetWorldLocation(Location);
		WStaticMeshComponent* const Component = Entity->AddComponent<WStaticMeshComponent>();
		REQUIRE(Component != nullptr);
		Component->Mesh = FMeshShapes::CreateBox(FVector3f(1.0f));
		return Entity->GetId();
	}
}

TEST_SUITE("FViewportPanel")
{
	TEST_CASE("With the right mouse button held, the mouse turns the camera where it stands")
	{
		FEditorUiTest Test;
		const FViewportCamera Before = Test.Editor.GetViewportCamera();

		PressOnViewport(Test, EMouseButton::Right);
		MoveBy(Test, FVector2D(50.0, 20.0));
		Release(Test, EMouseButton::Right);

		// A fifth of a degree for each pixel: to the right, which the yaw counts down, and downwards.
		const FViewportCamera& After = Test.Editor.GetViewportCamera();
		CHECK(After.Yaw == doctest::Approx(Before.Yaw - 10.0f));
		CHECK(After.Pitch == doctest::Approx(Before.Pitch - 4.0f));
		CHECK(After.Location == Before.Location);

		// With the button up again, the mouse is a mouse.
		MoveBy(Test, FVector2D(50.0, 20.0));
		CHECK(Test.Editor.GetViewportCamera() == After);
	}

	TEST_CASE("With the right mouse button held, the keys fly the camera at its speed")
	{
		FEditorUiTest Test;
		const FViewportCamera Before = Test.Editor.GetViewportCamera();
		constexpr int32 FrameCount = 30;
		const double ExpectedDistance = static_cast<double>(Before.FlySpeed) * FEditorUiTest::FrameSeconds * FrameCount;
		EKey Key = EKey::W;
		FVector ExpectedDirection(0.0);

		SUBCASE("W: the way the camera looks")
		{
			Key = EKey::W;
			ExpectedDirection = FVector(Before.GetForward());
		}
		SUBCASE("S: backwards")
		{
			Key = EKey::S;
			ExpectedDirection = -FVector(Before.GetForward());
		}
		SUBCASE("D: to the right")
		{
			Key = EKey::D;
			ExpectedDirection = FVector(Before.GetRight());
		}
		SUBCASE("A: to the left")
		{
			Key = EKey::A;
			ExpectedDirection = -FVector(Before.GetRight());
		}
		SUBCASE("E: up in the world, however the camera is tilted")
		{
			Key = EKey::E;
			ExpectedDirection = FVector(0.0, 1.0, 0.0);
		}
		SUBCASE("Q: down in the world")
		{
			Key = EKey::Q;
			ExpectedDirection = FVector(0.0, -1.0, 0.0);
		}

		PressOnViewport(Test, EMouseButton::Right);
		Test.Input.HandleKeyEvent(Key, EInputEvent::Pressed);
		Test.DrawFrames(FrameCount);
		Test.Input.HandleKeyEvent(Key, EInputEvent::Released);
		Test.DrawFrame();
		Release(Test, EMouseButton::Right);

		const FViewportCamera& After = Test.Editor.GetViewportCamera();
		CHECK(IsSamePosition(After.Location, Before.Location + ExpectedDirection * ExpectedDistance));
		CHECK(After.Yaw == Before.Yaw);
		CHECK(After.Pitch == Before.Pitch);
	}

	TEST_CASE("The keys do not move the camera unless the right mouse button is held on the viewport")
	{
		FEditorUiTest Test;
		const FViewportCamera Before = Test.Editor.GetViewportCamera();

		// The mouse is on the viewport, with no button down.
		Test.MoveMouse(Test.RequireItem("Viewport").GetCenter());
		Test.Input.HandleKeyEvent(EKey::W, EInputEvent::Pressed);
		Test.DrawFrames(10);
		CHECK(Test.Editor.GetViewportCamera() == Before);

		// The button is down, but on another panel.
		Test.MoveMouse(Test.RequireItem("Details/NothingSelected").GetCenter());
		Test.Input.HandleMouseButtonEvent(EMouseButton::Right, EInputEvent::Pressed);
		Test.DrawFrames(10);
		MoveBy(Test, FVector2D(40.0, 0.0));
		CHECK(Test.Editor.GetViewportCamera() == Before);
		CHECK_FALSE(Test.Ui->WantsMouseCaptured());

		Test.Input.HandleMouseButtonEvent(EMouseButton::Right, EInputEvent::Released);
		Test.Input.HandleKeyEvent(EKey::W, EInputEvent::Released);
		Test.DrawFrame();
	}

	TEST_CASE("While flying, the wheel makes the camera faster and slower, within its limits")
	{
		FEditorUiTest Test;
		const FViewportCamera Before = Test.Editor.GetViewportCamera();

		PressOnViewport(Test, EMouseButton::Right);
		TurnWheel(Test, 1.0);
		CHECK(Test.Editor.GetViewportCamera().FlySpeed == doctest::Approx(Before.FlySpeed * 1.25f));
		// Where the camera is and what it looks at stay as they were: the wheel does not move it now.
		CHECK(Test.Editor.GetViewportCamera().Location == Before.Location);
		CHECK(Test.Editor.GetViewportCamera().FocusDistance == Before.FocusDistance);

		TurnWheel(Test, -2.0);
		CHECK(Test.Editor.GetViewportCamera().FlySpeed == doctest::Approx(Before.FlySpeed / 1.25f));

		TurnWheel(Test, 100.0);
		CHECK(Test.Editor.GetViewportCamera().FlySpeed == FViewportCamera::MaxFlySpeed);
		TurnWheel(Test, -200.0);
		CHECK(Test.Editor.GetViewportCamera().FlySpeed == FViewportCamera::MinFlySpeed);
		CHECK(Test.Editor.GetViewportCamera().IsValid());
		Release(Test, EMouseButton::Right);
	}

	TEST_CASE("With Alt and the left mouse button, the mouse takes the camera around what it looks at")
	{
		FEditorUiTest Test;
		const FViewportCamera Before = Test.Editor.GetViewportCamera();
		const FVector FocusPoint = Before.GetFocusPoint();

		Test.SetKeyDown(EKey::LeftAlt, true);
		PressOnViewport(Test, EMouseButton::Left);
		MoveBy(Test, FVector2D(100.0, -20.0));
		Release(Test, EMouseButton::Left);
		Test.SetKeyDown(EKey::LeftAlt, false);

		// Three tenths of a degree for each pixel.
		const FViewportCamera& After = Test.Editor.GetViewportCamera();
		CHECK(After.Yaw == doctest::Approx(Before.Yaw - 30.0f));
		CHECK(After.Pitch == doctest::Approx(Before.Pitch + 6.0f));
		CHECK(IsSamePosition(After.GetFocusPoint(), FocusPoint));
		CHECK(After.FocusDistance == Before.FocusDistance);
		CHECK_FALSE(IsSamePosition(After.Location, Before.Location));
	}

	TEST_CASE("Without Alt, the left mouse button on the viewport does not steer the camera")
	{
		FEditorUiTest Test;
		const FViewportCamera Before = Test.Editor.GetViewportCamera();

		PressOnViewport(Test, EMouseButton::Left);
		CHECK_FALSE(Test.Ui->WantsMouseCaptured());
		MoveBy(Test, FVector2D(100.0, 40.0));
		Release(Test, EMouseButton::Left);

		CHECK(Test.Editor.GetViewportCamera() == Before);
	}

	TEST_CASE("With Alt and the right mouse button, the mouse moves the camera closer and away")
	{
		FEditorUiTest Test;
		const FViewportCamera Before = Test.Editor.GetViewportCamera();
		const FVector FocusPoint = Before.GetFocusPoint();

		Test.SetKeyDown(EKey::LeftAlt, true);
		PressOnViewport(Test, EMouseButton::Right);
		MoveBy(Test, FVector2D(40.0, 0.0));

		// Half a hundredth of the distance for each pixel to the right.
		const FViewportCamera Closer = Test.Editor.GetViewportCamera();
		CHECK(Closer.FocusDistance == doctest::Approx(Before.FocusDistance * 0.8));
		CHECK(IsSamePosition(Closer.GetFocusPoint(), FocusPoint));
		CHECK(Closer.Yaw == Before.Yaw);
		CHECK(Closer.Pitch == Before.Pitch);

		// Downwards is away.
		MoveBy(Test, FVector2D(0.0, 40.0));
		CHECK(Test.Editor.GetViewportCamera().FocusDistance > Closer.FocusDistance);
		CHECK(IsSamePosition(Test.Editor.GetViewportCamera().GetFocusPoint(), FocusPoint));
		Release(Test, EMouseButton::Right);
		Test.SetKeyDown(EKey::LeftAlt, false);
	}

	TEST_CASE("With the middle mouse button, the scene goes with the mouse")
	{
		FEditorUiTest Test;
		const FViewportCamera Before = Test.Editor.GetViewportCamera();
		const int32 PictureHeight = static_cast<int32>(Test.RequireItem("Viewport").GetHeight());
		const double PixelSize = Before.GetPixelSizeAtFocus(PictureHeight);

		PressOnViewport(Test, EMouseButton::Middle);
		MoveBy(Test, FVector2D(30.0, -20.0));
		Release(Test, EMouseButton::Middle);

		// The camera goes the other way, as far as the mouse went at the distance of what it looks at: to the
		// left for a mouse that went right, and down for one that went up. The height of the picture is known
		// here to within a pixel of its edge, and so the size of a pixel to within a part in some hundreds.
		const FViewportCamera& After = Test.Editor.GetViewportCamera();
		const FVector Moved = After.Location - Before.Location;
		const FVector ExpectedMove =
			-FVector(Before.GetRight()) * (30.0 * PixelSize) - FVector(Before.GetUp()) * (20.0 * PixelSize);
		CHECK(glm::length(Moved - ExpectedMove) < 0.005 * glm::length(ExpectedMove));
		CHECK(After.Yaw == Before.Yaw);
		CHECK(After.Pitch == Before.Pitch);
		CHECK(After.FocusDistance == Before.FocusDistance);
	}

	TEST_CASE("The wheel by itself moves the camera closer to what it looks at, over the viewport only")
	{
		FEditorUiTest Test;
		const FViewportCamera Before = Test.Editor.GetViewportCamera();
		const FVector FocusPoint = Before.GetFocusPoint();

		Test.MoveMouse(Test.RequireItem("Viewport").GetCenter());
		TurnWheel(Test, 1.0);

		const FViewportCamera Closer = Test.Editor.GetViewportCamera();
		CHECK(Closer.FocusDistance == doctest::Approx(Before.FocusDistance * 0.9));
		CHECK(IsSamePosition(Closer.GetFocusPoint(), FocusPoint));
		CHECK(Closer.FlySpeed == Before.FlySpeed);

		TurnWheel(Test, -1.0);
		CHECK(Test.Editor.GetViewportCamera().FocusDistance == doctest::Approx(Closer.FocusDistance * 1.1));

		// Over another panel, the wheel is that panel's.
		const FViewportCamera Unmoved = Test.Editor.GetViewportCamera();
		Test.MoveMouse(Test.RequireItem("Details/NothingSelected").GetCenter());
		TurnWheel(Test, 3.0);
		CHECK(Test.Editor.GetViewportCamera() == Unmoved);
	}

	TEST_CASE("The user interface wants the mouse to itself for as long as the camera is steered")
	{
		FEditorUiTest Test;
		CHECK_FALSE(Test.Ui->WantsMouseCaptured());

		SUBCASE("Flying")
		{
			PressOnViewport(Test, EMouseButton::Right);
			CHECK(Test.Ui->WantsMouseCaptured());
			// The mouse can then be anywhere, far beyond the picture: the camera is still what it steers.
			const float YawBefore = Test.Editor.GetViewportCamera().Yaw;
			MoveBy(Test, FVector2D(-5000.0, 0.0));
			CHECK(Test.Ui->WantsMouseCaptured());
			CHECK(Test.Editor.GetViewportCamera().Yaw != YawBefore);
			Release(Test, EMouseButton::Right);
		}
		SUBCASE("Moving sideways")
		{
			PressOnViewport(Test, EMouseButton::Middle);
			CHECK(Test.Ui->WantsMouseCaptured());
			Release(Test, EMouseButton::Middle);
		}
		SUBCASE("Going around")
		{
			Test.SetKeyDown(EKey::LeftAlt, true);
			PressOnViewport(Test, EMouseButton::Left);
			CHECK(Test.Ui->WantsMouseCaptured());
			Release(Test, EMouseButton::Left);
			Test.SetKeyDown(EKey::LeftAlt, false);
		}

		CHECK_FALSE(Test.Ui->WantsMouseCaptured());
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("F brings the camera to where it sees what is selected, without turning it")
	{
		FEditorUiTest Test;
		const int64 Crate = AddBox(Test, "Crate", FVector(40.0, 2.0, -70.0));
		Test.Editor.SetSelection({Crate});
		Test.DrawFrame();
		const FViewportCamera Before = Test.Editor.GetViewportCamera();

		Test.PressKey(EKey::F);

		const FViewportCamera& After = Test.Editor.GetViewportCamera();
		CHECK(IsSamePosition(After.GetFocusPoint(), FVector(40.0, 2.0, -70.0)));
		CHECK(After.Yaw == Before.Yaw);
		CHECK(After.Pitch == Before.Pitch);
		// Near enough to see it well, far enough to see all of it.
		CHECK(After.FocusDistance > 1.0);
		CHECK(After.FocusDistance < 10.0);
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("F with nothing selected, or typed into a field, leaves the camera where it is")
	{
		FEditorUiTest Test;
		const int64 Crate = AddBox(Test, "Crate", FVector(40.0, 2.0, -70.0));
		Test.DrawFrame();
		const FViewportCamera Before = Test.Editor.GetViewportCamera();

		Test.PressKey(EKey::F);
		CHECK(Test.Editor.GetViewportCamera() == Before);

		Test.Editor.SetSelection({Crate});
		Test.Click("Outliner/Search");
		Test.PressKey(EKey::F);
		Test.Type("f");
		CHECK(Test.Editor.GetViewportCamera() == Before);
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("Focus in the Edit menu does what F does")
	{
		FEditorUiTest Test;
		const int64 Crate = AddBox(Test, "Crate", FVector(-15.0, 0.0, 8.0));
		Test.Editor.SetSelection({Crate});
		Test.DrawFrame();

		Test.Click("Menu/Edit");
		Test.Click("Menu/Edit/Focus");

		CHECK(IsSamePosition(Test.Editor.GetViewportCamera().GetFocusPoint(), FVector(-15.0, 0.0, 8.0)));
	}
}
