#include "Assets/EditedAssets.h"
#include "Assets/TestAssets.h"
#include "Assets/TestEditedAssets.h"
#include "Commands/CommandResult.h"
#include "Core/FileHelper.h"
#include "Editor.h"
#include "History/UndoHistory.h"
#include "Json/JsonFormat.h"
#include "Json/JsonValue.h"
#include "Json/JsonWriter.h"
#include "Logging/LogMessage.h"
#include "Logging/LogVerbosity.h"
#include "Object/ObjectGlobals.h"
#include "Object/WeakObjectPtr.h"
#include "Rendering/Material.h"
#include "Rendering/StaticMeshComponent.h"
#include "Rendering/Texture.h"
#include "Scene/Scene.h"
#include "Scene/SceneSerializer.h"
#include "TestEditorObjects.h"
#include "TestSupport/ScopedLogCapture.h"
#include "TestSupport/ScopedTempDirectory.h"

#include <doctest/doctest.h>

#include <cstddef>
#include <filesystem>
#include <format>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
	using FNames = std::vector<std::string>;

	constexpr std::string_view RedPath = "Materials/Red.wwmaterial";
	constexpr std::string_view WoodPath = "Materials/Wood.wwmaterial";

	/** Gives the editor a command with the arguments written as JSON text, which a test expects to succeed. */
	FJsonValue Run(FEditor& Editor, std::string_view CommandName, std::string_view ArgumentText = "{}")
	{
		const FCommandResult Result = Editor.ExecuteCommand(CommandName, ParseTestJson(ArgumentText));
		INFO("Command error: ", Result.GetError());
		REQUIRE(Result.HasSucceeded());
		return Result.GetValue();
	}

	/** Returns the scene as the text of a scene file. */
	std::string WriteScene(const FEditor& Editor)
	{
		return FSceneSerializer::SaveToText(Editor.GetScene());
	}

	/** Changes a property of a material as one step of the history, as the user interface does. */
	void ChangeInStep(FEditor& Editor,
		WMaterial& Material,
		std::string_view PropertyName,
		std::string_view ValueText,
		std::string StepName)
	{
		Editor.BeginStep(std::move(StepName));
		ChangeMaterial(Editor, Material, PropertyName, ValueText);
		Editor.EndStep();
	}

	/** Saves a material through the editor, which the test expects to write its file. */
	void SaveMaterial(FEditor& Editor, WMaterial& Material)
	{
		bool bWritten = false;
		std::string Error;
		const bool bIsSaved = Editor.GetEditedAssets().Save(Material, bWritten, Error);
		INFO("Error: ", Error);
		REQUIRE(bIsSaved);
		CHECK(bWritten);
	}

	/** Returns the warnings that have been logged, in their order. */
	FNames GetLoggedWarnings(const FScopedLogCapture& Capture)
	{
		FNames Warnings;
		for (const FLogMessage& Message : Capture.GetMessages())
		{
			if (Message.Verbosity == ELogVerbosity::Warning)
			{
				Warnings.push_back(Message.Text);
			}
		}
		return Warnings;
	}

	/** Returns the part of the history's current state after its first line break: the assets. Empty if none. */
	std::string GetAssetState(const FEditor& Editor)
	{
		const std::string& State = Editor.GetHistory().GetCurrentState();
		const std::size_t LineBreak = State.find('\n');
		return LineBreak == std::string::npos ? std::string() : State.substr(LineBreak + 1);
	}
}

TEST_SUITE("FEditor undo and redo of assets")
{
	TEST_CASE("A change to a material is a step, which undo takes back and redo brings again, on the same object")
	{
		const FScopedContentDirectory Content;
		Content.WriteMaterial(std::string(RedPath));
		const std::string FileText = ReadContentFile(Content, RedPath);
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		WMaterial& Material = LoadTestMaterial(RedPath);

		ChangeInStep(Editor, Material, "Roughness", "0.25", "Set Roughness");

		CHECK(Editor.GetHistory().GetUndoStepNames() == FNames{"Set Roughness"});
		CHECK(Editor.Undo() == std::optional<std::string>("Set Roughness"));
		CHECK(Material.Roughness == 0.5f);
		CHECK(&LoadTestMaterial(RedPath) == &Material);
		CHECK(Editor.Redo() == std::optional<std::string>("Set Roughness"));
		CHECK(Material.Roughness == 0.25f);
		// Undo and redo change the material in memory; its file is written only when it is saved.
		CHECK(ReadContentFile(Content, RedPath) == FileText);
	}

	TEST_CASE("A change made outside a step is the step of a change without a command at the next undo")
	{
		const FScopedContentDirectory Content;
		Content.WriteMaterial(std::string(RedPath));
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		WMaterial& Material = LoadTestMaterial(RedPath);

		ChangeMaterial(Editor, Material, "Metallic", "1.0");

		CHECK(Editor.Undo() == std::optional<std::string>(FEditor::DirectChangeStepName));
		CHECK(Material.Metallic == 0.0f);
	}

	TEST_CASE("Changes are undone last first, and redone in the order they were made")
	{
		const FScopedContentDirectory Content;
		Content.WriteMaterial(std::string(RedPath));
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		WMaterial& Material = LoadTestMaterial(RedPath);
		ChangeInStep(Editor, Material, "Roughness", "0.25", "First");
		ChangeInStep(Editor, Material, "Metallic", "1.0", "Second");
		ChangeInStep(Editor, Material, "Roughness", "0.75", "Third");

		CHECK(Editor.Undo() == std::optional<std::string>("Third"));
		CHECK(Material.Roughness == 0.25f);
		CHECK(Material.Metallic == 1.0f);
		CHECK(Editor.Undo() == std::optional<std::string>("Second"));
		CHECK(Material.Metallic == 0.0f);
		CHECK(Editor.Undo() == std::optional<std::string>("First"));
		CHECK(Material.Roughness == 0.5f);
		CHECK_FALSE(Editor.Undo().has_value());

		CHECK(Editor.Redo() == std::optional<std::string>("First"));
		CHECK(Material.Roughness == 0.25f);
		CHECK(Editor.Redo() == std::optional<std::string>("Second"));
		CHECK(Material.Metallic == 1.0f);
		CHECK(Editor.Redo() == std::optional<std::string>("Third"));
		CHECK(Material.Roughness == 0.75f);
	}

	TEST_CASE("A state is the scene's text alone while no asset differs, and names an asset that does")
	{
		const FScopedContentDirectory Content;
		Content.WriteMaterial(std::string(RedPath));
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		WMaterial& Material = LoadTestMaterial(RedPath);
		const std::string SceneText =
			FJsonWriter::Write(FSceneSerializer::SaveToJson(Editor.GetScene()), EJsonFormat::Compact);
		CHECK(Editor.GetHistory().GetCurrentState() == SceneText);

		ChangeInStep(Editor, Material, "Roughness", "0.25", "Set Roughness");

		const std::string& State = Editor.GetHistory().GetCurrentState();
		CHECK(State.starts_with(SceneText + "\n"));
		const FJsonValue Assets = ParseTestJson(GetAssetState(Editor));
		REQUIRE(Assets.IsObject());
		REQUIRE(Assets.GetMembers().size() == 1);
		CHECK(Assets.GetMembers()[0].first == RedPath);
		const FJsonValue* const Roughness = Assets.GetMembers()[0].second.FindMember("Roughness");
		REQUIRE(Roughness != nullptr);
		CHECK(Roughness->GetNumber() == 0.25);

		// Undone, the material is as it was, and is in the state no longer.
		REQUIRE(Editor.Undo().has_value());
		CHECK(Editor.GetHistory().GetCurrentState() == SceneText);
	}

	TEST_CASE("Steps of the scene and of a material are undone each by itself")
	{
		const FScopedContentDirectory Content;
		Content.WriteMaterial(std::string(RedPath));
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		WMaterial& Material = LoadTestMaterial(RedPath);
		const std::string Empty = WriteScene(Editor);
		Run(Editor, "Entity.Create", R"({"Name": "First"})");
		const std::string OneEntity = WriteScene(Editor);
		ChangeInStep(Editor, Material, "Metallic", "1.0", "Set Metallic");
		Run(Editor, "Entity.Create", R"({"Name": "Second"})");
		const std::string TwoEntities = WriteScene(Editor);

		CHECK(Editor.Undo() == std::optional<std::string>("Entity.Create"));
		CHECK(WriteScene(Editor) == OneEntity);
		CHECK(Material.Metallic == 1.0f);
		CHECK(Editor.Undo() == std::optional<std::string>("Set Metallic"));
		CHECK(WriteScene(Editor) == OneEntity);
		CHECK(Material.Metallic == 0.0f);
		CHECK(Editor.Undo() == std::optional<std::string>("Entity.Create"));
		CHECK(WriteScene(Editor) == Empty);
		CHECK(Material.Metallic == 0.0f);

		CHECK(Editor.Redo() == std::optional<std::string>("Entity.Create"));
		CHECK(WriteScene(Editor) == OneEntity);
		CHECK(Material.Metallic == 0.0f);
		CHECK(Editor.Redo() == std::optional<std::string>("Set Metallic"));
		CHECK(WriteScene(Editor) == OneEntity);
		CHECK(Material.Metallic == 1.0f);
		CHECK(Editor.Redo() == std::optional<std::string>("Entity.Create"));
		CHECK(WriteScene(Editor) == TwoEntities);
		CHECK(Material.Metallic == 1.0f);
		CHECK(&LoadTestMaterial(RedPath) == &Material);
	}

	TEST_CASE("A material that is changed and then given to a component is put back by undoing both")
	{
		const FScopedContentDirectory Content;
		Content.WriteMaterial(std::string(RedPath));
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		WMaterial& Material = LoadTestMaterial(RedPath);
		Run(Editor, "Entity.Create", R"({"Name": "Wall"})");
		Run(Editor, "Component.Add", R"({"Entity": 1, "Class": "WStaticMeshComponent"})");
		ChangeInStep(Editor, Material, "Metallic", "1.0", "Set Metallic");
		Run(Editor,
			"Object.SetProperty",
			std::format(R"({{"Object": 2, "Property": "Material", "Value": "{}"}})", RedPath));
		const auto GetComponent = [&Editor]() -> const WStaticMeshComponent&
		{
			const std::vector<WStaticMeshComponent*> Components =
				Editor.GetScene().GetComponents<WStaticMeshComponent>();
			REQUIRE(Components.size() == 1);
			return *Components[0];
		};
		CHECK(GetComponent().Material.Get() == &Material);

		CHECK(Editor.Undo() == std::optional<std::string>("Object.SetProperty"));
		CHECK(GetComponent().Material == nullptr);
		CHECK(Material.Metallic == 1.0f);
		CHECK(Editor.Undo() == std::optional<std::string>("Set Metallic"));
		CHECK(Material.Metallic == 0.0f);

		// Both again: the component that redo brings back refers to the same material.
		REQUIRE(Editor.Redo().has_value());
		REQUIRE(Editor.Redo().has_value());
		CHECK(GetComponent().Material.Get() == &Material);
		CHECK(Material.Metallic == 1.0f);
	}

	TEST_CASE("A change after an undo drops what could have been redone")
	{
		const FScopedContentDirectory Content;
		Content.WriteMaterial(std::string(RedPath));
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		WMaterial& Material = LoadTestMaterial(RedPath);
		ChangeInStep(Editor, Material, "Metallic", "1.0", "Set Metallic");
		REQUIRE(Editor.Undo().has_value());

		ChangeInStep(Editor, Material, "Roughness", "0.0", "Set Roughness");

		CHECK_FALSE(Editor.GetHistory().CanRedo());
		CHECK(Editor.GetHistory().GetUndoStepNames() == FNames{"Set Roughness"});
		CHECK(Material.Metallic == 0.0f);
	}

	TEST_CASE("Changes to two materials and to the scene in one step are undone as one")
	{
		const FScopedContentDirectory Content;
		Content.WriteMaterial(std::string(RedPath));
		Content.WriteMaterial(std::string(WoodPath));
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		WMaterial& Red = LoadTestMaterial(RedPath);
		WMaterial& Wood = LoadTestMaterial(WoodPath);
		const std::string SceneBefore = WriteScene(Editor);

		Editor.BeginStep("Paint");
		ChangeMaterial(Editor, Red, "Metallic", "1.0");
		Run(Editor, "Entity.Create", R"({"Name": "Easel"})");
		ChangeMaterial(Editor, Wood, "Roughness", "0.125");
		Editor.EndStep();
		const std::string SceneAfter = WriteScene(Editor);

		CHECK(Editor.GetHistory().GetUndoStepNames() == FNames{"Paint"});
		CHECK(Editor.Undo() == std::optional<std::string>("Paint"));
		CHECK(Red.Metallic == 0.0f);
		CHECK(Wood.Roughness == 0.5f);
		CHECK(WriteScene(Editor) == SceneBefore);
		CHECK(Editor.Redo() == std::optional<std::string>("Paint"));
		CHECK(Red.Metallic == 1.0f);
		CHECK(Wood.Roughness == 0.125f);
		CHECK(WriteScene(Editor) == SceneAfter);
	}

	TEST_CASE("An undo while a step is open ends the step and undoes it")
	{
		const FScopedContentDirectory Content;
		Content.WriteMaterial(std::string(RedPath));
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		WMaterial& Material = LoadTestMaterial(RedPath);
		Editor.BeginStep("Drag");
		ChangeMaterial(Editor, Material, "Roughness", "0.3");
		ChangeMaterial(Editor, Material, "Roughness", "0.2");

		CHECK(Editor.Undo() == std::optional<std::string>("Drag"));

		CHECK_FALSE(Editor.IsStepOpen());
		CHECK(Material.Roughness == 0.5f);
	}

	TEST_CASE("A step in which a value went away and came back is no step")
	{
		const FScopedContentDirectory Content;
		Content.WriteMaterial(std::string(RedPath));
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		WMaterial& Material = LoadTestMaterial(RedPath);

		Editor.BeginStep("Drag");
		ChangeMaterial(Editor, Material, "Roughness", "0.3");
		ChangeMaterial(Editor, Material, "Roughness", "0.5");
		Editor.EndStep();

		CHECK_FALSE(Editor.GetHistory().CanUndo());
		CHECK_FALSE(Editor.GetEditedAssets().HasUnsavedChanges(RedPath));
	}

	TEST_CASE("A change that is refused, and a command that only reads, are no steps")
	{
		const FScopedContentDirectory Content;
		Content.WriteMaterial(std::string(RedPath));
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		WMaterial& Material = LoadTestMaterial(RedPath);
		std::string Error;

		CHECK_FALSE(Editor.GetEditedAssets().SetProperty(
			Material, FindMaterialProperty("Metallic"), ParseTestJson(R"("very")"), Error));
		Run(Editor, "Asset.List");

		CHECK_FALSE(Editor.Undo().has_value());
	}

	TEST_CASE("A change made by hand to a material that the editor keeps is a step of its own at the next undo")
	{
		const FScopedContentDirectory Content;
		Content.WriteMaterial(std::string(RedPath));
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		WMaterial& Material = LoadTestMaterial(RedPath);
		ChangeInStep(Editor, Material, "Metallic", "1.0", "Set Metallic");

		Material.Roughness = 0.125f;

		CHECK(Editor.Undo() == std::optional<std::string>(FEditor::DirectChangeStepName));
		CHECK(Material.Roughness == 0.5f);
		CHECK(Material.Metallic == 1.0f);
		CHECK(Editor.Undo() == std::optional<std::string>("Set Metallic"));
		CHECK(Material.Metallic == 0.0f);
	}

	TEST_CASE("A collection between a change, its undo and its redo changes nothing")
	{
		const FScopedContentDirectory Content;
		Content.WriteMaterial(std::string(RedPath));
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		const TWeakObjectPtr<WMaterial> Material = &LoadTestMaterial(RedPath);
		ChangeInStep(Editor, *Material.Get(), "Metallic", "1.0", "Set Metallic");

		CollectGarbage();
		REQUIRE(Editor.Undo().has_value());
		CollectGarbage();
		REQUIRE(Material.IsValid());
		CHECK(Material.Get()->Metallic == 0.0f);
		REQUIRE(Editor.Redo().has_value());
		CollectGarbage();

		REQUIRE(Material.IsValid());
		CHECK(Material.Get()->Metallic == 1.0f);
		CHECK(&LoadTestMaterial(RedPath) == Material.Get());
	}

	TEST_CASE("A material whose only user is deleted and collected comes back with it, changed as it was")
	{
		const FScopedContentDirectory Content;
		Content.WriteMaterial(std::string(RedPath));
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		Run(Editor, "Entity.Create", R"({"Name": "Wall"})");
		Run(Editor, "Component.Add", R"({"Entity": 1, "Class": "WStaticMeshComponent"})");
		Run(Editor,
			"Object.SetProperty",
			std::format(R"({{"Object": 2, "Property": "Material", "Value": "{}"}})", RedPath));
		const TWeakObjectPtr<WMaterial> Material = &LoadTestMaterial(RedPath);
		ChangeInStep(Editor, *Material.Get(), "Metallic", "1.0", "Set Metallic");

		Run(Editor, "Entity.Delete", R"({"Entity": 1})");
		CollectGarbage();
		REQUIRE(Material.IsValid());

		CHECK(Editor.Undo() == std::optional<std::string>("Entity.Delete"));
		const std::vector<WStaticMeshComponent*> Components = Editor.GetScene().GetComponents<WStaticMeshComponent>();
		REQUIRE(Components.size() == 1);
		CHECK(Components[0]->Material.Get() == Material.Get());
		CHECK(Material.Get()->Metallic == 1.0f);
		CHECK(Editor.Undo() == std::optional<std::string>("Set Metallic"));
		CHECK(Components[0]->Material.Get() == Material.Get());
		CHECK(Material.Get()->Metallic == 0.0f);
	}

	TEST_CASE("Undo after a save makes a material unsaved again, and undoing back to its file makes it saved")
	{
		const FScopedContentDirectory Content;
		Content.WriteMaterial(std::string(RedPath));
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		const FEditedAssets& Assets = Editor.GetEditedAssets();
		WMaterial& Material = LoadTestMaterial(RedPath);

		SUBCASE("Saved, then undone and redone")
		{
			ChangeInStep(Editor, Material, "Metallic", "1.0", "Set Metallic");
			SaveMaterial(Editor, Material);
			CHECK_FALSE(Assets.HasUnsavedChanges(RedPath));

			REQUIRE(Editor.Undo().has_value());
			CHECK(Assets.HasUnsavedChanges(RedPath));
			REQUIRE(Editor.Redo().has_value());
			CHECK_FALSE(Assets.HasUnsavedChanges(RedPath));
		}
		SUBCASE("Not saved, then undone")
		{
			ChangeInStep(Editor, Material, "Metallic", "1.0", "Set Metallic");
			CHECK(Assets.HasUnsavedChanges(RedPath));

			REQUIRE(Editor.Undo().has_value());
			CHECK_FALSE(Assets.HasUnsavedChanges(RedPath));
			CHECK(Assets.GetUnsaved().empty());
		}
	}

	TEST_CASE("A history that starts anew keeps unsaved materials as they are, and lets saved ones go")
	{
		const FScopedContentDirectory Content;
		Content.WriteMaterial(std::string(RedPath));
		Content.WriteMaterial(std::string(WoodPath));
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		const FEditedAssets& Assets = Editor.GetEditedAssets();
		const TWeakObjectPtr<WMaterial> Red = &LoadTestMaterial(RedPath);
		const TWeakObjectPtr<WMaterial> Wood = &LoadTestMaterial(WoodPath);
		ChangeInStep(Editor, *Red.Get(), "Metallic", "1.0", "Set Metallic");
		ChangeInStep(Editor, *Wood.Get(), "Roughness", "0.125", "Set Roughness");
		SaveMaterial(Editor, *Wood.Get());

		SUBCASE("Scene.New")
		{
			Run(Editor, "Scene.New");
		}
		SUBCASE("Scene.Open")
		{
			const std::string ScenePath = FFileHelper::PathToUtf8(Content.GetPath() / "Level.wwscene");
			FJsonValue Arguments = FJsonValue::MakeObject();
			Arguments.SetMember("Path", ScenePath);
			REQUIRE(Editor.ExecuteCommand("Scene.Save", Arguments).HasSucceeded());
			REQUIRE(Editor.ExecuteCommand("Scene.Open", Arguments).HasSucceeded());
		}
		SUBCASE("SetScene")
		{
			Editor.SetScene(*NewObject<WScene>(), std::filesystem::path());
		}

		CHECK_FALSE(Editor.GetHistory().CanUndo());
		CHECK(Assets.IsKept(RedPath));
		CHECK(Assets.GetUnsaved() == FNames{std::string(RedPath)});
		CHECK_FALSE(Assets.IsKept(WoodPath));
		CollectGarbage();
		REQUIRE(Red.IsValid());
		CHECK(Red.Get()->Metallic == 1.0f);
		CHECK_FALSE(Wood.IsValid());

		// The first step of the new history is undone to what the material was when the scene was replaced.
		ChangeInStep(Editor, *Red.Get(), "Roughness", "0.25", "Set Roughness");
		REQUIRE(Editor.Undo().has_value());
		CHECK(Red.Get()->Roughness == 0.5f);
		CHECK(Red.Get()->Metallic == 1.0f);
		CHECK_FALSE(Editor.GetHistory().CanUndo());

		// What is not saved can still be saved.
		SaveMaterial(Editor, *Red.Get());
		CHECK(Assets.GetUnsaved().empty());
	}

	TEST_CASE("Opening a project gives unsaved changes up, with a warning, and the material is read from its file")
	{
		const FScopedTempDirectory Directory;
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		const std::string ProjectDirectory = FFileHelper::PathToUtf8(Directory.GetPath() / "Game");
		FJsonValue NewArguments = FJsonValue::MakeObject();
		NewArguments.SetMember("Directory", ProjectDirectory);
		NewArguments.SetMember("Name", "Game");
		REQUIRE(Editor.ExecuteCommand("Project.New", NewArguments).HasSucceeded());
		Run(Editor, "Asset.CreateMaterial", std::format(R"({{"Path": "{}"}})", RedPath));
		const std::filesystem::path File =
			Directory.GetPath() / "Game" / "Content" / FFileHelper::PathFromUtf8(RedPath);
		std::string FileText;
		std::string Error;
		REQUIRE(FFileHelper::LoadFileToString(File, FileText, Error));
		ChangeInStep(Editor, LoadTestMaterial(RedPath), "Metallic", "1.0", "Set Metallic");
		const FScopedLogCapture Capture;

		FJsonValue OpenArguments = FJsonValue::MakeObject();
		OpenArguments.SetMember("Path", ProjectDirectory);
		REQUIRE(Editor.ExecuteCommand("Project.Open", OpenArguments).HasSucceeded());

		CHECK(GetLoggedWarnings(Capture) ==
			FNames{"Changes to these assets were not saved and are given up: Materials/Red.wwmaterial"});
		CHECK(Editor.GetEditedAssets().GetUnsaved().empty());
		std::string TextAfter;
		REQUIRE(FFileHelper::LoadFileToString(File, TextAfter, Error));
		CHECK(TextAfter == FileText);
		CHECK(LoadTestMaterial(RedPath).Metallic == 0.0f);
	}

	TEST_CASE("A value that can no longer be put back is a warning, and the history says what there is")
	{
		const FScopedContentDirectory Content;
		Content.WritePicture("Textures/Wood.png");
		Content.WriteMaterial(std::string(RedPath), R"({"BaseColorTexture": "Textures/Wood.png"})");
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		WMaterial& Material = LoadTestMaterial(RedPath);
		REQUIRE(Material.BaseColorTexture != nullptr);
		Editor.BeginStep("Strip");
		ChangeMaterial(Editor, Material, "BaseColorTexture", "null");
		ChangeMaterial(Editor, Material, "Metallic", "1.0");
		Editor.EndStep();
		// Nothing uses the picture any longer, and its file is gone too.
		std::filesystem::remove(Content.GetPath() / "Textures" / "Wood.png");
		CollectGarbage();
		const FScopedLogCapture Capture;

		CHECK(Editor.Undo() == std::optional<std::string>("Strip"));

		CHECK(GetLoggedWarnings(Capture) ==
			FNames{
				"Putting \"Materials/Red.wwmaterial\" back: property \"BaseColorTexture\": the content directory has "
				"no file \"Textures/Wood.png\""});
		CHECK(Material.BaseColorTexture == nullptr);
		CHECK(Material.Metallic == 0.0f);
		// The material is not as it was when the history began, and the current state says so.
		const FJsonValue Assets = ParseTestJson(GetAssetState(Editor));
		const FJsonValue* const Red = Assets.FindMember(RedPath);
		REQUIRE(Red != nullptr);
		REQUIRE(Red->FindMember("BaseColorTexture") != nullptr);
		CHECK(Red->FindMember("BaseColorTexture")->IsNull());

		// Redo puts back what can be put back.
		CHECK(Editor.Redo() == std::optional<std::string>("Strip"));
		CHECK(Material.Metallic == 1.0f);
	}
}
