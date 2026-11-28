#include "Assets/AssetManager.h"
#include "Assets/EditedAssets.h"
#include "Commands/TestAssetCommands.h"
#include "Core/FileHelper.h"
#include "Editor.h"
#include "History/UndoHistory.h"
#include "Math/LinearColor.h"
#include "Object/Class.h"
#include "Object/ObjectGlobals.h"
#include "Reflection/Property.h"
#include "Rendering/Material.h"
#include "Rendering/MaterialSerializer.h"
#include "Rendering/StaticMeshComponent.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/SceneSerializer.h"
#include "TestEditorObjects.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <format>
#include <ostream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace
{
	/**
	 * Returns every file and folder of the project's content directory, by its path below it with forward slashes,
	 * in the order of the paths.
	 */
	std::vector<std::string> ListContent(const FAssetCommandTest& Test)
	{
		std::vector<std::string> Entries;
		for (const std::filesystem::directory_entry& Entry :
			std::filesystem::recursive_directory_iterator(Test.GetContentDirectory()))
		{
			const std::u8string Relative =
				Entry.path().lexically_relative(Test.GetContentDirectory()).generic_u8string();
			Entries.emplace_back(Relative.begin(), Relative.end());
		}
		std::ranges::sort(Entries);
		return Entries;
	}

	/** Returns the text of a file of the content directory. */
	std::string ReadContentFile(const FAssetCommandTest& Test, const std::string& File)
	{
		std::string Text;
		std::string Error;
		INFO("Error: ", Error);
		REQUIRE(
			FFileHelper::LoadFileToString(Test.GetContentDirectory() / FFileHelper::PathFromUtf8(File), Text, Error));
		return Text;
	}

	/** Gives the editor Asset.CreateMaterial for a path. */
	FCommandResult CreateMaterial(FAssetCommandTest& Test, const std::string& Path)
	{
		FJsonValue Arguments = FJsonValue::MakeObject();
		Arguments.SetMember("Path", Path);
		return Test.Editor.ExecuteCommand("Asset.CreateMaterial", Arguments);
	}

	/** Gives the editor Asset.GetProperties for an asset path. */
	FCommandResult GetProperties(FAssetCommandTest& Test, const std::string& AssetPath)
	{
		FJsonValue Arguments = FJsonValue::MakeObject();
		Arguments.SetMember("Asset", AssetPath);
		return Test.Editor.ExecuteCommand("Asset.GetProperties", Arguments);
	}

	/** The saved properties of a new material, as compact JSON. */
	constexpr std::string_view NewMaterialProperties =
		R"({"BaseColor":[1.0,1.0,1.0,1.0],"Metallic":0.0,"Roughness":0.5,"EmissiveColor":[0.0,0.0,0.0,1.0],)"
		R"("BaseColorTexture":null,"MetallicRoughnessTexture":null,"NormalTexture":null,"NormalScale":1.0,)"
		R"("EmissiveTexture":null,"OcclusionTexture":null,"OcclusionStrength":1.0})";
}

TEST_SUITE("Asset commands for materials")
{
	TEST_CASE("Asset.CreateMaterial writes the file of a new material and returns it as Asset.Import does")
	{
		FAssetCommandTest Test;
		const std::string SceneBefore = FSceneSerializer::SaveToText(Test.Editor.GetScene());
		const std::vector<std::string> StepsBefore = Test.Editor.GetHistory().GetUndoStepNames();

		const FCommandResult Result = CreateMaterial(Test, "Materials/Metals/Brick.wwmaterial");

		INFO("Error: ", Result.GetError());
		REQUIRE(Result.HasSucceeded());
		CHECK(WriteTestJson(Result.GetValue()) ==
			R"({"File":"Materials/Metals/Brick.wwmaterial",)"
			R"("Assets":[{"Path":"Materials/Metals/Brick.wwmaterial","Class":"WMaterial"}],"Warnings":[]})");
		// The folders are made, and the file says what a new material is.
		CHECK(ReadContentFile(Test, "Materials/Metals/Brick.wwmaterial") ==
			FMaterialSerializer::SaveToText(*NewObject<WMaterial>()));

		// It is an asset of the project like any other.
		const FCommandResult List = Test.Execute("Asset.List");
		REQUIRE(List.HasSucceeded());
		CHECK(WriteTestJson(List.GetValue()) == R"(["Engine/Shapes","Materials/Metals/Brick.wwmaterial"])");
		const FCommandResult Described =
			Test.Execute("Asset.Describe", R"({"File": "Materials/Metals/Brick.wwmaterial"})");
		REQUIRE(Described.HasSucceeded());
		CHECK(Described.GetValue() == Result.GetValue());

		// Writing a file is no change to the scene, and no step of the history.
		CHECK(FSceneSerializer::SaveToText(Test.Editor.GetScene()) == SceneBefore);
		CHECK(Test.Editor.GetHistory().GetUndoStepNames() == StepsBefore);
	}

	TEST_CASE("The path that Asset.CreateMaterial returns is what the Material of a mesh takes")
	{
		FAssetCommandTest Test;
		REQUIRE(CreateMaterial(Test, "Materials/Brick.wwmaterial").HasSucceeded());
		WEntity* const Entity = Test.Editor.GetScene().CreateEntity("Wall");
		REQUIRE(Entity != nullptr);
		const WStaticMeshComponent* const Component = Entity->AddComponent<WStaticMeshComponent>();
		REQUIRE(Component != nullptr);

		const FCommandResult Result = Test.Execute("Object.SetProperty",
			std::format(R"({{"Object": {}, "Property": "Material", "Value": "Materials/Brick.wwmaterial"}})",
				Component->GetId()));

		INFO("Error: ", Result.GetError());
		REQUIRE(Result.HasSucceeded());
		REQUIRE(Component->Material != nullptr);
		CHECK(Component->Material->GetAssetPath() == "Materials/Brick.wwmaterial");
		CHECK(Component->Material->BaseColor == FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
		CHECK(Component->Material->Roughness == 0.5f);
	}

	TEST_CASE("Asset.CreateMaterial takes the ending of a material file in any case of letters")
	{
		FAssetCommandTest Test;

		const FCommandResult Result = CreateMaterial(Test, "Loud.WWMATERIAL");

		INFO("Error: ", Result.GetError());
		REQUIRE(Result.HasSucceeded());
		CHECK(WriteTestJson(Result.GetValue()) ==
			R"({"File":"Loud.WWMATERIAL","Assets":[{"Path":"Loud.WWMATERIAL","Class":"WMaterial"}],"Warnings":[]})");
		CHECK(ListContent(Test) == std::vector<std::string>{"Loud.WWMATERIAL"});
	}

	TEST_CASE("Asset.CreateMaterial says why it cannot make a material, and leaves the project as it was")
	{
		FAssetCommandTest Test;
		REQUIRE(Test.Import(Test.WriteMaterial("Red.wwmaterial", R"({"Metallic": 1.0})"), "Materials").HasSucceeded());
		const std::string RedText = ReadContentFile(Test, "Materials/Red.wwmaterial");
		const std::vector<std::string> ContentBefore = ListContent(Test);
		std::string Path;
		std::string Expected;

		SUBCASE("A path that leads out of the content directory")
		{
			Path = "../Brick.wwmaterial";
			Expected = "The material was not created: \"../Brick.wwmaterial\" is not the path of an asset: it must not "
					   "have the part \"..\".";
		}
		SUBCASE("A path with a key")
		{
			Path = "Materials/Brick.wwmaterial#Base";
			Expected = "The material was not created: \"Materials/Brick.wwmaterial#Base\" names an asset inside a "
					   "file, and a material is a file of its own.";
		}
		SUBCASE("A name without the ending")
		{
			Path = "Materials/Brick";
			Expected = "The material was not created: the name of a material file ends in \".wwmaterial\", as in "
					   "\"Materials/Brick.wwmaterial\", and \"Materials/Brick\" does not.";
		}
		SUBCASE("A name with another ending")
		{
			Path = "Materials/Brick.png";
			Expected = "The material was not created: the name of a material file ends in \".wwmaterial\", as in "
					   "\"Materials/Brick.wwmaterial\", and \"Materials/Brick.png\" does not.";
		}
		SUBCASE("A file that is there")
		{
			Path = "Materials/Red.wwmaterial";
			Expected = "The material was not created: the project already has a file \"Materials/Red.wwmaterial\".";
		}

		const FCommandResult Result = CreateMaterial(Test, Path);

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError() == Expected);
		CHECK(ListContent(Test) == ContentBefore);
		CHECK(ReadContentFile(Test, "Materials/Red.wwmaterial") == RedText);
	}

	TEST_CASE("Asset.CreateMaterial says why it cannot write the file where a file is in the way of a folder")
	{
		FAssetCommandTest Test;
		REQUIRE(Test.Import(Test.WritePicture("Wood.png")).HasSucceeded());
		const std::vector<std::string> ContentBefore = ListContent(Test);

		const FCommandResult Result = CreateMaterial(Test, "Wood.png/Brick.wwmaterial");

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError().starts_with("The material was not created: cannot "));
		CHECK(ListContent(Test) == ContentBefore);
	}

	TEST_CASE("Asset.CreateMaterial refuses a path whose material is still in use although its file is gone")
	{
		FAssetCommandTest Test;
		REQUIRE(CreateMaterial(Test, "Materials/Brick.wwmaterial").HasSucceeded());
		// A component of the scene keeps the material in use.
		WEntity* const Entity = Test.Editor.GetScene().CreateEntity("Wall");
		REQUIRE(Entity != nullptr);
		const WStaticMeshComponent* const Component = Entity->AddComponent<WStaticMeshComponent>();
		REQUIRE(Component != nullptr);
		REQUIRE(
			Test.Execute("Object.SetProperty",
					std::format(R"({{"Object": {}, "Property": "Material", "Value": "Materials/Brick.wwmaterial"}})",
						Component->GetId()))
				.HasSucceeded());
		CollectGarbage();
		std::filesystem::remove(Test.GetContentDirectory() / "Materials" / "Brick.wwmaterial");
		REQUIRE(FAssetManager::FindLoaded("Materials/Brick.wwmaterial") != nullptr);

		const FCommandResult Result = CreateMaterial(Test, "Materials/Brick.wwmaterial");

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError() ==
			"The material was not created: \"Materials/Brick.wwmaterial\" is still in use although its file is gone.");
		CHECK(ListContent(Test) == std::vector<std::string>{"Materials"});

		// Once nothing uses it, the path is free.
		Entity->Destroy();
		CollectGarbage();
		CHECK(CreateMaterial(Test, "Materials/Brick.wwmaterial").HasSucceeded());
	}

	TEST_CASE("Asset.CreateMaterial refuses a folder that the project spells otherwise, where the file system takes "
			  "the one for the other")
	{
		FAssetCommandTest Test;
		REQUIRE(CreateMaterial(Test, "Materials/Red.wwmaterial").HasSucceeded());
		// Whether the file system takes one spelling for another is asked of the directory, so that the test holds
		// on every file system.
		std::error_code ErrorCode;
		const bool bIgnoresCapitals = std::filesystem::exists(Test.GetContentDirectory() / "MATERIALS", ErrorCode);
		CAPTURE(bIgnoresCapitals);

		const FCommandResult Result = CreateMaterial(Test, "materials/Metals/New.wwmaterial");

		if (bIgnoresCapitals)
		{
			// Refused before anything is written: not even the folder that is not there is made.
			CHECK_FALSE(Result.HasSucceeded());
			CHECK(Result.GetError() ==
				"The material was not created: the content directory has no folder \"materials\", only one whose name "
				"is spelt differently.");
			CHECK(ListContent(Test) == std::vector<std::string>{"Materials", "Materials/Red.wwmaterial"});
		}
		else
		{
			// Elsewhere it is another folder.
			CHECK(Result.HasSucceeded());
			CHECK(ListContent(Test) ==
				std::vector<std::string>{"Materials",
					"Materials/Red.wwmaterial",
					"materials",
					"materials/Metals",
					"materials/Metals/New.wwmaterial"});
		}
	}

	TEST_CASE("Asset.GetProperties returns the path, the class, the values and the warnings of a material")
	{
		FAssetCommandTest Test;
		REQUIRE(CreateMaterial(Test, "Materials/Brick.wwmaterial").HasSucceeded());

		const FCommandResult Result = GetProperties(Test, "Materials/Brick.wwmaterial");

		INFO("Error: ", Result.GetError());
		REQUIRE(Result.HasSucceeded());
		CHECK(WriteTestJson(Result.GetValue()) ==
			std::format(R"({{"Path":"Materials/Brick.wwmaterial","Class":"WMaterial","Unsaved":false,)"
						R"("Properties":{},"Warnings":[]}})",
				NewMaterialProperties));
	}

	TEST_CASE("Asset.GetProperties says what a material file could not give its material")
	{
		FAssetCommandTest Test;
		REQUIRE(Test.Import(Test.WritePicture("Wood.png"), "Textures").HasSucceeded());
		REQUIRE(Test.Import(Test.WriteMaterial("Crate.wwmaterial",
								R"({"BaseColorTexture": "Textures/Wood.png", "NormalTexture": "Textures/Gone.png"})"),
						"Materials")
					.HasSucceeded());

		const FCommandResult Result = GetProperties(Test, "Materials/Crate.wwmaterial");

		REQUIRE(Result.HasSucceeded());
		const FJsonValue& Properties = *Result.GetValue().FindMember("Properties");
		CHECK(*Properties.FindMember("BaseColorTexture") == FJsonValue("Textures/Wood.png"));
		CHECK(Properties.FindMember("NormalTexture")->IsNull());
		CHECK(WriteTestJson(*Result.GetValue().FindMember("Warnings")) ==
			R"(["property \"NormalTexture\": the content directory has no file \"Textures/Gone.png\""])");
	}

	TEST_CASE("Asset.GetProperties reads an asset that cannot be changed, as a material of a model")
	{
		FAssetCommandTest Test;
		REQUIRE(Test.Import(Test.WriteSquare("Square.gltf"), "Models").HasSucceeded());

		const FCommandResult ModelMaterial = GetProperties(Test, "Models/Square.gltf#Materials/0");

		REQUIRE(ModelMaterial.HasSucceeded());
		CHECK(*ModelMaterial.GetValue().FindMember("Path") == FJsonValue("Models/Square.gltf#Materials/0"));
		CHECK(*ModelMaterial.GetValue().FindMember("Class") == FJsonValue("WMaterial"));
		CHECK(*ModelMaterial.GetValue().FindMember("Unsaved") == FJsonValue(false));
	}

	TEST_CASE("Asset.GetProperties reads a shape that the engine has built in without a project")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		const FCommandResult Cube =
			Editor.ExecuteCommand("Asset.GetProperties", ParseTestJson(R"({"Asset": "Engine/Shapes#Cube"})"));
		INFO("Error: ", Cube.GetError());
		REQUIRE(Cube.HasSucceeded());
		CHECK(WriteTestJson(Cube.GetValue()) ==
			R"({"Path":"Engine/Shapes#Cube","Class":"WStaticMesh","Unsaved":false,"Properties":{},"Warnings":[]})");
	}

	TEST_CASE("Asset.GetProperties says that a material that the editor has changed is not saved")
	{
		FAssetCommandTest Test;
		REQUIRE(CreateMaterial(Test, "Materials/Brick.wwmaterial").HasSucceeded());
		std::string Error;
		WMaterial* const Material = FAssetManager::Load<WMaterial>("Materials/Brick.wwmaterial", Error);
		REQUIRE(Material != nullptr);
		const FProperty* const Metallic = WMaterial::StaticClass()->FindProperty("Metallic");
		REQUIRE(Metallic != nullptr);
		REQUIRE(Test.Editor.GetEditedAssets().SetProperty(*Material, *Metallic, FJsonValue(1.0), Error));

		const FCommandResult Result = GetProperties(Test, "Materials/Brick.wwmaterial");

		REQUIRE(Result.HasSucceeded());
		CHECK(*Result.GetValue().FindMember("Unsaved") == FJsonValue(true));
		CHECK(*Result.GetValue().FindMember("Properties")->FindMember("Metallic") == FJsonValue(1.0));
	}

	TEST_CASE("Asset.GetProperties says why it cannot read an asset")
	{
		FAssetCommandTest Test;
		REQUIRE(Test.Import(Test.WriteSquare("Square.gltf"), "Models").HasSucceeded());
		std::string AssetPath;
		std::string Expected;

		SUBCASE("Text that is no asset path")
		{
			AssetPath = "../Brick.wwmaterial";
			Expected =
				"The asset could not be loaded: \"../Brick.wwmaterial\" is not the path of an asset: it must not "
				"have the part \"..\".";
		}
		SUBCASE("A file that is not there")
		{
			AssetPath = "Materials/Gone.wwmaterial";
			Expected =
				"The asset could not be loaded: the content directory has no file \"Materials/Gone.wwmaterial\".";
		}
		SUBCASE("A key that the file does not have")
		{
			AssetPath = "Models/Square.gltf#Materials/7";
			Expected = "The asset could not be loaded: the file \"Models/Square.gltf\" has no asset \"Materials/7\".";
		}

		const FCommandResult Result = GetProperties(Test, AssetPath);

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError() == Expected);
	}
}
