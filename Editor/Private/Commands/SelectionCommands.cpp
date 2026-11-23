#include "Commands/SelectionCommands.h"

#include "Commands/CommandEffect.h"
#include "Commands/CommandRegistry.h"
#include "Commands/CommandTargets.h"
#include "Editor.h"
#include "Json/JsonValue.h"

#include <string>
#include <utility>
#include <vector>

namespace
{
	FCommandResult SetSelection(FEditor& Editor, const FCommandArguments& Arguments)
	{
		std::vector<int64> EntityIds;
		std::string Error;
		if (!FCommandTargets::ReadEntityIds(Editor, Arguments.GetJson("Entities"), EntityIds, Error))
		{
			return FCommandResult::Failure(std::move(Error));
		}

		Editor.SetSelection(EntityIds);
		return FCommandResult::Success();
	}

	FCommandResult GetSelection(FEditor& Editor, const FCommandArguments& /*Arguments*/)
	{
		FJsonValue Entities = FJsonValue::MakeArray();
		for (const int64 EntityId : Editor.GetSelection())
		{
			Entities.AddElement(EntityId);
		}

		FJsonValue Value = FJsonValue::MakeObject();
		Value.SetMember("Entities", std::move(Entities));
		return FCommandResult::Success(std::move(Value));
	}
}

void FSelectionCommands::Register(FCommandRegistry& Registry)
{
	Registry.Register(
		{"Selection.Set",
			"Selects entities: the given ones and no others. What is selected is what the user works on; the "
			"editor's windows show the properties of the entity that is selected last. Selecting changes nothing in "
			"the scene and cannot be undone.",
			{FCommandParameter::Required("Entities",
				ECommandArgumentType::Json,
				"An array of the identifiers of the entities to select, in the order they count as selected. An "
				"empty array selects nothing.")},
			ECommandEffect::LeavesScene},
		SetSelection);

	Registry.Register({"Selection.Get",
						  "Returns the identifiers of the selected entities, in the order they were selected.",
						  {},
						  ECommandEffect::LeavesScene},
		GetSelection);
}
