#include "Assets/AssetManager.h"
#include "Core/FileHelper.h"
#include "Editor.h"
#include "Project/Project.h"
#include "Scene/Scene.h"
#include "TestEditorObjects.h"
#include "TestSupport/ScopedTempDirectory.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <initializer_list>
#include <optional>
#include <ostream>
#include <string>

namespace
{
	/** Returns the arguments of Project.New for a directory and a name. */
	FJsonValue MakeNewArguments(const std::filesystem::path& Directory, const std::string& Name)
	{
		FJsonValue Arguments = FJsonValue::MakeObject();
		Arguments.SetMember("Directory", FFileHelper::PathToUtf8(Directory));
		Arguments.SetMember("Name", Name);
		return Arguments;
	}

	/** Returns the arguments {"Path": Path}. */
	FJsonValue MakePathArguments(const std::filesystem::path& Path)
	{
		FJsonValue Arguments = FJsonValue::MakeObject();
		Arguments.SetMember("Path", FFileHelper::PathToUtf8(Path));
		return Arguments;
	}
}

TEST_SUITE("Project commands")
{
	TEST_CASE("Project.New creates a project and makes its content directory the one assets are loaded from")
	{
		const FScopedEditorTest EditorTest;
		const FScopedTempDirectory Directory;
		const std::filesystem::path ProjectDirectory = Directory.GetPath() / "Tetris";
		{
			FEditor Editor;
			REQUIRE(Editor.GetProject() == nullptr);
			Editor.GetScene().SetName("Before");
			REQUIRE(Editor.GetScene().CreateEntity("Old") != nullptr);

			const FCommandResult Result =
				Editor.ExecuteCommand("Project.New", MakeNewArguments(ProjectDirectory, "Tetris"));

			INFO("Error: ", Result.GetError());
			REQUIRE(Result.HasSucceeded());
			FJsonValue Expected = FJsonValue::MakeObject();
			Expected.SetMember("Name", "Tetris");
			Expected.SetMember("File", FFileHelper::PathToUtf8(ProjectDirectory / "Tetris.wwproject"));
			Expected.SetMember("Directory", FFileHelper::PathToUtf8(ProjectDirectory));
			Expected.SetMember("ContentDirectory", FFileHelper::PathToUtf8(ProjectDirectory / "Content"));
			CHECK(WriteTestJson(Result.GetValue()) == WriteTestJson(Expected));

			REQUIRE(Editor.GetProject() != nullptr);
			CHECK(Editor.GetProject()->GetName() == "Tetris");
			CHECK(FAssetManager::GetContentDirectory() == ProjectDirectory / "Content");
			CHECK(std::filesystem::is_directory(ProjectDirectory / "Content"));
			// The scene of before has nothing to do with the new project.
			CHECK(Editor.GetScene().GetName() == "Untitled");
			CHECK(Editor.GetScene().GetAllEntities().empty());
			CHECK_FALSE(Editor.GetHistory().CanUndo());
		}

		// The editor gave the content directory up when it was destroyed.
		CHECK(FAssetManager::GetContentDirectory().empty());
	}

	TEST_CASE("Project.Open opens a project by its file or its directory")
	{
		const FScopedEditorTest EditorTest;
		const FScopedTempDirectory Directory;
		std::string Error;
		const std::optional<FProject> Created = FProject::Create(Directory.GetPath(), "Tetris", Error);
		REQUIRE(Created.has_value());
		FEditor Editor;
		std::filesystem::path Path;

		SUBCASE("By its file")
		{
			Path = Created->GetFile();
		}
		SUBCASE("By its directory")
		{
			Path = Directory.GetPath();
		}

		const FCommandResult Result = Editor.ExecuteCommand("Project.Open", MakePathArguments(Path));

		INFO("Error: ", Result.GetError());
		REQUIRE(Result.HasSucceeded());
		REQUIRE(Editor.GetProject() != nullptr);
		CHECK(Editor.GetProject()->GetFile() == Created->GetFile());
		CHECK(FAssetManager::GetContentDirectory() == Directory.GetPath() / "Content");
		CHECK(*Result.GetValue().FindMember("Name") == FJsonValue("Tetris"));
	}

	TEST_CASE("A project command that fails leaves the editor with the project and scene it had")
	{
		const FScopedEditorTest EditorTest;
		const FScopedTempDirectory Directory;
		FEditor Editor;
		REQUIRE(Editor.ExecuteCommand("Project.New", MakeNewArguments(Directory.GetPath() / "First", "First"))
					.HasSucceeded());
		REQUIRE(Editor.GetScene().CreateEntity("Kept") != nullptr);

		SUBCASE("Project.New where there is a project already")
		{
			const FCommandResult Result =
				Editor.ExecuteCommand("Project.New", MakeNewArguments(Directory.GetPath() / "First", "Second"));

			CHECK_FALSE(Result.HasSucceeded());
			CHECK(Result.GetError().starts_with("The project was not created: the directory "));
			CHECK(Result.GetError().ends_with("holds a project already."));
		}
		SUBCASE("Project.New with a name that cannot be a file's")
		{
			const FCommandResult Result =
				Editor.ExecuteCommand("Project.New", MakeNewArguments(Directory.GetPath() / "Second", "A/B"));

			CHECK_FALSE(Result.HasSucceeded());
			CHECK(
				Result.GetError().starts_with("The project was not created: \"A/B\" cannot be the name of a project"));
		}
		SUBCASE("Project.Open of a directory without a project")
		{
			const FCommandResult Result =
				Editor.ExecuteCommand("Project.Open", MakePathArguments(Directory.GetPath() / "Nowhere"));

			CHECK_FALSE(Result.HasSucceeded());
			CHECK(Result.GetError().starts_with("The project was not opened: "));
		}

		REQUIRE(Editor.GetProject() != nullptr);
		CHECK(Editor.GetProject()->GetName() == "First");
		CHECK(Editor.GetScene().FindEntityByName("Kept") != nullptr);
		CHECK(FAssetManager::GetContentDirectory() == Directory.GetPath() / "First" / "Content");
	}

	TEST_CASE("Project.Get returns the project, and fails while none is open")
	{
		const FScopedEditorTest EditorTest;
		const FScopedTempDirectory Directory;
		FEditor Editor;

		const FCommandResult Without = Editor.ExecuteCommand("Project.Get", FJsonValue());
		CHECK_FALSE(Without.HasSucceeded());
		CHECK(Without.GetError() == "No project is open. Create one with Project.New or open one with Project.Open.");

		const FCommandResult Created =
			Editor.ExecuteCommand("Project.New", MakeNewArguments(Directory.GetPath(), "Tetris"));
		REQUIRE(Created.HasSucceeded());

		const FCommandResult With = Editor.ExecuteCommand("Project.Get", FJsonValue());
		REQUIRE(With.HasSucceeded());
		CHECK(WriteTestJson(With.GetValue()) == WriteTestJson(Created.GetValue()));
	}

	TEST_CASE("With a project open, a scene is saved and opened by a path relative to the content directory")
	{
		const FScopedEditorTest EditorTest;
		const FScopedTempDirectory Directory;
		FEditor Editor;
		REQUIRE(Editor.ExecuteCommand("Project.New", MakeNewArguments(Directory.GetPath(), "Tetris")).HasSucceeded());
		REQUIRE(Editor.ExecuteCommand("Entity.Create", ParseTestJson(R"({"Name": "Player"})")).HasSucceeded());
		const std::filesystem::path ScenePath = Directory.GetPath() / "Content" / "Scenes" / "Level1.json";

		const FCommandResult Saved =
			Editor.ExecuteCommand("Scene.Save", ParseTestJson(R"({"Path": "Scenes/Level1.json"})"));

		INFO("Error: ", Saved.GetError());
		REQUIRE(Saved.HasSucceeded());
		CHECK(std::filesystem::is_regular_file(ScenePath));
		CHECK(Editor.GetScenePath() == ScenePath);

		REQUIRE(Editor.ExecuteCommand("Scene.New", FJsonValue()).HasSucceeded());
		const FCommandResult Opened =
			Editor.ExecuteCommand("Scene.Open", ParseTestJson(R"({"Path": "Scenes/Level1.json"})"));

		INFO("Error: ", Opened.GetError());
		REQUIRE(Opened.HasSucceeded());
		CHECK(Editor.GetScene().FindEntityByName("Player") != nullptr);

		// A path that says where it is in the file system is taken as it is.
		const std::filesystem::path Elsewhere = Directory.GetPath() / "Elsewhere.json";
		REQUIRE(Editor.ExecuteCommand("Scene.Save", MakePathArguments(Elsewhere)).HasSucceeded());
		CHECK(std::filesystem::is_regular_file(Elsewhere));
	}

	TEST_CASE("ResolveFilePath leaves every path as it is while no project is open")
	{
		const FScopedEditorTest EditorTest;
		const FEditor Editor;

		CHECK(Editor.ResolveFilePath(std::filesystem::path("Scenes") / "Level1.json") ==
			std::filesystem::path("Scenes") / "Level1.json");
		CHECK(Editor.ResolveFilePath(std::filesystem::path()).empty());
	}

	TEST_CASE("MakeProjectPath names a file of the project's content by its place there, and any other by its path")
	{
		const FScopedEditorTest EditorTest;
		const FScopedTempDirectory Directory;
		FEditor Editor;
		const std::filesystem::path Outside = Directory.GetPath() / "Elsewhere" / "Level1.wwscene";

		// Without a project every file is named by its whole path.
		CHECK(Editor.MakeProjectPath(Outside) == FFileHelper::PathToUtf8(Outside));

		REQUIRE(Editor.ExecuteCommand("Project.New", MakeNewArguments(Directory.GetPath() / "Game", "Game"))
					.HasSucceeded());
		const std::filesystem::path Content = Directory.GetPath() / "Game" / "Content";

		CHECK(Editor.MakeProjectPath(Content / "Level1.wwscene") == "Level1.wwscene");
		CHECK(Editor.MakeProjectPath(Content / "Scenes" / "Caves" / "Deep.wwscene") == "Scenes/Caves/Deep.wwscene");
		CHECK(Editor.MakeProjectPath(Outside) == FFileHelper::PathToUtf8(Outside));
		// Beside the content directory is not in it.
		const std::filesystem::path Beside = Directory.GetPath() / "Game" / "Notes.txt";
		CHECK(Editor.MakeProjectPath(Beside) == FFileHelper::PathToUtf8(Beside));

		// What it returns is what ResolveFilePath makes the file of again.
		for (const std::filesystem::path& File : {Content / "Scenes" / "Town.wwscene", Outside, Beside})
		{
			CHECK(Editor.ResolveFilePath(FFileHelper::PathFromUtf8(Editor.MakeProjectPath(File))) == File);
		}
	}

	TEST_CASE("SetProject of another project moves the content directory to it")
	{
		const FScopedEditorTest EditorTest;
		const FScopedTempDirectory Directory;
		std::string Error;
		std::optional<FProject> First = FProject::Create(Directory.GetPath() / "First", "First", Error);
		std::optional<FProject> Second = FProject::Create(Directory.GetPath() / "Second", "Second", Error);
		REQUIRE(First.has_value());
		REQUIRE(Second.has_value());
		FEditor Editor;

		Editor.SetProject(std::move(*First));
		CHECK(FAssetManager::GetContentDirectory() == Directory.GetPath() / "First" / "Content");
		Editor.SetProject(std::move(*Second));

		CHECK(FAssetManager::GetContentDirectory() == Directory.GetPath() / "Second" / "Content");
		REQUIRE(Editor.GetProject() != nullptr);
		CHECK(Editor.GetProject()->GetName() == "Second");
	}
}
