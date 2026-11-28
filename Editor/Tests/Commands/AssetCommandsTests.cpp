#include "Commands/TestAssetCommands.h"
#include "Core/FileHelper.h"
#include "Editor.h"
#include "Rendering/Material.h"
#include "Rendering/StaticMesh.h"
#include "Rendering/StaticMeshComponent.h"
#include "Rendering/TestGltfFiles.h"
#include "Rendering/Texture.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "TestEditorObjects.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <format>
#include <initializer_list>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

TEST_SUITE("Asset commands")
{
	TEST_CASE("The commands for the assets of a project fail while no project is open")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		const std::string ExpectedError = "No project is open, and assets belong to a project. Create one with "
										  "Project.New or open one with Project.Open.";

		for (const FCommandResult& Result : {
				 Editor.ExecuteCommand("Asset.Import", ParseTestJson(R"({"Source": "Wood.png"})")),
				 Editor.ExecuteCommand("Asset.Describe", ParseTestJson(R"({"File": "Wood.png"})")),
				 Editor.ExecuteCommand("Asset.Place", ParseTestJson(R"({"File": "Crate.glb"})")),
				 Editor.ExecuteCommand("Asset.CreateMaterial", ParseTestJson(R"({"Path": "Brick.wwmaterial"})")),
				 Editor.ExecuteCommand("Asset.GetProperties", ParseTestJson(R"({"Asset": "Brick.wwmaterial"})")),
			 })
		{
			CHECK_FALSE(Result.HasSucceeded());
			CHECK(Result.GetError() == ExpectedError);
		}
	}

	TEST_CASE("What the engine has built in is listed, described and used without a project")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;

		const FCommandResult List = Editor.ExecuteCommand("Asset.List", FJsonValue());
		REQUIRE(List.HasSucceeded());
		CHECK(WriteTestJson(List.GetValue()) == R"(["Engine/Shapes"])");

		const FCommandResult Description =
			Editor.ExecuteCommand("Asset.Describe", ParseTestJson(R"({"File": "Engine/Shapes"})"));
		REQUIRE(Description.HasSucceeded());
		CHECK(WriteTestJson(Description.GetValue()) ==
			R"({"File":"Engine/Shapes","Assets":[{"Path":"Engine/Shapes#Cube","Class":"WStaticMesh"},)"
			R"({"Path":"Engine/Shapes#Sphere","Class":"WStaticMesh"},)"
			R"({"Path":"Engine/Shapes#Plane","Class":"WStaticMesh"}],"Warnings":[]})");

		// The paths are what a component's property takes.
		WEntity* const Entity = Editor.GetScene().CreateEntity("Ball");
		const WStaticMeshComponent* const Component = Entity->AddComponent<WStaticMeshComponent>();
		REQUIRE(Editor
					.ExecuteCommand("Object.SetProperty",
						ParseTestJson(
							std::format(R"({{"Object": {}, "Property": "Mesh", "Value": "Engine/Shapes#Sphere"}})",
								Component->GetId())))
					.HasSucceeded());
		REQUIRE(Component->Mesh != nullptr);
		CHECK(Component->Mesh->GetAssetPath() == "Engine/Shapes#Sphere");

		// A model of the engine's cannot be placed like a model of the project: it is one mesh, not a scene.
		const FCommandResult Placed =
			Editor.ExecuteCommand("Asset.Place", ParseTestJson(R"({"File": "Engine/Shapes"})"));
		CHECK_FALSE(Placed.HasSucceeded());
	}

	TEST_CASE("Asset.Import copies a picture into a folder of the project and says which assets it is")
	{
		FAssetCommandTest Test;
		const std::filesystem::path Source = Test.WritePicture("Wood.png");

		const FCommandResult Result = Test.Import(Source, "Textures");

		INFO("Error: ", Result.GetError());
		REQUIRE(Result.HasSucceeded());
		CHECK(WriteTestJson(Result.GetValue()) ==
			R"({"File":"Textures/Wood.png","Assets":[{"Path":"Textures/Wood.png","Class":"WTexture"},)"
			R"({"Path":"Textures/Wood.png#Linear","Class":"WTexture"}],"Warnings":[]})");
		CHECK(std::filesystem::is_regular_file(Test.GetContentDirectory() / "Textures" / "Wood.png"));
		// The file was copied: the one that was imported is still where it was.
		CHECK(std::filesystem::is_regular_file(Source));
	}

	TEST_CASE("Asset.Import without a folder puts the file at the top of the content directory")
	{
		FAssetCommandTest Test;

		const FCommandResult Result = Test.Import(Test.WriteEnvironment("Evening.hdr"));

		INFO("Error: ", Result.GetError());
		REQUIRE(Result.HasSucceeded());
		CHECK(WriteTestJson(Result.GetValue()) ==
			R"({"File":"Evening.hdr","Assets":[{"Path":"Evening.hdr","Class":"WEnvironmentMap"}],"Warnings":[]})");
		CHECK(std::filesystem::is_regular_file(Test.GetContentDirectory() / "Evening.hdr"));
	}

	TEST_CASE("Asset.Import of a model lists its materials, textures and meshes")
	{
		FAssetCommandTest Test;

		const FCommandResult Result = Test.Import(Test.WriteSquare("Square.gltf"), "Models");

		INFO("Error: ", Result.GetError());
		REQUIRE(Result.HasSucceeded());
		CHECK(WriteTestJson(Result.GetValue()) ==
			R"({"File":"Models/Square.gltf","Assets":[{"Path":"Models/Square.gltf#Materials/0","Class":"WMaterial"},)"
			R"({"Path":"Models/Square.gltf#Meshes/0/Primitives/0","Class":"WStaticMesh"}],"Warnings":[]})");
	}

	TEST_CASE("Asset.Import of a .gltf file brings the files along that it keeps its data in")
	{
		FAssetCommandTest Test;
		const std::filesystem::path Source = Test.WriteSquareWithExternalFiles("Square.gltf");

		const FCommandResult Result = Test.Import(Source, "Models/Square");

		INFO("Error: ", Result.GetError());
		REQUIRE(Result.HasSucceeded());
		const std::filesystem::path Folder = Test.GetContentDirectory() / "Models" / "Square";
		CHECK(std::filesystem::is_regular_file(Folder / "Square.gltf"));
		CHECK(std::filesystem::is_regular_file(Folder / "Square.bin"));
		CHECK(std::filesystem::is_regular_file(Folder / "Textures" / "Paint.png"));

		const std::string Value = WriteTestJson(Result.GetValue());
		CHECK(Value.find(R"({"Path":"Models/Square/Square.gltf#Textures/0/Srgb","Class":"WTexture"})") !=
			std::string::npos);
		CHECK(Value.find(R"("Warnings":[])") != std::string::npos);
	}

	TEST_CASE("Asset.Import says why it cannot import a file, and leaves nothing in the project")
	{
		FAssetCommandTest Test;
		std::filesystem::path Source;
		std::string Folder;
		std::string ExpectedStart;
		std::filesystem::path NotLeftBehind;

		SUBCASE("A file that is not there")
		{
			Source = Test.GetSourceDirectory() / "Missing.png";
			ExpectedStart = "There is no file '";
			NotLeftBehind = Test.GetContentDirectory() / "Missing.png";
		}
		SUBCASE("A kind of file that is no asset")
		{
			Source = Test.WriteText("Notes.txt", "Some notes");
			ExpectedStart = "'" + FFileHelper::PathToUtf8(Source) +
				"' is not a kind of file that can be imported: a model is a .gltf or .glb file, a picture a .png, .jpg "
				"or .jpeg file, an environment a .hdr file, and a material a .wwmaterial file.";
			NotLeftBehind = Test.GetContentDirectory() / "Notes.txt";
		}
		SUBCASE("A material file that is not valid")
		{
			Source = Test.WriteText("Broken.wwmaterial", "[]");
			ExpectedStart = "The file was not imported: ";
			NotLeftBehind = Test.GetContentDirectory() / "Broken.wwmaterial";
		}
		SUBCASE("A folder that leads out of the content directory")
		{
			Source = Test.WritePicture("Wood.png");
			Folder = "../Outside";
			ExpectedStart = "The file cannot be imported into the folder \"../Outside\" as \"Wood.png\"";
			NotLeftBehind = Test.Directory.GetPath() / "Project" / "Outside" / "Wood.png";
		}
		SUBCASE("A picture that cannot be read")
		{
			Source = Test.WriteText("Broken.png", "This is not a picture");
			ExpectedStart = "The file was not imported: ";
			NotLeftBehind = Test.GetContentDirectory() / "Broken.png";
		}
		SUBCASE("A model that is not valid")
		{
			Source = Test.WriteText("Broken.gltf", "{ this is not glTF");
			ExpectedStart = "The file was not imported: ";
			NotLeftBehind = Test.GetContentDirectory() / "Broken.gltf";
		}
		SUBCASE("A model whose data is missing")
		{
			Source = Test.WriteText("Square.gltf", FTestGltf().Build(R"({"byteLength": 140, "uri": "Gone.bin"})"));
			ExpectedStart = "The file was not imported: ";
			NotLeftBehind = Test.GetContentDirectory() / "Square.gltf";
		}

		const FCommandResult Result = Test.Import(Source, Folder);

		CHECK_FALSE(Result.HasSucceeded());
		INFO("Error: ", Result.GetError());
		CHECK(Result.GetError().starts_with(ExpectedStart));
		CHECK_FALSE(std::filesystem::exists(NotLeftBehind));
	}

	TEST_CASE("Asset.Import refuses a model that keeps data outside its own directory")
	{
		FAssetCommandTest Test;
		const FTestGltfBuffer Buffer = MakeTestSquareBuffer();
		Test.WriteText(
			"Shared.bin", std::string_view(reinterpret_cast<const char*>(Buffer.Bytes.data()), Buffer.Bytes.size()));
		const std::filesystem::path Source =
			Test.WriteText("Model/Square.gltf", FTestGltf().Build(R"({"byteLength": 140, "uri": "../Shared.bin"})"));

		const FCommandResult Result = Test.Import(Source, "Models");

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError() ==
			"The file was not imported: it keeps data in \"../Shared.bin\", which is not beside or below it.");
		CHECK_FALSE(std::filesystem::exists(Test.GetContentDirectory() / "Models"));
	}

	TEST_CASE("Asset.Import does not replace a file the project has already")
	{
		FAssetCommandTest Test;
		const std::filesystem::path Source = Test.WriteSquareWithExternalFiles("Square.gltf");
		REQUIRE(Test.Import(Source, "Models").HasSucceeded());

		SUBCASE("The file itself")
		{
			const FCommandResult Result = Test.Import(Source, "Models");

			CHECK_FALSE(Result.HasSucceeded());
			CHECK(Result.GetError().starts_with("The file was not imported: the project already has a file '"));
		}
		SUBCASE("A file it would bring along")
		{
			// Another model that keeps its data in a file of the same name.
			const std::filesystem::path Other =
				Test.WriteText("Other.gltf", FTestGltf().Build(R"({"byteLength": 140, "uri": "Square.bin"})"));

			const FCommandResult Result = Test.Import(Other, "Models");

			CHECK_FALSE(Result.HasSucceeded());
			CHECK(Result.GetError().find("Square.bin") != std::string::npos);
			CHECK_FALSE(std::filesystem::exists(Test.GetContentDirectory() / "Models" / "Other.gltf"));
		}
	}

	TEST_CASE("Asset.List returns the asset files of the project and the built-in ones, in the order of their names")
	{
		FAssetCommandTest Test;
		CHECK(WriteTestJson(Test.Execute("Asset.List").GetValue()) == R"(["Engine/Shapes"])");

		REQUIRE(Test.Import(Test.WritePicture("Wood.png"), "Textures").HasSucceeded());
		REQUIRE(Test.Import(Test.WriteSquare("Square.gltf"), "Models").HasSucceeded());
		REQUIRE(Test.Import(Test.WriteEnvironment("Evening.hdr")).HasSucceeded());
		REQUIRE(Test.Import(Test.WriteMaterial("Red.wwmaterial"), "Materials").HasSucceeded());
		// A scene is a file of the content directory, but not an asset.
		REQUIRE(Test.Execute("Scene.Save", R"({"Path": "Scenes/Level.json"})").HasSucceeded());

		const FCommandResult Result = Test.Execute("Asset.List");

		REQUIRE(Result.HasSucceeded());
		CHECK(WriteTestJson(Result.GetValue()) ==
			R"(["Engine/Shapes","Evening.hdr","Materials/Red.wwmaterial","Models/Square.gltf","Textures/Wood.png"])");
	}

	TEST_CASE("Asset.Import of a material file says what it could not give the material, as a model does")
	{
		FAssetCommandTest Test;
		REQUIRE(Test.Import(Test.WritePicture("Wood.png"), "Textures").HasSucceeded());

		// The picture is in the project; the other texture is not, and is not brought along.
		const std::string Properties =
			R"({"BaseColorTexture": "Textures/Wood.png", "NormalTexture": "Textures/Bumps.png#Linear"})";
		const FCommandResult Result = Test.Import(Test.WriteMaterial("Crate.wwmaterial", Properties), "Materials");

		REQUIRE(Result.HasSucceeded());
		CHECK(WriteTestJson(Result.GetValue()) ==
			R"({"File":"Materials/Crate.wwmaterial",)"
			R"("Assets":[{"Path":"Materials/Crate.wwmaterial","Class":"WMaterial"}],)"
			R"("Warnings":["property \"NormalTexture\": the content directory has no file \"Textures/Bumps.png\""]})");

		// Asset.Describe says the same of it.
		const FCommandResult Described = Test.Execute("Asset.Describe", R"({"File": "Materials/Crate.wwmaterial"})");
		REQUIRE(Described.HasSucceeded());
		CHECK(Described.GetValue() == Result.GetValue());
	}

	TEST_CASE("Asset.Describe returns the assets of a file of the project")
	{
		FAssetCommandTest Test;
		REQUIRE(Test.Import(Test.WriteSquare("Square.gltf"), "Models").HasSucceeded());

		const FCommandResult Result = Test.Execute("Asset.Describe", R"({"File": "Models/Square.gltf"})");

		REQUIRE(Result.HasSucceeded());
		CHECK(WriteTestJson(Result.GetValue()) ==
			R"({"File":"Models/Square.gltf","Assets":[{"Path":"Models/Square.gltf#Materials/0","Class":"WMaterial"},)"
			R"({"Path":"Models/Square.gltf#Meshes/0/Primitives/0","Class":"WStaticMesh"}],"Warnings":[]})");

		const FCommandResult Missing = Test.Execute("Asset.Describe", R"({"File": "Models/Gone.gltf"})");
		CHECK_FALSE(Missing.HasSucceeded());
		CHECK(Missing.GetError() ==
			"The file cannot be described: the content directory has no file \"Models/Gone.gltf\".");
	}

	TEST_CASE("Asset.Place puts a model into the scene, with its meshes and materials")
	{
		FAssetCommandTest Test;
		REQUIRE(Test.Import(Test.WriteSquare("Square.gltf"), "Models").HasSucceeded());

		const FCommandResult Result = Test.Execute("Asset.Place", R"({"File": "Models/Square.gltf"})");

		INFO("Error: ", Result.GetError());
		REQUIRE(Result.HasSucceeded());
		CHECK(WriteTestJson(Result.GetValue()) == R"({"Id":1,"Warnings":[]})");
		const WEntity* const Root = Test.Editor.GetScene().FindEntityById(1);
		REQUIRE(Root != nullptr);
		CHECK(Root->GetName() == "Square");
		CHECK(Root->GetParent() == nullptr);
		const std::vector<WStaticMeshComponent*> Components =
			Test.Editor.GetScene().GetComponents<WStaticMeshComponent>();
		REQUIRE(Components.size() == 1);
		REQUIRE(Components[0]->Mesh != nullptr);
		CHECK(Components[0]->Mesh->GetAssetPath() == "Models/Square.gltf#Meshes/0/Primitives/0");
		REQUIRE(Components[0]->Material != nullptr);
		CHECK(Components[0]->Material->GetAssetPath() == "Models/Square.gltf#Materials/0");
	}

	TEST_CASE("Asset.Place attaches the model to the given entity, and can be undone")
	{
		FAssetCommandTest Test;
		REQUIRE(Test.Import(Test.WriteSquare("Square.gltf")).HasSucceeded());
		REQUIRE(Test.Execute("Entity.Create", R"({"Name": "Room"})").HasSucceeded());

		const FCommandResult Result = Test.Execute("Asset.Place", R"({"File": "Square.gltf", "Parent": 1})");

		INFO("Error: ", Result.GetError());
		REQUIRE(Result.HasSucceeded());
		const WEntity* const Room = Test.Editor.GetScene().FindEntityById(1);
		REQUIRE(Room != nullptr);
		REQUIRE(Room->GetChildren().size() == 1);
		CHECK(Room->GetChildren()[0]->GetName() == "Square");

		const std::optional<std::string> Undone = Test.Editor.Undo();
		REQUIRE(Undone.has_value());
		CHECK(*Undone == "Asset.Place");
		CHECK(Test.Editor.GetScene().GetAllEntities().size() == 1);
		CHECK(Test.Editor.GetScene().GetComponents<WStaticMeshComponent>().empty());
	}

	TEST_CASE("A scene with a placed model is saved and opened again with the model")
	{
		FAssetCommandTest Test;
		REQUIRE(Test.Import(Test.WriteSquareWithExternalFiles("Square.gltf"), "Models").HasSucceeded());
		REQUIRE(Test.Execute("Asset.Place", R"({"File": "Models/Square.gltf"})").HasSucceeded());
		REQUIRE(Test.Execute("Scene.Save", R"({"Path": "Scenes/Level.json"})").HasSucceeded());
		REQUIRE(Test.Execute("Scene.New").HasSucceeded());

		const FCommandResult Opened = Test.Execute("Scene.Open", R"({"Path": "Scenes/Level.json"})");

		INFO("Error: ", Opened.GetError());
		REQUIRE(Opened.HasSucceeded());
		CHECK(WriteTestJson(Opened.GetValue()) == R"({"Warnings":[]})");
		const std::vector<WStaticMeshComponent*> Components =
			Test.Editor.GetScene().GetComponents<WStaticMeshComponent>();
		REQUIRE(Components.size() == 1);
		REQUIRE(Components[0]->Mesh != nullptr);
		CHECK(Components[0]->Mesh->GetTriangleCount() == 2);
		REQUIRE(Components[0]->Material != nullptr);
		REQUIRE(Components[0]->Material->BaseColorTexture != nullptr);
		CHECK(Components[0]->Material->BaseColorTexture->GetAssetPath() == "Models/Square.gltf#Textures/0/Srgb");
	}

	TEST_CASE("Asset.Place says why it cannot place a file, and places nothing")
	{
		FAssetCommandTest Test;
		REQUIRE(Test.Import(Test.WriteSquare("Square.gltf")).HasSucceeded());
		REQUIRE(Test.Import(Test.WritePicture("Wood.png")).HasSucceeded());
		std::string ArgumentText;
		std::string ExpectedError;

		SUBCASE("A picture")
		{
			ArgumentText = R"({"File": "Wood.png"})";
			ExpectedError = "\"Wood.png\" cannot be placed: only a whole model, a .gltf or .glb file, is placed in a "
							"scene. Single assets are used through the properties of components.";
		}
		SUBCASE("A material file")
		{
			REQUIRE(Test.Import(Test.WriteMaterial("Red.wwmaterial")).HasSucceeded());
			ArgumentText = R"({"File": "Red.wwmaterial"})";
			ExpectedError = "\"Red.wwmaterial\" cannot be placed: only a whole model, a .gltf or .glb file, is placed "
							"in a scene. Single assets are used through the properties of components.";
		}
		SUBCASE("One asset of a model")
		{
			ArgumentText = R"({"File": "Square.gltf#Meshes/0/Primitives/0"})";
			ExpectedError = "\"Square.gltf#Meshes/0/Primitives/0\" cannot be placed: only a whole model, a .gltf or "
							".glb file, is placed in a scene. Single assets are used through the properties of "
							"components.";
		}
		SUBCASE("A model the project does not have")
		{
			ArgumentText = R"({"File": "Gone.gltf"})";
			ExpectedError = "The file was not placed: the content directory has no file \"Gone.gltf\".";
		}
		SUBCASE("Text that is not the name of a file")
		{
			ArgumentText = R"({"File": "../Square.gltf"})";
			ExpectedError = "The file cannot be placed: \"../Square.gltf\" is not the path of an asset: it must not "
							"have the part \"..\".";
		}
		SUBCASE("A parent that does not exist")
		{
			ArgumentText = R"({"File": "Square.gltf", "Parent": 99})";
			ExpectedError = "There is no entity with the identifier 99.";
		}

		const FCommandResult Result = Test.Execute("Asset.Place", ArgumentText);

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError() == ExpectedError);
		CHECK(Test.Editor.GetScene().GetAllEntities().empty());
		CHECK_FALSE(Test.Editor.GetHistory().CanUndo());
	}
}
