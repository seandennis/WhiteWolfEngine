#include "Ui/UiCommands.h"

#include "Editor.h"
#include "Json/JsonValue.h"

#include <utility>

void FUiCommands::NewScene(FEditor& Editor)
{
	Editor.ExecuteCommand("Scene.New", FJsonValue());
}

void FUiCommands::Undo(FEditor& Editor)
{
	Editor.ExecuteCommand("Edit.Undo", FJsonValue());
}

void FUiCommands::Redo(FEditor& Editor)
{
	Editor.ExecuteCommand("Edit.Redo", FJsonValue());
}

void FUiCommands::SetSelection(FEditor& Editor, const std::vector<int64>& EntityIds)
{
	FJsonValue Entities = FJsonValue::MakeArray();
	for (const int64 EntityId : EntityIds)
	{
		Entities.AddElement(EntityId);
	}

	FJsonValue Arguments = FJsonValue::MakeObject();
	Arguments.SetMember("Entities", std::move(Entities));
	Editor.ExecuteCommand("Selection.Set", Arguments);
}
