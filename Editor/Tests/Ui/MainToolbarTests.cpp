#include "Assets/TestAssets.h"
#include "Math/Color.h"
#include "Math/Quat.h"
#include "Math/Vector.h"
#include "Object/Class.h"
#include "Object/ObjectGlobals.h"
#include "Rendering/Image.h"
#include "Rendering/SceneViewMode.h"
#include "Rendering/StaticMesh.h"
#include "Rendering/StaticMeshComponent.h"
#include "Scene/Component.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Ui/TestEditorUi.h"
#include "Viewport/ViewportCamera.h"

#include <doctest/doctest.h>
#include <glm/geometric.hpp>

#include <cmath>
#include <format>
#include <ostream>
#include <string>
#include <vector>

namespace
{
	/** Chooses a line of the list that the Add button of the toolbar opens. */
	void Add(FEditorUiTest& Test, const std::string& Label)
	{
		Test.Click("Toolbar/Add");
		Test.Click(std::format("Toolbar/Add/{}", Label));
	}

	/** Returns what the last step of the history is called, or nothing if there is none. */
	std::string GetLastStepName(const FEditorUiTest& Test)
	{
		const std::vector<std::string> Names = Test.Editor.GetHistory().GetUndoStepNames();
		return Names.empty() ? std::string() : Names.front();
	}
}

TEST_SUITE("FMainToolbar")
{
	TEST_CASE("Add makes a cube where the camera looks, and selects it")
	{
		FEditorUiTest Test;
		const FVector FocusPoint = Test.Editor.GetViewportCamera().GetFocusPoint();
		CHECK_FALSE(Test.HasItem("Toolbar/Add/Cube"));

		Test.Click("Toolbar/Add");
		REQUIRE(Test.HasItem("Toolbar/Add/Cube"));
		Test.Click("Toolbar/Add/Cube");

		const WScene& Scene = Test.Editor.GetScene();
		REQUIRE(Scene.GetAllEntities().size() == 1);
		const WEntity* const Cube = Scene.GetAllEntities()[0];
		CHECK(Cube->GetName() == "Cube");

		// At what the camera looks at, to the centimetre, so that its place reads as a number.
		const FVector Location = Cube->GetWorldLocation();
		CHECK(glm::length(Location - FocusPoint) < 0.01);
		for (const double Coordinate : {Location.x, Location.y, Location.z})
		{
			CHECK(std::round(Coordinate * 100.0) == doctest::Approx(Coordinate * 100.0));
		}

		REQUIRE(Cube->GetComponents().size() == 1);
		const WStaticMeshComponent* const Component = Cast<WStaticMeshComponent>(Cube->GetComponents()[0].Get());
		REQUIRE(Component != nullptr);
		REQUIRE(Component->Mesh != nullptr);
		CHECK(Component->Mesh->GetAssetPath() == "Engine/Shapes#Cube");
		CHECK(Test.Editor.GetSelection() == std::vector<int64>{Cube->GetId()});
		CHECK(GetLastStepName(Test) == "Add Cube");
		// The list has closed.
		CHECK_FALSE(Test.HasItem("Toolbar/Add/Cube"));

		// All of it was one thing to do, and is one thing to undo.
		Test.PressKey(EKey::Z, {EKey::LeftControl});
		CHECK(Test.Editor.GetScene().GetAllEntities().empty());
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("Each line of the Add menu makes an entity with what the line names")
	{
		FEditorUiTest Test;
		std::string Label;
		std::string ExpectedName;
		std::string ExpectedClass;
		std::string ExpectedMesh;

		SUBCASE("An entity with nothing on it")
		{
			Label = "Empty Entity";
			ExpectedName = "Entity";
		}
		SUBCASE("A sphere")
		{
			Label = "Sphere";
			ExpectedName = "Sphere";
			ExpectedClass = "WStaticMeshComponent";
			ExpectedMesh = "Engine/Shapes#Sphere";
		}
		SUBCASE("A plane")
		{
			Label = "Plane";
			ExpectedName = "Plane";
			ExpectedClass = "WStaticMeshComponent";
			ExpectedMesh = "Engine/Shapes#Plane";
		}
		SUBCASE("A sun")
		{
			Label = "Directional Light";
			ExpectedName = "Directional Light";
			ExpectedClass = "WDirectionalLightComponent";
		}
		SUBCASE("A lamp")
		{
			Label = "Point Light";
			ExpectedName = "Point Light";
			ExpectedClass = "WPointLightComponent";
		}
		SUBCASE("The light of the sky")
		{
			Label = "Sky Light";
			ExpectedName = "Sky Light";
			ExpectedClass = "WSkyLightComponent";
		}

		Add(Test, Label);

		REQUIRE(Test.Editor.GetScene().GetAllEntities().size() == 1);
		const WEntity* const Entity = Test.Editor.GetScene().GetAllEntities()[0];
		CHECK(Entity->GetName() == ExpectedName);
		CHECK(Test.Editor.GetSelection() == std::vector<int64>{Entity->GetId()});
		CHECK(GetLastStepName(Test) == std::format("Add {}", ExpectedName));

		if (ExpectedClass.empty())
		{
			CHECK(Entity->GetComponents().empty());
		}
		else
		{
			REQUIRE(Entity->GetComponents().size() == 1);
			const WComponent* const Component = Entity->GetComponents()[0];
			CHECK(Component->GetClass()->GetName() == ExpectedClass);
			if (!ExpectedMesh.empty())
			{
				const WStaticMeshComponent* const MeshComponent = Cast<WStaticMeshComponent>(Component);
				REQUIRE(MeshComponent != nullptr);
				REQUIRE(MeshComponent->Mesh != nullptr);
				CHECK(MeshComponent->Mesh->GetAssetPath() == ExpectedMesh);
			}
		}
		// The Details panel shows what was added, ready to be changed.
		CHECK(Test.HasItem("Details/Name"));
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("A sun that is added shines down at a slant, and everything else stands as it is")
	{
		FEditorUiTest Test;

		Add(Test, "Directional Light");
		Add(Test, "Cube");

		const WScene& Scene = Test.Editor.GetScene();
		REQUIRE(Scene.GetAllEntities().size() == 2);
		// A light shines along its -Z axis.
		const FVector3f SunDirection =
			Scene.GetAllEntities()[0]->GetWorldTransform().Rotation * FVector3f(0.0f, 0.0f, -1.0f);
		CHECK(SunDirection.y < -0.5f);
		CHECK(SunDirection.y > -0.95f);
		CHECK(Scene.GetAllEntities()[1]->GetWorldTransform().Rotation == FQuat(1.0f, 0.0f, 0.0f, 0.0f));
	}

	TEST_CASE("What Add makes is in the picture of the viewport at once")
	{
		FEditorUiTest Test;
		// Every surface in exactly its colour: a mesh without a material is white.
		Test.Editor.SetViewMode(ESceneViewMode::Unlit);
		Test.DrawFrame();
		const FVector2D Middle = Test.RequireItem("Viewport").GetCenter();
		const FColor White(255, 255, 255);
		REQUIRE_FALSE(Test.ReadPicture().GetPixel(static_cast<int32>(Middle.x), static_cast<int32>(Middle.y)) == White);

		Add(Test, "Cube");

		// The camera looks at where the cube was put.
		CHECK(Test.ReadPicture().GetPixel(static_cast<int32>(Middle.x), static_cast<int32>(Middle.y)) == White);
	}

	TEST_CASE("Things are added to the scene one after another, each a step of its own")
	{
		FEditorUiTest Test;

		Add(Test, "Cube");
		Add(Test, "Sphere");
		Add(Test, "Point Light");

		const WScene& Scene = Test.Editor.GetScene();
		REQUIRE(Scene.GetAllEntities().size() == 3);
		CHECK(Test.Editor.GetSelection() == std::vector<int64>{Scene.GetAllEntities()[2]->GetId()});
		const std::vector<std::string> ExpectedSteps = {"Add Point Light", "Add Sphere", "Add Cube"};
		CHECK(Test.Editor.GetHistory().GetUndoStepNames() == ExpectedSteps);

		Test.PressKey(EKey::Z, {EKey::LeftControl});
		CHECK(Test.Editor.GetScene().GetAllEntities().size() == 2);
	}
}
