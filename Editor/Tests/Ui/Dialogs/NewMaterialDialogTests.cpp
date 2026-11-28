#include "Assets/TestAssets.h"
#include "Core/FileHelper.h"
#include "Logging/LogMessage.h"
#include "Logging/LogVerbosity.h"
#include "TestSupport/ScopedTempDirectory.h"
#include "Ui/TestEditorUi.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <ostream>
#include <string>
#include <vector>

// Making a material is tested as a person does it: with the File menu, and by typing into the window it opens.

namespace
{
	/** Opens the window with File > New Material..., which the test expects to open. */
	void OpenNewMaterialWindow(FEditorUiTest& Test)
	{
		Test.Click("Menu/File");
		Test.Click("Menu/File/New Material...");
		REQUIRE(Test.HasItem("NewMaterial/Path"));
	}

	/** Returns the warnings that have been logged, in their order. */
	std::vector<std::string> GetLoggedWarnings(const FEditorUiTest& Test)
	{
		std::vector<std::string> Warnings;
		for (const FLogMessage& Message : Test.Capture.GetMessages())
		{
			if (Message.Verbosity == ELogVerbosity::Warning)
			{
				Warnings.push_back(Message.Text);
			}
		}
		return Warnings;
	}
}

TEST_SUITE("FNewMaterialDialog")
{
	TEST_CASE("File > New Material makes a material under the name it suggests when Enter is pressed")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		const std::filesystem::path Content = CreateTestProject(Test.Editor, Directory.GetPath());

		OpenNewMaterialWindow(Test);
		// Nothing is made until the user says so.
		CHECK_FALSE(std::filesystem::exists(Content / "Materials"));
		Test.PressKey(EKey::Enter);

		CHECK_FALSE(Test.HasItem("NewMaterial/Path"));
		CHECK(std::filesystem::is_regular_file(Content / "Materials" / "NewMaterial.wwmaterial"));
		// It is no change to the scene, and no step of the history.
		CHECK_FALSE(Test.Editor.GetHistory().CanUndo());
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("The name that the window suggests is one that no file of the project has")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		const std::filesystem::path Content = CreateTestProject(Test.Editor, Directory.GetPath());
		REQUIRE(Test.Editor
					.ExecuteCommand(
						"Asset.CreateMaterial", ParseTestJson(R"({"Path": "Materials/NewMaterial.wwmaterial"})"))
					.HasSucceeded());
		REQUIRE(Test.Editor
					.ExecuteCommand(
						"Asset.CreateMaterial", ParseTestJson(R"({"Path": "Materials/NewMaterial2.wwmaterial"})"))
					.HasSucceeded());

		OpenNewMaterialWindow(Test);
		Test.Click("NewMaterial/Create");

		CHECK_FALSE(Test.HasItem("NewMaterial/Path"));
		CHECK(std::filesystem::is_regular_file(Content / "Materials" / "NewMaterial3.wwmaterial"));
	}

	TEST_CASE("The window suggests the folder of materials as the project spells it")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		CreateTestProject(Test.Editor, Directory.GetPath());
		// Where the file system does not tell capitals apart, this folder is the one that the window suggests.
		REQUIRE(
			Test.Editor.ExecuteCommand("Asset.CreateMaterial", ParseTestJson(R"({"Path": "materials/Red.wwmaterial"})"))
				.HasSucceeded());

		OpenNewMaterialWindow(Test);
		Test.PressKey(EKey::Enter);

		CHECK_FALSE(Test.HasItem("NewMaterial/Path"));
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("The window does not suggest the name of a material that is in use although its file is gone")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		const std::filesystem::path Content = CreateTestProject(Test.Editor, Directory.GetPath());
		REQUIRE(Test.Editor
					.ExecuteCommand(
						"Asset.CreateMaterial", ParseTestJson(R"({"Path": "Materials/NewMaterial.wwmaterial"})"))
					.HasSucceeded());
		// Changed, the editor keeps it, and then its file goes.
		REQUIRE(Test.Editor
					.ExecuteCommand("Asset.SetProperty",
						ParseTestJson(
							R"({"Asset": "Materials/NewMaterial.wwmaterial", "Property": "Metallic", "Value": 1})"))
					.HasSucceeded());
		std::filesystem::remove(Content / "Materials" / "NewMaterial.wwmaterial");

		OpenNewMaterialWindow(Test);
		Test.PressKey(EKey::Enter);

		CHECK_FALSE(Test.HasItem("NewMaterial/Path"));
		CHECK(std::filesystem::is_regular_file(Content / "Materials" / "NewMaterial2.wwmaterial"));
		CHECK_FALSE(std::filesystem::exists(Content / "Materials" / "NewMaterial.wwmaterial"));
	}

	TEST_CASE("What is typed replaces the suggestion, and a name without the ending is given it")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		const std::filesystem::path Content = CreateTestProject(Test.Editor, Directory.GetPath());

		OpenNewMaterialWindow(Test);
		Test.Type("Metals/Gold");
		Test.Click("NewMaterial/Create");

		CHECK_FALSE(Test.HasItem("NewMaterial/Path"));
		CHECK(std::filesystem::is_regular_file(Content / "Metals" / "Gold.wwmaterial"));
		CHECK_FALSE(std::filesystem::exists(Content / "Materials"));
	}

	TEST_CASE("A material that cannot be made keeps the window open, and the log has the reason")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		CreateTestProject(Test.Editor, Directory.GetPath());
		REQUIRE(
			Test.Editor.ExecuteCommand("Asset.CreateMaterial", ParseTestJson(R"({"Path": "Materials/Red.wwmaterial"})"))
				.HasSucceeded());

		OpenNewMaterialWindow(Test);
		Test.Type("Materials/Red");
		Test.Click("NewMaterial/Create");

		CHECK(Test.HasItem("NewMaterial/Path"));
		CHECK(GetLoggedWarnings(Test) ==
			std::vector<std::string>{"Command Asset.CreateMaterial failed: The material was not created: the project "
									 "already has a file \"Materials/Red.wwmaterial\"."});

		// Another name is made, and closes the window. The keyboard went to the button that was clicked.
		Test.Click("NewMaterial/Path");
		Test.Type("Materials/Blue");
		Test.PressKey(EKey::Enter);
		CHECK_FALSE(Test.HasItem("NewMaterial/Path"));
	}

	TEST_CASE("Cancel and Escape close the window, and make nothing")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		const std::filesystem::path Content = CreateTestProject(Test.Editor, Directory.GetPath());

		SUBCASE("Cancel")
		{
			OpenNewMaterialWindow(Test);
			Test.Click("NewMaterial/Cancel");
		}
		SUBCASE("Escape")
		{
			OpenNewMaterialWindow(Test);
			Test.PressKey(EKey::Escape);
		}

		CHECK_FALSE(Test.HasItem("NewMaterial/Path"));
		CHECK_FALSE(std::filesystem::exists(Content / "Materials"));
	}

	TEST_CASE("New Material can be chosen only while a project is open")
	{
		FEditorUiTest Test;

		Test.Click("Menu/File");
		Test.Click("Menu/File/New Material...");

		CHECK_FALSE(Test.HasItem("NewMaterial/Path"));
	}
}
