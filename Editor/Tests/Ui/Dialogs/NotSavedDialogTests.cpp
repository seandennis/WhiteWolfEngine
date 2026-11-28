#include "Assets/EditedAssets.h"
#include "Assets/TestAssets.h"
#include "Core/FileHelper.h"
#include "TestSupport/ScopedTempDirectory.h"
#include "Ui/TestEditorUi.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

// Saving what was changed is tested as a person does it: with Ctrl+S, the File menu and the toolbar, and by
// answering the window that says what could not be written.

namespace
{
	constexpr std::string_view RedPath = "Materials/Red.wwmaterial";

	/** Gives the editor a command with the arguments written as JSON text, which the test expects to succeed. */
	void Run(FEditorUiTest& Test, std::string_view CommandName, std::string_view ArgumentText)
	{
		const FCommandResult Result = Test.Editor.ExecuteCommand(CommandName, ParseTestJson(ArgumentText));
		INFO(CommandName, ": ", Result.GetError());
		REQUIRE(Result.HasSucceeded());
	}

	/** Returns the text of a file, which a test expects to be there. */
	std::string ReadFile(const std::filesystem::path& Path)
	{
		std::string Text;
		std::string Error;
		const bool bLoaded = FFileHelper::LoadFileToString(Path, Text, Error);
		INFO("Error: ", Error);
		REQUIRE(bLoaded);
		return Text;
	}

	/**
	 * A project with a material whose change is not saved, and a scene with an entity, Dragon, that its file, if it
	 * has one, does not have yet.
	 */
	struct FChangedProject
	{
		FChangedProject(FEditorUiTest& Test, const std::filesystem::path& Directory, bool bSceneHasFile)
			: Content(CreateTestProject(Test.Editor, Directory))
		{
			Run(Test, "Asset.CreateMaterial", R"({"Path": "Materials/Red.wwmaterial"})");
			Run(Test,
				"Asset.SetProperty",
				R"({"Asset": "Materials/Red.wwmaterial", "Property": "Metallic", "Value": 1})");
			if (bSceneHasFile)
			{
				Run(Test, "Scene.Save", R"({"Path": "Town.wwscene"})");
			}
			Run(Test, "Entity.Create", R"({"Name": "Dragon"})");
			Test.DrawFrame();
		}

		/** Returns whether the material's file says what the material is now. */
		bool IsMaterialWritten() const
		{
			return ReadFile(Content / "Materials" / "Red.wwmaterial").find("\"Metallic\": 1.0") != std::string::npos;
		}

		/** Returns whether the scene's file has the entity that was added last. */
		bool IsSceneWritten() const
		{
			const std::filesystem::path File = Content / "Town.wwscene";
			return std::filesystem::exists(File) && ReadFile(File).find("\"Dragon\"") != std::string::npos;
		}

		/** Puts a folder with something in it where the material's file is, so that the file cannot be written. */
		void BlockMaterialFile() const
		{
			const std::filesystem::path File = Content / "Materials" / "Red.wwmaterial";
			std::filesystem::remove(File);
			std::filesystem::create_directories(File / "Inside");
		}

		std::filesystem::path Content;
	};

	/** Whether the scene has a file to be saved to when the test begins. */
	constexpr bool bWithSceneFile = true;
	constexpr bool bWithoutSceneFile = false;
}

TEST_SUITE("FNotSavedDialog")
{
	TEST_CASE("Ctrl+S saves the materials that were changed, and then the scene")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		const FChangedProject Project(Test, Directory.GetPath(), bWithSceneFile);
		REQUIRE_FALSE(Project.IsMaterialWritten());

		SUBCASE("Ctrl+S")
		{
			Test.PressKey(EKey::S, {EKey::LeftControl});
		}
		SUBCASE("File > Save")
		{
			Test.Click("Menu/File");
			Test.Click("Menu/File/Save");
		}
		SUBCASE("The toolbar's Save")
		{
			Test.Click("Toolbar/Save");
		}

		CHECK(Project.IsMaterialWritten());
		CHECK(Project.IsSceneWritten());
		CHECK(Test.Editor.GetEditedAssets().GetUnsaved().empty());
		CHECK_FALSE(Test.HasItem("NotSaved/Close"));
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("Ctrl+S on a scene without a file saves its materials, and then asks where to save the scene")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		const FChangedProject Project(Test, Directory.GetPath(), bWithoutSceneFile);

		Test.PressKey(EKey::S, {EKey::LeftControl});

		CHECK(Project.IsMaterialWritten());
		CHECK(Test.HasItem("SaveScene/Path"));
		CHECK_FALSE(Test.HasItem("NotSaved/Close"));
	}

	TEST_CASE("A material that cannot be written is said in a window, and the scene is saved once it is closed")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		const FChangedProject Project(Test, Directory.GetPath(), bWithSceneFile);
		Project.BlockMaterialFile();

		Test.PressKey(EKey::S, {EKey::LeftControl});

		REQUIRE(Test.HasItem("NotSaved/Close"));
		CHECK_FALSE(Project.IsSceneWritten());
		CHECK(Test.Editor.GetEditedAssets().GetUnsaved() == std::vector<std::string>{std::string(RedPath)});

		SUBCASE("Close")
		{
			Test.Click("NotSaved/Close");
		}
		SUBCASE("Enter")
		{
			Test.PressKey(EKey::Enter);
		}
		SUBCASE("Escape")
		{
			Test.PressKey(EKey::Escape);
		}

		CHECK_FALSE(Test.HasItem("NotSaved/Close"));
		CHECK(Project.IsSceneWritten());
		// The material keeps its changes, to be saved once its file can be written.
		CHECK(Test.Editor.GetEditedAssets().GetUnsaved() == std::vector<std::string>{std::string(RedPath)});
	}

	TEST_CASE("After the window about a material, a scene without a file has the user asked where to save it")
	{
		FEditorUiTest Test;
		const FScopedTempDirectory Directory;
		const FChangedProject Project(Test, Directory.GetPath(), bWithoutSceneFile);
		Project.BlockMaterialFile();
		Test.PressKey(EKey::S, {EKey::LeftControl});
		REQUIRE(Test.HasItem("NotSaved/Close"));
		CHECK_FALSE(Test.HasItem("SaveScene/Path"));

		Test.Click("NotSaved/Close");
		Test.DrawFrame();

		CHECK_FALSE(Test.HasItem("NotSaved/Close"));
		CHECK(Test.HasItem("SaveScene/Path"));
	}
}
