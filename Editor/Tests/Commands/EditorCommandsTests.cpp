#include "Editor.h"
#include "TestEditorObjects.h"

#include <doctest/doctest.h>

#include <ostream>
#include <string>

TEST_SUITE("Editor commands")
{
	TEST_CASE("Editor.ListCommands returns every command with its summary and parameters")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;

		const FCommandResult Result = Editor.ExecuteCommand("Editor.ListCommands", FJsonValue());

		REQUIRE(Result.HasSucceeded());
		REQUIRE(Result.GetValue().IsArray());
		const FJsonValue::FArray& Commands = Result.GetValue().GetElements();
		REQUIRE(Commands.size() == Editor.GetCommands().GetCommands().size());

		// One command in full: its parameters in order, each with its type and whether it can be left out.
		const FJsonValue* CreateEntity = nullptr;
		for (const FJsonValue& Command : Commands)
		{
			const FJsonValue* const Name = Command.FindMember("Name");
			REQUIRE(Name != nullptr);
			if (*Name == FJsonValue("Entity.Create"))
			{
				CreateEntity = &Command;
			}
		}
		REQUIRE(CreateEntity != nullptr);
		const FJsonValue Expected = ParseTestJson(R"({
			"Name": "Entity.Create",
			"Summary": "Creates an entity in the scene being edited. Returns its identifier.",
			"ChangesScene": true,
			"Parameters": [
				{
					"Name": "Name",
					"Type": "String",
					"Optional": false,
					"Description": "The entity's name. Names need not be unique."
				},
				{
					"Name": "Parent",
					"Type": "Integer",
					"Optional": true,
					"Description": "The identifier of the entity to attach it to. At the root of the scene if left out."
				}
			]
		})");
		CHECK(WriteTestJson(*CreateEntity) == WriteTestJson(Expected));
	}

	TEST_CASE("Editor.ListCommands says which commands leave the scene as it is")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;

		const FCommandResult Result = Editor.ExecuteCommand("Editor.ListCommands", FJsonValue());

		REQUIRE(Result.HasSucceeded());
		for (const FJsonValue& Command : Result.GetValue().GetElements())
		{
			const FJsonValue* const Name = Command.FindMember("Name");
			const FJsonValue* const ChangesScene = Command.FindMember("ChangesScene");
			REQUIRE(Name != nullptr);
			REQUIRE(ChangesScene != nullptr);
			if (*Name == FJsonValue("Scene.GetHierarchy"))
			{
				CHECK(*ChangesScene == FJsonValue(false));
			}
			if (*Name == FJsonValue("Entity.Delete"))
			{
				CHECK(*ChangesScene == FJsonValue(true));
			}
			// It changes a material, and leaves the scene as it is.
			if (*Name == FJsonValue("Asset.SetProperty"))
			{
				CHECK(*ChangesScene == FJsonValue(false));
			}
		}
	}

	TEST_CASE("Editor.ListCommands lists the commands in the order of their names, including ones added later")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		Editor.GetCommands().Register({"Aardvark.Feed", "Feeds the aardvark.", {}},
			[](FEditor& /*CommandEditor*/, const FCommandArguments& /*Arguments*/)
			{ return FCommandResult::Success(); });

		const FCommandResult Result = Editor.ExecuteCommand("Editor.ListCommands", FJsonValue());

		REQUIRE(Result.HasSucceeded());
		const FJsonValue::FArray& Commands = Result.GetValue().GetElements();
		REQUIRE(Commands.size() >= 2);
		CHECK(WriteTestJson(Commands[0]) ==
			R"({"Name":"Aardvark.Feed","Summary":"Feeds the aardvark.","ChangesScene":true,"Parameters":[]})");
		std::string PreviousName;
		for (const FJsonValue& Command : Commands)
		{
			const FJsonValue* const Name = Command.FindMember("Name");
			REQUIRE(Name != nullptr);
			CHECK(PreviousName < Name->GetString());
			PreviousName = Name->GetString();
		}
	}
}
