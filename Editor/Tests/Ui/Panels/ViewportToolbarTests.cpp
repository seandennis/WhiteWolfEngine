#include "Assets/TestAssets.h"
#include "Math/Transform.h"
#include "Math/Vector.h"
#include "Rendering/SceneViewMode.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Ui/Panels/TestViewport.h"
#include "Ui/TestEditorUi.h"
#include "Viewport/GizmoMode.h"
#include "Viewport/GizmoSettings.h"
#include "Viewport/GizmoSpace.h"
#include "Viewport/ViewportCamera.h"

#include <doctest/doctest.h>

#include <cmath>
#include <cstddef>
#include <format>
#include <initializer_list>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

// The toolbar that lies over the viewport's picture, and the keys that do what its first buttons do. Both are
// used as a person uses them, through FEditorUi; what they set is asked of the editor.

namespace
{
	/** Returns the name of an item of the viewport's toolbar. */
	std::string Tool(std::string_view Name)
	{
		return std::format("Viewport/Toolbar/{}", Name);
	}

	const FGizmoSettings& GetSettings(const FEditorUiTest& Test)
	{
		return Test.Editor.GetGizmoSettings();
	}

	/** Adds an entity that shows nothing, where the viewport's camera looks, selects it, and returns its identifier. */
	int64 AddSelectedEntity(FEditorUiTest& Test)
	{
		WEntity* const Entity = Test.Editor.GetScene().CreateEntity("Crate");
		REQUIRE(Entity != nullptr);
		Entity->SetWorldLocation(Test.Editor.GetViewportCamera().GetFocusPoint());
		Test.Editor.SetSelection({Entity->GetId()});
		Test.DrawFrame();
		return Entity->GetId();
	}

	/** Adds a box that fills the picture of the viewport, so that a click anywhere on the picture is a click on it. */
	int64 AddWall(FEditorUiTest& Test)
	{
		const int64 Wall = AddBox(Test, "Wall", Test.Editor.GetViewportCamera().GetFocusPoint());
		FTransform Transform = Test.Editor.GetScene().FindEntityById(Wall)->GetRelativeTransform();
		Transform.Scale = FVector3f(10.0f);
		Test.Editor.GetScene().FindEntityById(Wall)->SetRelativeTransform(Transform);
		Test.DrawFrame();
		return Wall;
	}

	/** Every item of the toolbar that is there whether or not a list is open. */
	const std::vector<std::string> ToolNames = {"Move",
		"Rotate",
		"Scale",
		"Space",
		"SnapLocation",
		"LocationSnap",
		"SnapRotation",
		"RotationSnap",
		"SnapScale",
		"ScaleSnap",
		"CameraSpeed",
		"ViewMode"};
}

TEST_SUITE("FViewportToolbar")
{
	TEST_CASE("The toolbar lies over the top of the viewport's picture, with all its buttons in a row")
	{
		FEditorUiTest Test;
		const FUiRectangle Picture = Test.RequireItem("Viewport");
		const FUiRectangle First = Test.RequireItem(Tool("Move"));

		for (const std::string& Name : ToolNames)
		{
			CAPTURE(Name);
			const FUiRectangle Item = Test.RequireItem(Tool(Name));
			CHECK(Item.Left >= Picture.Left);
			CHECK(Item.Right <= Picture.Right);
			CHECK(Item.Top >= Picture.Top);
			// On the row of the first of them, near the top of the picture.
			CHECK(Item.Top == doctest::Approx(First.Top));
			CHECK(Item.Top < Picture.Top + 40.0f);
		}
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("Where the picture is too narrow for one row, the toolbar goes on in the next")
	{
		FEditorUiTest Test(760, 600);
		const FUiRectangle Picture = Test.RequireItem("Viewport");
		const FUiRectangle First = Test.RequireItem(Tool("Move"));
		const FUiRectangle Last = Test.RequireItem(Tool("ViewMode"));

		// Everything is still there, and within the picture.
		for (const std::string& Name : ToolNames)
		{
			CAPTURE(Name);
			const FUiRectangle Item = Test.RequireItem(Tool(Name));
			CHECK(Item.Left >= Picture.Left);
			CHECK(Item.Right <= Picture.Right);
		}
		CHECK(Last.Top >= First.Bottom);

		// What goes on in the next row can be used as well as what is in the first.
		Test.Click(Tool("ViewMode"));
		Test.Click(Tool("ViewMode/Unlit"));
		CHECK(Test.Editor.GetViewMode() == ESceneViewMode::Unlit);
	}

	TEST_CASE("The first buttons choose what dragging the gizmo does")
	{
		FEditorUiTest Test;
		const int64 Crate = AddSelectedEntity(Test);
		const std::size_t StepCount = Test.Editor.GetHistory().GetUndoStepNames().size();
		REQUIRE(GetSettings(Test).Mode == EGizmoMode::Translate);
		REQUIRE(Test.HasItem("Viewport/Gizmo/XY"));

		Test.Click(Tool("Rotate"));
		CHECK(GetSettings(Test).Mode == EGizmoMode::Rotate);
		// The gizmo is the one for turning: rings, and no squares.
		CHECK(Test.HasItem("Viewport/Gizmo/X"));
		CHECK_FALSE(Test.HasItem("Viewport/Gizmo/XY"));

		Test.Click(Tool("Scale"));
		CHECK(GetSettings(Test).Mode == EGizmoMode::Scale);
		CHECK(Test.HasItem("Viewport/Gizmo/All"));

		Test.Click(Tool("Move"));
		CHECK(GetSettings(Test).Mode == EGizmoMode::Translate);
		CHECK(Test.HasItem("Viewport/Gizmo/XY"));

		// None of it is a change to the scene, or to what is selected.
		CHECK(Test.Editor.GetHistory().GetUndoStepNames().size() == StepCount);
		CHECK(Test.Editor.GetSelection() == std::vector<int64>{Crate});
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("The button of the axes goes from those of the world to a thing's own, and back")
	{
		FEditorUiTest Test;
		REQUIRE(GetSettings(Test).Space == EGizmoSpace::World);

		Test.Click(Tool("Space"));
		CHECK(GetSettings(Test).Space == EGizmoSpace::Local);
		// Whichever of its two words it shows, it is the same button in the same place.
		const FUiRectangle Showing = Test.RequireItem(Tool("Space"));

		Test.Click(Tool("Space"));
		CHECK(GetSettings(Test).Space == EGizmoSpace::World);
		CHECK(Test.RequireItem(Tool("Space")) == Showing);
		CHECK(GetSettings(Test).Mode == EGizmoMode::Translate);
	}

	TEST_CASE("The buttons that show the steps switch stepping on and off")
	{
		FEditorUiTest Test;
		REQUIRE_FALSE(GetSettings(Test).bSnapLocation);
		REQUIRE_FALSE(GetSettings(Test).bSnapRotation);
		REQUIRE_FALSE(GetSettings(Test).bSnapScale);

		Test.Click(Tool("SnapLocation"));
		CHECK(GetSettings(Test).bSnapLocation);
		CHECK_FALSE(GetSettings(Test).bSnapRotation);
		CHECK_FALSE(GetSettings(Test).bSnapScale);

		Test.Click(Tool("SnapRotation"));
		Test.Click(Tool("SnapScale"));
		CHECK(GetSettings(Test).bSnapLocation);
		CHECK(GetSettings(Test).bSnapRotation);
		CHECK(GetSettings(Test).bSnapScale);

		Test.Click(Tool("SnapLocation"));
		Test.Click(Tool("SnapScale"));
		CHECK_FALSE(GetSettings(Test).bSnapLocation);
		CHECK(GetSettings(Test).bSnapRotation);
		CHECK_FALSE(GetSettings(Test).bSnapScale);

		// The steps themselves are as they were.
		CHECK(GetSettings(Test).LocationSnap == FGizmoSettings().LocationSnap);
		CHECK(GetSettings(Test).RotationSnap == FGizmoSettings().RotationSnap);
		CHECK(GetSettings(Test).ScaleSnap == FGizmoSettings().ScaleSnap);
	}

	TEST_CASE("The lists beside them choose the steps")
	{
		FEditorUiTest Test;
		CHECK_FALSE(Test.HasItem(Tool("LocationSnap/0.5")));

		Test.Click(Tool("LocationSnap"));
		REQUIRE(Test.HasItem(Tool("LocationSnap/0.5")));
		// The list is below its button, as that of any field with an arrow.
		CHECK(Test.RequireItem(Tool("LocationSnap/0.5")).Top >= Test.RequireItem(Tool("LocationSnap")).Bottom);
		Test.Click(Tool("LocationSnap/0.5"));
		CHECK(GetSettings(Test).LocationSnap == 0.5);
		// It has closed, and choosing a step has not switched stepping on.
		CHECK_FALSE(Test.HasItem(Tool("LocationSnap/0.5")));
		CHECK_FALSE(GetSettings(Test).bSnapLocation);

		Test.Click(Tool("RotationSnap"));
		Test.Click(Tool("RotationSnap/45"));
		CHECK(GetSettings(Test).RotationSnap == 45.0f);

		Test.Click(Tool("ScaleSnap"));
		Test.Click(Tool("ScaleSnap/0.5"));
		CHECK(GetSettings(Test).ScaleSnap == 0.5f);
		CHECK(GetSettings(Test).LocationSnap == 0.5);
		CHECK(GetSettings(Test).IsValid());
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("Every step that the lists offer is one that the gizmo takes")
	{
		FEditorUiTest Test;

		for (const std::string_view Step : {"0.01", "0.05", "0.1", "0.25", "0.5", "1", "5", "10"})
		{
			CAPTURE(Step);
			Test.Click(Tool("LocationSnap"));
			Test.Click(Tool(std::format("LocationSnap/{}", Step)));
			CHECK(std::format("{:.6g}", GetSettings(Test).LocationSnap) == Step);
		}
		for (const std::string_view Step : {"1", "5", "10", "15", "30", "45", "60", "90"})
		{
			CAPTURE(Step);
			Test.Click(Tool("RotationSnap"));
			Test.Click(Tool(std::format("RotationSnap/{}", Step)));
			CHECK(std::format("{:.6g}", GetSettings(Test).RotationSnap) == Step);
		}
		for (const std::string_view Step : {"0.1", "0.25", "0.5", "1"})
		{
			CAPTURE(Step);
			Test.Click(Tool("ScaleSnap"));
			Test.Click(Tool(std::format("ScaleSnap/{}", Step)));
			CHECK(std::format("{:.6g}", GetSettings(Test).ScaleSnap) == Step);
		}
		CHECK(GetSettings(Test).IsValid());
	}

	TEST_CASE("What the toolbar sets is what a drag of the gizmo does")
	{
		FEditorUiTest Test;
		FViewportCamera Camera = Test.Editor.GetViewportCamera();
		Camera.Location = FVector(0.0, 0.0, 10.0);
		Camera.Yaw = 0.0f;
		Camera.Pitch = 0.0f;
		Camera.FocusDistance = 10.0;
		Test.Editor.SetViewportCamera(Camera);
		const int64 Crate = AddSelectedEntity(Test);

		// In steps of half a metre, a drag of some way along X is a whole number of halves.
		Test.Click(Tool("LocationSnap"));
		Test.Click(Tool("LocationSnap/0.5"));
		Test.Click(Tool("SnapLocation"));
		Test.DragBy("Viewport/Gizmo/X", FVector2D(70.0, 0.0));

		const FVector Location = Test.Editor.GetScene().FindEntityById(Crate)->GetRelativeTransform().Location;
		CHECK(Location.x >= 0.5);
		CHECK(std::round(Location.x * 2.0) == Location.x * 2.0);
		CHECK(Location.y == 0.0);
		CHECK(Location.z == 0.0);
	}

	TEST_CASE("The speed of the camera is dragged and typed in the toolbar")
	{
		FEditorUiTest Test;
		const FViewportCamera Before = Test.Editor.GetViewportCamera();

		Test.DragBy(Tool("CameraSpeed"), FVector2D(30.0, 0.0));

		const FViewportCamera Faster = Test.Editor.GetViewportCamera();
		CHECK(Faster.FlySpeed > Before.FlySpeed);
		CHECK(Faster.FlySpeed <= FViewportCamera::MaxFlySpeed);
		// Nothing else about the camera has changed.
		FViewportCamera Expected = Before;
		Expected.FlySpeed = Faster.FlySpeed;
		CHECK(Faster == Expected);

		// Far to the left is the slowest that the camera flies, and no slower. A second press that follows the
		// first at once would be a double click, which is another way to make a field to type in: some time
		// goes by first.
		constexpr int32 FramesOfHalfASecond = 30;
		Test.DrawFrames(FramesOfHalfASecond);
		Test.DragBy(Tool("CameraSpeed"), FVector2D(-600.0, 0.0));
		CHECK(Test.Editor.GetViewportCamera().FlySpeed == FViewportCamera::MinFlySpeed);

		// A click with Ctrl held makes a field to type in.
		Test.SetKeyDown(EKey::LeftControl, true);
		Test.Click(Tool("CameraSpeed"));
		Test.SetKeyDown(EKey::LeftControl, false);
		Test.Type("12.5");
		Test.PressKey(EKey::Enter);
		CHECK(Test.Editor.GetViewportCamera().FlySpeed == 12.5f);
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("A speed that is too fast, or that is no number, is not the camera's")
	{
		FEditorUiTest Test;
		const auto TypeSpeed = [&Test](std::string_view Text)
		{
			Test.SetKeyDown(EKey::LeftControl, true);
			Test.Click(Tool("CameraSpeed"));
			Test.SetKeyDown(EKey::LeftControl, false);
			Test.Type(Text);
			Test.PressKey(EKey::Enter);
		};

		TypeSpeed("5000000");
		CHECK(Test.Editor.GetViewportCamera().FlySpeed == FViewportCamera::MaxFlySpeed);

		TypeSpeed("-3");
		CHECK(Test.Editor.GetViewportCamera().FlySpeed == FViewportCamera::MinFlySpeed);

		TypeSpeed("nan");
		CHECK(Test.Editor.GetViewportCamera().IsValid());
		TypeSpeed("fast");
		CHECK(Test.Editor.GetViewportCamera().IsValid());
		CHECK(Test.Editor.GetViewportCamera().FlySpeed == FViewportCamera::MinFlySpeed);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("The last list chooses how the scene is shaded")
	{
		FEditorUiTest Test;
		REQUIRE(Test.Editor.GetViewMode() == ESceneViewMode::Lit);
		CHECK_FALSE(Test.HasItem(Tool("ViewMode/Unlit")));

		Test.Click(Tool("ViewMode"));
		REQUIRE(Test.HasItem(Tool("ViewMode/Lit")));
		REQUIRE(Test.HasItem(Tool("ViewMode/Unlit")));
		REQUIRE(Test.HasItem(Tool("ViewMode/Ambient Occlusion")));
		Test.Click(Tool("ViewMode/Unlit"));
		CHECK(Test.Editor.GetViewMode() == ESceneViewMode::Unlit);
		CHECK_FALSE(Test.HasItem(Tool("ViewMode/Unlit")));

		Test.Click(Tool("ViewMode"));
		Test.Click(Tool("ViewMode/Ambient Occlusion"));
		CHECK(Test.Editor.GetViewMode() == ESceneViewMode::AmbientOcclusion);

		Test.Click(Tool("ViewMode"));
		Test.Click(Tool("ViewMode/Lit"));
		CHECK(Test.Editor.GetViewMode() == ESceneViewMode::Lit);
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("What the mouse does on the toolbar, it does not do on the picture behind it")
	{
		FEditorUiTest Test;
		const int64 Wall = AddWall(Test);
		const FViewportCamera Camera = Test.Editor.GetViewportCamera();

		// Beside the toolbar, a click on the picture selects the wall that fills it.
		Test.Click("Viewport");
		REQUIRE(Test.Editor.GetSelection() == std::vector<int64>{Wall});
		Test.Editor.SetSelection({});
		Test.DrawFrame();

		// On a button of the toolbar it does what the button is for, and selects nothing.
		Test.Click(Tool("Rotate"));
		CHECK(GetSettings(Test).Mode == EGizmoMode::Rotate);
		CHECK(Test.Editor.GetSelection().empty());

		// Nor do the buttons and the wheel steer the camera there.
		const FVector2D OnToolbar = Test.RequireItem(Tool("Scale")).GetCenter();
		PressAt(Test, OnToolbar, EMouseButton::Right);
		CHECK_FALSE(Test.Ui->WantsMouseCaptured());
		MoveBy(Test, FVector2D(40.0, 30.0));
		Release(Test, EMouseButton::Right);
		Test.MoveMouse(OnToolbar);
		Test.Input.HandleScroll(FVector2D(0.0, 2.0));
		Test.DrawFrame();
		CHECK(Test.Editor.GetViewportCamera() == Camera);
		CHECK(Test.Editor.GetSelection().empty());
	}

	TEST_CASE("W, E and R choose what dragging the gizmo does, and Space the next of the three")
	{
		FEditorUiTest Test;
		REQUIRE(GetSettings(Test).Mode == EGizmoMode::Translate);

		Test.PressKey(EKey::E);
		CHECK(GetSettings(Test).Mode == EGizmoMode::Rotate);
		Test.PressKey(EKey::R);
		CHECK(GetSettings(Test).Mode == EGizmoMode::Scale);
		Test.PressKey(EKey::W);
		CHECK(GetSettings(Test).Mode == EGizmoMode::Translate);

		Test.PressKey(EKey::Space);
		CHECK(GetSettings(Test).Mode == EGizmoMode::Rotate);
		Test.PressKey(EKey::Space);
		CHECK(GetSettings(Test).Mode == EGizmoMode::Scale);
		Test.PressKey(EKey::Space);
		CHECK(GetSettings(Test).Mode == EGizmoMode::Translate);

		// With Ctrl held, these are other shortcuts or none.
		Test.PressKey(EKey::E, {EKey::LeftControl});
		Test.PressKey(EKey::R, {EKey::LeftControl});
		CHECK(GetSettings(Test).Mode == EGizmoMode::Translate);
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("The keys of the gizmo are letters where something is being typed")
	{
		FEditorUiTest Test;
		WScene& Scene = Test.Editor.GetScene();
		const int64 Dancer = Scene.CreateEntity("Dancer")->GetId();
		const int64 Wall = Scene.CreateEntity("Wall")->GetId();
		Test.DrawFrame();
		Test.Click("Outliner/Search");

		// Each key is pressed where it would change what the gizmo does, were it the gizmo's: a letter from a mode
		// that is not its own, and Space, which goes on from any. One look at the end would not do: E, R and Space
		// after one another come back to where they began.
		const auto StaysWhenPressed = [&Test](EKey Key, EGizmoMode Before)
		{
			FGizmoSettings Settings = GetSettings(Test);
			Settings.Mode = Before;
			Test.Editor.SetGizmoSettings(Settings);
			Test.PressKey(Key);
			return GetSettings(Test).Mode == Before;
		};
		CHECK(StaysWhenPressed(EKey::W, EGizmoMode::Rotate));
		CHECK(StaysWhenPressed(EKey::E, EGizmoMode::Scale));
		CHECK(StaysWhenPressed(EKey::R, EGizmoMode::Translate));
		CHECK(StaysWhenPressed(EKey::Space, EGizmoMode::Translate));

		// The letters are the field's: "er" finds the dancer and not the wall.
		Test.Type("er");
		CHECK(Test.HasItem(std::format("Outliner/{}", Dancer)));
		CHECK_FALSE(Test.HasItem(std::format("Outliner/{}", Wall)));
		CHECK(GetSettings(Test).Mode == EGizmoMode::Translate);
	}

	TEST_CASE("While a mouse button is held, the keys of the gizmo are the camera's")
	{
		FEditorUiTest Test;
		const FViewportCamera Before = Test.Editor.GetViewportCamera();

		// With the right button, E flies the camera upwards.
		PressOnViewport(Test, EMouseButton::Right);
		Test.Input.HandleKeyEvent(EKey::E, EInputEvent::Pressed);
		Test.DrawFrames(10);
		Test.Input.HandleKeyEvent(EKey::E, EInputEvent::Released);
		Test.DrawFrame();
		Release(Test, EMouseButton::Right);
		CHECK(Test.Editor.GetViewportCamera().Location.y > Before.Location.y);
		CHECK(GetSettings(Test).Mode == EGizmoMode::Translate);

		// With any other button down, the keys do nothing. Each key is tried by itself: R and then Space would come
		// back to moving.
		const auto ChangesGizmoWhileHeld = [&Test](EMouseButton Button, EKey Key)
		{
			const EGizmoMode Before = GetSettings(Test).Mode;
			PressOnViewport(Test, Button);
			Test.PressKey(Key);
			Release(Test, Button);
			return GetSettings(Test).Mode != Before;
		};
		CHECK_FALSE(ChangesGizmoWhileHeld(EMouseButton::Left, EKey::R));
		CHECK_FALSE(ChangesGizmoWhileHeld(EMouseButton::Left, EKey::Space));
		CHECK_FALSE(ChangesGizmoWhileHeld(EMouseButton::Middle, EKey::R));
		CHECK_FALSE(ChangesGizmoWhileHeld(EMouseButton::Middle, EKey::Space));
		CHECK(GetSettings(Test).Mode == EGizmoMode::Translate);

		// With the buttons up again, they are the gizmo's.
		Test.PressKey(EKey::R);
		CHECK(GetSettings(Test).Mode == EGizmoMode::Scale);
	}

	TEST_CASE("A key of the gizmo that goes down with a mouse button, while the mouse moves, is the camera's")
	{
		FEditorUiTest Test;
		REQUIRE(GetSettings(Test).Mode == EGizmoMode::Translate);
		const FViewportCamera Before = Test.Editor.GetViewportCamera();
		const FVector2D Middle = Test.RequireItem("Viewport").GetCenter();
		Test.MoveMouse(Middle);

		// All between two frames, as a hand does that begins to fly upwards: the mouse moves on, and the button and
		// the key go down.
		Test.Input.HandleMouseMove(Middle + FVector2D(3.0, 0.0));
		Test.Input.HandleMouseButtonEvent(EMouseButton::Right, EInputEvent::Pressed);
		Test.Input.HandleKeyEvent(EKey::E, EInputEvent::Pressed);
		Test.DrawFrames(10);
		Test.Input.HandleKeyEvent(EKey::E, EInputEvent::Released);
		Test.DrawFrame();
		Release(Test, EMouseButton::Right);

		// One thing and not two: E flew the camera upwards, and the gizmo is as it was.
		CHECK(Test.Editor.GetViewportCamera().Location.y > Before.Location.y);
		CHECK(GetSettings(Test).Mode == EGizmoMode::Translate);
	}

	TEST_CASE("A key of the gizmo that goes down in the frame in which the mouse button comes up is the gizmo's")
	{
		FEditorUiTest Test;
		REQUIRE(GetSettings(Test).Mode == EGizmoMode::Translate);

		SUBCASE("With the mouse where it was")
		{
			PressOnViewport(Test, EMouseButton::Left);
			Test.DrawFrames(3);
		}
		SUBCASE("With the mouse moving on")
		{
			PressOnViewport(Test, EMouseButton::Left);
			Test.DrawFrames(3);
			Test.Input.HandleMouseMove(Test.Input.GetMousePosition() + FVector2D(8.0, 0.0));
		}

		// The button comes up and E goes down between two frames.
		Test.Input.HandleMouseButtonEvent(EMouseButton::Left, EInputEvent::Released);
		Test.Input.HandleKeyEvent(EKey::E, EInputEvent::Pressed);
		Test.DrawFrame();
		Test.Input.HandleKeyEvent(EKey::E, EInputEvent::Released);
		Test.DrawFrames(3);

		CHECK(GetSettings(Test).Mode == EGizmoMode::Rotate);
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("While a window has to be answered, the keys of the gizmo do nothing")
	{
		FEditorUiTest Test;

		Test.Click("Menu/Help");
		Test.Click("Menu/Help/About White Wolf Engine");
		REQUIRE(Test.HasItem("About/Close"));
		Test.PressKey(EKey::E);
		Test.PressKey(EKey::Space);
		CHECK(GetSettings(Test).Mode == EGizmoMode::Translate);

		Test.Click("About/Close");
		Test.PressKey(EKey::E);
		CHECK(GetSettings(Test).Mode == EGizmoMode::Rotate);
	}

	TEST_CASE("The Edit menu has what dragging the gizmo does, with the keys for it")
	{
		FEditorUiTest Test;

		Test.Click("Menu/Edit");
		REQUIRE(Test.HasItem("Menu/Edit/Move"));
		REQUIRE(Test.HasItem("Menu/Edit/Rotate"));
		REQUIRE(Test.HasItem("Menu/Edit/Scale"));
		Test.Click("Menu/Edit/Rotate");
		CHECK(GetSettings(Test).Mode == EGizmoMode::Rotate);

		Test.Click("Menu/Edit");
		Test.Click("Menu/Edit/Scale");
		CHECK(GetSettings(Test).Mode == EGizmoMode::Scale);

		Test.Click("Menu/Edit");
		Test.Click("Menu/Edit/Move");
		CHECK(GetSettings(Test).Mode == EGizmoMode::Translate);
	}

	TEST_CASE("The toolbar shows what was set elsewhere: by a key, or by the code")
	{
		FEditorUiTest Test;

		// A step that the lists do not offer is shown, and is left alone by the toolbar.
		FGizmoSettings Settings;
		Settings.Mode = EGizmoMode::Scale;
		Settings.LocationSnap = 0.3;
		Settings.RotationSnap = 22.5f;
		Settings.ScaleSnap = 0.2f;
		Settings.bSnapRotation = true;
		Test.Editor.SetGizmoSettings(Settings);
		Test.DrawFrames(3);
		CHECK(GetSettings(Test) == Settings);

		// Its buttons are where they were, and work on from what is set.
		Test.Click(Tool("SnapRotation"));
		Settings.bSnapRotation = false;
		CHECK(GetSettings(Test) == Settings);
		Test.Click(Tool("Space"));
		Settings.Space = EGizmoSpace::Local;
		CHECK(GetSettings(Test) == Settings);
	}
}
