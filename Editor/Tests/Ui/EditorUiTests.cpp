#include "Ui/EditorUi.h"

#include "Assets/TestAssets.h"
#include "Core/FileHelper.h"
#include "Math/Color.h"
#include "Math/LinearColor.h"
#include "Math/Transform.h"
#include "Object/ObjectGlobals.h"
#include "Rendering/DirectionalLightComponent.h"
#include "Rendering/Image.h"
#include "Rendering/Material.h"
#include "Rendering/MeshShapes.h"
#include "Rendering/SceneViewMode.h"
#include "Rendering/StaticMesh.h"
#include "Rendering/StaticMeshComponent.h"
#include "Rendering/StaticMeshVertex.h"
#include "Rendering/TestReferenceImages.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Ui/TestEditorUi.h"

#include <doctest/doctest.h>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>

#include <format>
#include <limits>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

namespace
{
	constexpr FColor Red(255, 0, 0);

	/** Adds an entity that shows a mesh in one colour. */
	WEntity* AddMesh(
		WScene& Scene, const char* Name, WStaticMesh* Mesh, const FLinearColor& Color, const FVector& Location)
	{
		WEntity* const Entity = Scene.CreateEntity(Name);
		REQUIRE(Entity != nullptr);
		Entity->SetWorldLocation(Location);

		WStaticMeshComponent* const Component = Entity->AddComponent<WStaticMeshComponent>();
		REQUIRE(Component != nullptr);
		Component->Mesh = Mesh;
		Component->Material = NewObject<WMaterial>();
		Component->Material->BaseColor = Color;
		return Entity;
	}

	/** A square of one metre in the XY plane, centred on its origin, whose front faces +Z. */
	WStaticMesh* MakeSquareMesh()
	{
		const FVector3f Front(0.0f, 0.0f, 1.0f);
		const std::vector<FStaticMeshVertex> Vertices{
			{FVector3f(-0.5f, -0.5f, 0.0f), Front, FVector2f(0.0f, 1.0f)},
			{FVector3f(0.5f, -0.5f, 0.0f), Front, FVector2f(1.0f, 1.0f)},
			{FVector3f(0.5f, 0.5f, 0.0f), Front, FVector2f(1.0f, 0.0f)},
			{FVector3f(-0.5f, 0.5f, 0.0f), Front, FVector2f(0.0f, 0.0f)},
		};

		WStaticMesh* const Mesh = NewObject<WStaticMesh>();
		std::string Error;
		REQUIRE(Mesh->SetGeometry(Vertices, {0, 1, 2, 0, 2, 3}, Error));
		return Mesh;
	}

	/**
	 * Fills the editor's scene with a little of everything the default layout shows: a floor with a crate on it in
	 * the light of a sun, and entities below one another. The crate is selected.
	 */
	void BuildSampleScene(FEditor& Editor)
	{
		WScene& Scene = Editor.GetScene();
		Scene.SetName("Sample");

		WEntity* const Sun = Scene.CreateEntity("Sun");
		REQUIRE(Sun != nullptr);
		// From behind the left shoulder of the viewport's camera, so that the crate's shadow falls where it is seen.
		FTransform SunTransform;
		SunTransform.Rotation = glm::angleAxis(glm::radians(-55.0f), FVector3f(0.0f, 1.0f, 0.0f)) *
			glm::angleAxis(glm::radians(-50.0f), FVector3f(1.0f, 0.0f, 0.0f));
		Sun->SetRelativeTransform(SunTransform);
		REQUIRE(Sun->AddComponent<WDirectionalLightComponent>() != nullptr);

		AddMesh(
			Scene, "Floor", FMeshShapes::CreatePlane(12.0f, 12.0f), FLinearColor(0.35f, 0.35f, 0.35f), FVector(0.0));
		const WEntity* const Crate = AddMesh(Scene,
			"Crate",
			FMeshShapes::CreateBox(FVector3f(1.0f)),
			FLinearColor(0.8f, 0.35f, 0.1f),
			FVector(0.0, 0.5, 0.0));

		WEntity* const Player = Scene.CreateEntity("Player");
		WEntity* const CameraArm = Scene.CreateEntity("Camera Arm", Player);
		REQUIRE(Scene.CreateEntity("Camera", CameraArm) != nullptr);
		REQUIRE(Scene.CreateEntity("Sword", Player) != nullptr);

		Editor.SetSelection({Crate->GetId()});
	}

	/** Returns the pixel of a picture at a place of the user interface. */
	FColor GetPixelAt(const FImage& Picture, const FVector2D& Position)
	{
		return Picture.GetPixel(static_cast<int32>(Position.x), static_cast<int32>(Position.y));
	}
}

TEST_SUITE("FEditorUi")
{
	TEST_CASE("The default layout looks like its reference image")
	{
		FEditorUiTest Test;
		BuildSampleScene(Test.Editor);
		Test.DrawFrames(2);

		const FImage Picture = Test.ReadPicture();

		CheckAgainstReferenceImage(Picture,
			FFileHelper::PathFromUtf8(WW_REFERENCE_IMAGE_DIRECTORY),
			FFileHelper::PathFromUtf8(WW_TEST_OUTPUT_DIRECTORY),
			"EditorDefaultLayout");
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("The user interface covers the whole surface, whatever its size")
	{
		int32 Width = 0;
		int32 Height = 0;
		SUBCASE("A small window")
		{
			Width = 1280;
			Height = 720;
		}
		SUBCASE("A tall, narrow one")
		{
			Width = 500;
			Height = 900;
		}
		SUBCASE("One too small for anything")
		{
			Width = 48;
			Height = 32;
		}
		FEditorUiTest Test(Width, Height);

		const FImage Picture = Test.ReadPicture();

		// Before the user interface is drawn the surface is cleared to black, which is none of its colours.
		REQUIRE(Picture.GetWidth() == Width);
		REQUIRE(Picture.GetHeight() == Height);
		int32 UncoveredPixelCount = 0;
		for (const FColor& Pixel : Picture.GetPixels())
		{
			if (Pixel == FColor(0, 0, 0))
			{
				++UncoveredPixelCount;
			}
		}
		CHECK(UncoveredPixelCount == 0);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("The user interface is laid out again for a surface of another size")
	{
		FEditorUiTest Test;
		const FUiRectangle Before = Test.RequireItem("Viewport");
		const std::unique_ptr<FRenderTarget> Larger = CreateTestTarget(*Test.Device, 1600, 1000);

		std::string Error;
		for (int32 Frame = 0; Frame < 3; ++Frame)
		{
			REQUIRE(Test.Ui->Draw(*Larger, Test.Input, FEditorUiTest::FrameSeconds, Error));
		}

		const FUiRectangle After = Test.RequireItem("Viewport");
		CHECK(After.GetWidth() > Before.GetWidth());
		CHECK(After.GetHeight() > Before.GetHeight());
		CHECK(After.Right <= 1600.0f);
		CHECK(After.Bottom <= 1000.0f);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("At twice the scale the items of the user interface are twice as large, and still share the surface")
	{
		FEditorUiTest Test;
		REQUIRE(Test.Ui->GetScale() == 1.0f);
		const FUiRectangle SearchBefore = Test.RequireItem("Outliner/Search");
		const FUiRectangle MenuBefore = Test.RequireItem("Menu/File");

		Test.Ui->SetScale(2.0f);
		Test.DrawFrames(3);

		CHECK(Test.Ui->GetScale() == 2.0f);
		const FUiRectangle SearchAfter = Test.RequireItem("Outliner/Search");
		const FUiRectangle MenuAfter = Test.RequireItem("Menu/File");
		// Letters are drawn anew for their size and not stretched, so twice is not exact to the pixel.
		CHECK(SearchAfter.GetHeight() == doctest::Approx(2.0f * SearchBefore.GetHeight()).epsilon(0.1));
		CHECK(MenuAfter.GetHeight() == doctest::Approx(2.0f * MenuBefore.GetHeight()).epsilon(0.1));
		CHECK(MenuAfter.GetWidth() == doctest::Approx(2.0f * MenuBefore.GetWidth()).epsilon(0.1));

		// The panels are where they were: the outliner to the right of the viewport, which reaches the bottom.
		const FUiRectangle Viewport = Test.RequireItem("Viewport");
		CHECK(Viewport.Right < SearchAfter.Left);
		CHECK(Viewport.Bottom >= static_cast<float>(FEditorUiTest::DefaultHeight) - 2.0f);
		CHECK(SearchAfter.Right <= static_cast<float>(FEditorUiTest::DefaultWidth));
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("Setting the scale back gives the user interface its first size again")
	{
		FEditorUiTest Test;
		const FUiRectangle Before = Test.RequireItem("Outliner/Search");
		const FImage PictureBefore = Test.ReadPicture();

		Test.Ui->SetScale(1.5f);
		Test.DrawFrames(3);
		Test.Ui->SetScale(1.0f);
		Test.DrawFrames(3);

		CHECK(Test.RequireItem("Outliner/Search") == Before);
		CHECK(Test.ReadPicture() == PictureBefore);
	}

	TEST_CASE("A scale beyond the limits is taken as the nearest limit, and one that is no number as one")
	{
		FEditorUiTest Test;

		Test.Ui->SetScale(0.01f);
		CHECK(Test.Ui->GetScale() == FEditorUi::MinScale);

		Test.Ui->SetScale(100.0f);
		CHECK(Test.Ui->GetScale() == FEditorUi::MaxScale);

		Test.Ui->SetScale(-2.0f);
		CHECK(Test.Ui->GetScale() == FEditorUi::MinScale);

		Test.Ui->SetScale(std::numeric_limits<float>::quiet_NaN());
		CHECK(Test.Ui->GetScale() == 1.0f);

		Test.Ui->SetScale(std::numeric_limits<float>::infinity());
		CHECK(Test.Ui->GetScale() == 1.0f);

		// At either limit the user interface is still drawn.
		Test.Ui->SetScale(FEditorUi::MaxScale);
		Test.DrawFrames(3);
		Test.Ui->SetScale(FEditorUi::MinScale);
		Test.DrawFrames(3);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("The viewport shows the scene from the editor's camera, in the editor's view mode")
	{
		FEditorUiTest Test;
		// A red square two metres ahead of a camera that looks along -Z covers the middle of the picture.
		AddMesh(Test.Editor.GetScene(),
			"Square",
			MakeSquareMesh(),
			FLinearColor(1.0f, 0.0f, 0.0f),
			FVector(0.0, 0.0, -2.0));
		REQUIRE(
			Test.Editor
				.ExecuteCommand("Viewport.SetCamera", ParseTestJson(R"({"Location": [0, 0, 0], "Yaw": 0, "Pitch": 0})"))
				.HasSucceeded());
		REQUIRE(
			Test.Editor.ExecuteCommand("Viewport.SetViewMode", ParseTestJson(R"({"Mode": "Unlit"})")).HasSucceeded());
		Test.DrawFrame();

		const FUiRectangle Viewport = Test.RequireItem("Viewport");
		const FImage Picture = Test.ReadPicture();

		CHECK(GetPixelAt(Picture, Viewport.GetCenter()) == Red);
		const FColor Corner = GetPixelAt(Picture, FVector2D(Viewport.Left + 4.0, Viewport.Top + 4.0));
		CHECK(Corner != Red);
		CHECK(Corner.R == Corner.G);
		CHECK(Corner.G == Corner.B);

		// Turned around, the camera has the square behind it.
		REQUIRE(Test.Editor.ExecuteCommand("Viewport.SetCamera", ParseTestJson(R"({"Yaw": 180})")).HasSucceeded());
		Test.DrawFrame();

		CHECK(GetPixelAt(Test.ReadPicture(), Viewport.GetCenter()) == Corner);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("The viewport fills its panel")
	{
		FEditorUiTest Test;

		const FUiRectangle Viewport = Test.RequireItem("Viewport");

		// To the left of the outliner, below the menu bar, the toolbar and the row of tabs, down to the bottom of
		// the surface.
		const float ToolbarBottom = Test.RequireItem("Toolbar/Add").Bottom;
		CHECK(Viewport.Left <= 2.0f);
		CHECK(Viewport.Top > ToolbarBottom);
		CHECK(Viewport.Top < ToolbarBottom + 45.0f);
		CHECK(Viewport.Right > 900.0f);
		CHECK(Viewport.Right < Test.RequireItem("Outliner/Search").Left);
		CHECK(Viewport.Bottom >= static_cast<float>(FEditorUiTest::DefaultHeight) - 2.0f);
	}

	TEST_CASE("The outliner lists the entities of the scene, the children of an entity below it")
	{
		FEditorUiTest Test;
		WScene& Scene = Test.Editor.GetScene();
		const WEntity* const Player = Scene.CreateEntity("Player");
		const WEntity* const Camera = Scene.CreateEntity("Camera", Scene.FindEntityById(Player->GetId()));
		const WEntity* const Ground = Scene.CreateEntity("Ground");
		Test.DrawFrame();

		const FUiRectangle PlayerRow = Test.RequireItem(std::format("Outliner/{}", Player->GetId()));
		const FUiRectangle CameraRow = Test.RequireItem(std::format("Outliner/{}", Camera->GetId()));
		const FUiRectangle GroundRow = Test.RequireItem(std::format("Outliner/{}", Ground->GetId()));

		CHECK(PlayerRow.Top < CameraRow.Top);
		CHECK(CameraRow.Top < GroundRow.Top);
		CHECK(PlayerRow.Left > Test.RequireItem("Viewport").Right);
	}

	TEST_CASE("A click on the row of an entity selects it, and a click on another selects that one instead")
	{
		FEditorUiTest Test;
		const int64 Player = Test.Editor.GetScene().CreateEntity("Player")->GetId();
		const int64 Ground = Test.Editor.GetScene().CreateEntity("Ground")->GetId();
		Test.DrawFrame();

		Test.Click(std::format("Outliner/{}", Player));
		CHECK(Test.Editor.GetSelection() == std::vector<int64>{Player});

		Test.Click(std::format("Outliner/{}", Ground));
		CHECK(Test.Editor.GetSelection() == std::vector<int64>{Ground});
	}

	TEST_CASE("A click with Ctrl held adds an entity to what is selected, and another takes it out again")
	{
		FEditorUiTest Test;
		const int64 Player = Test.Editor.GetScene().CreateEntity("Player")->GetId();
		const int64 Ground = Test.Editor.GetScene().CreateEntity("Ground")->GetId();
		Test.DrawFrame();
		Test.Click(std::format("Outliner/{}", Player));

		Test.SetKeyDown(EKey::LeftControl, true);
		Test.Click(std::format("Outliner/{}", Ground));
		const std::vector<int64> Both = {Player, Ground};
		CHECK(Test.Editor.GetSelection() == Both);

		Test.Click(std::format("Outliner/{}", Player));
		CHECK(Test.Editor.GetSelection() == std::vector<int64>{Ground});
		Test.SetKeyDown(EKey::LeftControl, false);
	}

	TEST_CASE("A click on one of several selected rows selects that one alone")
	{
		FEditorUiTest Test;
		const int64 Player = Test.Editor.GetScene().CreateEntity("Player")->GetId();
		const int64 Ground = Test.Editor.GetScene().CreateEntity("Ground")->GetId();
		Test.Editor.SetSelection({Player, Ground});
		Test.DrawFrame();

		Test.Click(std::format("Outliner/{}", Player));

		CHECK(Test.Editor.GetSelection() == std::vector<int64>{Player});
	}

	TEST_CASE("A click on the empty part of the outliner selects nothing")
	{
		FEditorUiTest Test;
		const int64 Player = Test.Editor.GetScene().CreateEntity("Player")->GetId();
		Test.DrawFrame();
		Test.Click(std::format("Outliner/{}", Player));
		REQUIRE(Test.Editor.GetSelection().size() == 1);

		Test.Click("Outliner/Empty");

		CHECK(Test.Editor.GetSelection().empty());
	}

	TEST_CASE("A click on the arrow of a row closes it and selects nothing")
	{
		FEditorUiTest Test;
		WScene& Scene = Test.Editor.GetScene();
		WEntity* const Player = Scene.CreateEntity("Player");
		const int64 Camera = Scene.CreateEntity("Camera", Player)->GetId();
		Test.DrawFrame();
		REQUIRE(Test.HasItem(std::format("Outliner/{}", Camera)));

		const FUiRectangle Row = Test.RequireItem(std::format("Outliner/{}", Player->GetId()));
		Test.ClickAt(FVector2D(Row.Left + 8.0, Row.GetCenter().y));

		CHECK_FALSE(Test.HasItem(std::format("Outliner/{}", Camera)));
		CHECK(Test.Editor.GetSelection().empty());
	}

	TEST_CASE("The outliner lists only the entities whose names contain what is typed into its search field")
	{
		FEditorUiTest Test;
		WScene& Scene = Test.Editor.GetScene();
		WEntity* const Player = Scene.CreateEntity("Player");
		const int64 Camera = Scene.CreateEntity("Main Camera", Player)->GetId();
		const int64 Ground = Scene.CreateEntity("Ground")->GetId();
		Test.DrawFrame();

		Test.Click("Outliner/Search");
		Test.Type("cam");

		CHECK(Test.HasItem(std::format("Outliner/{}", Camera)));
		CHECK_FALSE(Test.HasItem(std::format("Outliner/{}", Player->GetId())));
		CHECK_FALSE(Test.HasItem(std::format("Outliner/{}", Ground)));

		// What is found can be selected like any row.
		Test.Click(std::format("Outliner/{}", Camera));
		CHECK(Test.Editor.GetSelection() == std::vector<int64>{Camera});
	}

	TEST_CASE("A right click on a row selects its entity and opens a menu about it")
	{
		FEditorUiTest Test;
		const int64 Player = Test.Editor.GetScene().CreateEntity("Player")->GetId();
		const int64 Ground = Test.Editor.GetScene().CreateEntity("Ground")->GetId();
		Test.DrawFrame();
		Test.Click(std::format("Outliner/{}", Ground));
		CHECK_FALSE(Test.HasItem("Outliner/Menu/Delete"));

		Test.Click(std::format("Outliner/{}", Player), EMouseButton::Right);

		CHECK(Test.Editor.GetSelection() == std::vector<int64>{Player});
		CHECK(Test.HasItem("Outliner/Menu/Create Child Entity"));
		CHECK(Test.HasItem("Outliner/Menu/Rename"));
		CHECK(Test.HasItem("Outliner/Menu/Duplicate"));
		CHECK(Test.HasItem("Outliner/Menu/Delete"));

		// Escape closes the menu and changes nothing.
		Test.PressKey(EKey::Escape);
		CHECK_FALSE(Test.HasItem("Outliner/Menu/Delete"));
		CHECK(Test.Editor.GetScene().GetAllEntities().size() == 2);
	}

	TEST_CASE("A right click on a row that is selected with others leaves them all selected")
	{
		FEditorUiTest Test;
		const int64 Player = Test.Editor.GetScene().CreateEntity("Player")->GetId();
		const int64 Ground = Test.Editor.GetScene().CreateEntity("Ground")->GetId();
		Test.Editor.SetSelection({Player, Ground});
		Test.DrawFrame();

		Test.Click(std::format("Outliner/{}", Player), EMouseButton::Right);
		const std::vector<int64> Both = {Player, Ground};
		CHECK(Test.Editor.GetSelection() == Both);

		// The menu's entries are about everything that is selected.
		Test.Click("Outliner/Menu/Delete");
		CHECK(Test.Editor.GetScene().GetAllEntities().empty());
	}

	TEST_CASE("The menu of a row creates an entity below it, and the menu of the empty part one at the root")
	{
		FEditorUiTest Test;
		WEntity* const Player = Test.Editor.GetScene().CreateEntity("Player");
		Test.DrawFrame();

		Test.Click(std::format("Outliner/{}", Player->GetId()), EMouseButton::Right);
		Test.Click("Outliner/Menu/Create Child Entity");

		REQUIRE(Player->GetChildren().size() == 1);
		const int64 Child = Player->GetChildren()[0]->GetId();
		CHECK(Player->GetChildren()[0]->GetName() == "Entity");
		// What was created is what the user goes on to work with.
		CHECK(Test.Editor.GetSelection() == std::vector<int64>{Child});
		CHECK(Test.HasItem(std::format("Outliner/{}", Child)));

		Test.Click("Outliner/Empty", EMouseButton::Right);
		CHECK_FALSE(Test.HasItem("Outliner/Menu/Delete"));
		Test.Click("Outliner/Menu/Create Entity");

		REQUIRE(Test.Editor.GetScene().GetRootEntities().size() == 2);
		const WEntity* const Created = Test.Editor.GetScene().GetRootEntities()[1];
		CHECK(Created->GetName() == "Entity");
		CHECK(Test.Editor.GetSelection() == std::vector<int64>{Created->GetId()});

		// Each was one step of the history, named for what the user did.
		const std::vector<std::string> ExpectedSteps = {
			"Create Entity", "Create Entity", std::string(FEditor::DirectChangeStepName)};
		CHECK(Test.Editor.GetHistory().GetUndoStepNames() == ExpectedSteps);
	}

	TEST_CASE("F2 turns the row of the selected entity into a text field, and Enter gives the entity what was typed")
	{
		FEditorUiTest Test;
		const WEntity* const Player = Test.Editor.GetScene().CreateEntity("Player");
		const int64 PlayerId = Player->GetId();
		Test.DrawFrame();
		Test.Click(std::format("Outliner/{}", PlayerId));
		CHECK_FALSE(Test.HasItem("Outliner/Rename"));

		Test.PressKey(EKey::F2);
		REQUIRE(Test.HasItem("Outliner/Rename"));
		// The old name is selected, so what is typed takes its place.
		Test.Type("Hero");
		Test.PressKey(EKey::Enter);

		CHECK(Test.Editor.GetScene().FindEntityById(PlayerId)->GetName() == "Hero");
		CHECK_FALSE(Test.HasItem("Outliner/Rename"));
		CHECK(Test.Editor.GetHistory().GetUndoStepNames().front() == "Rename");

		// One undo gives the entity its old name back.
		Test.PressKey(EKey::Z, {EKey::LeftControl});
		CHECK(Test.Editor.GetScene().FindEntityById(PlayerId)->GetName() == "Player");
	}

	TEST_CASE("Rename in the menu of a row does what F2 does, and a name can be typed onto the end of the old one")
	{
		FEditorUiTest Test;
		const int64 Player = Test.Editor.GetScene().CreateEntity("Player")->GetId();
		Test.DrawFrame();

		Test.Click(std::format("Outliner/{}", Player), EMouseButton::Right);
		Test.Click("Outliner/Menu/Rename");
		REQUIRE(Test.HasItem("Outliner/Rename"));
		Test.PressKey(EKey::End);
		Test.Type(" 2");
		Test.PressKey(EKey::Enter);

		CHECK(Test.Editor.GetScene().FindEntityById(Player)->GetName() == "Player 2");
	}

	TEST_CASE("Escape ends the typing of a name and leaves the entity its name")
	{
		FEditorUiTest Test;
		const int64 Player = Test.Editor.GetScene().CreateEntity("Player")->GetId();
		Test.Editor.SetSelection({Player});
		Test.DrawFrame();
		Test.PressKey(EKey::F2);
		REQUIRE(Test.HasItem("Outliner/Rename"));
		Test.Type("Villain");

		Test.PressKey(EKey::Escape);

		CHECK_FALSE(Test.HasItem("Outliner/Rename"));
		CHECK(Test.Editor.GetScene().FindEntityById(Player)->GetName() == "Player");
		// Nothing was renamed, so nothing is there to undo.
		CHECK(Test.Editor.GetHistory().GetUndoStepNames().empty());
	}

	TEST_CASE("A click elsewhere gives the entity the name that was typed")
	{
		FEditorUiTest Test;
		const int64 Player = Test.Editor.GetScene().CreateEntity("Player")->GetId();
		Test.Editor.SetSelection({Player});
		Test.DrawFrame();
		Test.PressKey(EKey::F2);
		REQUIRE(Test.HasItem("Outliner/Rename"));
		Test.Type("Hero");

		Test.Click("Outliner/Search");

		CHECK_FALSE(Test.HasItem("Outliner/Rename"));
		CHECK(Test.Editor.GetScene().FindEntityById(Player)->GetName() == "Hero");
	}

	TEST_CASE("A name that is confirmed as it was is not a step of the history")
	{
		FEditorUiTest Test;
		const int64 Player = Test.Editor.GetScene().CreateEntity("Player")->GetId();
		Test.Editor.SetSelection({Player});
		Test.DrawFrame();
		Test.PressKey(EKey::Z, {EKey::LeftControl});
		Test.PressKey(EKey::Y, {EKey::LeftControl});
		const std::size_t StepCountBefore = Test.Editor.GetHistory().GetUndoStepNames().size();

		Test.PressKey(EKey::F2);
		REQUIRE(Test.HasItem("Outliner/Rename"));
		Test.PressKey(EKey::Enter);

		CHECK(Test.Editor.GetScene().FindEntityById(Player)->GetName() == "Player");
		CHECK(Test.Editor.GetHistory().GetUndoStepNames().size() == StepCountBefore);
	}

	TEST_CASE("The text field for a name goes away with the entity")
	{
		FEditorUiTest Test;
		REQUIRE(Test.Editor.ExecuteCommand("Entity.Create", ParseTestJson(R"({"Name": "Player"})")).HasSucceeded());
		const int64 Player = Test.Editor.GetScene().FindEntityByName("Player")->GetId();
		Test.Editor.SetSelection({Player});
		Test.DrawFrame();
		Test.PressKey(EKey::F2);
		REQUIRE(Test.HasItem("Outliner/Rename"));

		// An agent, or another window, undoes the creation while the name is being typed.
		REQUIRE(Test.Editor.ExecuteCommand("Edit.Undo", FJsonValue()).HasSucceeded());
		Test.DrawFrames(2);

		CHECK_FALSE(Test.HasItem("Outliner/Rename"));
		CHECK(Test.Editor.GetScene().GetAllEntities().empty());
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("Delete deletes everything that is selected, as one step")
	{
		FEditorUiTest Test;
		WScene& Scene = Test.Editor.GetScene();
		WEntity* const Player = Scene.CreateEntity("Player");
		const int64 Camera = Scene.CreateEntity("Camera", Player)->GetId();
		const int64 Ground = Scene.CreateEntity("Ground")->GetId();
		const int64 Tree = Scene.CreateEntity("Tree")->GetId();
		// The camera goes with the player, whose child it is; that it is selected too changes nothing.
		Test.Editor.SetSelection({Player->GetId(), Camera, Ground});
		Test.DrawFrame();

		Test.PressKey(EKey::Delete);

		REQUIRE(Test.Editor.GetScene().GetAllEntities().size() == 1);
		CHECK(Test.Editor.GetScene().GetAllEntities()[0]->GetId() == Tree);
		CHECK(Test.Editor.GetHistory().GetUndoStepNames().front() == "Delete");

		// One undo brings all of them back, and they are selected as they were.
		Test.PressKey(EKey::Z, {EKey::LeftControl});
		CHECK(Test.Editor.GetScene().GetAllEntities().size() == 4);
		CHECK(Test.Editor.GetSelection().size() == 3);
	}

	TEST_CASE("Delete with nothing selected, or in a text field, deletes no entity")
	{
		FEditorUiTest Test;
		const int64 Player = Test.Editor.GetScene().CreateEntity("Player")->GetId();
		Test.DrawFrame();

		Test.PressKey(EKey::Delete);
		CHECK(Test.Editor.GetScene().GetAllEntities().size() == 1);

		Test.Editor.SetSelection({Player});
		Test.Click("Outliner/Search");
		Test.Type("Pl");
		Test.PressKey(EKey::Home);
		Test.PressKey(EKey::Delete);

		// The key took a letter out of the search field: "l" still finds the player.
		CHECK(Test.Editor.GetScene().GetAllEntities().size() == 1);
		CHECK(Test.HasItem(std::format("Outliner/{}", Player)));
	}

	TEST_CASE("Ctrl+D copies what is selected, as one step, and selects the copies")
	{
		FEditorUiTest Test;
		WScene& Scene = Test.Editor.GetScene();
		WEntity* const Player = Scene.CreateEntity("Player");
		const int64 Camera = Scene.CreateEntity("Camera", Player)->GetId();
		const int64 Ground = Scene.CreateEntity("Ground")->GetId();
		Test.Editor.SetSelection({Player->GetId(), Camera, Ground});
		Test.DrawFrame();

		Test.PressKey(EKey::D, {EKey::LeftControl});

		// The player with its camera, and the ground: the camera is copied with the player, not once more.
		REQUIRE(Scene.GetRootEntities().size() == 4);
		const WEntity* const PlayerCopy = Scene.GetRootEntities()[2];
		const WEntity* const GroundCopy = Scene.GetRootEntities()[3];
		CHECK(PlayerCopy->GetName() == "Player");
		CHECK(PlayerCopy->GetChildren().size() == 1);
		CHECK(GroundCopy->GetName() == "Ground");
		CHECK(Scene.GetAllEntities().size() == 6);
		const std::vector<int64> Copies = {PlayerCopy->GetId(), GroundCopy->GetId()};
		CHECK(Test.Editor.GetSelection() == Copies);
		CHECK(Test.Editor.GetHistory().GetUndoStepNames().front() == "Duplicate");

		Test.PressKey(EKey::Z, {EKey::LeftControl});
		CHECK(Test.Editor.GetScene().GetAllEntities().size() == 3);
	}

	TEST_CASE("A row that is dragged onto another becomes its child, where it is in the world")
	{
		FEditorUiTest Test;
		WScene& Scene = Test.Editor.GetScene();
		WEntity* const Player = Scene.CreateEntity("Player");
		Player->SetWorldLocation(FVector(10.0, 0.0, 0.0));
		WEntity* const Sword = Scene.CreateEntity("Sword");
		Sword->SetWorldLocation(FVector(11.0, 1.0, 0.0));
		const int64 PlayerId = Player->GetId();
		const int64 SwordId = Sword->GetId();
		Test.DrawFrame();

		Test.Drag(std::format("Outliner/{}", SwordId), std::format("Outliner/{}", PlayerId));

		const WEntity* const MovedSword = Test.Editor.GetScene().FindEntityById(SwordId);
		REQUIRE(MovedSword->GetParent() != nullptr);
		CHECK(MovedSword->GetParent()->GetId() == PlayerId);
		CHECK(MovedSword->GetWorldLocation() == FVector(11.0, 1.0, 0.0));
		CHECK(Test.Editor.GetHistory().GetUndoStepNames().front() == "Attach");

		// Dragged onto the empty part of the panel, it has no parent again.
		Test.Drag(std::format("Outliner/{}", SwordId), "Outliner/Empty");

		CHECK(Test.Editor.GetScene().FindEntityById(SwordId)->GetParent() == nullptr);
		CHECK(Test.Editor.GetScene().FindEntityById(SwordId)->GetWorldLocation() == FVector(11.0, 1.0, 0.0));
	}

	TEST_CASE("A row cannot be dropped on itself or on what is below it")
	{
		FEditorUiTest Test;
		WScene& Scene = Test.Editor.GetScene();
		WEntity* const Player = Scene.CreateEntity("Player");
		const int64 Arm = Scene.CreateEntity("Arm", Player)->GetId();
		const int64 PlayerId = Player->GetId();
		Test.DrawFrame();
		const std::size_t StepCountBefore = Test.Editor.GetHistory().GetUndoStepNames().size();

		Test.Drag(std::format("Outliner/{}", PlayerId), std::format("Outliner/{}", Arm));

		CHECK(Test.Editor.GetScene().FindEntityById(PlayerId)->GetParent() == nullptr);
		CHECK(Test.Editor.GetScene().FindEntityById(Arm)->GetParent()->GetId() == PlayerId);
		CHECK(Test.Editor.GetHistory().GetUndoStepNames().size() == StepCountBefore);
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("Dragging one of several selected rows moves all of them, as one step")
	{
		FEditorUiTest Test;
		WScene& Scene = Test.Editor.GetScene();
		const int64 Crate = Scene.CreateEntity("Crate")->GetId();
		const int64 Sword = Scene.CreateEntity("Sword")->GetId();
		const int64 Shield = Scene.CreateEntity("Shield")->GetId();
		Test.DrawFrame();
		Test.Click(std::format("Outliner/{}", Sword));
		Test.SetKeyDown(EKey::LeftControl, true);
		Test.Click(std::format("Outliner/{}", Shield));
		Test.SetKeyDown(EKey::LeftControl, false);

		Test.Drag(std::format("Outliner/{}", Shield), std::format("Outliner/{}", Crate));

		const WScene& After = Test.Editor.GetScene();
		REQUIRE(After.FindEntityById(Sword)->GetParent() != nullptr);
		REQUIRE(After.FindEntityById(Shield)->GetParent() != nullptr);
		CHECK(After.FindEntityById(Sword)->GetParent()->GetId() == Crate);
		CHECK(After.FindEntityById(Shield)->GetParent()->GetId() == Crate);

		Test.PressKey(EKey::Z, {EKey::LeftControl});
		CHECK(Test.Editor.GetScene().FindEntityById(Sword)->GetParent() == nullptr);
		CHECK(Test.Editor.GetScene().FindEntityById(Shield)->GetParent() == nullptr);
	}

	TEST_CASE("The Edit menu duplicates, deletes and renames what is selected, and nothing when nothing is")
	{
		FEditorUiTest Test;
		const int64 Player = Test.Editor.GetScene().CreateEntity("Player")->GetId();
		Test.DrawFrame();

		// Without a selection the entries are there and do nothing.
		Test.Click("Menu/Edit");
		Test.Click("Menu/Edit/Duplicate");
		Test.Click("Menu/Edit/Delete");
		Test.Click("Menu/Edit/Rename");
		Test.PressKey(EKey::Escape);
		CHECK(Test.Editor.GetScene().GetAllEntities().size() == 1);
		CHECK_FALSE(Test.HasItem("Outliner/Rename"));

		Test.Click(std::format("Outliner/{}", Player));
		Test.Click("Menu/Edit");
		Test.Click("Menu/Edit/Duplicate");
		REQUIRE(Test.Editor.GetScene().GetAllEntities().size() == 2);
		const int64 Copy = Test.Editor.GetScene().GetRootEntities()[1]->GetId();
		CHECK(Test.Editor.GetSelection() == std::vector<int64>{Copy});

		Test.Click("Menu/Edit");
		Test.Click("Menu/Edit/Rename");
		REQUIRE(Test.HasItem("Outliner/Rename"));
		Test.Type("Twin");
		Test.PressKey(EKey::Enter);
		CHECK(Test.Editor.GetScene().FindEntityById(Copy)->GetName() == "Twin");

		Test.Click("Menu/Edit");
		Test.Click("Menu/Edit/Delete");
		REQUIRE(Test.Editor.GetScene().GetAllEntities().size() == 1);
		CHECK(Test.Editor.GetScene().GetAllEntities()[0]->GetId() == Player);
	}

	TEST_CASE("Ctrl+Z undoes the last change to the scene and Ctrl+Y redoes it")
	{
		FEditorUiTest Test;
		REQUIRE(Test.Editor.ExecuteCommand("Entity.Create", ParseTestJson(R"({"Name": "Player"})")).HasSucceeded());
		Test.DrawFrame();

		Test.PressKey(EKey::Z, {EKey::LeftControl});
		CHECK(Test.Editor.GetScene().GetAllEntities().empty());

		Test.PressKey(EKey::Y, {EKey::LeftControl});
		CHECK(Test.Editor.GetScene().FindEntityByName("Player") != nullptr);

		Test.PressKey(EKey::Z, {EKey::RightControl});
		CHECK(Test.Editor.GetScene().GetAllEntities().empty());

		Test.PressKey(EKey::Z, {EKey::LeftControl, EKey::LeftShift});
		CHECK(Test.Editor.GetScene().FindEntityByName("Player") != nullptr);
	}

	TEST_CASE("Z and Y without Ctrl change nothing")
	{
		FEditorUiTest Test;
		REQUIRE(Test.Editor.ExecuteCommand("Entity.Create", ParseTestJson(R"({"Name": "Player"})")).HasSucceeded());
		Test.DrawFrame();

		Test.PressKey(EKey::Z);
		Test.PressKey(EKey::Y);
		Test.PressKey(EKey::N);

		CHECK(Test.Editor.GetScene().FindEntityByName("Player") != nullptr);
		CHECK(Test.Editor.GetHistory().CanUndo());
	}

	TEST_CASE("Ctrl+N replaces the scene with an empty one")
	{
		FEditorUiTest Test;
		REQUIRE(Test.Editor.ExecuteCommand("Entity.Create", ParseTestJson(R"({"Name": "Player"})")).HasSucceeded());
		Test.DrawFrame();

		Test.PressKey(EKey::N, {EKey::LeftControl});

		CHECK(Test.Editor.GetScene().GetAllEntities().empty());
		CHECK_FALSE(Test.Editor.GetHistory().CanUndo());
	}

	TEST_CASE("Ctrl+Z in a text field undoes what was typed there, not a change to the scene")
	{
		FEditorUiTest Test;
		REQUIRE(Test.Editor.ExecuteCommand("Entity.Create", ParseTestJson(R"({"Name": "Player"})")).HasSucceeded());
		Test.DrawFrame();
		Test.Click("Outliner/Search");
		Test.Type("x");

		Test.PressKey(EKey::Z, {EKey::LeftControl});

		CHECK(Test.Editor.GetScene().FindEntityByName("Player") != nullptr);
	}

	TEST_CASE("The entries of the File and Edit menus do what their shortcuts do")
	{
		FEditorUiTest Test;
		REQUIRE(Test.Editor.ExecuteCommand("Entity.Create", ParseTestJson(R"({"Name": "Player"})")).HasSucceeded());
		Test.DrawFrame();
		// A menu's entries exist only while it is open.
		CHECK_FALSE(Test.HasItem("Menu/Edit/Undo"));

		Test.Click("Menu/Edit");
		Test.Click("Menu/Edit/Undo");
		CHECK(Test.Editor.GetScene().GetAllEntities().empty());
		CHECK_FALSE(Test.HasItem("Menu/Edit/Undo"));

		Test.Click("Menu/Edit");
		Test.Click("Menu/Edit/Redo");
		CHECK(Test.Editor.GetScene().FindEntityByName("Player") != nullptr);

		Test.Click("Menu/File");
		Test.Click("Menu/File/New Scene");
		CHECK(Test.Editor.GetScene().GetAllEntities().empty());
	}

	TEST_CASE("An entry of the Edit menu that has nothing to undo or redo does nothing")
	{
		FEditorUiTest Test;
		Test.Editor.GetScene().SetName("Kept");

		Test.Click("Menu/Edit");
		Test.Click("Menu/Edit/Undo");
		Test.Click("Menu/Edit/Redo");

		// The menu is still open, because nothing was chosen.
		CHECK(Test.HasItem("Menu/Edit/Undo"));
		CHECK(Test.Editor.GetScene().GetName() == "Kept");
	}

	TEST_CASE("The Window menu closes a panel and opens it again, and brings back the default layout")
	{
		FEditorUiTest Test;
		const FUiRectangle Before = Test.RequireItem("Viewport");

		const FUiRectangle DetailsBefore = Test.RequireItem("Details/NothingSelected");

		Test.Click("Menu/Window");
		Test.Click("Menu/Window/Outliner");
		CHECK_FALSE(Test.HasItem("Outliner/Search"));
		// The panel below the outliner takes the room it had, and the viewport keeps its own.
		CHECK(Test.RequireItem("Details/NothingSelected").Top < DetailsBefore.Top);
		CHECK(Test.RequireItem("Viewport") == Before);

		Test.Click("Menu/Window");
		Test.Click("Menu/Window/Details");
		CHECK_FALSE(Test.HasItem("Details/NothingSelected"));
		// With nothing left beside it, the viewport is as wide as the window.
		CHECK(Test.RequireItem("Viewport").GetWidth() > Before.GetWidth());

		Test.Click("Menu/Window");
		Test.Click("Menu/Window/Viewport");
		CHECK_FALSE(Test.HasItem("Viewport"));

		Test.Click("Menu/Window");
		Test.Click("Menu/Window/Outliner");
		CHECK(Test.HasItem("Outliner/Search"));
		CHECK_FALSE(Test.HasItem("Viewport"));

		Test.Click("Menu/Window");
		Test.Click("Menu/Window/Load Default Layout");
		CHECK(Test.RequireItem("Viewport") == Before);
		CHECK(Test.HasItem("Outliner/Search"));
		CHECK(Test.RequireItem("Details/NothingSelected") == DetailsBefore);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("Help opens a window about the editor, which keeps the shortcuts from the scene until it is closed")
	{
		FEditorUiTest Test;
		REQUIRE(Test.Editor.ExecuteCommand("Entity.Create", ParseTestJson(R"({"Name": "Player"})")).HasSucceeded());
		Test.DrawFrame();

		Test.Click("Menu/Help");
		Test.Click("Menu/Help/About White Wolf Engine");
		REQUIRE(Test.HasItem("About/Close"));

		Test.PressKey(EKey::Z, {EKey::LeftControl});
		Test.PressKey(EKey::N, {EKey::LeftControl});
		CHECK(Test.Editor.GetScene().FindEntityByName("Player") != nullptr);

		SUBCASE("Closed with its button")
		{
			Test.Click("About/Close");
		}
		SUBCASE("Closed with Escape")
		{
			Test.PressKey(EKey::Escape);
		}

		CHECK_FALSE(Test.HasItem("About/Close"));
		Test.PressKey(EKey::Z, {EKey::LeftControl});
		CHECK(Test.Editor.GetScene().GetAllEntities().empty());
	}

	TEST_CASE("Nothing is under a mouse that has left the window")
	{
		FEditorUiTest Test;
		const FUiRectangle Title = Test.RequireItem("Menu/File");
		const FVector2D Corner(Title.Left + 2.0, Title.Top + 2.0);
		const FColor Plain = GetPixelAt(Test.ReadPicture(), Corner);

		// The title of a menu is highlighted while the mouse is over it, and only then.
		Test.MoveMouse(Title.GetCenter());
		Test.DrawFrame();
		CHECK(GetPixelAt(Test.ReadPicture(), Corner) != Plain);

		Test.Input.HandleMouseLeave();
		Test.DrawFrames(2);
		CHECK(GetPixelAt(Test.ReadPicture(), Corner) == Plain);
	}

	TEST_CASE("A user interface can be created again after one was destroyed")
	{
		{
			const FEditorUiTest First;
			CHECK(First.HasItem("Viewport"));
		}

		const FEditorUiTest Second;
		CHECK(Second.HasItem("Viewport"));
		CHECK(CountLoggedErrors(Second.Capture) == 0);
	}
}
