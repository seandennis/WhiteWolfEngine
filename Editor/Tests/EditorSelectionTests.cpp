#include "Editor.h"
#include "Object/ObjectGlobals.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "TestEditorObjects.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <format>
#include <ostream>
#include <vector>

TEST_SUITE("FEditor selection")
{
	TEST_CASE("A new editor has nothing selected")
	{
		const FScopedEditorTest EditorTest;
		const FEditor Editor;

		CHECK(Editor.GetSelection().empty());
		CHECK_FALSE(Editor.IsSelected(1));
	}

	TEST_CASE("SetSelection selects the given entities, in the given order, and no others")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		const int64 Player = Editor.GetScene().CreateEntity("Player")->GetId();
		const int64 Ground = Editor.GetScene().CreateEntity("Ground")->GetId();
		const int64 Lamp = Editor.GetScene().CreateEntity("Lamp")->GetId();

		Editor.SetSelection({Lamp, Player});

		CHECK(Editor.GetSelection() == std::vector<int64>{Lamp, Player});
		CHECK(Editor.IsSelected(Lamp));
		CHECK(Editor.IsSelected(Player));
		CHECK_FALSE(Editor.IsSelected(Ground));

		Editor.SetSelection({Ground});

		CHECK(Editor.GetSelection() == std::vector<int64>{Ground});
		CHECK_FALSE(Editor.IsSelected(Lamp));

		Editor.SetSelection({});

		CHECK(Editor.GetSelection().empty());
	}

	TEST_CASE("An identifier that is given twice counts where it is given first")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		const int64 Player = Editor.GetScene().CreateEntity("Player")->GetId();
		const int64 Ground = Editor.GetScene().CreateEntity("Ground")->GetId();

		Editor.SetSelection({Player, Ground, Player});

		CHECK(Editor.GetSelection() == std::vector<int64>{Player, Ground});
	}

	TEST_CASE("An identifier that no entity has is ignored, also once an entity gets it")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		const int64 Player = Editor.GetScene().CreateEntity("Player")->GetId();
		const int64 NextIdentifier = Player + 1;

		Editor.SetSelection({NextIdentifier, Player});
		CHECK(Editor.GetSelection() == std::vector<int64>{Player});

		const int64 Ground = Editor.GetScene().CreateEntity("Ground")->GetId();
		REQUIRE(Ground == NextIdentifier);
		CHECK(Editor.GetSelection() == std::vector<int64>{Player});
		CHECK_FALSE(Editor.IsSelected(Ground));
	}

	TEST_CASE("A component's identifier does not select anything")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		WEntity* const Player = Editor.GetScene().CreateEntity("Player");
		const int64 Component = Player->AddComponent<WEditorTestComponent>()->GetId();

		Editor.SetSelection({Component});

		CHECK(Editor.GetSelection().empty());
	}

	TEST_CASE("A destroyed entity is no longer selected, and is selected again when an undo brings it back")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		// Created with commands, so that each is a step of the history and the undo below undoes only the deletion.
		REQUIRE(Editor.ExecuteCommand("Entity.Create", ParseTestJson(R"({"Name": "Player"})")).HasSucceeded());
		REQUIRE(Editor.ExecuteCommand("Entity.Create", ParseTestJson(R"({"Name": "Ground"})")).HasSucceeded());
		const int64 Player = Editor.GetScene().FindEntityByName("Player")->GetId();
		const int64 Ground = Editor.GetScene().FindEntityByName("Ground")->GetId();
		Editor.SetSelection({Player, Ground});

		REQUIRE(Editor.ExecuteCommand("Entity.Delete", ParseTestJson(std::format(R"({{"Entity": {}}})", Player)))
					.HasSucceeded());

		CHECK(Editor.GetSelection() == std::vector<int64>{Ground});
		CHECK_FALSE(Editor.IsSelected(Player));

		REQUIRE(Editor.Undo().has_value());

		CHECK(Editor.GetSelection() == std::vector<int64>{Player, Ground});
		CHECK(Editor.IsSelected(Player));
	}

	TEST_CASE("Selecting is not a step of the history")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		REQUIRE(Editor.ExecuteCommand("Entity.Create", ParseTestJson(R"({"Name": "Player"})")).HasSucceeded());
		const int64 Player = Editor.GetScene().FindEntityByName("Player")->GetId();

		Editor.SetSelection({Player});

		CHECK(Editor.GetHistory().GetUndoStepNames().size() == 1);

		// Undoing the creation takes the entity away, and what was selected with it.
		REQUIRE(Editor.Undo().has_value());
		CHECK(Editor.GetSelection().empty());
	}

	TEST_CASE("A scene that replaces the one being edited starts with nothing selected")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		const int64 Player = Editor.GetScene().CreateEntity("Player")->GetId();
		Editor.SetSelection({Player});

		WScene* const NewScene = NewObject<WScene>();
		const int64 Other = NewScene->CreateEntity("Other")->GetId();
		// The same number names another entity in the new scene.
		REQUIRE(Other == Player);
		Editor.SetScene(*NewScene, std::filesystem::path());

		CHECK(Editor.GetSelection().empty());
		CHECK_FALSE(Editor.IsSelected(Other));
	}

	TEST_CASE("Giving the scene a file leaves what is selected")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		const int64 Player = Editor.GetScene().CreateEntity("Player")->GetId();
		Editor.SetSelection({Player});

		Editor.SetScene(Editor.GetScene(), std::filesystem::path("Saved.json"));

		CHECK(Editor.GetSelection() == std::vector<int64>{Player});
	}
}
