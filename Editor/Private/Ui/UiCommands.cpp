#include "Ui/UiCommands.h"

#include "Commands/CommandResult.h"
#include "Editor.h"
#include "History/ScopedEditorStep.h"
#include "Json/JsonValue.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"

#include <algorithm>
#include <utility>

namespace
{
	/** Returns the arguments of a command that acts on one entity. */
	FJsonValue MakeEntityArguments(int64 EntityId)
	{
		FJsonValue Arguments = FJsonValue::MakeObject();
		Arguments.SetMember("Entity", EntityId);
		return Arguments;
	}

	/**
	 * Returns those of the entities that exist and are not below another of them, in the order they were given: the
	 * ones to act on when what is done to an entity is done to everything below it.
	 */
	std::vector<int64> GetTopmostEntities(const FEditor& Editor, const std::vector<int64>& EntityIds)
	{
		const WScene& Scene = Editor.GetScene();
		std::vector<int64> Topmost;
		for (const int64 EntityId : EntityIds)
		{
			const WEntity* const Entity = Scene.FindEntityById(EntityId);
			if (Entity == nullptr)
			{
				continue;
			}

			const bool bIsBelowAnother = std::ranges::any_of(EntityIds,
				[&Scene, Entity](int64 OtherId) { return Entity->IsDescendantOf(Scene.FindEntityById(OtherId)); });
			if (!bIsBelowAnother)
			{
				Topmost.push_back(EntityId);
			}
		}
		return Topmost;
	}
}

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

std::optional<int64> FUiCommands::CreateEntity(FEditor& Editor, const std::string& Name, std::optional<int64> ParentId)
{
	FJsonValue Arguments = FJsonValue::MakeObject();
	Arguments.SetMember("Name", Name);
	if (ParentId.has_value())
	{
		Arguments.SetMember("Parent", *ParentId);
	}

	const FScopedEditorStep Step(Editor, "Create Entity");
	const FCommandResult Result = Editor.ExecuteCommand("Entity.Create", Arguments);
	if (!Result.HasSucceeded())
	{
		return std::nullopt;
	}

	const int64 EntityId = Result.GetValue().FindMember("Id")->GetInteger();
	SetSelection(Editor, {EntityId});
	return EntityId;
}

void FUiCommands::RenameEntity(FEditor& Editor, int64 EntityId, const std::string& Name)
{
	FJsonValue Arguments = MakeEntityArguments(EntityId);
	Arguments.SetMember("Name", Name);

	const FScopedEditorStep Step(Editor, "Rename");
	Editor.ExecuteCommand("Entity.Rename", Arguments);
}

void FUiCommands::DeleteEntities(FEditor& Editor, const std::vector<int64>& EntityIds)
{
	const FScopedEditorStep Step(Editor, "Delete");
	for (const int64 EntityId : GetTopmostEntities(Editor, EntityIds))
	{
		Editor.ExecuteCommand("Entity.Delete", MakeEntityArguments(EntityId));
	}
}

void FUiCommands::DuplicateEntities(FEditor& Editor, const std::vector<int64>& EntityIds)
{
	const std::vector<int64> Originals = GetTopmostEntities(Editor, EntityIds);
	if (Originals.empty())
	{
		return;
	}

	const FScopedEditorStep Step(Editor, "Duplicate");
	std::vector<int64> Copies;
	for (const int64 EntityId : Originals)
	{
		const FCommandResult Result = Editor.ExecuteCommand("Entity.Duplicate", MakeEntityArguments(EntityId));
		if (Result.HasSucceeded())
		{
			Copies.push_back(Result.GetValue().FindMember("Id")->GetInteger());
		}
	}
	// The copies are what the user goes on to work with, as after a paste.
	SetSelection(Editor, Copies);
}

void FUiCommands::AttachEntities(FEditor& Editor, const std::vector<int64>& EntityIds, std::optional<int64> ParentId)
{
	const FScopedEditorStep Step(Editor, "Attach");
	for (const int64 EntityId : GetTopmostEntities(Editor, EntityIds))
	{
		FJsonValue Arguments = MakeEntityArguments(EntityId);
		if (ParentId.has_value())
		{
			Arguments.SetMember("Parent", *ParentId);
		}
		Editor.ExecuteCommand("Entity.SetParent", Arguments);
	}
}

bool FUiCommands::CanAttachEntities(
	const FEditor& Editor, const std::vector<int64>& EntityIds, std::optional<int64> ParentId)
{
	if (!ParentId.has_value())
	{
		return true;
	}

	const WScene& Scene = Editor.GetScene();
	const WEntity* const Parent = Scene.FindEntityById(*ParentId);
	if (Parent == nullptr)
	{
		return false;
	}
	return std::ranges::none_of(EntityIds,
		[&Scene, Parent, &ParentId](int64 EntityId)
		{ return EntityId == *ParentId || Parent->IsDescendantOf(Scene.FindEntityById(EntityId)); });
}
