#include "Assets/TestAssets.h"
#include "Json/JsonValue.h"
#include "Math/Color.h"
#include "Math/Quat.h"
#include "Math/Transform.h"
#include "Math/Vector.h"
#include "Rendering/Image.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Ui/Panels/TestViewport.h"
#include "Ui/TestEditorUi.h"
#include "Viewport/GizmoMode.h"
#include "Viewport/GizmoSettings.h"
#include "Viewport/GizmoSpace.h"
#include "Viewport/ViewportCamera.h"

#include <doctest/doctest.h>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>
#include <format>
#include <initializer_list>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

// The gizmo is used here as a person uses it: with the mouse, on the picture of the scene, through FEditorUi. What
// a drag of a handle comes to is worked out by FGizmoDrag, which is tested by itself; these tests are about the
// gizmo being where it should be, the mouse taking hold of it, and the editor being given what a drag comes to.
//
// Most of them look at the scene from straight in front: from ten metres along Z, level, at where the world
// begins. X is then to the right in the picture, Y up, and Z points at the camera.

namespace
{
	const FQuat NoRotation(1.0f, 0.0f, 0.0f, 0.0f);
	const FVector3f AxisZ(0.0f, 0.0f, 1.0f);

	/** How far a drag may be from what the numbers say and still be right, as a part of the whole. */
	constexpr double DragTolerance = 0.01;

	void LookFromFront(FEditorUiTest& Test)
	{
		FViewportCamera Camera = Test.Editor.GetViewportCamera();
		Camera.Location = FVector(0.0, 0.0, 10.0);
		Camera.Yaw = 0.0f;
		Camera.Pitch = 0.0f;
		Camera.FocusDistance = 10.0;
		Test.Editor.SetViewportCamera(Camera);
	}

	/**
	 * Returns how many metres a pixel of the viewport's picture is at what the camera looks at. The height of the
	 * picture is known here to within a pixel of its edge, and so this to within a part in some hundreds.
	 */
	double GetPixelSize(const FEditorUiTest& Test)
	{
		const int32 PictureHeight = static_cast<int32>(Test.RequireItem("Viewport").GetHeight());
		return Test.Editor.GetViewportCamera().GetPixelSizeAtFocus(PictureHeight);
	}

	/**
	 * Returns a place on the viewport's picture that is well below its middle, where a gizmo in the middle of the
	 * picture has no handle.
	 */
	FVector2D FindPlaceBesideGizmo(const FEditorUiTest& Test)
	{
		constexpr double PartOfHeight = 0.35;
		const FUiRectangle Picture = Test.RequireItem("Viewport");
		return Picture.GetCenter() + FVector2D(0.0, static_cast<double>(Picture.GetHeight()) * PartOfHeight);
	}

	/** Returns how long the axes of the gizmo are in the picture, in pixels. */
	double GetAxisLength(const FEditorUiTest& Test)
	{
		return static_cast<double>(Test.RequireItem("Viewport/Gizmo").GetWidth()) * 0.5;
	}

	/** Adds an entity that shows nothing, at a place in the world, and returns its identifier. */
	int64 AddEntity(FEditorUiTest& Test, const char* Name, const FVector& Location, WEntity* Parent = nullptr)
	{
		WEntity* const Entity = Test.Editor.GetScene().CreateEntity(Name, Parent);
		REQUIRE(Entity != nullptr);
		Entity->SetWorldLocation(Location);
		return Entity->GetId();
	}

	/** Returns an entity as it is in the editor's scene now; an undo replaces the scene's objects. */
	const WEntity& GetEntity(const FEditorUiTest& Test, int64 EntityId)
	{
		const WEntity* const Entity = Test.Editor.GetScene().FindEntityById(EntityId);
		REQUIRE(Entity != nullptr);
		return *Entity;
	}

	const FTransform& GetTransform(const FEditorUiTest& Test, int64 EntityId)
	{
		return GetEntity(Test, EntityId).GetRelativeTransform();
	}

	/** Sets what the gizmo does, and along which axes, and shows it. */
	void SetGizmo(FEditorUiTest& Test, EGizmoMode Mode, EGizmoSpace Space = EGizmoSpace::World)
	{
		FGizmoSettings Settings = Test.Editor.GetGizmoSettings();
		Settings.Mode = Mode;
		Settings.Space = Space;
		Test.Editor.SetGizmoSettings(Settings);
		Test.DrawFrame();
	}

	/** Returns the name of the place where the mouse takes hold of a handle of the gizmo. */
	std::string Handle(std::string_view Name)
	{
		return std::format("Viewport/Gizmo/{}", Name);
	}

	/** Returns which of the handles the gizmo has places for now, in the order X, Y, Z, YZ, XZ, XY, All. */
	std::vector<std::string> FindHandles(const FEditorUiTest& Test)
	{
		std::vector<std::string> Found;
		for (const char* const Name : {"X", "Y", "Z", "YZ", "XZ", "XY", "All"})
		{
			if (Test.HasItem(Handle(Name)))
			{
				Found.emplace_back(Name);
			}
		}
		return Found;
	}

	/** Returns how many steps of the history that can be undone have a name. */
	int64 CountSteps(const FEditorUiTest& Test, std::string_view Name)
	{
		const std::vector<std::string> Names = Test.Editor.GetHistory().GetUndoStepNames();
		return static_cast<int64>(std::ranges::count(Names, Name));
	}

	/** Returns what the last step of the history is called, or nothing if there is none. */
	std::string GetLastStepName(const FEditorUiTest& Test)
	{
		const std::vector<std::string> Names = Test.Editor.GetHistory().GetUndoStepNames();
		return Names.empty() ? std::string() : Names.front();
	}

	/** Returns the pixel of the surface that is at the middle of an item. */
	FColor ReadMiddle(const FEditorUiTest& Test, const std::string& ItemName)
	{
		const FVector2D Middle = Test.RequireItem(ItemName).GetCenter();
		return Test.ReadPicture().GetPixel(static_cast<int32>(Middle.x), static_cast<int32>(Middle.y));
	}

	// The colours of the gizmo: the red of X, the green of Y, and the yellow of the handle that the mouse has.

	bool IsRed(FColor Pixel)
	{
		return Pixel.R > 170 && Pixel.G < 110 && Pixel.B < 110;
	}

	bool IsGreen(FColor Pixel)
	{
		return Pixel.G > 140 && Pixel.R < 140 && Pixel.B < 100;
	}

	bool IsYellow(FColor Pixel)
	{
		return Pixel.R > 220 && Pixel.G > 180 && Pixel.B < 110;
	}

	bool IsNear(const FVector2D& Actual, const FVector2D& Expected, double Tolerance)
	{
		return glm::length(Actual - Expected) <= Tolerance;
	}

	/** Returns by how many degrees a rotation turns things around +Z, counter-clockwise as seen from where Z points. */
	double GetTurnAroundZ(const FQuat& Rotation)
	{
		const FVector3f Turned = Rotation * FVector3f(1.0f, 0.0f, 0.0f);
		return glm::degrees(std::atan2(static_cast<double>(Turned.y), static_cast<double>(Turned.x)));
	}
}

TEST_SUITE("FViewportGizmo")
{
	TEST_CASE("The gizmo is on the entity that was selected last, and with nothing selected there is none")
	{
		FEditorUiTest Test;
		const FViewportCamera Camera = Test.Editor.GetViewportCamera();
		const FVector Beside = Camera.GetFocusPoint() - FVector(Camera.GetRight()) * 2.0;
		const int64 Left = AddEntity(Test, "Left", Beside);
		const int64 Middle = AddEntity(Test, "Middle", Camera.GetFocusPoint());
		Test.DrawFrame();
		CHECK_FALSE(Test.HasItem("Viewport/Gizmo"));
		CHECK(FindHandles(Test).empty());

		Test.Editor.SetSelection({Middle});
		Test.DrawFrame();
		const FUiRectangle OnMiddle = Test.RequireItem("Viewport/Gizmo");
		CHECK(IsNear(OnMiddle.GetCenter(), FindInViewport(Test, Camera.GetFocusPoint()), 1.5));
		// Its axes are as long in the picture whichever way they go.
		CHECK(OnMiddle.GetWidth() == doctest::Approx(OnMiddle.GetHeight()));
		CHECK(OnMiddle.GetWidth() > 100.0f);

		Test.Editor.SetSelection({Middle, Left});
		Test.DrawFrame();
		CHECK(IsNear(Test.RequireItem("Viewport/Gizmo").GetCenter(), FindInViewport(Test, Beside), 1.5));

		Test.Editor.SetSelection({Left, Middle});
		Test.DrawFrame();
		CHECK(
			IsNear(Test.RequireItem("Viewport/Gizmo").GetCenter(), FindInViewport(Test, Camera.GetFocusPoint()), 1.5));

		Test.Editor.SetSelection({});
		Test.DrawFrame();
		CHECK_FALSE(Test.HasItem("Viewport/Gizmo"));
		CHECK(FindHandles(Test).empty());
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("The gizmo has the handles of what it is set to do")
	{
		FEditorUiTest Test;
		const int64 Crate = AddEntity(Test, "Crate", Test.Editor.GetViewportCamera().GetFocusPoint());
		Test.Editor.SetSelection({Crate});
		Test.DrawFrame();

		// From the side and above, as the camera first looks, every handle can be used.
		const std::vector<std::string> ToMove = {"X", "Y", "Z", "YZ", "XZ", "XY"};
		CHECK(FindHandles(Test) == ToMove);

		SetGizmo(Test, EGizmoMode::Rotate);
		const std::vector<std::string> ToTurn = {"X", "Y", "Z"};
		CHECK(FindHandles(Test) == ToTurn);

		SetGizmo(Test, EGizmoMode::Scale);
		const std::vector<std::string> ToSize = {"X", "Y", "Z", "All"};
		CHECK(FindHandles(Test) == ToSize);

		// From straight in front, the handles along Z cannot be used: Z points at the camera.
		LookFromFront(Test);
		Test.Editor.GetScene().FindEntityById(Crate)->SetWorldLocation(FVector(0.0));
		SetGizmo(Test, EGizmoMode::Translate);
		const std::vector<std::string> FromFront = {"X", "Y", "XY"};
		CHECK(FindHandles(Test) == FromFront);
	}

	TEST_CASE("The handles have the colours of their axes, and the one under the mouse is yellow")
	{
		FEditorUiTest Test;
		LookFromFront(Test);
		const int64 Crate = AddEntity(Test, "Crate", FVector(0.0));
		Test.Editor.SetSelection({Crate});
		Test.DrawFrame();

		CHECK(IsRed(ReadMiddle(Test, Handle("X"))));
		CHECK(IsGreen(ReadMiddle(Test, Handle("Y"))));

		Test.MoveMouse(Test.RequireItem(Handle("X")).GetCenter());
		CHECK(IsYellow(ReadMiddle(Test, Handle("X"))));
		CHECK(IsGreen(ReadMiddle(Test, Handle("Y"))));

		// The handle that is dragged stays yellow, wherever the mouse goes.
		Test.Input.HandleMouseButtonEvent(EMouseButton::Left, EInputEvent::Pressed);
		Test.DrawFrame();
		MoveBy(Test, FVector2D(30.0, 60.0));
		CHECK(IsYellow(ReadMiddle(Test, Handle("X"))));
		Release(Test, EMouseButton::Left);

		// With the mouse elsewhere, no handle is.
		Test.MoveMouse(FindPlaceBesideGizmo(Test));
		CHECK(IsRed(ReadMiddle(Test, Handle("X"))));
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("Dragging an arrow moves what is selected along the arrow's axis, as one step")
	{
		FEditorUiTest Test;
		LookFromFront(Test);
		const int64 Crate = AddEntity(Test, "Crate", FVector(0.0));
		Test.Editor.SetSelection({Crate});
		Test.DrawFrame();
		const double PixelSize = GetPixelSize(Test);
		const FViewportCamera Camera = Test.Editor.GetViewportCamera();

		// To the right, and downwards as well: only what the mouse moves along the arrow counts.
		Test.DragBy(Handle("X"), FVector2D(60.0, 25.0));

		const FTransform AlongX = GetTransform(Test, Crate);
		CHECK(AlongX.Location.x == doctest::Approx(60.0 * PixelSize).epsilon(DragTolerance));
		CHECK(AlongX.Location.y == 0.0);
		CHECK(AlongX.Location.z == 0.0);
		CHECK(AlongX.Rotation == NoRotation);
		CHECK(AlongX.Scale == FVector3f(1.0f));
		CHECK(GetLastStepName(Test) == "Move");
		CHECK(CountSteps(Test, "Move") == 1);
		CHECK_FALSE(Test.Editor.IsStepOpen());

		// The gizmo has gone with what it moved. Neither the camera nor what is selected has changed.
		CHECK(IsNear(Test.RequireItem("Viewport/Gizmo").GetCenter(), FindInViewport(Test, AlongX.Location), 1.5));
		CHECK(Test.Editor.GetViewportCamera() == Camera);
		CHECK(Test.Editor.GetSelection() == std::vector<int64>{Crate});

		// Up in the picture is up in the world.
		Test.DragBy(Handle("Y"), FVector2D(0.0, -40.0));

		const FTransform AlongY = GetTransform(Test, Crate);
		CHECK(AlongY.Location.x == AlongX.Location.x);
		CHECK(AlongY.Location.y == doctest::Approx(40.0 * PixelSize).epsilon(DragTolerance));
		CHECK(AlongY.Location.z == 0.0);
		CHECK(CountSteps(Test, "Move") == 2);

		// Each drag is undone as the one thing that it was.
		Test.PressKey(EKey::Z, {EKey::LeftControl});
		CHECK(GetTransform(Test, Crate) == AlongX);
		Test.PressKey(EKey::Z, {EKey::LeftControl});
		CHECK(GetTransform(Test, Crate) == FTransform());
		Test.PressKey(EKey::Y, {EKey::LeftControl});
		CHECK(GetTransform(Test, Crate) == AlongX);
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("Dragging a square moves what is selected in the square's plane")
	{
		FEditorUiTest Test;
		LookFromFront(Test);
		const int64 Crate = AddEntity(Test, "Crate", FVector(0.0));
		Test.Editor.SetSelection({Crate});
		Test.DrawFrame();
		const double PixelSize = GetPixelSize(Test);

		Test.DragBy(Handle("XY"), FVector2D(40.0, -30.0));

		const FVector Location = GetTransform(Test, Crate).Location;
		CHECK(Location.x == doctest::Approx(40.0 * PixelSize).epsilon(DragTolerance));
		CHECK(Location.y == doctest::Approx(30.0 * PixelSize).epsilon(DragTolerance));
		CHECK(Location.z == 0.0);
		CHECK(GetLastStepName(Test) == "Move");
		CHECK(CountSteps(Test, "Move") == 1);
	}

	TEST_CASE("Dragging a ring turns what is selected around the ring's axis")
	{
		FEditorUiTest Test;
		LookFromFront(Test);
		const int64 Crate = AddEntity(Test, "Crate", FVector(0.0));
		Test.Editor.SetSelection({Crate});
		SetGizmo(Test, EGizmoMode::Rotate);
		const double AxisLength = GetAxisLength(Test);

		// The ring around Z is seen squarely, and taken hold of at its upper right. Up and to the left from there
		// is along the ring, counter-clockwise, and the length of an axis along it is one radian.
		const FVector2D OnRing = Test.RequireItem(Handle("Z")).GetCenter();
		const FVector2D Middle = Test.RequireItem("Viewport/Gizmo").GetCenter();
		REQUIRE(OnRing.x > Middle.x + 30.0);
		REQUIRE(OnRing.y < Middle.y - 30.0);
		Test.DragBy(Handle("Z"), FVector2D(-30.0, -30.0));

		const FTransform Turned = GetTransform(Test, Crate);
		const double ExpectedDegrees = glm::degrees(std::sqrt(1800.0) / AxisLength);
		CHECK(GetTurnAroundZ(Turned.Rotation) == doctest::Approx(ExpectedDegrees).epsilon(DragTolerance));
		CHECK(std::abs(glm::dot(glm::axis(Turned.Rotation), AxisZ)) == doctest::Approx(1.0f));
		// It turns where it is.
		CHECK(Turned.Location == FVector(0.0));
		CHECK(Turned.Scale == FVector3f(1.0f));
		CHECK(GetLastStepName(Test) == "Rotate");
		CHECK(CountSteps(Test, "Rotate") == 1);

		Test.PressKey(EKey::Z, {EKey::LeftControl});
		CHECK(GetTransform(Test, Crate) == FTransform());
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("Dragging a box changes the size of what is selected along the box's axis, or along all three")
	{
		FEditorUiTest Test;
		LookFromFront(Test);
		const int64 Crate = AddEntity(Test, "Crate", FVector(0.0));
		Test.Editor.SetSelection({Crate});
		SetGizmo(Test, EGizmoMode::Scale);
		const double AxisLength = GetAxisLength(Test);

		// Half the length of the axis is half the size again.
		Test.DragBy(Handle("X"), FVector2D(AxisLength * 0.5, 0.0));

		const FTransform Wider = GetTransform(Test, Crate);
		CHECK(Wider.Scale.x == doctest::Approx(1.5f).epsilon(DragTolerance));
		CHECK(Wider.Scale.y == 1.0f);
		CHECK(Wider.Scale.z == 1.0f);
		CHECK(Wider.Location == FVector(0.0));
		CHECK(Wider.Rotation == NoRotation);
		CHECK(GetLastStepName(Test) == "Scale");

		// The box in the middle: to the right and upwards is larger, by what the two come to together.
		Test.DragBy(Handle("All"), FVector2D(AxisLength * 0.25, -AxisLength * 0.25));

		const FTransform Larger = GetTransform(Test, Crate);
		CHECK(Larger.Scale.x == doctest::Approx(2.25f).epsilon(DragTolerance));
		CHECK(Larger.Scale.y == doctest::Approx(1.5f).epsilon(DragTolerance));
		CHECK(Larger.Scale.z == doctest::Approx(1.5f).epsilon(DragTolerance));
		CHECK(CountSteps(Test, "Scale") == 2);

		Test.PressKey(EKey::Z, {EKey::LeftControl});
		CHECK(GetTransform(Test, Crate) == Wider);
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("The gizmo moves and turns things along the axes of the world, or along their own")
	{
		FEditorUiTest Test;
		LookFromFront(Test);
		const int64 Crate = AddEntity(Test, "Crate", FVector(0.0));
		// A quarter turn around Z: the crate's own X points up.
		FTransform Upright;
		Upright.Rotation = glm::angleAxis(glm::radians(90.0f), AxisZ);
		Test.Editor.GetScene().FindEntityById(Crate)->SetRelativeTransform(Upright);
		Test.Editor.SetSelection({Crate});
		Test.DrawFrame();
		const double PixelSize = GetPixelSize(Test);
		const FVector2D Middle = Test.RequireItem("Viewport/Gizmo").GetCenter();

		// Along the axes of the world, the arrow of X points to the right whatever way the crate is turned.
		const FVector2D WorldArrow = Test.RequireItem(Handle("X")).GetCenter();
		CHECK(WorldArrow.x > Middle.x + 50.0);
		CHECK(WorldArrow.y == doctest::Approx(Middle.y).epsilon(0.01));

		SetGizmo(Test, EGizmoMode::Translate, EGizmoSpace::Local);
		const FVector2D OwnArrow = Test.RequireItem(Handle("X")).GetCenter();
		CHECK(OwnArrow.x == doctest::Approx(Middle.x).epsilon(0.01));
		CHECK(OwnArrow.y < Middle.y - 50.0);

		// Dragged upwards, it moves the crate along the crate's own X, which is up.
		Test.DragBy(Handle("X"), FVector2D(0.0, -50.0));

		const FTransform Moved = GetTransform(Test, Crate);
		CHECK(std::abs(Moved.Location.x) < 1.0e-5);
		CHECK(Moved.Location.y == doctest::Approx(50.0 * PixelSize).epsilon(DragTolerance));
		CHECK(Moved.Location.z == 0.0);
		CHECK(Moved.Rotation == Upright.Rotation);
	}

	TEST_CASE("Sizes are changed along a thing's own axes, whichever axes the gizmo is set to")
	{
		FEditorUiTest Test;
		LookFromFront(Test);
		const int64 Crate = AddEntity(Test, "Crate", FVector(0.0));
		FTransform Upright;
		Upright.Rotation = glm::angleAxis(glm::radians(90.0f), AxisZ);
		Test.Editor.GetScene().FindEntityById(Crate)->SetRelativeTransform(Upright);
		Test.Editor.SetSelection({Crate});
		SetGizmo(Test, EGizmoMode::Scale, EGizmoSpace::World);
		const double AxisLength = GetAxisLength(Test);
		const FVector2D Middle = Test.RequireItem("Viewport/Gizmo").GetCenter();

		// The box of the crate's X is above the middle, where the crate's X points.
		const FVector2D BoxX = Test.RequireItem(Handle("X")).GetCenter();
		CHECK(BoxX.x == doctest::Approx(Middle.x).epsilon(0.01));
		CHECK(BoxX.y < Middle.y - 50.0);

		Test.DragBy(Handle("X"), FVector2D(0.0, -AxisLength));

		const FTransform Sized = GetTransform(Test, Crate);
		CHECK(Sized.Scale.x == doctest::Approx(2.0f).epsilon(DragTolerance));
		CHECK(Sized.Scale.y == 1.0f);
		CHECK(Sized.Scale.z == 1.0f);
		CHECK(Sized.Rotation == Upright.Rotation);
	}

	TEST_CASE("What is below a turned and sized entity is moved in the world as the mouse says")
	{
		FEditorUiTest Test;
		LookFromFront(Test);
		WEntity* const Cart = Test.Editor.GetScene().CreateEntity("Cart");
		REQUIRE(Cart != nullptr);
		FTransform CartTransform;
		CartTransform.Location = FVector(-1.0, -1.0, 0.0);
		CartTransform.Rotation = glm::angleAxis(glm::radians(90.0f), AxisZ);
		CartTransform.Scale = FVector3f(2.0f);
		Cart->SetRelativeTransform(CartTransform);
		const int64 CartId = Cart->GetId();

		// One metre along the cart's X, which is two metres up from the cart in the world.
		WEntity* const WheelEntity = Test.Editor.GetScene().CreateEntity("Wheel", Cart);
		REQUIRE(WheelEntity != nullptr);
		FTransform WheelTransform;
		WheelTransform.Location = FVector(1.0, 0.0, 0.0);
		WheelTransform.Rotation = glm::angleAxis(glm::radians(30.0f), AxisZ);
		WheelTransform.Scale = FVector3f(0.5f, 1.0f, 1.5f);
		WheelEntity->SetRelativeTransform(WheelTransform);
		const int64 Wheel = WheelEntity->GetId();
		Test.Editor.SetSelection({Wheel});
		Test.DrawFrame();
		const double PixelSize = GetPixelSize(Test);
		const FVector WorldBefore = GetEntity(Test, Wheel).GetWorldLocation();
		REQUIRE(glm::length(WorldBefore - FVector(-1.0, 1.0, 0.0)) < 1.0e-5);

		Test.DragBy(Handle("X"), FVector2D(60.0, 0.0));

		// To the right in the world, which is against the cart's Y, at half the distance for a cart of twice the
		// size.
		const FVector WorldAfter = GetEntity(Test, Wheel).GetWorldLocation();
		CHECK(WorldAfter.x == doctest::Approx(WorldBefore.x + 60.0 * PixelSize).epsilon(DragTolerance));
		CHECK(WorldAfter.y == doctest::Approx(WorldBefore.y));
		CHECK(std::abs(WorldAfter.z) < 1.0e-9);
		const FTransform Relative = GetTransform(Test, Wheel);
		CHECK(Relative.Location.x == doctest::Approx(1.0));
		CHECK(Relative.Location.y == doctest::Approx(-30.0 * PixelSize).epsilon(DragTolerance));
		// What a move does not change is exactly what it was.
		CHECK(Relative.Rotation == WheelTransform.Rotation);
		CHECK(Relative.Scale == WheelTransform.Scale);
		CHECK(GetTransform(Test, CartId) == CartTransform);
	}

	TEST_CASE("Everything that is selected is dragged, and what is below something selected only with it")
	{
		FEditorUiTest Test;
		LookFromFront(Test);
		const int64 Cart = AddEntity(Test, "Cart", FVector(-2.0, 1.0, 0.0));
		const int64 Wheel =
			AddEntity(Test, "Wheel", FVector(-2.0, 0.0, 0.0), Test.Editor.GetScene().FindEntityById(Cart));
		const int64 Crate = AddEntity(Test, "Crate", FVector(0.0));
		const FTransform WheelBefore = GetTransform(Test, Wheel);
		// The crate is selected last, and has the gizmo.
		Test.Editor.SetSelection({Cart, Wheel, Crate});
		Test.DrawFrame();
		const double PixelSize = GetPixelSize(Test);

		Test.DragBy(Handle("X"), FVector2D(60.0, 0.0));

		const double Distance = GetTransform(Test, Crate).Location.x;
		CHECK(Distance == doctest::Approx(60.0 * PixelSize).epsilon(DragTolerance));
		CHECK(GetTransform(Test, Cart).Location == FVector(-2.0 + Distance, 1.0, 0.0));
		// The wheel has gone with the cart, once: where it is on the cart is as it was.
		CHECK(GetTransform(Test, Wheel) == WheelBefore);
		CHECK(GetEntity(Test, Wheel).GetWorldLocation().x == doctest::Approx(-2.0 + Distance));
		CHECK(CountSteps(Test, "Move") == 1);

		// One undo puts all of them back.
		Test.PressKey(EKey::Z, {EKey::LeftControl});
		CHECK(GetTransform(Test, Crate).Location == FVector(0.0));
		CHECK(GetTransform(Test, Cart).Location == FVector(-2.0, 1.0, 0.0));
	}

	TEST_CASE("Things are turned around the place of the gizmo, in steps if the gizmo is set to steps")
	{
		FEditorUiTest Test;
		LookFromFront(Test);
		const int64 Moon = AddEntity(Test, "Moon", FVector(-2.0, 0.0, 0.0));
		const int64 Planet = AddEntity(Test, "Planet", FVector(0.0));
		Test.Editor.SetSelection({Moon, Planet});
		FGizmoSettings Settings;
		Settings.Mode = EGizmoMode::Rotate;
		Settings.bSnapRotation = true;
		Settings.RotationSnap = 90.0f;
		Test.Editor.SetGizmoSettings(Settings);
		Test.DrawFrame();
		const double AxisLength = GetAxisLength(Test);

		// One and a half radians along the ring is some 86 degrees, and in steps of 90 a quarter turn.
		const double Along = AxisLength * 1.5 / std::sqrt(2.0);
		Test.DragBy(Handle("Z"), FVector2D(-Along, -Along));

		CHECK(GetTurnAroundZ(GetTransform(Test, Planet).Rotation) == doctest::Approx(90.0));
		CHECK(GetTransform(Test, Planet).Location == FVector(0.0));
		// The moon has gone a quarter of the way around the planet, and has turned as far itself.
		CHECK(GetTurnAroundZ(GetTransform(Test, Moon).Rotation) == doctest::Approx(90.0));
		CHECK(glm::length(GetTransform(Test, Moon).Location - FVector(0.0, -2.0, 0.0)) < 1.0e-5);
		CHECK(CountSteps(Test, "Rotate") == 1);
	}

	TEST_CASE("Things are moved and sized in steps if the gizmo is set to steps")
	{
		FEditorUiTest Test;
		LookFromFront(Test);
		const int64 Crate = AddEntity(Test, "Crate", FVector(0.0));
		Test.Editor.SetSelection({Crate});
		FGizmoSettings Settings;
		Settings.bSnapLocation = true;
		Settings.LocationSnap = 0.5;
		Settings.bSnapScale = true;
		Settings.ScaleSnap = 0.25f;
		Test.Editor.SetGizmoSettings(Settings);
		Test.DrawFrame();
		const double PixelSize = GetPixelSize(Test);

		// Some 1.2 metres is two steps of half a metre, to the last digit.
		Test.DragBy(Handle("X"), FVector2D(std::round(1.2 / PixelSize), 0.0));
		CHECK(GetTransform(Test, Crate).Location == FVector(1.0, 0.0, 0.0));
		CHECK(CountSteps(Test, "Move") == 1);

		// A drag that does not come to a step moves nothing, and is no step of the history either.
		Test.DragBy(Handle("X"), FVector2D(std::round(0.2 / PixelSize), 0.0));
		CHECK(GetTransform(Test, Crate).Location == FVector(1.0, 0.0, 0.0));
		CHECK(CountSteps(Test, "Move") == 1);

		// A third of the axis' length is a third more, and in steps of a quarter, a quarter more.
		Settings.Mode = EGizmoMode::Scale;
		Test.Editor.SetGizmoSettings(Settings);
		Test.DrawFrame();
		Test.DragBy(Handle("X"), FVector2D(GetAxisLength(Test) / 3.0, 0.0));
		CHECK(GetTransform(Test, Crate).Scale == FVector3f(1.25f, 1.0f, 1.0f));
	}

	TEST_CASE("A press on a handle without a drag changes nothing, and is no click on what is behind the handle")
	{
		FEditorUiTest Test;
		LookFromFront(Test);
		const int64 Crate = AddEntity(Test, "Crate", FVector(0.0));
		Test.Editor.SetSelection({Crate});
		Test.DrawFrame();

		// A box two metres behind the head of the arrow of X. With no gizmo there, a click on that place selects
		// the box.
		const FVector2D OnHandle = Test.RequireItem(Handle("X")).GetCenter();
		const double HandleDistance =
			(OnHandle.x - Test.RequireItem("Viewport/Gizmo").GetCenter().x) * GetPixelSize(Test);
		const int64 Behind = AddBox(Test, "Behind", FVector(HandleDistance, 0.0, -2.0));
		Test.Editor.SetSelection({});
		Test.DrawFrame();
		Test.ClickAt(OnHandle);
		REQUIRE(Test.Editor.GetSelection() == std::vector<int64>{Behind});

		Test.Editor.SetSelection({Crate});
		Test.DrawFrame();
		const FTransform Before = GetTransform(Test, Crate);

		Test.Click(Handle("X"));

		CHECK(Test.Editor.GetSelection() == std::vector<int64>{Crate});
		CHECK(GetTransform(Test, Crate) == Before);
		CHECK(CountSteps(Test, "Move") == 0);
		CHECK_FALSE(Test.Editor.IsStepOpen());

		// Nor does a drag of the handle, which ends over the box, select the box.
		Test.DragBy(Handle("X"), FVector2D(10.0, 0.0));
		CHECK(Test.Editor.GetSelection() == std::vector<int64>{Crate});
		CHECK(CountSteps(Test, "Move") == 1);
	}

	TEST_CASE("While a handle is dragged, the mouse steers no camera and stays a mouse")
	{
		FEditorUiTest Test;
		LookFromFront(Test);
		const int64 Crate = AddEntity(Test, "Crate", FVector(0.0));
		Test.Editor.SetSelection({Crate});
		Test.DrawFrame();
		const FViewportCamera Camera = Test.Editor.GetViewportCamera();

		PressAt(Test, Test.RequireItem(Handle("X")).GetCenter(), EMouseButton::Left);
		CHECK(Test.Editor.IsStepOpen());
		CHECK_FALSE(Test.Ui->WantsMouseCaptured());
		MoveBy(Test, FVector2D(40.0, 0.0));
		CHECK(GetTransform(Test, Crate).Location.x > 0.1);

		// The wheel, which moves the camera closer when nothing is held, does not while the gizmo is.
		Test.Input.HandleScroll(FVector2D(0.0, 2.0));
		Test.DrawFrame();
		CHECK(Test.Editor.GetViewportCamera() == Camera);
		CHECK_FALSE(Test.Ui->WantsMouseCaptured());

		Release(Test, EMouseButton::Left);
		CHECK_FALSE(Test.Editor.IsStepOpen());
		CHECK(Test.Editor.GetViewportCamera() == Camera);
	}

	TEST_CASE("The buttons that steer the camera steer it on a handle too")
	{
		FEditorUiTest Test;
		LookFromFront(Test);
		const int64 Crate = AddEntity(Test, "Crate", FVector(0.0));
		Test.Editor.SetSelection({Crate});
		Test.DrawFrame();
		const FViewportCamera Camera = Test.Editor.GetViewportCamera();
		const FVector2D OnHandle = Test.RequireItem(Handle("X")).GetCenter();

		// The right button looks around.
		PressAt(Test, OnHandle, EMouseButton::Right);
		CHECK(Test.Ui->WantsMouseCaptured());
		MoveBy(Test, FVector2D(50.0, 0.0));
		Release(Test, EMouseButton::Right);
		CHECK(Test.Editor.GetViewportCamera().Yaw == doctest::Approx(Camera.Yaw - 10.0f));
		CHECK(GetTransform(Test, Crate) == FTransform());

		// The middle button moves it sideways.
		Test.Editor.SetViewportCamera(Camera);
		Test.DrawFrame();
		PressAt(Test, OnHandle, EMouseButton::Middle);
		MoveBy(Test, FVector2D(50.0, 0.0));
		Release(Test, EMouseButton::Middle);
		CHECK(Test.Editor.GetViewportCamera().Location.x < Camera.Location.x);
		CHECK(Test.Editor.GetViewportCamera().Yaw == Camera.Yaw);

		// The left button with Alt goes around what the camera looks at, beside the gizmo. On a handle, it
		// drags a copy.
		Test.Editor.SetViewportCamera(Camera);
		Test.DrawFrame();
		Test.SetKeyDown(EKey::LeftAlt, true);
		PressAt(Test, FindPlaceBesideGizmo(Test), EMouseButton::Left);
		MoveBy(Test, FVector2D(50.0, 0.0));
		Release(Test, EMouseButton::Left);
		Test.SetKeyDown(EKey::LeftAlt, false);
		CHECK(Test.Editor.GetViewportCamera().Yaw == doctest::Approx(Camera.Yaw - 15.0f));

		CHECK(GetTransform(Test, Crate) == FTransform());
		CHECK(Test.Editor.GetScene().GetAllEntities().size() == 1);
		CHECK(CountSteps(Test, "Move") == 0);
	}

	TEST_CASE("With Alt held, a drag makes a copy of what is selected and moves the copy")
	{
		FEditorUiTest Test;
		LookFromFront(Test);
		const int64 Crate = AddEntity(Test, "Crate", FVector(0.0));
		Test.Editor.SetSelection({Crate});
		Test.DrawFrame();
		const double PixelSize = GetPixelSize(Test);
		const FViewportCamera Camera = Test.Editor.GetViewportCamera();

		Test.SetKeyDown(EKey::LeftAlt, true);
		Test.DragBy(Handle("X"), FVector2D(60.0, 0.0));
		Test.SetKeyDown(EKey::LeftAlt, false);

		// The crate is where it was. Beside it is a copy, which is selected in its place and has the gizmo.
		REQUIRE(Test.Editor.GetScene().GetAllEntities().size() == 2);
		CHECK(GetTransform(Test, Crate) == FTransform());
		const std::vector<int64> Selection = Test.Editor.GetSelection();
		REQUIRE(Selection.size() == 1);
		const int64 Copy = Selection[0];
		CHECK(Copy != Crate);
		const FTransform CopyTransform = GetTransform(Test, Copy);
		CHECK(CopyTransform.Location.x == doctest::Approx(60.0 * PixelSize).epsilon(DragTolerance));
		CHECK(CopyTransform.Location.y == 0.0);
		CHECK(CopyTransform.Location.z == 0.0);
		CHECK(
			IsNear(Test.RequireItem("Viewport/Gizmo").GetCenter(), FindInViewport(Test, CopyTransform.Location), 1.5));

		// All of it is one step, called by what it made. The camera has not gone around anything, which is
		// what Alt and the left button do beside the gizmo.
		CHECK(GetLastStepName(Test) == "Duplicate");
		CHECK(CountSteps(Test, "Duplicate") == 1);
		CHECK(CountSteps(Test, "Move") == 0);
		CHECK_FALSE(Test.Editor.IsStepOpen());
		CHECK(Test.Editor.GetViewportCamera() == Camera);

		// One undo takes the copy away again, and one redo brings it back to where it was dragged.
		Test.PressKey(EKey::Z, {EKey::LeftControl});
		CHECK(Test.Editor.GetScene().GetAllEntities().size() == 1);
		CHECK(GetTransform(Test, Crate) == FTransform());
		Test.PressKey(EKey::Y, {EKey::LeftControl});
		REQUIRE(Test.Editor.GetScene().GetAllEntities().size() == 2);
		CHECK(GetTransform(Test, Copy) == CopyTransform);
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("A press with Alt that drags nowhere makes no copy")
	{
		FEditorUiTest Test;
		LookFromFront(Test);
		const int64 Crate = AddEntity(Test, "Crate", FVector(0.0));
		Test.Editor.SetSelection({Crate});
		FGizmoSettings Settings;
		Settings.bSnapLocation = true;
		Settings.LocationSnap = 10.0;
		Test.Editor.SetGizmoSettings(Settings);
		Test.DrawFrame();
		const FViewportCamera Camera = Test.Editor.GetViewportCamera();

		Test.SetKeyDown(EKey::LeftAlt, true);
		Test.Click(Handle("X"));
		CHECK(Test.Editor.GetScene().GetAllEntities().size() == 1);

		// Nor does a drag that does not come to one of the steps in which things are moved.
		Test.DragBy(Handle("X"), FVector2D(30.0, 0.0));
		Test.SetKeyDown(EKey::LeftAlt, false);

		CHECK(Test.Editor.GetScene().GetAllEntities().size() == 1);
		CHECK(GetTransform(Test, Crate) == FTransform());
		CHECK(Test.Editor.GetSelection() == std::vector<int64>{Crate});
		CHECK(CountSteps(Test, "Duplicate") == 0);
		CHECK_FALSE(Test.Editor.IsStepOpen());
		CHECK(Test.Editor.GetViewportCamera() == Camera);
	}

	TEST_CASE("With Alt held, everything that is selected is copied, with what is below it")
	{
		FEditorUiTest Test;
		LookFromFront(Test);
		const int64 Cart = AddEntity(Test, "Cart", FVector(-2.0, 1.0, 0.0));
		const int64 Wheel =
			AddEntity(Test, "Wheel", FVector(-2.0, 0.0, 0.0), Test.Editor.GetScene().FindEntityById(Cart));
		const int64 Crate = AddEntity(Test, "Crate", FVector(0.0));
		// The crate is selected last, and has the gizmo.
		Test.Editor.SetSelection({Cart, Wheel, Crate});
		Test.DrawFrame();

		Test.SetKeyDown(EKey::LeftAlt, true);
		Test.DragBy(Handle("X"), FVector2D(60.0, 0.0));
		Test.SetKeyDown(EKey::LeftAlt, false);

		// A copy of the cart, with a wheel of its own, and one of the crate: three entities more.
		const WScene& Scene = Test.Editor.GetScene();
		REQUIRE(Scene.GetAllEntities().size() == 6);
		CHECK(GetTransform(Test, Cart).Location == FVector(-2.0, 1.0, 0.0));
		CHECK(GetEntity(Test, Wheel).GetWorldLocation() == FVector(-2.0, 0.0, 0.0));
		CHECK(GetTransform(Test, Crate) == FTransform());

		// The copies are selected, in the order of what they are copies of, and have moved as one.
		const std::vector<int64> Selection = Test.Editor.GetSelection();
		REQUIRE(Selection.size() == 2);
		const WEntity& CartCopy = GetEntity(Test, Selection[0]);
		const WEntity& CrateCopy = GetEntity(Test, Selection[1]);
		const double Distance = CrateCopy.GetRelativeTransform().Location.x;
		CHECK(Distance > 0.5);
		CHECK(CartCopy.GetRelativeTransform().Location == FVector(-2.0 + Distance, 1.0, 0.0));
		REQUIRE(CartCopy.GetChildren().size() == 1);
		CHECK(CartCopy.GetChildren()[0]->GetWorldLocation().x == doctest::Approx(-2.0 + Distance));
		CHECK(CountSteps(Test, "Duplicate") == 1);

		Test.PressKey(EKey::Z, {EKey::LeftControl});
		CHECK(Test.Editor.GetScene().GetAllEntities().size() == 3);
	}

	TEST_CASE("With Alt held, a ring turns a copy and a box sizes one")
	{
		FEditorUiTest Test;
		LookFromFront(Test);
		const int64 Crate = AddEntity(Test, "Crate", FVector(0.0));
		Test.Editor.SetSelection({Crate});
		SetGizmo(Test, EGizmoMode::Rotate);

		Test.SetKeyDown(EKey::LeftAlt, true);
		Test.DragBy(Handle("Z"), FVector2D(-30.0, -30.0));

		REQUIRE(Test.Editor.GetScene().GetAllEntities().size() == 2);
		REQUIRE(Test.Editor.GetSelection().size() == 1);
		const int64 Turned = Test.Editor.GetSelection()[0];
		CHECK(GetTransform(Test, Crate) == FTransform());
		CHECK(GetTurnAroundZ(GetTransform(Test, Turned).Rotation) > 10.0);
		CHECK(GetTransform(Test, Turned).Location == FVector(0.0));

		// The copy that is selected now is copied in its turn.
		SetGizmo(Test, EGizmoMode::Scale);
		Test.DragBy(Handle("All"), FVector2D(GetAxisLength(Test) * 0.5, 0.0));
		Test.SetKeyDown(EKey::LeftAlt, false);

		REQUIRE(Test.Editor.GetScene().GetAllEntities().size() == 3);
		REQUIRE(Test.Editor.GetSelection().size() == 1);
		const int64 Sized = Test.Editor.GetSelection()[0];
		CHECK(GetTransform(Test, Turned).Scale == FVector3f(1.0f));
		CHECK(GetTransform(Test, Sized).Scale.x == doctest::Approx(1.5f).epsilon(DragTolerance));
		CHECK(GetTransform(Test, Sized).Rotation == GetTransform(Test, Turned).Rotation);
		CHECK(CountSteps(Test, "Duplicate") == 2);
	}

	TEST_CASE("Undoing in the middle of a drag of copies takes the copies away and ends the drag")
	{
		FEditorUiTest Test;
		LookFromFront(Test);
		const int64 Crate = AddEntity(Test, "Crate", FVector(0.0));
		Test.Editor.SetSelection({Crate});
		Test.DrawFrame();

		Test.SetKeyDown(EKey::LeftAlt, true);
		PressAt(Test, Test.RequireItem(Handle("X")).GetCenter(), EMouseButton::Left);
		MoveBy(Test, FVector2D(40.0, 0.0));
		Test.SetKeyDown(EKey::LeftAlt, false);
		REQUIRE(Test.Editor.GetScene().GetAllEntities().size() == 2);

		Test.PressKey(EKey::Z, {EKey::LeftControl});
		CHECK(Test.Editor.GetScene().GetAllEntities().size() == 1);

		// The button is still down, and the mouse moves on: nothing is copied a second time.
		MoveBy(Test, FVector2D(40.0, 0.0));
		Release(Test, EMouseButton::Left);
		Test.DrawFrames(3);

		CHECK(Test.Editor.GetScene().GetAllEntities().size() == 1);
		CHECK(GetTransform(Test, Crate) == FTransform());
		CHECK(CountSteps(Test, "Duplicate") == 0);
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("Undoing in the middle of a drag ends the drag")
	{
		FEditorUiTest Test;
		LookFromFront(Test);
		const int64 Crate = AddEntity(Test, "Crate", FVector(0.0));
		Test.Editor.SetSelection({Crate});
		Test.DrawFrame();

		PressAt(Test, Test.RequireItem(Handle("X")).GetCenter(), EMouseButton::Left);
		MoveBy(Test, FVector2D(40.0, 0.0));
		REQUIRE(GetTransform(Test, Crate).Location.x > 0.1);

		// What the drag has done so far is the step that is undone.
		Test.PressKey(EKey::Z, {EKey::LeftControl});
		CHECK(GetTransform(Test, Crate) == FTransform());
		CHECK_FALSE(Test.Editor.IsStepOpen());

		// The button is still down, and the mouse moves on: nothing follows it any more.
		MoveBy(Test, FVector2D(40.0, 0.0));
		CHECK(GetTransform(Test, Crate) == FTransform());
		Release(Test, EMouseButton::Left);
		Test.DrawFrames(3);

		CHECK(GetTransform(Test, Crate) == FTransform());
		CHECK(Test.Editor.GetSelection() == std::vector<int64>{Crate});
		CHECK(CountSteps(Test, "Move") == 0);
		const std::vector<std::string> Undone = {"Move"};
		CHECK(Test.Editor.GetHistory().GetRedoStepNames() == Undone);
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("A drag ends when what is selected is no longer what it was")
	{
		FEditorUiTest Test;
		LookFromFront(Test);
		const int64 Barrel = AddEntity(Test, "Barrel", FVector(-2.0, 1.0, 0.0));
		const int64 Crate = AddEntity(Test, "Crate", FVector(0.0));
		const FTransform BarrelBefore = GetTransform(Test, Barrel);
		Test.Editor.SetSelection({Crate});
		Test.DrawFrame();

		PressAt(Test, Test.RequireItem(Handle("X")).GetCenter(), EMouseButton::Left);
		MoveBy(Test, FVector2D(40.0, 0.0));
		const FTransform Dragged = GetTransform(Test, Crate);
		REQUIRE(Dragged.Location.x > 0.1);
		std::vector<int64> NewSelection;

		// As a command of an agent may do while the user drags.
		SUBCASE("Nothing is selected")
		{
		}
		SUBCASE("Something else is selected")
		{
			NewSelection = {Barrel};
		}
		SUBCASE("Something else is selected as well")
		{
			NewSelection = {Crate, Barrel};
		}
		Test.Editor.SetSelection(NewSelection);
		Test.DrawFrame();
		CHECK(Test.HasItem("Viewport/Gizmo") == !NewSelection.empty());
		CHECK_FALSE(Test.Editor.IsStepOpen());
		CHECK(CountSteps(Test, "Move") == 1);

		// The button is still down, and the mouse moves on: nothing follows it, whatever has the gizmo now.
		MoveBy(Test, FVector2D(40.0, 0.0));
		Release(Test, EMouseButton::Left);
		Test.DrawFrames(3);

		CHECK(GetTransform(Test, Crate) == Dragged);
		CHECK(GetTransform(Test, Barrel) == BarrelBefore);
		CHECK(Test.Editor.GetSelection() == NewSelection);
		CHECK(CountSteps(Test, "Move") == 1);
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("A drag ends when one of the things that it moves is gone")
	{
		FEditorUiTest Test;
		LookFromFront(Test);
		const int64 Barrel = AddEntity(Test, "Barrel", FVector(-2.0, 1.0, 0.0));
		const int64 Crate = AddEntity(Test, "Crate", FVector(0.0));
		Test.Editor.SetSelection({Barrel, Crate});
		Test.DrawFrame();

		PressAt(Test, Test.RequireItem(Handle("X")).GetCenter(), EMouseButton::Left);
		MoveBy(Test, FVector2D(40.0, 0.0));
		const FTransform Dragged = GetTransform(Test, Crate);
		REQUIRE(Dragged.Location.x > 0.1);

		// The barrel is deleted by a command, which is part of the step that the drag has open.
		FJsonValue Arguments = FJsonValue::MakeObject();
		Arguments.SetMember("Entity", Barrel);
		REQUIRE(Test.Editor.ExecuteCommand("Entity.Delete", Arguments).HasSucceeded());
		Test.DrawFrame();
		CHECK_FALSE(Test.Editor.IsStepOpen());

		MoveBy(Test, FVector2D(40.0, 0.0));
		Release(Test, EMouseButton::Left);
		Test.DrawFrames(3);

		CHECK(GetTransform(Test, Crate) == Dragged);
		CHECK(Test.Editor.GetScene().FindEntityById(Barrel) == nullptr);
		CHECK(Test.Editor.GetSelection() == std::vector<int64>{Crate});
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("What is typed in the Details panel is a step of its own before a drag of the gizmo begins")
	{
		FEditorUiTest Test;
		LookFromFront(Test);
		const int64 Crate = AddEntity(Test, "Crate", FVector(0.0));
		Test.Editor.SetSelection({Crate});
		Test.DrawFrame();

		// The size is being typed, and nothing has ended the typing, when the mouse takes hold of the arrow.
		Test.SetKeyDown(EKey::LeftControl, true);
		Test.Click(std::format("Details/{}/RelativeTransform/Scale/X", Crate));
		Test.SetKeyDown(EKey::LeftControl, false);
		Test.Type("2");
		REQUIRE(GetTransform(Test, Crate).Scale == FVector3f(1.0f));

		Test.DragBy(Handle("X"), FVector2D(60.0, 0.0));

		const FTransform After = GetTransform(Test, Crate);
		CHECK(After.Scale == FVector3f(2.0f, 1.0f, 1.0f));
		CHECK(After.Location.x > 0.1);
		const std::vector<std::string> Names = Test.Editor.GetHistory().GetUndoStepNames();
		REQUIRE(Names.size() >= 2);
		CHECK(Names[0] == "Move");
		CHECK(Names[1] == "Set Transform");

		// The drag is undone without the size.
		Test.PressKey(EKey::Z, {EKey::LeftControl});
		CHECK(GetTransform(Test, Crate).Location == FVector(0.0));
		CHECK(GetTransform(Test, Crate).Scale == FVector3f(2.0f, 1.0f, 1.0f));
	}

	TEST_CASE("With the viewport closed there is no gizmo, and it is back when the viewport is")
	{
		FEditorUiTest Test;
		const int64 Crate = AddEntity(Test, "Crate", Test.Editor.GetViewportCamera().GetFocusPoint());
		Test.Editor.SetSelection({Crate});
		Test.DrawFrame();
		REQUIRE(Test.HasItem("Viewport/Gizmo"));

		Test.Click("Menu/Window");
		Test.Click("Menu/Window/Viewport");
		CHECK_FALSE(Test.HasItem("Viewport/Gizmo"));
		CHECK(FindHandles(Test).empty());

		Test.Click("Menu/Window");
		Test.Click("Menu/Window/Viewport");
		CHECK(Test.HasItem("Viewport/Gizmo"));
		CHECK(Test.HasItem(Handle("X")));
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("The gizmo is larger where the user interface is larger")
	{
		FEditorUiTest Test;
		const int64 Crate = AddEntity(Test, "Crate", Test.Editor.GetViewportCamera().GetFocusPoint());
		Test.Editor.SetSelection({Crate});
		Test.DrawFrame();
		const double AtScaleOne = GetAxisLength(Test);

		Test.Ui->SetScale(2.0f);
		Test.DrawFrames(3);

		CHECK(GetAxisLength(Test) == doctest::Approx(AtScaleOne * 2.0));
	}

	TEST_CASE("A gizmo that is behind the camera is not there")
	{
		FEditorUiTest Test;
		const FViewportCamera Camera = Test.Editor.GetViewportCamera();
		const int64 Crate = AddEntity(Test, "Crate", Camera.Location - FVector(Camera.GetForward()) * 5.0);
		Test.Editor.SetSelection({Crate});
		Test.DrawFrame();

		CHECK_FALSE(Test.HasItem("Viewport/Gizmo"));
		CHECK(FindHandles(Test).empty());

		// A click on the picture is then a click on the scene, as with no gizmo.
		Test.Click("Viewport");
		CHECK(Test.Editor.GetSelection().empty());
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}
}
