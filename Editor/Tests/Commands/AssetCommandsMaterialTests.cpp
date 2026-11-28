#include "Assets/AssetManager.h"
#include "Assets/EditedAssets.h"
#include "Commands/CommandResult.h"
#include "Commands/TestAssetCommands.h"
#include "Core/FileHelper.h"
#include "Editor.h"
#include "History/UndoHistory.h"
#include "Json/JsonValue.h"
#include "Math/LinearColor.h"
#include "Object/Class.h"
#include "Object/ObjectGlobals.h"
#include "Reflection/Property.h"
#include "Rendering/Material.h"
#include "Rendering/MaterialSerializer.h"
#include "Rendering/StaticMesh.h"
#include "Rendering/StaticMeshComponent.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/SceneSerializer.h"
#include "TestEditorObjects.h"
#include "TestSupport/ScopedTempDirectory.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <format>
#include <initializer_list>
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

	/** Gives the editor Asset.SetProperty for a property of an asset, with the value as JSON text. */
	FCommandResult SetProperty(
		FAssetCommandTest& Test, const std::string& AssetPath, const std::string& Property, std::string_view ValueText)
	{
		FJsonValue Arguments = FJsonValue::MakeObject();
		Arguments.SetMember("Asset", AssetPath);
		Arguments.SetMember("Property", Property);
		Arguments.SetMember("Value", ParseTestJson(ValueText));
		return Test.Editor.ExecuteCommand("Asset.SetProperty", Arguments);
	}

	/** Gives the editor Asset.Save for an asset path. */
	FCommandResult Save(FAssetCommandTest& Test, const std::string& AssetPath)
	{
		FJsonValue Arguments = FJsonValue::MakeObject();
		Arguments.SetMember("Asset", AssetPath);
		return Test.Editor.ExecuteCommand("Asset.Save", Arguments);
	}

	/** Returns one saved property of an asset as Asset.GetProperties gives it, which a test expects to succeed. */
	FJsonValue GetPropertyValue(FAssetCommandTest& Test, const std::string& AssetPath, std::string_view Property)
	{
		const FCommandResult Result = GetProperties(Test, AssetPath);
		INFO("Error: ", Result.GetError());
		REQUIRE(Result.HasSucceeded());
		const FJsonValue* const Value = Result.GetValue().FindMember("Properties")->FindMember(Property);
		REQUIRE(Value != nullptr);
		return *Value;
	}

	/** Returns whether Asset.GetProperties says that an asset has changes that are not saved. */
	bool IsUnsaved(FAssetCommandTest& Test, const std::string& AssetPath)
	{
		const FCommandResult Result = GetProperties(Test, AssetPath);
		INFO("Error: ", Result.GetError());
		REQUIRE(Result.HasSucceeded());
		return Result.GetValue().FindMember("Unsaved")->GetBoolean();
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
		SUBCASE("A folder whose name Windows does not keep")
		{
			Path = "New./Brick.wwmaterial";
			Expected = "The material was not created: the name \"New.\" ends in a period, which Windows does not keep.";
		}
		SUBCASE("Such a folder below one that is not there")
		{
			Path = "New/Sub./Brick.wwmaterial";
			Expected = "The material was not created: the name \"Sub.\" ends in a period, which Windows does not keep.";
		}
		SUBCASE("A folder whose name ends in a space, below one that is not there")
		{
			Path = "New/Sub /Brick.wwmaterial";
			Expected = "The material was not created: the name \"Sub \" ends in a space, which Windows does not keep.";
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

	TEST_CASE("Asset.CreateMaterial that cannot write its file leaves no folder behind")
	{
		FAssetCommandTest Test;
		REQUIRE(Test.Import(Test.WritePicture("Wood.png")).HasSucceeded());
		const std::vector<std::string> ContentBefore = ListContent(Test);

		// No file system takes a name of three hundred letters, so the file cannot be written into the folders that
		// were made for it.
		const FCommandResult Result = CreateMaterial(Test, "New/Deeper/" + std::string(300, 'a') + ".wwmaterial");

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError().starts_with("The material was not created: "));
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
		// Another spelling of it is the same file where capitals are not told apart, and is refused everywhere.
		const FCommandResult OtherSpelling = CreateMaterial(Test, "Materials/brick.wwmaterial");
		CHECK(OtherSpelling.GetError() ==
			"The material was not created: \"Materials/brick.wwmaterial\" differs only in capitals from "
			"\"Materials/Brick.wwmaterial\", which is still in use although its file is gone.");
		CHECK(ListContent(Test) == std::vector<std::string>{"Materials"});

		// Once nothing uses it, the path is free.
		Entity->Destroy();
		CollectGarbage();
		CHECK(CreateMaterial(Test, "Materials/Brick.wwmaterial").HasSucceeded());
	}

	TEST_CASE("A material in use with its file gone is found under capitals beyond A to Z, as Windows takes them")
	{
		FAssetCommandTest Test;
		// O with two dots, as a capital and as a small letter.
		const std::string CapitalPath = "Materials/\xC3\x96l.wwmaterial";
		const std::string SmallPath = "Materials/\xC3\xB6l.wwmaterial";
		REQUIRE(CreateMaterial(Test, CapitalPath).HasSucceeded());
		REQUIRE(SetProperty(Test, CapitalPath, "Metallic", "1.0").HasSucceeded());
		std::filesystem::remove(Test.GetContentDirectory() / FFileHelper::PathFromUtf8(CapitalPath));

		const FCommandResult Result = CreateMaterial(Test, SmallPath);

#if defined(_WIN32)
		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError() ==
			std::format("The material was not created: \"{}\" differs only in capitals from \"{}\", which is still in "
						"use although its file is gone.",
				SmallPath,
				CapitalPath));
#else
		// Elsewhere only A to Z are compared without their case, and this is another file.
		CHECK(Result.HasSucceeded());
#endif
	}

	TEST_CASE("Every loaded spelling of a path is looked at, where the file system tells capitals apart")
	{
		FAssetCommandTest Test;
		REQUIRE(CreateMaterial(Test, "Materials/brick.wwmaterial").HasSucceeded());
		std::error_code ErrorCode;
		const bool bIgnoresCapitals =
			std::filesystem::exists(Test.GetContentDirectory() / "MATERIALS" / "BRICK.WWMATERIAL", ErrorCode);
		CAPTURE(bIgnoresCapitals);
		if (!bIgnoresCapitals)
		{
			// Two files whose names differ only in capitals, both kept by the editor, and the file of one of them gone.
			REQUIRE(CreateMaterial(Test, "Materials/BRICK.wwmaterial").HasSucceeded());
			REQUIRE(SetProperty(Test, "Materials/brick.wwmaterial", "Metallic", "1.0").HasSucceeded());
			REQUIRE(SetProperty(Test, "Materials/BRICK.wwmaterial", "Metallic", "1.0").HasSucceeded());
			std::filesystem::remove(Test.GetContentDirectory() / "Materials" / "BRICK.wwmaterial");

			const FCommandResult Result = CreateMaterial(Test, "Materials/Brick.wwmaterial");

			CHECK(Result.GetError() ==
				"The material was not created: \"Materials/Brick.wwmaterial\" differs only in capitals from "
				"\"Materials/BRICK.wwmaterial\", which is still in use although its file is gone.");
		}
	}
	TEST_CASE("Asset.Import of a material file is refused where a material of that path is in use and its file is gone")
	{
		FAssetCommandTest Test;
		REQUIRE(CreateMaterial(Test, "Materials/Brick.wwmaterial").HasSucceeded());
		// Changed, the material is kept by the editor with nothing else using it, and its file goes.
		REQUIRE(SetProperty(Test, "Materials/Brick.wwmaterial", "Metallic", "1.0").HasSucceeded());
		std::filesystem::remove(Test.GetContentDirectory() / "Materials" / "Brick.wwmaterial");
		std::string Name;
		std::string Expected;

		SUBCASE("The same name")
		{
			Name = "Brick.wwmaterial";
			Expected = "The file was not imported: \"Materials/Brick.wwmaterial\" is still in use although its file is "
					   "gone.";
		}
		SUBCASE("Another spelling of it")
		{
			Name = "BRICK.wwmaterial";
			Expected = "The file was not imported: \"Materials/BRICK.wwmaterial\" differs only in capitals from "
					   "\"Materials/Brick.wwmaterial\", which is still in use although its file is gone.";
		}

		const FCommandResult Result = Test.Import(Test.WriteMaterial(Name, R"({"Roughness": 0.0})"), "Materials");

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError() == Expected);
		CHECK(ListContent(Test) == std::vector<std::string>{"Materials"});
		CHECK(GetPropertyValue(Test, "Materials/Brick.wwmaterial", "Metallic") == FJsonValue(1.0));

		// Once nothing keeps or uses the old material, the file is imported.
		REQUIRE(Test.Execute("Edit.Undo").HasSucceeded());
		REQUIRE(Test.Execute("Scene.New").HasSucceeded());
		CollectGarbage();
		CHECK(Test.Import(Test.WriteMaterial(Name, R"({"Roughness": 0.0})"), "Materials").HasSucceeded());
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

	TEST_CASE("Asset.SetProperty changes a material in memory, and Asset.Save writes it to its file")
	{
		FAssetCommandTest Test;
		REQUIRE(CreateMaterial(Test, "Materials/Brick.wwmaterial").HasSucceeded());
		const std::string NewText = ReadContentFile(Test, "Materials/Brick.wwmaterial");

		const FCommandResult Changed = SetProperty(Test, "Materials/Brick.wwmaterial", "Metallic", "1.0");

		INFO("Error: ", Changed.GetError());
		REQUIRE(Changed.HasSucceeded());
		CHECK(Changed.GetValue().IsNull());
		CHECK(GetPropertyValue(Test, "Materials/Brick.wwmaterial", "Metallic") == FJsonValue(1.0));
		CHECK(IsUnsaved(Test, "Materials/Brick.wwmaterial"));
		CHECK(ReadContentFile(Test, "Materials/Brick.wwmaterial") == NewText);

		const FCommandResult Saved = Save(Test, "Materials/Brick.wwmaterial");

		REQUIRE(Saved.HasSucceeded());
		CHECK(WriteTestJson(Saved.GetValue()) == R"({"Path":"Materials/Brick.wwmaterial","Written":true})");
		CHECK_FALSE(IsUnsaved(Test, "Materials/Brick.wwmaterial"));
		std::string Error;
		const WMaterial* const Material = FAssetManager::Load<WMaterial>("Materials/Brick.wwmaterial", Error);
		REQUIRE(Material != nullptr);
		CHECK(ReadContentFile(Test, "Materials/Brick.wwmaterial") == FMaterialSerializer::SaveToText(*Material));
		CHECK(Material->Metallic == 1.0f);

		// Nothing more to write.
		const FCommandResult Again = Save(Test, "Materials/Brick.wwmaterial");
		REQUIRE(Again.HasSucceeded());
		CHECK(WriteTestJson(Again.GetValue()) == R"({"Path":"Materials/Brick.wwmaterial","Written":false})");
	}

	TEST_CASE("Asset.SetProperty is a step of the history, which Edit.Undo takes back in memory")
	{
		FAssetCommandTest Test;
		REQUIRE(CreateMaterial(Test, "Materials/Brick.wwmaterial").HasSucceeded());
		REQUIRE(SetProperty(Test, "Materials/Brick.wwmaterial", "Roughness", "0.25").HasSucceeded());
		CHECK(Test.Editor.GetHistory().GetUndoStepNames() == std::vector<std::string>{"Asset.SetProperty"});

		const FCommandResult Undone = Test.Execute("Edit.Undo");

		REQUIRE(Undone.HasSucceeded());
		CHECK(WriteTestJson(Undone.GetValue()) == R"({"Undone":"Asset.SetProperty"})");
		CHECK(GetPropertyValue(Test, "Materials/Brick.wwmaterial", "Roughness") == FJsonValue(0.5));
		CHECK_FALSE(IsUnsaved(Test, "Materials/Brick.wwmaterial"));
		REQUIRE(Test.Execute("Edit.Redo").HasSucceeded());
		CHECK(GetPropertyValue(Test, "Materials/Brick.wwmaterial", "Roughness") == FJsonValue(0.25));
	}

	TEST_CASE("Asset.SetProperty to the value that a material has is no step, and inside a step is part of it")
	{
		FAssetCommandTest Test;
		REQUIRE(CreateMaterial(Test, "Materials/Brick.wwmaterial").HasSucceeded());

		REQUIRE(SetProperty(Test, "Materials/Brick.wwmaterial", "Roughness", "0.5").HasSucceeded());
		CHECK_FALSE(Test.Editor.GetHistory().CanUndo());
		CHECK_FALSE(IsUnsaved(Test, "Materials/Brick.wwmaterial"));

		Test.Editor.BeginStep("Paint");
		REQUIRE(SetProperty(Test, "Materials/Brick.wwmaterial", "Roughness", "0.25").HasSucceeded());
		REQUIRE(SetProperty(Test, "Materials/Brick.wwmaterial", "Metallic", "1.0").HasSucceeded());
		Test.Editor.EndStep();
		CHECK(Test.Editor.GetHistory().GetUndoStepNames() == std::vector<std::string>{"Paint"});
	}

	TEST_CASE("Everything that uses a material shows what Asset.SetProperty gives it")
	{
		FAssetCommandTest Test;
		REQUIRE(CreateMaterial(Test, "Materials/Brick.wwmaterial").HasSucceeded());
		std::vector<const WStaticMeshComponent*> Components;
		for (const std::string_view Name : {"Left", "Right"})
		{
			WEntity* const Entity = Test.Editor.GetScene().CreateEntity(std::string(Name));
			REQUIRE(Entity != nullptr);
			const WStaticMeshComponent* const Component = Entity->AddComponent<WStaticMeshComponent>();
			REQUIRE(Component != nullptr);
			REQUIRE(Test.Execute("Object.SetProperty",
							std::format(
								R"({{"Object": {}, "Property": "Material", "Value": "Materials/Brick.wwmaterial"}})",
								Component->GetId()))
						.HasSucceeded());
			Components.push_back(Component);
		}

		REQUIRE(SetProperty(Test, "Materials/Brick.wwmaterial", "BaseColor", "[1, 0, 0, 1]").HasSucceeded());

		for (const WStaticMeshComponent* const Component : Components)
		{
			REQUIRE(Component->Material != nullptr);
			CHECK(Component->Material->BaseColor == FLinearColor(1.0f, 0.0f, 0.0f, 1.0f));
		}
		CHECK(Components[0]->Material.Get() == Components[1]->Material.Get());
	}

	TEST_CASE("Asset.SetProperty says why it cannot change a material, and changes nothing")
	{
		FAssetCommandTest Test;
		REQUIRE(Test.Import(Test.WriteSquare("Square.gltf"), "Models").HasSucceeded());
		REQUIRE(Test.Import(Test.WritePicture("Wood.png"), "Textures").HasSucceeded());
		REQUIRE(CreateMaterial(Test, "Materials/Brick.wwmaterial").HasSucceeded());
		REQUIRE(CreateMaterial(Test, "Materials/Other.wwmaterial").HasSucceeded());
		const std::string BrickText = ReadContentFile(Test, "Materials/Brick.wwmaterial");
		const std::string CannotBeChanged =
			"\"{}\" cannot be changed: it is what its file says. Only a material that is a file of its own, whose "
			"name ends in \".wwmaterial\", is changed and saved by the editor.";
		std::string AssetPath = "Materials/Brick.wwmaterial";
		std::string Property = "Metallic";
		std::string ValueText = "1.0";
		std::string Expected;

		SUBCASE("A material of a model")
		{
			AssetPath = "Models/Square.gltf#Materials/0";
			Expected = std::vformat(CannotBeChanged, std::make_format_args(AssetPath));
		}
		SUBCASE("A picture")
		{
			AssetPath = "Textures/Wood.png";
			Expected = std::vformat(CannotBeChanged, std::make_format_args(AssetPath));
		}
		SUBCASE("A material of a model that is not there: the name is enough")
		{
			AssetPath = "Models/Gone.gltf#Materials/0";
			Expected = std::vformat(CannotBeChanged, std::make_format_args(AssetPath));
		}
		SUBCASE("A shape that the engine has built in")
		{
			AssetPath = "Engine/Shapes#Cube";
			Expected = std::vformat(CannotBeChanged, std::make_format_args(AssetPath));
		}
		SUBCASE("Text that is no asset path")
		{
			AssetPath = "../Brick.wwmaterial";
			Expected =
				"The asset could not be loaded: \"../Brick.wwmaterial\" is not the path of an asset: it must not "
				"have the part \"..\".";
		}
		SUBCASE("A material file that is not there")
		{
			AssetPath = "Materials/Gone.wwmaterial";
			Expected =
				"The asset could not be loaded: the content directory has no file \"Materials/Gone.wwmaterial\".";
		}
		SUBCASE("A property that a material does not have")
		{
			Property = "Colour";
			Expected = "The class WMaterial has no property \"Colour\".";
		}
		SUBCASE("A property that is not saved")
		{
			Property = "Package";
			ValueText = "null";
			Expected =
				"The property \"Package\" of the class WMaterial is not saved, and commands neither read nor change "
				"such properties.";
		}
		SUBCASE("Text for a number")
		{
			ValueText = R"("shiny")";
			Expected =
				"The property \"Metallic\" of the class WMaterial was not changed: expected a number but found a "
				"string.";
		}
		SUBCASE("A texture that the project does not have")
		{
			Property = "BaseColorTexture";
			ValueText = R"("Textures/Gone.png")";
			Expected =
				"The property \"BaseColorTexture\" of the class WMaterial was not changed: the content directory "
				"has no file \"Textures/Gone.png\".";
		}
		SUBCASE("A material for a texture")
		{
			Property = "BaseColorTexture";
			ValueText = R"("Materials/Other.wwmaterial")";
			Expected =
				"The property \"BaseColorTexture\" of the class WMaterial was not changed: the reference is to a "
				"WMaterial, which is not a WTexture.";
		}

		const FCommandResult Result = SetProperty(Test, AssetPath, Property, ValueText);

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError() == Expected);
		CHECK_FALSE(Test.Editor.GetHistory().CanUndo());
		CHECK(Test.Editor.GetEditedAssets().GetUnsaved().empty());
		CHECK(GetPropertyValue(Test, "Materials/Brick.wwmaterial", "Metallic") == FJsonValue(0.0));
		CHECK(ReadContentFile(Test, "Materials/Brick.wwmaterial") == BrickText);
	}

	TEST_CASE("Asset.Save refuses what the editor does not save, and writes a file that is gone")
	{
		FAssetCommandTest Test;
		REQUIRE(Test.Import(Test.WriteSquare("Square.gltf"), "Models").HasSucceeded());
		REQUIRE(CreateMaterial(Test, "Materials/Brick.wwmaterial").HasSucceeded());

		const FCommandResult Refused = Save(Test, "Models/Square.gltf#Materials/0");
		CHECK_FALSE(Refused.HasSucceeded());
		CHECK(Refused.GetError() ==
			"\"Models/Square.gltf#Materials/0\" cannot be changed: it is what its file says. Only a material that is a "
			"file of its own, whose name ends in \".wwmaterial\", is changed and saved by the editor.");

		// A material whose file was removed while it was in use is written again, with what it is.
		REQUIRE(SetProperty(Test, "Materials/Brick.wwmaterial", "Metallic", "1.0").HasSucceeded());
		std::filesystem::remove(Test.GetContentDirectory() / "Materials" / "Brick.wwmaterial");
		const FCommandResult Saved = Save(Test, "Materials/Brick.wwmaterial");
		REQUIRE(Saved.HasSucceeded());
		CHECK(WriteTestJson(Saved.GetValue()) == R"({"Path":"Materials/Brick.wwmaterial","Written":true})");
		CHECK(ReadContentFile(Test, "Materials/Brick.wwmaterial").find("\"Metallic\": 1.0") != std::string::npos);
	}

	TEST_CASE("Asset.Save says why it cannot write a material's file, which keeps its changes")
	{
		FAssetCommandTest Test;
		REQUIRE(CreateMaterial(Test, "Materials/Brick.wwmaterial").HasSucceeded());
		REQUIRE(SetProperty(Test, "Materials/Brick.wwmaterial", "Metallic", "1.0").HasSucceeded());
		// A folder with something in it has taken the place of the file.
		std::filesystem::remove(Test.GetContentDirectory() / "Materials" / "Brick.wwmaterial");
		std::filesystem::create_directories(Test.GetContentDirectory() / "Materials" / "Brick.wwmaterial" / "Inside");

		const FCommandResult Result = Save(Test, "Materials/Brick.wwmaterial");

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError().starts_with(
			"The material \"Materials/Brick.wwmaterial\" was not saved: cannot replace '"));
		CHECK(IsUnsaved(Test, "Materials/Brick.wwmaterial"));
		CHECK(GetPropertyValue(Test, "Materials/Brick.wwmaterial", "Metallic") == FJsonValue(1.0));
	}

	TEST_CASE("An agent makes a red metal, gives it to a cube, saves both, and another editor finds them so")
	{
		const FScopedEditorTest EditorTest;
		const FScopedTempDirectory Directory;
		const std::filesystem::path ProjectDirectory = Directory.GetPath() / "Game";
		{
			FEditor Editor;
			const auto Run = [&Editor](std::string_view CommandName, const std::string& ArgumentText)
			{
				const FCommandResult Result = Editor.ExecuteCommand(CommandName, ParseTestJson(ArgumentText));
				INFO(CommandName, ": ", Result.GetError());
				REQUIRE(Result.HasSucceeded());
				// The worst the collector can do between two messages of an agent.
				CollectGarbage();
				return Result.GetValue();
			};
			FJsonValue NewProject = FJsonValue::MakeObject();
			NewProject.SetMember("Directory", FFileHelper::PathToUtf8(ProjectDirectory));
			NewProject.SetMember("Name", "Game");
			REQUIRE(Editor.ExecuteCommand("Project.New", NewProject).HasSucceeded());

			Run("Asset.CreateMaterial", R"({"Path": "Materials/RedMetal.wwmaterial"})");
			Run("Asset.SetProperty",
				R"({"Asset": "Materials/RedMetal.wwmaterial", "Property": "BaseColor", "Value": [1, 0, 0, 1]})");
			Run("Asset.SetProperty",
				R"({"Asset": "Materials/RedMetal.wwmaterial", "Property": "Metallic", "Value": 1})");
			Run("Asset.SetProperty",
				R"({"Asset": "Materials/RedMetal.wwmaterial", "Property": "Roughness", "Value": 0.25})");
			Run("Asset.Save", R"({"Asset": "Materials/RedMetal.wwmaterial"})");
			const int64 Cube = Run("Entity.Create", R"({"Name": "Cube"})").FindMember("Id")->GetInteger();
			const int64 Component =
				Run("Component.Add", std::format(R"({{"Entity": {}, "Class": "WStaticMeshComponent"}})", Cube))
					.FindMember("Id")
					->GetInteger();
			Run("Object.SetProperty",
				std::format(R"({{"Object": {}, "Property": "Mesh", "Value": "Engine/Shapes#Cube"}})", Component));
			Run("Object.SetProperty",
				std::format(R"({{"Object": {}, "Property": "Material", "Value": "Materials/RedMetal.wwmaterial"}})",
					Component));
			const FJsonValue Properties =
				*Run("Asset.GetProperties", R"({"Asset": "Materials/RedMetal.wwmaterial"})").FindMember("Properties");
			CHECK(*Properties.FindMember("Metallic") == FJsonValue(1.0));
			Run("Scene.Save", R"({"Path": "Scenes/Level.wwscene"})");
		}

		FEditor Reader;
		FJsonValue OpenProject = FJsonValue::MakeObject();
		OpenProject.SetMember("Path", FFileHelper::PathToUtf8(ProjectDirectory));
		REQUIRE(Reader.ExecuteCommand("Project.Open", OpenProject).HasSucceeded());
		REQUIRE(
			Reader.ExecuteCommand("Scene.Open", ParseTestJson(R"({"Path": "Scenes/Level.wwscene"})")).HasSucceeded());

		const std::vector<WStaticMeshComponent*> Components = Reader.GetScene().GetComponents<WStaticMeshComponent>();
		REQUIRE(Components.size() == 1);
		REQUIRE(Components[0]->Material != nullptr);
		CHECK(Components[0]->Material->GetAssetPath() == "Materials/RedMetal.wwmaterial");
		CHECK(Components[0]->Material->BaseColor == FLinearColor(1.0f, 0.0f, 0.0f, 1.0f));
		CHECK(Components[0]->Material->Metallic == 1.0f);
		CHECK(Components[0]->Material->Roughness == 0.25f);
		REQUIRE(Components[0]->Mesh != nullptr);
		CHECK(Components[0]->Mesh->GetAssetPath() == "Engine/Shapes#Cube");
	}
}
