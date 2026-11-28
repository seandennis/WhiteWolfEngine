#include "Editor.h"

#include "Assets/AssetManager.h"
#include "Commands/CommandResult.h"
#include "Commands/TestAssetCommands.h"
#include "Core/FileHelper.h"
#include "Json/JsonValue.h"
#include "Logging/LogMessage.h"
#include "Logging/LogVerbosity.h"
#include "Object/ObjectGlobals.h"
#include "Object/WeakObjectPtr.h"
#include "Play/PlayState.h"
#include "Project/Project.h"
#include "Rendering/Material.h"
#include "Rendering/SceneViewMode.h"
#include "Rendering/StaticMesh.h"
#include "Rendering/StaticMeshComponent.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/SceneSerializer.h"
#include "TestEditorObjects.h"
#include "TestSupport/ScopedLogCapture.h"
#include "TestSupport/ScopedTempDirectory.h"
#include "Viewport/GizmoMode.h"
#include "Viewport/GizmoSettings.h"
#include "Viewport/ViewportCamera.h"

#include <doctest/doctest.h>

#include <algorithm>
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
	/** Gives the editor a command with arguments written as JSON text, which the test expects to succeed. */
	FJsonValue Execute(FEditor& Editor, std::string_view Name, std::string_view ArgumentText = "{}")
	{
		const FCommandResult Result = Editor.ExecuteCommand(Name, ParseTestJson(ArgumentText));
		INFO("Command ", std::string(Name), ": ", Result.GetError());
		REQUIRE(Result.HasSucceeded());
		return Result.GetValue();
	}

	/** Gives the editor a command that makes an entity or a component, and returns the identifier it returned. */
	int64 ExecuteForId(FEditor& Editor, std::string_view Name, std::string_view ArgumentText)
	{
		const FJsonValue Value = Execute(Editor, Name, ArgumentText);
		const FJsonValue* const Id = Value.FindMember("Id");
		REQUIRE(Id != nullptr);
		return Id->GetInteger();
	}

	/** Creates an entity with a mover through commands, in the scene the editor shows, and returns its identifier. */
	int64 CreateMover(FEditor& Editor, std::string_view Name)
	{
		const int64 EntityId = ExecuteForId(Editor, "Entity.Create", std::format(R"({{"Name": "{}"}})", Name));
		Execute(Editor,
			"Component.Add",
			std::format(R"({{"Entity": {}, "Class": "WEditorTestMoverComponent"}})", EntityId));
		return EntityId;
	}

	/** Returns where an entity of the scene that the editor shows is, relative to its parent. */
	FVector GetLocation(const FEditor& Editor, int64 EntityId)
	{
		const WEntity* const Entity = Editor.GetScene().FindEntityById(EntityId);
		REQUIRE(Entity != nullptr);
		return Entity->GetRelativeTransform().Location;
	}

	/** Returns the scene that the editor shows as the text of a scene file. */
	std::string WriteShownScene(const FEditor& Editor)
	{
		return FSceneSerializer::SaveToText(Editor.GetScene());
	}

	/** Returns the text of a file of the project's content, which the test expects to be there. */
	std::string ReadContentText(const FAssetCommandTest& Test, std::string_view File)
	{
		std::string Text;
		std::string Error;
		const bool bIsRead =
			FFileHelper::LoadFileToString(Test.GetContentDirectory() / FFileHelper::PathFromUtf8(File), Text, Error);
		INFO("Error: ", Error);
		REQUIRE(bIsRead);
		return Text;
	}

	/** What the editor says when it refuses a command during a game. */
	std::string RefusedDuringGame(std::string_view CommandName)
	{
		return std::format("{} cannot be given while a game plays, because it would act on what waits unchanged until "
						   "the game stops: the scene being edited, its file, its history, the project or a material. "
						   "Play.Stop stops the game.",
			CommandName);
	}

	/** Returns true if the log has a message of the category LogEditor that is exactly Text. */
	bool HasEditorMessage(const FScopedLogCapture& Capture, ELogVerbosity Verbosity, std::string_view Text)
	{
		const std::vector<FLogMessage> Messages = Capture.GetMessages();
		return std::ranges::any_of(Messages,
			[Verbosity, Text](const FLogMessage& Message)
			{ return Message.Category == "LogEditor" && Message.Verbosity == Verbosity && Message.Text == Text; });
	}
}

TEST_SUITE("FEditor play")
{
	TEST_CASE("StartPlay plays a copy of the scene being edited, which waits untouched")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		const int64 MoverId = CreateMover(Editor, "Mover");
		WScene* const Edited = &Editor.GetScene();
		const std::string EditedText = WriteShownScene(Editor);

		Editor.StartPlay();

		CHECK(Editor.IsPlaying());
		CHECK(Editor.GetPlayState() == EPlayState::Playing);
		CHECK(&Editor.GetScene() != Edited);
		CHECK(Editor.GetScene().IsPlaying());
		// The copy is what a file of the scene would hold, with the same identifiers, and its components began play.
		CHECK(WriteShownScene(Editor) == EditedText);
		CHECK(Editor.GetScene().FindEntityById(MoverId) != nullptr);
		CHECK(FEditorTestMoverEvents::Get().BeginPlayCount == 1);

		Editor.StepPlay(3);

		CHECK(GetLocation(Editor, MoverId) == FVector(3.0, 0.0, 0.0));
		CHECK_FALSE(Edited->IsPlaying());
		// Weak, because a collected scene is no object to ask.
		const TWeakObjectPtr<WScene> EditedReference = Edited;
		CollectGarbage();
		REQUIRE(EditedReference.IsValid());
		CHECK(FSceneSerializer::SaveToText(*Edited) == EditedText);
		Editor.StopPlay();
	}

	TEST_CASE("StopPlay gives back the very scene that was being edited, as it was")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		{
			// Far from the origin, with references, a component of a class that this program does not have, and a
			// shape that the engine has built in.
			WScene* const Original = NewObject<WScene>();
			WEntity* const Far = Original->CreateEntity("Far");
			FTransform FarTransform;
			FarTransform.Location = FVector(1.0e13, -1.0e13, 1.0e13);
			Far->SetRelativeTransform(FarTransform);
			Far->AddComponent<WEditorTestMoverComponent>();
			WEntity* const Pointer = Original->CreateEntity("Pointer", Far);
			Pointer->AddComponent<WEditorTestComponent>()->TargetEntity = Far;
			Pointer->AddComponent<WEditorTestDerivedComponent>();
			std::string Error;
			Original->CreateEntity("Cube")->AddComponent<WStaticMeshComponent>()->Mesh =
				FAssetManager::Load<WStaticMesh>(FAssetManager::CubeShape, Error);
			std::string Text = FSceneSerializer::SaveToText(*Original);
			Original->Destroy();
			const std::string KnownClass = "WEditorTestDerivedComponent";
			const std::size_t Position = Text.find(KnownClass);
			REQUIRE(Position != std::string::npos);
			Text.replace(Position, KnownClass.size(), "WPluginRigidBodyComponent");
			WScene* const Loaded = FSceneSerializer::LoadFromText(Text, Error);
			REQUIRE(Loaded != nullptr);
			Editor.SetScene(*Loaded, std::filesystem::path());
		}
		WScene* const Edited = &Editor.GetScene();
		const std::string EditedText = WriteShownScene(Editor);

		Editor.StartPlay();
		Editor.StepPlay(5);
		REQUIRE(WriteShownScene(Editor) != EditedText);
		Editor.StopPlay();

		CHECK_FALSE(Editor.IsPlaying());
		CHECK(Editor.GetPlayState() == EPlayState::Stopped);
		CHECK(&Editor.GetScene() == Edited);
		CHECK(WriteShownScene(Editor) == EditedText);
	}

	TEST_CASE("The copy is collected once the game has stopped, and its components ended play")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		CreateMover(Editor, "First");
		CreateMover(Editor, "Second");
		Editor.StartPlay();
		const TWeakObjectPtr<WScene> Copy = &Editor.GetScene();

		Editor.StopPlay();
		CollectGarbage();

		CHECK_FALSE(Copy.IsValid());
		CHECK(FEditorTestMoverEvents::Get().BeginPlayCount == 2);
		CHECK(FEditorTestMoverEvents::Get().EndPlayCount == 2);
	}

	TEST_CASE("The history waits during a game, and after it an undo takes back the step made before it")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		const int64 MoverId = CreateMover(Editor, "Mover");
		// Steps to undo and to redo.
		Execute(Editor, "Entity.Create", R"({"Name": "Undone"})");
		REQUIRE(Editor.Undo().has_value());
		const std::vector<std::string> UndoStepsBefore = Editor.GetHistory().GetUndoStepNames();
		const std::vector<std::string> RedoStepsBefore = Editor.GetHistory().GetRedoStepNames();
		REQUIRE(RedoStepsBefore.size() == 1);

		Editor.StartPlay();
		WScene* const Copy = &Editor.GetScene();
		CHECK(Editor.GetHistory().GetUndoStepNames() == UndoStepsBefore);
		CHECK_FALSE(Editor.Undo().has_value());
		CHECK_FALSE(Editor.Redo().has_value());
		CHECK(&Editor.GetScene() == Copy);
		Execute(Editor, "Entity.Create", R"({"Name": "During"})");
		Editor.StepPlay(2);
		CHECK(Editor.GetHistory().GetUndoStepNames() == UndoStepsBefore);
		Editor.StopPlay();

		CHECK(Editor.GetHistory().GetUndoStepNames() == UndoStepsBefore);
		CHECK(Editor.GetHistory().GetRedoStepNames() == RedoStepsBefore);
		const std::optional<std::string> Undone = Editor.Undo();
		REQUIRE(Undone.has_value());
		CHECK(*Undone == "Component.Add");
		CHECK(Editor.GetScene().FindEntityById(MoverId) != nullptr);
		CHECK(Editor.GetScene().FindEntityByName("During") == nullptr);
	}

	TEST_CASE("A change made without a command before a game is a step first, and an open step is ended")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;

		SUBCASE("A change without a command")
		{
			REQUIRE(Editor.GetScene().CreateEntity("Direct") != nullptr);

			Editor.StartPlay();

			CHECK(Editor.GetHistory().GetUndoStepNames().front() == FEditor::DirectChangeStepName);
		}
		SUBCASE("An open step")
		{
			Editor.BeginStep("Drag");
			Execute(Editor, "Entity.Create", R"({"Name": "Dragged"})");

			Editor.StartPlay();

			CHECK_FALSE(Editor.IsStepOpen());
			CHECK(Editor.GetHistory().GetUndoStepNames().front() == "Drag");
		}
		Editor.StopPlay();
		REQUIRE(Editor.Undo().has_value());
		CHECK(Editor.GetScene().GetRootEntities().empty());
	}

	TEST_CASE("A step begun during a game records nothing, and StopPlay ends it")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		const int64 MoverId = CreateMover(Editor, "Mover");
		const std::vector<std::string> UndoStepsBefore = Editor.GetHistory().GetUndoStepNames();
		Editor.StartPlay();

		Editor.BeginStep("Move");
		Execute(Editor,
			"Object.SetProperty",
			std::format(R"({{"Object": {}, "Property": "Name", "Value": "Moved"}})", MoverId));
		Editor.StopPlay();

		CHECK_FALSE(Editor.IsStepOpen());
		CHECK(Editor.GetHistory().GetUndoStepNames() == UndoStepsBefore);
		CHECK(Editor.GetScene().FindEntityById(MoverId)->GetName() == "Mover");
	}

	TEST_CASE("Commands during a game change only the copy, and are no steps")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		const int64 MoverId = CreateMover(Editor, "Mover");
		const int64 OtherId = ExecuteForId(Editor, "Entity.Create", R"({"Name": "Other"})");
		const std::string EditedText = WriteShownScene(Editor);
		const std::vector<std::string> UndoStepsBefore = Editor.GetHistory().GetUndoStepNames();
		Editor.StartPlay();

		const int64 MadeId = ExecuteForId(Editor, "Entity.Create", R"({"Name": "Made"})");
		Execute(Editor,
			"Object.SetProperty",
			std::format(R"({{"Object": {}, "Property": "Name", "Value": "Renamed"}})", MoverId));
		const int64 AddedId = ExecuteForId(
			Editor, "Component.Add", std::format(R"({{"Entity": {}, "Class": "WEditorTestComponent"}})", MadeId));
		Execute(Editor, "Component.Remove", std::format(R"({{"Component": {}}})", AddedId));
		Execute(Editor, "Entity.Delete", std::format(R"({{"Entity": {}}})", OtherId));

		CHECK(Editor.GetScene().FindEntityById(MadeId) != nullptr);
		CHECK(Editor.GetScene().FindEntityById(MoverId)->GetName() == "Renamed");
		CHECK(Editor.GetScene().FindEntityById(OtherId) == nullptr);
		CHECK(Editor.GetHistory().GetUndoStepNames() == UndoStepsBefore);
		Editor.StopPlay();

		CHECK(WriteShownScene(Editor) == EditedText);
		CHECK(Editor.GetHistory().GetUndoStepNames() == UndoStepsBefore);
	}

	TEST_CASE("The commands that would act on what waits unchanged are refused during a game, and do nothing")
	{
		const FScopedEditorTest EditorTest;
		const FScopedTempDirectory Directory;
		const FScopedLogCapture Capture;
		FEditor Editor;
		CreateMover(Editor, "Mover");
		Execute(Editor, "Entity.Create", R"({"Name": "Undone"})");
		REQUIRE(Editor.Undo().has_value());
		const std::filesystem::path SceneFile = Directory.GetPath() / "Level.wwscene";
		const std::string Path = FFileHelper::PathToUtf8(SceneFile);
		const std::string ProjectDirectory = FFileHelper::PathToUtf8(Directory.GetPath() / "Project");
		Editor.StartPlay();
		WScene* const Copy = &Editor.GetScene();
		const std::string CopyText = WriteShownScene(Editor);
		const std::vector<std::string> UndoStepsBefore = Editor.GetHistory().GetUndoStepNames();
		const std::vector<std::string> RedoStepsBefore = Editor.GetHistory().GetRedoStepNames();

		std::string Name;
		std::string ArgumentText = "{}";
		SUBCASE("Scene.New")
		{
			Name = "Scene.New";
		}
		SUBCASE("Scene.Open")
		{
			Name = "Scene.Open";
			ArgumentText = std::format(R"({{"Path": {}}})", WriteTestJson(FJsonValue(Path)));
		}
		SUBCASE("Scene.Save")
		{
			Name = "Scene.Save";
			ArgumentText = std::format(R"({{"Path": {}}})", WriteTestJson(FJsonValue(Path)));
		}
		SUBCASE("Project.New")
		{
			Name = "Project.New";
			ArgumentText =
				std::format(R"({{"Directory": {}, "Name": "Game"}})", WriteTestJson(FJsonValue(ProjectDirectory)));
		}
		SUBCASE("Project.Open")
		{
			Name = "Project.Open";
			ArgumentText = std::format(R"({{"Path": {}}})", WriteTestJson(FJsonValue(ProjectDirectory)));
		}
		SUBCASE("Edit.Undo")
		{
			Name = "Edit.Undo";
		}
		SUBCASE("Edit.Redo")
		{
			Name = "Edit.Redo";
		}
		SUBCASE("Asset.SetProperty")
		{
			Name = "Asset.SetProperty";
			ArgumentText = R"({"Asset": "Materials/Red.wwmaterial", "Property": "Roughness", "Value": 0.25})";
		}
		CAPTURE(Name);

		const FCommandResult Result = Editor.ExecuteCommand(Name, ParseTestJson(ArgumentText));

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError() == RefusedDuringGame(Name));
		CHECK(HasEditorMessage(
			Capture, ELogVerbosity::Warning, std::format("Command {} failed: {}", Name, RefusedDuringGame(Name))));
		CHECK(Editor.IsPlaying());
		CHECK(&Editor.GetScene() == Copy);
		CHECK(WriteShownScene(Editor) == CopyText);
		CHECK(Editor.GetHistory().GetUndoStepNames() == UndoStepsBefore);
		CHECK(Editor.GetHistory().GetRedoStepNames() == RedoStepsBefore);
		CHECK(Editor.GetProject() == nullptr);
		CHECK(Editor.GetScenePath().empty());
		CHECK_FALSE(std::filesystem::exists(SceneFile));
		CHECK_FALSE(std::filesystem::exists(Directory.GetPath() / "Project"));
		Editor.StopPlay();
	}

	TEST_CASE("What is selected carries into a game and back")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		const int64 FirstId = CreateMover(Editor, "First");
		const int64 SecondId = CreateMover(Editor, "Second");
		Editor.SetSelection({FirstId});

		Editor.StartPlay();
		CHECK(Editor.GetSelection() == std::vector<int64>{FirstId});
		Editor.SetSelection({SecondId, FirstId});
		Editor.StopPlay();

		CHECK(Editor.GetSelection() == std::vector<int64>{SecondId, FirstId});
	}

	TEST_CASE("An identifier that an undo left among the selected is dropped when a game begins")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		const int64 UndoneId = ExecuteForId(Editor, "Entity.Create", R"({"Name": "Undone"})");
		Editor.SetSelection({UndoneId});
		REQUIRE(Editor.Undo().has_value());
		Editor.StartPlay();

		// The copy gives the identifier out again, to the next entity that the game makes.
		const int64 MadeId = ExecuteForId(Editor, "Entity.Create", R"({"Name": "Made"})");

		REQUIRE(MadeId == UndoneId);
		CHECK_FALSE(Editor.IsSelected(MadeId));
		CHECK(Editor.GetSelection().empty());
		Editor.StopPlay();
	}

	TEST_CASE("An entity that only the game had is not selected after it, nor the entity given its identifier next")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		Editor.StartPlay();
		const int64 GameOnlyId = ExecuteForId(Editor, "Entity.Create", R"({"Name": "Game only"})");
		Editor.SetSelection({GameOnlyId});

		Editor.StopPlay();
		CHECK(Editor.GetSelection().empty());
		const int64 NextId = ExecuteForId(Editor, "Entity.Create", R"({"Name": "Next"})");

		REQUIRE(NextId == GameOnlyId);
		CHECK_FALSE(Editor.IsSelected(NextId));
	}

	TEST_CASE("The camera, the view mode and the gizmo stay as a game left them")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		Editor.StartPlay();
		FViewportCamera Camera;
		Camera.Location = FVector(1.0, 2.0, 3.0);
		Camera.Yaw = 45.0f;
		FGizmoSettings Gizmo;
		Gizmo.Mode = EGizmoMode::Rotate;

		Editor.SetViewportCamera(Camera);
		Editor.SetViewMode(ESceneViewMode::Unlit);
		Editor.SetGizmoSettings(Gizmo);
		Editor.StopPlay();

		CHECK(Editor.GetViewportCamera().Location == Camera.Location);
		CHECK(Editor.GetViewportCamera().Yaw == Camera.Yaw);
		CHECK(Editor.GetViewMode() == ESceneViewMode::Unlit);
		CHECK(Editor.GetGizmoSettings().Mode == EGizmoMode::Rotate);
	}

	TEST_CASE("A material that the editor keeps is the game's too, and is as it was after the game, unsaved or saved")
	{
		FAssetCommandTest Test;
		REQUIRE(Test.Execute("Asset.CreateMaterial", R"({"Path": "Materials/Red.wwmaterial"})").HasSucceeded());
		REQUIRE(Test.Execute("Asset.SetProperty",
						R"({"Asset": "Materials/Red.wwmaterial", "Property": "Roughness", "Value": 0.25})")
					.HasSucceeded());
		const int64 CubeId = ExecuteForId(Test.Editor, "Entity.Create", R"({"Name": "Cube"})");
		const int64 ShapeId = ExecuteForId(
			Test.Editor, "Component.Add", std::format(R"({{"Entity": {}, "Class": "WStaticMeshComponent"}})", CubeId));
		Execute(Test.Editor,
			"Object.SetProperty",
			std::format(R"({{"Object": {}, "Property": "Material", "Value": "Materials/Red.wwmaterial"}})", ShapeId));
		const WMaterial* const Kept =
			Cast<WStaticMeshComponent>(Test.Editor.GetScene().FindComponentById(ShapeId))->Material.Get();
		REQUIRE(Kept != nullptr);
		const std::string FileText = ReadContentText(Test, "Materials/Red.wwmaterial");

		Test.Editor.StartPlay();
		const WStaticMeshComponent* const Shown =
			Cast<WStaticMeshComponent>(Test.Editor.GetScene().FindComponentById(ShapeId));
		REQUIRE(Shown != nullptr);
		CHECK(Shown->Material.Get() == Kept);
		Test.Editor.StopPlay();

		const FJsonValue Unsaved =
			Execute(Test.Editor, "Asset.GetProperties", R"({"Asset": "Materials/Red.wwmaterial"})");
		CHECK(Unsaved.FindMember("Unsaved")->GetBoolean());
		CHECK(Unsaved.FindMember("Properties")->FindMember("Roughness")->GetNumber() == 0.25);
		CHECK(ReadContentText(Test, "Materials/Red.wwmaterial") == FileText);

		// Saving is no change to the scene or the history, and can be done during a game.
		Test.Editor.StartPlay();
		Execute(Test.Editor, "Asset.Save", R"({"Asset": "Materials/Red.wwmaterial"})");
		Test.Editor.StopPlay();

		const FJsonValue Saved =
			Execute(Test.Editor, "Asset.GetProperties", R"({"Asset": "Materials/Red.wwmaterial"})");
		CHECK_FALSE(Saved.FindMember("Unsaved")->GetBoolean());
		CHECK(ReadContentText(Test, "Materials/Red.wwmaterial") != FileText);
	}

	TEST_CASE("Another scene, another project and the end of the editor stop a game first")
	{
		const FScopedEditorTest EditorTest;
		const FScopedTempDirectory Directory;
		TWeakObjectPtr<WScene> Copy;
		TWeakObjectPtr<WScene> Edited;
		std::filesystem::path FirstContent;
		{
			FEditor Editor;
			// The game ends in the project it was played in, before anything of that project is given up.
			std::string Error;
			std::optional<FProject> First = FProject::Create(Directory.GetPath() / "First", "First", Error);
			REQUIRE(First.has_value());
			FirstContent = First->GetContentDirectory();
			Editor.SetProject(std::move(*First));
			CreateMover(Editor, "Mover");
			Edited = &Editor.GetScene();
			Editor.StartPlay();
			Copy = &Editor.GetScene();

			SUBCASE("Another scene")
			{
				WScene* const NewScene = NewObject<WScene>();
				Editor.SetScene(*NewScene, std::filesystem::path());

				CHECK_FALSE(Editor.IsPlaying());
				CHECK(FEditorTestMoverEvents::Get().EndPlayCount == 1);
				CHECK(&Editor.GetScene() == NewScene);
				CHECK_FALSE(Copy.IsValid());
				CHECK_FALSE(Edited.IsValid());
			}
			SUBCASE("Another project")
			{
				std::optional<FProject> Second = FProject::Create(Directory.GetPath() / "Second", "Second", Error);
				REQUIRE(Second.has_value());
				Editor.SetProject(std::move(*Second));

				CHECK_FALSE(Editor.IsPlaying());
				CHECK(FEditorTestMoverEvents::Get().EndPlayCount == 1);
				CHECK(Editor.GetScene().GetRootEntities().empty());
				CHECK_FALSE(Copy.IsValid());
				CHECK_FALSE(Edited.IsValid());
			}
			SUBCASE("The end of the editor")
			{
				CHECK(Editor.IsPlaying());
			}
		}

		CHECK(FEditorTestMoverEvents::Get().EndPlayCount == 1);
		CHECK(FEditorTestMoverEvents::Get().EndPlayContentDirectory == FirstContent);
		// Nothing is left in the root set.
		CollectGarbage();
		CHECK_FALSE(Copy.IsValid());
		CHECK_FALSE(Edited.IsValid());
	}

	TEST_CASE("Giving SetScene the copy that its call stops is a fatal error")
	{
		const FScopedEditorTest EditorTest;
		const FScopedLogCapture Capture;
		FEditor Editor;
		Editor.StartPlay();
		WScene& Copy = Editor.GetScene();

		CHECK_THROWS_AS(Editor.SetScene(Copy, std::filesystem::path()), FFatalErrorException);

		CHECK_FALSE(Editor.IsPlaying());
		CHECK(Editor.GetScene().GetName() == "Untitled");
	}

	TEST_CASE("Starting a game while one exists, and the other play functions without one, are fatal errors")
	{
		const FScopedEditorTest EditorTest;
		const FScopedLogCapture Capture;
		FEditor Editor;

		CHECK_THROWS_AS(Editor.StopPlay(), FFatalErrorException);
		CHECK_THROWS_AS(Editor.PausePlay(), FFatalErrorException);
		CHECK_THROWS_AS(Editor.ResumePlay(), FFatalErrorException);
		CHECK_THROWS_AS(Editor.StepPlay(1), FFatalErrorException);
		CHECK_FALSE(Editor.IsPlaying());

		Editor.StartPlay();
		CHECK_THROWS_AS(Editor.StartPlay(), FFatalErrorException);
		CHECK_THROWS_AS(Editor.StepPlay(0), FFatalErrorException);
		CHECK(Editor.IsPlaying());
		CHECK(Editor.GetScene().GetPlayedFrameCount() == 0);
		Editor.StopPlay();
	}

	TEST_CASE("PausePlay and ResumePlay change whether the game is paused, and StepPlay plays either way")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		const int64 MoverId = CreateMover(Editor, "Mover");
		Editor.StartPlay();

		Editor.PausePlay();
		CHECK(Editor.GetPlayState() == EPlayState::Paused);
		CHECK(Editor.IsPlaying());
		Editor.StepPlay(2);
		CHECK(Editor.GetPlayState() == EPlayState::Paused);
		CHECK(GetLocation(Editor, MoverId) == FVector(2.0, 0.0, 0.0));

		Editor.ResumePlay();
		CHECK(Editor.GetPlayState() == EPlayState::Playing);
		Editor.StepPlay(1);
		CHECK(GetLocation(Editor, MoverId) == FVector(3.0, 0.0, 0.0));
		Editor.StopPlay();
	}

	TEST_CASE("A hundred frames give the same copy at once, as sixty and forty, and one at a time")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		const int64 MoverId = CreateMover(Editor, "Mover");
		Execute(Editor,
			"Object.SetProperty",
			std::format(R"({{"Object": {}, "Property": "Velocity", "Value": [1.1, -2.3, 0.7]}})",
				Editor.GetScene().FindEntityById(MoverId)->GetComponents()[0]->GetId()));

		std::vector<std::string> Texts;
		for (const std::vector<int32>& Steps :
			{std::vector<int32>{100}, std::vector<int32>{60, 40}, std::vector<int32>(100, 1)})
		{
			// Each game begins from the scene being edited.
			Editor.StartPlay();
			CHECK(Editor.GetScene().GetPlayedFrameCount() == 0);
			for (const int32 FrameCount : Steps)
			{
				Editor.StepPlay(FrameCount);
			}
			Texts.push_back(WriteShownScene(Editor));
			Editor.StopPlay();
		}

		CHECK(Texts[0] == Texts[1]);
		CHECK(Texts[0] == Texts[2]);
		CHECK(Texts[0].find("\"FrameCount\": 100") != std::string::npos);
	}

	TEST_CASE("Beginning and ending a game are a line each in the log")
	{
		const FScopedEditorTest EditorTest;
		const FScopedLogCapture Capture;
		FEditor Editor;
		Editor.GetScene().SetName("Level1");

		Editor.StartPlay();
		Editor.StepPlay(120);
		Editor.StopPlay();

		CHECK(HasEditorMessage(Capture, ELogVerbosity::Log, "The game began, in a copy of the scene \"Level1\"."));
		CHECK(HasEditorMessage(
			Capture, ELogVerbosity::Log, "The game ended at frame 120, after 2 seconds of game time."));
	}
}
