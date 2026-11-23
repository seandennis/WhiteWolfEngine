#include "Assets/Asset.h"
#include "Assets/TestAssets.h"
#include "Core/FileHelper.h"
#include "Math/Color.h"
#include "Math/LinearColor.h"
#include "Object/ObjectGlobals.h"
#include "Rendering/EnvironmentMap.h"
#include "Rendering/HdrImage.h"
#include "Rendering/Image.h"
#include "Rendering/ImageFile.h"
#include "Rendering/Material.h"
#include "Rendering/SkyLightComponent.h"
#include "Rendering/StaticMesh.h"
#include "Rendering/StaticMeshComponent.h"
#include "Rendering/TestGltfFiles.h"
#include "Rendering/Texture.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "TestSupport/ScopedTempDirectory.h"
#include "Ui/TestEditorUi.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <format>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

// The list from which an asset is chosen for a property, used as a person uses it: in the Details panel, with a
// mouse, through FEditorUi. The editor has a project in a temporary directory, with a picture, an environment and
// a model in it.

namespace
{
	constexpr int32 SurfaceWidth = 1280;
	constexpr int32 SurfaceHeight = 1500;

	/**
	 * How many frames are drawn after an entity is selected, before anything in the Details panel is clicked: a
	 * panel that has more rows than it has room for gets a bar to scroll with in its second frame, which moves
	 * its rows aside.
	 */
	constexpr int32 PanelSettlingFrameCount = 3;

	/** Returns the name of the widget of a property of an entity or component, or of something below it. */
	std::string Row(int64 ObjectId, std::string_view Property)
	{
		return std::format("Details/{}/{}", ObjectId, Property);
	}

	/** An editor with its user interface and a project, into which the test writes the files that it needs. */
	struct FAssetPickerTest : FEditorUiTest
	{
		FAssetPickerTest()
			: FEditorUiTest(SurfaceWidth, SurfaceHeight)
		{
			Content = CreateTestProject(Editor, Directory.GetPath());
		}

		/** Returns where a file of the project's content is, and makes sure that its directory is there. */
		std::filesystem::path MakeFilePath(const std::string& File) const
		{
			std::filesystem::path Path = Content / FFileHelper::PathFromUtf8(File);
			std::filesystem::create_directories(Path.parent_path());
			return Path;
		}

		void WritePicture(const std::string& File) const
		{
			std::string Error;
			REQUIRE(FImageFile::SavePng(FImage(2, 2, FColor(255, 0, 0, 255)), MakeFilePath(File), Error));
		}

		void WriteEnvironment(const std::string& File) const
		{
			std::string Error;
			REQUIRE(FImageFile::SaveHdr(FHdrImage(4, 2, FLinearColor(1.0f, 1.0f, 1.0f)), MakeFilePath(File), Error));
		}

		/** Writes the test square, a model of one mesh with one material. */
		void WriteSquare(const std::string& File) const
		{
			WriteTestGltfFile(MakeFilePath(File), FTestGltf().BuildWithEmbeddedBuffer(MakeTestSquareBuffer()));
		}

		void WriteText(const std::string& File, std::string_view Text) const
		{
			WriteTestGltfFile(MakeFilePath(File), Text);
		}

		/** Writes one file of every kind, and one that is no asset. */
		void WriteOneOfEach() const
		{
			WritePicture("Textures/Wood.png");
			WriteEnvironment("Skies/Noon.hdr");
			WriteSquare("Models/Square.gltf");
			WriteText("Notes.txt", "Not an asset");
		}

		/** Adds an entity with a component of a class, selects it, and returns the identifier of the component. */
		template <typename ComponentType>
		int64 AddSelected()
		{
			WEntity* const Entity = Editor.GetScene().CreateEntity("Thing");
			REQUIRE(Entity != nullptr);
			const ComponentType* const Component = Entity->AddComponent<ComponentType>();
			REQUIRE(Component != nullptr);
			Editor.SetSelection({Entity->GetId()});
			DrawFrames(PanelSettlingFrameCount);
			return Component->GetId();
		}

		/** Returns a component as it is in the editor's scene now. */
		template <typename ComponentType>
		const ComponentType& Get(int64 ComponentId) const
		{
			const ComponentType* const Component =
				Cast<ComponentType>(Editor.GetScene().FindComponentById(ComponentId));
			REQUIRE(Component != nullptr);
			return *Component;
		}

		FScopedTempDirectory Directory;
		std::filesystem::path Content;
	};

	/** Returns the asset path of what a property refers to, or "None" if it refers to nothing. */
	std::string GetPath(const WAsset* Asset)
	{
		return Asset != nullptr ? Asset->GetAssetPath() : std::string("None");
	}
}

TEST_SUITE("FAssetPicker")
{
	TEST_CASE("The button beside the path of an asset opens a list of what the property can be given")
	{
		FAssetPickerTest Test;
		Test.WriteOneOfEach();
		const int64 Component = Test.AddSelected<WStaticMeshComponent>();
		const std::string Mesh = Row(Component, "Mesh");
		CHECK_FALSE(Test.HasItem(Mesh + "/Assets/None"));

		Test.Click(Mesh + "/Choose");

		// A mesh is one of the engine's shapes or in a model. A picture, an environment and a note are none.
		CHECK(Test.HasItem(Mesh + "/Assets/None"));
		CHECK(Test.HasItem(Mesh + "/Assets/Engine/Shapes"));
		CHECK(Test.HasItem(Mesh + "/Assets/Models/Square.gltf"));
		CHECK_FALSE(Test.HasItem(Mesh + "/Assets/Textures/Wood.png"));
		CHECK_FALSE(Test.HasItem(Mesh + "/Assets/Skies/Noon.hdr"));
		CHECK_FALSE(Test.HasItem(Mesh + "/Assets/Notes.txt"));
		// The list is below the button that opened it, and the files are in the order of their names.
		CHECK(Test.RequireItem(Mesh + "/Assets/None").Top >= Test.RequireItem(Mesh + "/Choose").Top);
		CHECK(Test.RequireItem(Mesh + "/Assets/Models/Square.gltf").Top >
			Test.RequireItem(Mesh + "/Assets/Engine/Shapes").Top);
		// Nothing has been looked into yet, and nothing chosen.
		CHECK_FALSE(Test.HasItem(Mesh + "/Assets/Engine/Shapes#Cube"));
		CHECK(Test.Get<WStaticMeshComponent>(Component).Mesh == nullptr);
	}

	TEST_CASE("A file of several assets opens a list of its own, from which one of them is chosen")
	{
		FAssetPickerTest Test;
		Test.WriteOneOfEach();
		const int64 Component = Test.AddSelected<WStaticMeshComponent>();
		const std::string Mesh = Row(Component, "Mesh");

		Test.Click(Mesh + "/Choose");
		Test.Click(Mesh + "/Assets/Engine/Shapes");
		REQUIRE(Test.HasItem(Mesh + "/Assets/Engine/Shapes#Cube"));
		REQUIRE(Test.HasItem(Mesh + "/Assets/Engine/Shapes#Sphere"));
		REQUIRE(Test.HasItem(Mesh + "/Assets/Engine/Shapes#Plane"));
		Test.Click(Mesh + "/Assets/Engine/Shapes#Sphere");

		CHECK(GetPath(Test.Get<WStaticMeshComponent>(Component).Mesh.Get()) == "Engine/Shapes#Sphere");
		// The lists have closed, and choosing was one step that is undone like any other.
		CHECK_FALSE(Test.HasItem(Mesh + "/Assets/None"));
		CHECK(Test.Editor.GetHistory().GetUndoStepNames().front() == "Set Mesh");
		Test.PressKey(EKey::Z, {EKey::LeftControl});
		CHECK(Test.Get<WStaticMeshComponent>(Component).Mesh == nullptr);
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("The meshes and the materials of a model are chosen from the model's list")
	{
		FAssetPickerTest Test;
		Test.WriteOneOfEach();
		const int64 Component = Test.AddSelected<WStaticMeshComponent>();
		const std::string Mesh = Row(Component, "Mesh");
		const std::string Material = Row(Component, "Material");

		Test.Click(Mesh + "/Choose");
		Test.Click(Mesh + "/Assets/Models/Square.gltf");
		// Of what the model has, the meshes are offered for a mesh.
		REQUIRE(Test.HasItem(Mesh + "/Assets/Models/Square.gltf#Meshes/0/Primitives/0"));
		CHECK_FALSE(Test.HasItem(Mesh + "/Assets/Models/Square.gltf#Materials/0"));
		Test.Click(Mesh + "/Assets/Models/Square.gltf#Meshes/0/Primitives/0");
		CHECK(GetPath(Test.Get<WStaticMeshComponent>(Component).Mesh.Get()) ==
			"Models/Square.gltf#Meshes/0/Primitives/0");

		// And for a material, the materials. The engine's shapes have none, and are not in the list at all.
		Test.Click(Material + "/Choose");
		CHECK_FALSE(Test.HasItem(Material + "/Assets/Engine/Shapes"));
		Test.Click(Material + "/Assets/Models/Square.gltf");
		REQUIRE(Test.HasItem(Material + "/Assets/Models/Square.gltf#Materials/0"));
		CHECK_FALSE(Test.HasItem(Material + "/Assets/Models/Square.gltf#Meshes/0/Primitives/0"));
		Test.Click(Material + "/Assets/Models/Square.gltf#Materials/0");

		CHECK(GetPath(Test.Get<WStaticMeshComponent>(Component).Material.Get()) == "Models/Square.gltf#Materials/0");
		CHECK(GetPath(Test.Get<WStaticMeshComponent>(Component).Mesh.Get()) ==
			"Models/Square.gltf#Meshes/0/Primitives/0");
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("A picture is a line of the list by itself, as a colour and as data")
	{
		FAssetPickerTest Test;
		Test.WriteOneOfEach();
		const int64 Gadget = Test.AddSelected<WEditorTestGadgetComponent>();
		const std::string Picture = Row(Gadget, "Picture");

		Test.Click(Picture + "/Choose");
		REQUIRE(Test.HasItem(Picture + "/Assets/Textures/Wood.png"));
		REQUIRE(Test.HasItem(Picture + "/Assets/Textures/Wood.png#Linear"));
		// A model may have textures, and is offered; the shapes and the environment are not.
		CHECK(Test.HasItem(Picture + "/Assets/Models/Square.gltf"));
		CHECK_FALSE(Test.HasItem(Picture + "/Assets/Engine/Shapes"));
		CHECK_FALSE(Test.HasItem(Picture + "/Assets/Skies/Noon.hdr"));
		Test.Click(Picture + "/Assets/Textures/Wood.png");
		CHECK(GetPath(Test.Get<WEditorTestGadgetComponent>(Gadget).Picture.Get()) == "Textures/Wood.png");

		Test.Click(Picture + "/Choose");
		Test.Click(Picture + "/Assets/Textures/Wood.png#Linear");
		CHECK(GetPath(Test.Get<WEditorTestGadgetComponent>(Gadget).Picture.Get()) == "Textures/Wood.png#Linear");
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("An environment is chosen for a sky light, and a model that has none of a kind says so")
	{
		FAssetPickerTest Test;
		Test.WriteOneOfEach();
		const int64 Light = Test.AddSelected<WSkyLightComponent>();
		const std::string Environment = Row(Light, "Environment");

		Test.Click(Environment + "/Choose");
		REQUIRE(Test.HasItem(Environment + "/Assets/Skies/Noon.hdr"));
		CHECK_FALSE(Test.HasItem(Environment + "/Assets/Textures/Wood.png"));
		CHECK_FALSE(Test.HasItem(Environment + "/Assets/Models/Square.gltf"));
		Test.Click(Environment + "/Assets/Skies/Noon.hdr");
		CHECK(GetPath(Test.Get<WSkyLightComponent>(Light).Environment.Get()) == "Skies/Noon.hdr");

		// The square has no texture. Its list for a picture says that there is nothing, and offers nothing.
		const int64 Gadget = Test.AddSelected<WEditorTestGadgetComponent>();
		const std::string Picture = Row(Gadget, "Picture");
		Test.Click(Picture + "/Choose");
		Test.Click(Picture + "/Assets/Models/Square.gltf");
		CHECK(Test.HasItem(Picture + "/Assets/Models/Square.gltf/Nothing"));
		CHECK_FALSE(Test.HasItem(Picture + "/Assets/Models/Square.gltf#Materials/0"));
		CHECK(Test.Get<WEditorTestGadgetComponent>(Gadget).Picture == nullptr);
	}

	TEST_CASE("None takes the asset away that a property has")
	{
		FAssetPickerTest Test;
		const int64 Component = Test.AddSelected<WStaticMeshComponent>();
		const std::string Mesh = Row(Component, "Mesh");
		Test.Click(Mesh + "/Choose");
		Test.Click(Mesh + "/Assets/Engine/Shapes");
		Test.Click(Mesh + "/Assets/Engine/Shapes#Cube");
		REQUIRE(GetPath(Test.Get<WStaticMeshComponent>(Component).Mesh.Get()) == "Engine/Shapes#Cube");

		Test.Click(Mesh + "/Choose");
		Test.Click(Mesh + "/Assets/None");

		CHECK(Test.Get<WStaticMeshComponent>(Component).Mesh == nullptr);
		CHECK_FALSE(Test.HasItem(Mesh + "/Assets/None"));
	}

	TEST_CASE("Without a project the list has what the engine has built in, and nothing else")
	{
		FEditorUiTest Test(SurfaceWidth, SurfaceHeight);
		WEntity* const Entity = Test.Editor.GetScene().CreateEntity("Thing");
		const int64 Component = Entity->AddComponent<WStaticMeshComponent>()->GetId();
		const int64 Gadget = Entity->AddComponent<WEditorTestGadgetComponent>()->GetId();
		Test.Editor.SetSelection({Entity->GetId()});
		Test.DrawFrames(PanelSettlingFrameCount);

		Test.Click(Row(Component, "Mesh") + "/Choose");
		CHECK(Test.HasItem(Row(Component, "Mesh") + "/Assets/None"));
		CHECK(Test.HasItem(Row(Component, "Mesh") + "/Assets/Engine/Shapes"));
		Test.PressKey(EKey::Escape);
		CHECK_FALSE(Test.HasItem(Row(Component, "Mesh") + "/Assets/None"));

		// The engine has no picture built in: there is nothing to choose but no picture.
		Test.Click(Row(Gadget, "Picture") + "/Choose");
		CHECK(Test.HasItem(Row(Gadget, "Picture") + "/Assets/None"));
		CHECK_FALSE(Test.HasItem(Row(Gadget, "Picture") + "/Assets/Engine/Shapes"));
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("A model that cannot be read says why in its list, and offers nothing")
	{
		FAssetPickerTest Test;
		Test.WriteText("Models/Broken.gltf", "This is no model");
		Test.WriteSquare("Models/Square.gltf");
		const int64 Component = Test.AddSelected<WStaticMeshComponent>();
		const std::string Mesh = Row(Component, "Mesh");

		Test.Click(Mesh + "/Choose");
		REQUIRE(Test.HasItem(Mesh + "/Assets/Models/Broken.gltf"));
		Test.Click(Mesh + "/Assets/Models/Broken.gltf");

		CHECK(Test.HasItem(Mesh + "/Assets/Models/Broken.gltf/Error"));
		CHECK_FALSE(Test.HasItem(Mesh + "/Assets/Models/Broken.gltf/Nothing"));
		CHECK(Test.Get<WStaticMeshComponent>(Component).Mesh == nullptr);

		// The model beside it is none the worse for that.
		Test.Click(Mesh + "/Assets/Models/Square.gltf");
		Test.Click(Mesh + "/Assets/Models/Square.gltf#Meshes/0/Primitives/0");
		CHECK(GetPath(Test.Get<WStaticMeshComponent>(Component).Mesh.Get()) ==
			"Models/Square.gltf#Meshes/0/Primitives/0");
	}

	TEST_CASE("What the project has is asked again each time the list is opened")
	{
		FAssetPickerTest Test;
		const int64 Gadget = Test.AddSelected<WEditorTestGadgetComponent>();
		const std::string Picture = Row(Gadget, "Picture");

		Test.Click(Picture + "/Choose");
		CHECK_FALSE(Test.HasItem(Picture + "/Assets/Textures/Wood.png"));
		Test.PressKey(EKey::Escape);

		Test.WritePicture("Textures/Wood.png");
		Test.Click(Picture + "/Choose");
		CHECK(Test.HasItem(Picture + "/Assets/Textures/Wood.png"));
	}

	TEST_CASE("The path of an asset can still be typed, beside the button")
	{
		FAssetPickerTest Test;
		const int64 Component = Test.AddSelected<WStaticMeshComponent>();
		const std::string Mesh = Row(Component, "Mesh");
		// The field and the button share the room that the field had.
		CHECK(Test.RequireItem(Mesh + "/Choose").Left >= Test.RequireItem(Mesh).Right);
		CHECK(Test.RequireItem(Mesh + "/Choose").Top == doctest::Approx(Test.RequireItem(Mesh).Top));

		Test.Click(Mesh);
		Test.Type("Engine/Shapes#Plane");
		Test.PressKey(EKey::Enter);

		CHECK(GetPath(Test.Get<WStaticMeshComponent>(Component).Mesh.Get()) == "Engine/Shapes#Plane");
	}
}
