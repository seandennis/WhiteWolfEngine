#include "Editor.h"
#include "Object/ObjectGlobals.h"
#include "Object/WeakObjectPtr.h"
#include "Scene/Component.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "TestEditorObjects.h"

#include <doctest/doctest.h>

#include <format>
#include <ostream>
#include <string>
#include <vector>

TEST_SUITE("Entity commands")
{
	TEST_CASE("Entity.Create adds a named entity at the root of the scene and returns its identifier")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;

		const FCommandResult Result = Editor.ExecuteCommand("Entity.Create", ParseTestJson(R"({"Name": "Player"})"));

		REQUIRE(Result.HasSucceeded());
		const std::vector<WEntity*> Entities = Editor.GetScene().GetAllEntities();
		REQUIRE(Entities.size() == 1);
		CHECK(Entities[0]->GetName() == "Player");
		CHECK(Entities[0]->GetParent() == nullptr);
		CHECK(WriteTestJson(Result.GetValue()) == std::format(R"({{"Id":{}}})", Entities[0]->GetId()));
	}

	TEST_CASE("Entity.Create attaches the entity to the given parent")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		WEntity* const Parent = Editor.GetScene().CreateEntity("Player");
		REQUIRE(Parent != nullptr);

		const FCommandResult Result = Editor.ExecuteCommand(
			"Entity.Create", ParseTestJson(std::format(R"({{"Name": "Camera", "Parent": {}}})", Parent->GetId())));

		REQUIRE(Result.HasSucceeded());
		REQUIRE(Parent->GetChildren().size() == 1);
		const WEntity* const Child = Parent->GetChildren()[0];
		CHECK(Child->GetName() == "Camera");
		CHECK(WriteTestJson(Result.GetValue()) == std::format(R"({{"Id":{}}})", Child->GetId()));
	}

	TEST_CASE("Entity.Create under a parent that does not exist fails and creates nothing")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;

		const FCommandResult Result =
			Editor.ExecuteCommand("Entity.Create", ParseTestJson(R"({"Name": "Camera", "Parent": 99})"));

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError() == "There is no entity with the identifier 99.");
		CHECK(Editor.GetScene().GetAllEntities().empty());
	}

	TEST_CASE("Entity.Delete destroys an entity with its descendants")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		WScene& Scene = Editor.GetScene();
		WEntity* const Player = Scene.CreateEntity("Player");
		REQUIRE(Player != nullptr);
		WEntity* const Camera = Scene.CreateEntity("Camera", Player);
		REQUIRE(Camera != nullptr);
		WEntity* const Ground = Scene.CreateEntity("Ground");
		REQUIRE(Ground != nullptr);
		const TWeakObjectPtr<WComponent> Component = Camera->AddComponent<WEditorTestComponent>();
		REQUIRE(Component.IsValid());

		const FCommandResult Result =
			Editor.ExecuteCommand("Entity.Delete", ParseTestJson(std::format(R"({{"Entity": {}}})", Player->GetId())));

		REQUIRE(Result.HasSucceeded());
		CHECK(Result.GetValue().IsNull());
		CHECK_FALSE(IsValid(Player));
		CHECK_FALSE(IsValid(Camera));
		CHECK_FALSE(Component.IsValid());
		REQUIRE(Scene.GetAllEntities().size() == 1);
		CHECK(Scene.GetAllEntities()[0] == Ground);
	}

	TEST_CASE("Entity.Delete of an entity that does not exist fails")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		WEntity* const Player = Editor.GetScene().CreateEntity("Player");
		REQUIRE(Player != nullptr);
		const WComponent* const Component = Player->AddComponent<WEditorTestComponent>();
		REQUIRE(Component != nullptr);
		int64 MissingId = 99;

		SUBCASE("An identifier that nothing has")
		{
		}
		SUBCASE("The identifier of a component")
		{
			MissingId = Component->GetId();
		}

		const FCommandResult Result =
			Editor.ExecuteCommand("Entity.Delete", ParseTestJson(std::format(R"({{"Entity": {}}})", MissingId)));

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError() == std::format("There is no entity with the identifier {}.", MissingId));
		CHECK(IsValid(Player));
		CHECK(IsValid(Component));
	}

	TEST_CASE("An entity that was deleted cannot be deleted again")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		const WEntity* const Player = Editor.GetScene().CreateEntity("Player");
		REQUIRE(Player != nullptr);
		const FJsonValue Arguments = ParseTestJson(std::format(R"({{"Entity": {}}})", Player->GetId()));
		REQUIRE(Editor.ExecuteCommand("Entity.Delete", Arguments).HasSucceeded());

		const FCommandResult Result = Editor.ExecuteCommand("Entity.Delete", Arguments);

		CHECK_FALSE(Result.HasSucceeded());
	}

	TEST_CASE("Entity.Rename gives an entity another name")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		const WEntity* const Entity = Editor.GetScene().CreateEntity("Player");
		REQUIRE(Entity != nullptr);

		const FCommandResult Result = Editor.ExecuteCommand(
			"Entity.Rename", ParseTestJson(std::format(R"({{"Entity": {}, "Name": "Hero"}})", Entity->GetId())));

		REQUIRE(Result.HasSucceeded());
		CHECK(Entity->GetName() == "Hero");
	}

	TEST_CASE("Entity.Rename of an entity that does not exist fails")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;

		const FCommandResult Result =
			Editor.ExecuteCommand("Entity.Rename", ParseTestJson(R"({"Entity": 99, "Name": "Hero"})"));

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError() == "There is no entity with the identifier 99.");
	}

	TEST_CASE("Entity.SetParent moves an entity under another and leaves it where it is in the world")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		WEntity* const Parent = Editor.GetScene().CreateEntity("Parent");
		REQUIRE(Parent != nullptr);
		Parent->SetWorldLocation(FVector(10.0, 0.0, 0.0));
		WEntity* const Child = Editor.GetScene().CreateEntity("Child");
		REQUIRE(Child != nullptr);
		Child->SetWorldLocation(FVector(1.0, 0.0, 0.0));
		std::string ArgumentText = std::format(R"({{"Entity": {}, "Parent": {}}})", Child->GetId(), Parent->GetId());

		SUBCASE("By default")
		{
		}
		SUBCASE("When asked to")
		{
			ArgumentText = std::format(
				R"({{"Entity": {}, "Parent": {}, "KeepWorldTransform": true}})", Child->GetId(), Parent->GetId());
		}

		const FCommandResult Result = Editor.ExecuteCommand("Entity.SetParent", ParseTestJson(ArgumentText));

		REQUIRE(Result.HasSucceeded());
		CHECK(Child->GetParent() == Parent);
		CHECK(Child->GetWorldLocation() == FVector(1.0, 0.0, 0.0));
		CHECK(Child->GetRelativeTransform().Location == FVector(-9.0, 0.0, 0.0));
	}

	TEST_CASE("Entity.SetParent keeps the relative transform instead when asked to")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		WEntity* const Parent = Editor.GetScene().CreateEntity("Parent");
		REQUIRE(Parent != nullptr);
		Parent->SetWorldLocation(FVector(10.0, 0.0, 0.0));
		WEntity* const Child = Editor.GetScene().CreateEntity("Child");
		REQUIRE(Child != nullptr);
		Child->SetWorldLocation(FVector(1.0, 0.0, 0.0));

		const FCommandResult Result = Editor.ExecuteCommand("Entity.SetParent",
			ParseTestJson(std::format(
				R"({{"Entity": {}, "Parent": {}, "KeepWorldTransform": false}})", Child->GetId(), Parent->GetId())));

		REQUIRE(Result.HasSucceeded());
		CHECK(Child->GetParent() == Parent);
		CHECK(Child->GetRelativeTransform().Location == FVector(1.0, 0.0, 0.0));
		CHECK(Child->GetWorldLocation() == FVector(11.0, 0.0, 0.0));
	}

	TEST_CASE("Entity.SetParent without a parent moves the entity to the root of the scene")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		WEntity* const Parent = Editor.GetScene().CreateEntity("Parent");
		REQUIRE(Parent != nullptr);
		const WEntity* const Child = Editor.GetScene().CreateEntity("Child", Parent);
		REQUIRE(Child != nullptr);

		const FCommandResult Result = Editor.ExecuteCommand(
			"Entity.SetParent", ParseTestJson(std::format(R"({{"Entity": {}}})", Child->GetId())));

		REQUIRE(Result.HasSucceeded());
		CHECK(Child->GetParent() == nullptr);
		CHECK(Parent->GetChildren().empty());
		REQUIRE(Editor.GetScene().GetRootEntities().size() == 2);
		CHECK(Editor.GetScene().GetRootEntities()[1].Get() == Child);
	}

	TEST_CASE("Entity.SetParent refuses to put an entity under itself or one of its descendants")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		WEntity* const Grandparent = Editor.GetScene().CreateEntity("Grandparent");
		REQUIRE(Grandparent != nullptr);
		WEntity* const Parent = Editor.GetScene().CreateEntity("Parent", Grandparent);
		REQUIRE(Parent != nullptr);
		const WEntity* const Child = Editor.GetScene().CreateEntity("Child", Parent);
		REQUIRE(Child != nullptr);
		int64 NewParentId = 0;

		SUBCASE("Itself")
		{
			NewParentId = Grandparent->GetId();
		}
		SUBCASE("Its child")
		{
			NewParentId = Parent->GetId();
		}
		SUBCASE("A descendant further down")
		{
			NewParentId = Child->GetId();
		}

		const FCommandResult Result = Editor.ExecuteCommand("Entity.SetParent",
			ParseTestJson(std::format(R"({{"Entity": {}, "Parent": {}}})", Grandparent->GetId(), NewParentId)));

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError() ==
			std::format("The entity {} cannot become a child of the entity {}, which is the entity itself or one "
						"of its descendants.",
				Grandparent->GetId(),
				NewParentId));
		CHECK(Grandparent->GetParent() == nullptr);
		CHECK(Parent->GetParent() == Grandparent);
		CHECK(Child->GetParent() == Parent);
	}

	TEST_CASE("Entity.SetParent with an entity or a parent that does not exist fails")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		const WEntity* const Entity = Editor.GetScene().CreateEntity("Entity");
		REQUIRE(Entity != nullptr);
		std::string ArgumentText;

		SUBCASE("The entity")
		{
			ArgumentText = std::format(R"({{"Entity": 99, "Parent": {}}})", Entity->GetId());
		}
		SUBCASE("The parent")
		{
			ArgumentText = std::format(R"({{"Entity": {}, "Parent": 99}})", Entity->GetId());
		}

		const FCommandResult Result = Editor.ExecuteCommand("Entity.SetParent", ParseTestJson(ArgumentText));

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError() == "There is no entity with the identifier 99.");
		CHECK(Entity->GetParent() == nullptr);
	}

	TEST_CASE("Entity.Duplicate copies an entity with its components and descendants, and returns the copy")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		WScene& Scene = Editor.GetScene();
		WEntity* const Player = Scene.CreateEntity("Player");
		WEntity* const Camera = Scene.CreateEntity("Camera", Player);
		WEditorTestComponent* const Component = Player->AddComponent<WEditorTestComponent>();
		Component->Health = 55;
		Component->TargetEntity = Camera;

		const FCommandResult Result = Editor.ExecuteCommand(
			"Entity.Duplicate", ParseTestJson(std::format(R"({{"Entity": {}}})", Player->GetId())));

		REQUIRE(Result.HasSucceeded());
		REQUIRE(Scene.GetRootEntities().size() == 2);
		const WEntity* const Copy = Scene.GetRootEntities()[1];
		CHECK(WriteTestJson(Result.GetValue()) == std::format(R"({{"Id":{}}})", Copy->GetId()));
		CHECK(Copy->GetName() == "Player");
		CHECK(Copy->GetId() != Player->GetId());
		REQUIRE(Copy->GetChildren().size() == 1);
		CHECK(Copy->GetChildren()[0]->GetName() == "Camera");
		const WEditorTestComponent* const ComponentCopy = Copy->FindComponent<WEditorTestComponent>();
		REQUIRE(ComponentCopy != nullptr);
		CHECK(ComponentCopy->Health == 55);
		// The copy's component looks at the copy's camera, not at the original's.
		CHECK(ComponentCopy->TargetEntity == Copy->GetChildren()[0]);
		CHECK(Component->TargetEntity == Camera);
	}

	TEST_CASE("Entity.Duplicate is one step of the history, which an undo takes back whole")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		REQUIRE(Editor.ExecuteCommand("Entity.Create", ParseTestJson(R"({"Name": "Player"})")).HasSucceeded());
		const int64 Player = Editor.GetScene().FindEntityByName("Player")->GetId();
		REQUIRE(Editor
					.ExecuteCommand(
						"Entity.Create", ParseTestJson(std::format(R"({{"Name": "Camera", "Parent": {}}})", Player)))
					.HasSucceeded());

		REQUIRE(Editor.ExecuteCommand("Entity.Duplicate", ParseTestJson(std::format(R"({{"Entity": {}}})", Player)))
					.HasSucceeded());
		CHECK(Editor.GetScene().GetAllEntities().size() == 4);
		CHECK(Editor.GetHistory().GetUndoStepNames().front() == "Entity.Duplicate");

		REQUIRE(Editor.Undo().has_value());
		CHECK(Editor.GetScene().GetAllEntities().size() == 2);

		REQUIRE(Editor.Redo().has_value());
		CHECK(Editor.GetScene().GetAllEntities().size() == 4);
	}

	TEST_CASE("Entity.Duplicate of an entity that does not exist fails and copies nothing")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		WEntity* const Player = Editor.GetScene().CreateEntity("Player");
		const int64 Component = Player->AddComponent<WEditorTestComponent>()->GetId();
		std::string ArgumentText;
		std::string ExpectedError;

		SUBCASE("An identifier that nothing has")
		{
			ArgumentText = R"({"Entity": 99})";
			ExpectedError = "There is no entity with the identifier 99.";
		}
		SUBCASE("The identifier of a component")
		{
			ArgumentText = std::format(R"({{"Entity": {}}})", Component);
			ExpectedError = std::format("There is no entity with the identifier {}.", Component);
		}

		const FCommandResult Result = Editor.ExecuteCommand("Entity.Duplicate", ParseTestJson(ArgumentText));

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError() == ExpectedError);
		CHECK(Editor.GetScene().GetAllEntities().size() == 1);
	}
}
