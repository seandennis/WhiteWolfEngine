#include "Commands/EditCommands.h"

#include "Commands/CommandEffect.h"
#include "Commands/CommandRegistry.h"
#include "Commands/CommandWhilePlaying.h"
#include "Editor.h"
#include "History/UndoHistory.h"
#include "Json/JsonValue.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{
	/** Returns names as a JSON array of strings. */
	FJsonValue MakeNameList(std::vector<std::string> Names)
	{
		FJsonValue List = FJsonValue::MakeArray();
		for (std::string& Name : Names)
		{
			List.AddElement(std::move(Name));
		}
		return List;
	}

	FCommandResult Undo(FEditor& Editor, const FCommandArguments& /*Arguments*/)
	{
		std::optional<std::string> StepName = Editor.Undo();
		if (!StepName.has_value())
		{
			return FCommandResult::Failure("There is nothing to undo.");
		}

		FJsonValue Value = FJsonValue::MakeObject();
		Value.SetMember("Undone", std::move(*StepName));
		return FCommandResult::Success(std::move(Value));
	}

	FCommandResult Redo(FEditor& Editor, const FCommandArguments& /*Arguments*/)
	{
		std::optional<std::string> StepName = Editor.Redo();
		if (!StepName.has_value())
		{
			return FCommandResult::Failure("There is nothing to redo.");
		}

		FJsonValue Value = FJsonValue::MakeObject();
		Value.SetMember("Redone", std::move(*StepName));
		return FCommandResult::Success(std::move(Value));
	}

	FCommandResult GetHistory(FEditor& Editor, const FCommandArguments& /*Arguments*/)
	{
		const FUndoHistory& History = Editor.GetHistory();

		FJsonValue Value = FJsonValue::MakeObject();
		Value.SetMember("Undo", MakeNameList(History.GetUndoStepNames()));
		Value.SetMember("Redo", MakeNameList(History.GetRedoStepNames()));
		return FCommandResult::Success(std::move(Value));
	}
}

void FEditCommands::Register(FCommandRegistry& Registry)
{
	Registry.Register(
		{"Edit.Undo",
			"Undoes the last change to the scene or to a material: puts both back as they were before the last "
			"command that changed one of them. Returns the name of that command. Entities and components keep their "
			"identifiers. No file is written: a material is put back in memory, and is unsaved while it then "
			"differs from its file, as Asset.GetProperties says (\"Unsaved\"); Asset.Save writes it.",
			{},
			ECommandEffect::ChangesScene,
			ECommandWhilePlaying::Refused},
		Undo);

	Registry.Register(
		{"Edit.Redo",
			"Redoes the change that was undone last. Returns the name of the command that made it. Nothing can be "
			"redone once the scene or a material has changed since the undo.",
			{},
			ECommandEffect::ChangesScene,
			ECommandWhilePlaying::Refused},
		Redo);

	Registry.Register(
		{"Edit.GetHistory",
			"Returns the names of the commands whose changes can be undone, the most recent first, and of those "
			"that can be redone, the next one first. A change made without a command is called \"Direct change\".",
			{},
			ECommandEffect::LeavesScene},
		GetHistory);
}
