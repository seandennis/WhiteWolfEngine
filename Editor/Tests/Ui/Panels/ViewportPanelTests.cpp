#include "Assets/TestAssets.h"
#include "Object/ObjectGlobals.h"
#include "Rendering/StaticMesh.h"
#include "Rendering/StaticMeshComponent.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Ui/Panels/TestViewport.h"
#include "Ui/TestEditorUi.h"
#include "Viewport/ViewportCamera.h"

#include <doctest/doctest.h>
#include <glm/geometric.hpp>

#include <cmath>
#include <cstddef>
#include <ostream>
#include <string>
#include <vector>

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

	/** Turns the wheel of the mouse by a number of notches, away from the user if positive. */
	void TurnWheel(FEditorUiTest& Test, double Notches)
	{
		Test.Input.HandleScroll(FVector2D(0.0, Notches));
		Test.DrawFrame();
	}

	/** Returns true for the orange of the line around what is selected: much red, some green and hardly any blue. */
	bool IsOrange(FColor Pixel)
	{
		return Pixel.R > 200 && Pixel.G > 100 && Pixel.G < 190 && Pixel.B < 60;
	}

	/**
	 * Returns how many pixels of the viewport's picture are that orange, among those whose distance from the left
	 * edge of the surface is at least From and less than To.
	 */
	int32 CountOrangePixels(const FEditorUiTest& Test, double From, double To)
	{
		const FUiRectangle Picture = Test.RequireItem("Viewport");
		const FImage Image = Test.ReadPicture();
		int32 Count = 0;
		for (int32 Y = static_cast<int32>(Picture.Top); Y < static_cast<int32>(Picture.Bottom); ++Y)
		{
			for (int32 X = static_cast<int32>(Picture.Left); X < static_cast<int32>(Picture.Right); ++X)
			{
				const bool bIsInRange = static_cast<double>(X) >= From && static_cast<double>(X) < To;
				Count += bIsInRange && IsOrange(Image.GetPixel(X, Y)) ? 1 : 0;
			}
		}
		return Count;
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

	TEST_CASE("A click on a mesh in the viewport selects its entity, and a click beside it selects nothing")
	{
		FEditorUiTest Test;
		const FViewportCamera Camera = Test.Editor.GetViewportCamera();
		const int64 Crate = AddBox(Test, "Crate", Camera.GetFocusPoint());
		Test.DrawFrame();
		const std::size_t StepCount = Test.Editor.GetHistory().GetUndoStepNames().size();
		REQUIRE(Test.Editor.GetSelection().empty());

		// What the camera looks at is in the middle of the picture.
		Test.Click("Viewport");

		CHECK(Test.Editor.GetSelection() == std::vector<int64>{Crate});
		// What is selected shows where it does for a click in the outliner.
		CHECK(Test.HasItem("Details/Name"));

		// Three metres to the side there is nothing.
		Test.ClickAt(FindInViewport(Test, Camera.GetFocusPoint() + FVector(Camera.GetRight()) * 3.0));

		CHECK(Test.Editor.GetSelection().empty());
		// Selecting is no change to the scene, and so no step of its history. Nor does it move the camera.
		CHECK(Test.Editor.GetHistory().GetUndoStepNames().size() == StepCount);
		CHECK(Test.Editor.GetViewportCamera() == Camera);
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("Of two meshes behind one another, a click selects the one that is seen")
	{
		FEditorUiTest Test;
		const FViewportCamera Camera = Test.Editor.GetViewportCamera();
		const FVector Ahead = FVector(Camera.GetForward());
		// The one that is hidden is made first: the order of the entities is not what decides.
		const int64 Behind = AddBox(Test, "Behind", Camera.GetFocusPoint() + Ahead * 3.0);
		const int64 InFront = AddBox(Test, "In Front", Camera.GetFocusPoint());
		Test.DrawFrame();

		Test.Click("Viewport");
		CHECK(Test.Editor.GetSelection() == std::vector<int64>{InFront});

		// A mesh that is not drawn is clicked through. Nothing is selected for that click: what is selected has
		// the gizmo on it, and a press on the gizmo is no click on what is behind it.
		WEntity* const FrontEntity = Test.Editor.GetScene().FindEntityById(InFront);
		REQUIRE(FrontEntity != nullptr);
		FrontEntity->FindComponent<WStaticMeshComponent>()->bVisible = false;
		Test.Editor.SetSelection({});
		Test.DrawFrame();

		Test.Click("Viewport");
		CHECK(Test.Editor.GetSelection() == std::vector<int64>{Behind});
	}

	TEST_CASE("A click selects the entity that shows the mesh, not the one above it")
	{
		FEditorUiTest Test;
		const FViewportCamera Camera = Test.Editor.GetViewportCamera();
		WEntity* const Cart = Test.Editor.GetScene().CreateEntity("Cart");
		REQUIRE(Cart != nullptr);
		const int64 Wheel = AddBox(Test, "Wheel", Camera.GetFocusPoint());
		REQUIRE(Test.Editor.GetScene().FindEntityById(Wheel)->SetParent(Cart));
		Test.DrawFrame();

		Test.Click("Viewport");

		CHECK(Test.Editor.GetSelection() == std::vector<int64>{Wheel});
	}

	TEST_CASE("With Ctrl held, a click adds an entity to what is selected or takes it out")
	{
		FEditorUiTest Test;
		const FViewportCamera Camera = Test.Editor.GetViewportCamera();
		const FVector Right = FVector(Camera.GetRight());
		const int64 Left = AddBox(Test, "Left", Camera.GetFocusPoint() - Right * 1.5);
		const int64 Middle = AddBox(Test, "Middle", Camera.GetFocusPoint());
		Test.DrawFrame();
		const FVector2D OnLeft = FindInViewport(Test, Camera.GetFocusPoint() - Right * 1.5);
		const FVector2D OnMiddle = FindInViewport(Test, Camera.GetFocusPoint());
		const FVector2D OnNothing = FindInViewport(Test, Camera.GetFocusPoint() + Right * 3.0);

		Test.ClickAt(OnLeft);
		REQUIRE(Test.Editor.GetSelection() == std::vector<int64>{Left});

		Test.SetKeyDown(EKey::LeftControl, true);
		Test.ClickAt(OnMiddle);
		CHECK(Test.Editor.GetSelection() == std::vector<int64>{Left, Middle});

		Test.ClickAt(OnLeft);
		CHECK(Test.Editor.GetSelection() == std::vector<int64>{Middle});

		// On nothing, it leaves what is selected as it is.
		Test.ClickAt(OnNothing);
		CHECK(Test.Editor.GetSelection() == std::vector<int64>{Middle});
		Test.SetKeyDown(EKey::LeftControl, false);

		// Without Ctrl, a click selects what it is on and nothing else.
		Test.ClickAt(OnLeft);
		CHECK(Test.Editor.GetSelection() == std::vector<int64>{Left});
	}

	TEST_CASE("A press that takes the mouse somewhere else is no click")
	{
		FEditorUiTest Test;
		const FViewportCamera Camera = Test.Editor.GetViewportCamera();
		const FVector Right = FVector(Camera.GetRight());
		const int64 Left = AddBox(Test, "Left", Camera.GetFocusPoint() - Right * 1.5);
		AddBox(Test, "Middle", Camera.GetFocusPoint());
		Test.DrawFrame();
		const FVector2D OnLeft = FindInViewport(Test, Camera.GetFocusPoint() - Right * 1.5);
		const FVector2D OnMiddle = FindInViewport(Test, Camera.GetFocusPoint());
		const FVector2D OnNothing = FindInViewport(Test, Camera.GetFocusPoint() + Right * 3.0);
		Test.ClickAt(OnLeft);
		REQUIRE(Test.Editor.GetSelection() == std::vector<int64>{Left});

		// From nothing onto a mesh, from a mesh onto nothing, and from one mesh onto the other.
		Test.DragBetween(OnNothing, OnMiddle);
		CHECK(Test.Editor.GetSelection() == std::vector<int64>{Left});
		Test.DragBetween(OnMiddle, OnNothing);
		CHECK(Test.Editor.GetSelection() == std::vector<int64>{Left});
		Test.DragBetween(OnLeft, OnMiddle);
		CHECK(Test.Editor.GetSelection() == std::vector<int64>{Left});
	}

	TEST_CASE("The buttons that steer the camera select nothing")
	{
		FEditorUiTest Test;
		const FViewportCamera Camera = Test.Editor.GetViewportCamera();
		const FVector Right = FVector(Camera.GetRight());
		const int64 Left = AddBox(Test, "Left", Camera.GetFocusPoint() - Right * 1.5);
		AddBox(Test, "Middle", Camera.GetFocusPoint());
		Test.DrawFrame();
		Test.ClickAt(FindInViewport(Test, Camera.GetFocusPoint() - Right * 1.5));
		REQUIRE(Test.Editor.GetSelection() == std::vector<int64>{Left});

		// On the mesh in the middle, without moving the mouse: the right button, the middle one, and the left one
		// with Alt held, which is the beginning of going around what the camera looks at.
		Test.Click("Viewport", EMouseButton::Right);
		Test.Click("Viewport", EMouseButton::Middle);
		Test.SetKeyDown(EKey::LeftAlt, true);
		Test.Click("Viewport");
		Test.SetKeyDown(EKey::LeftAlt, false);

		CHECK(Test.Editor.GetSelection() == std::vector<int64>{Left});
		CHECK(Test.Editor.GetViewportCamera() == Camera);
	}

	TEST_CASE("What is selected has an orange line around it in the viewport, and nothing else has")
	{
		FEditorUiTest Test;
		const FViewportCamera Camera = Test.Editor.GetViewportCamera();
		const FVector Right = FVector(Camera.GetRight());
		const int64 Left = AddBox(Test, "Left", Camera.GetFocusPoint() - Right * 2.0);
		const int64 Middle = AddBox(Test, "Middle", Camera.GetFocusPoint());
		Test.DrawFrame();
		const FVector2D OnLeft = FindInViewport(Test, Camera.GetFocusPoint() - Right * 2.0);
		const FVector2D OnMiddle = FindInViewport(Test, Camera.GetFocusPoint());
		const double Between = (OnLeft.x + OnMiddle.x) * 0.5;
		REQUIRE(CountOrangePixels(Test, 0.0, 1.0e9) == 0);

		// The line is around the one that is selected: on its side of the middle between the two.
		Test.Editor.SetSelection({Middle});
		Test.DrawFrame();
		CHECK(CountOrangePixels(Test, Between, 1.0e9) > 100);
		CHECK(CountOrangePixels(Test, 0.0, Between) == 0);
		// It goes around the box, and is not on it.
		CHECK_FALSE(
			IsOrange(Test.ReadPicture().GetPixel(static_cast<int32>(OnMiddle.x), static_cast<int32>(OnMiddle.y))));

		Test.Editor.SetSelection({Left});
		Test.DrawFrame();
		CHECK(CountOrangePixels(Test, 0.0, Between) > 100);
		CHECK(CountOrangePixels(Test, Between, 1.0e9) == 0);

		// Both, and then neither.
		Test.Editor.SetSelection({Left, Middle});
		Test.DrawFrame();
		CHECK(CountOrangePixels(Test, 0.0, Between) > 100);
		CHECK(CountOrangePixels(Test, Between, 1.0e9) > 100);

		Test.Editor.SetSelection({});
		Test.DrawFrame();
		CHECK(CountOrangePixels(Test, 0.0, 1.0e9) == 0);
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("The line around what is selected is wider where the user interface is larger")
	{
		FEditorUiTest Test;
		const FViewportCamera Camera = Test.Editor.GetViewportCamera();
		const int64 Crate = AddBox(Test, "Crate", Camera.GetFocusPoint());

		// The gizmo is on what was selected last. Here that is an entity behind the camera, so that no gizmo lies
		// over the line around the crate, a larger one over more of it.
		WEntity* const Elsewhere = Test.Editor.GetScene().CreateEntity("Elsewhere");
		REQUIRE(Elsewhere != nullptr);
		Elsewhere->SetWorldLocation(Camera.Location - FVector(Camera.GetForward()) * 10.0);
		Test.Editor.SetSelection({Crate, Elsewhere->GetId()});
		Test.DrawFrame();
		REQUIRE_FALSE(Test.HasItem("Viewport/Gizmo"));
		const int32 AtScaleOne = CountOrangePixels(Test, 0.0, 1.0e9);
		REQUIRE(AtScaleOne > 100);

		// Twice the scale is twice the width, around a box that the larger panels leave about as large.
		Test.Ui->SetScale(2.0f);
		Test.DrawFrames(3);

		CHECK(CountOrangePixels(Test, 0.0, 1.0e9) > AtScaleOne * 3 / 2);
	}
}
