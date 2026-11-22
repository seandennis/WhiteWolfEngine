#include "Commands/EditorCommands.h"

#include "Commands/CommandArgumentType.h"
#include "Commands/CommandEffect.h"
#include "Commands/CommandRegistry.h"
#include "Editor.h"
#include "Json/JsonValue.h"

#include <utility>

namespace
{
	FCommandResult ListCommands(FEditor& Editor, const FCommandArguments& /*Arguments*/)
	{
		FJsonValue CommandList = FJsonValue::MakeArray();
		for (const FCommandDescription* const Description : Editor.GetCommands().GetCommands())
		{
			FJsonValue ParameterList = FJsonValue::MakeArray();
			for (const FCommandParameter& Parameter : Description->Parameters)
			{
				FJsonValue ParameterJson = FJsonValue::MakeObject();
				ParameterJson.SetMember("Name", Parameter.Name);
				ParameterJson.SetMember("Type", GetCommandArgumentTypeName(Parameter.Type));
				ParameterJson.SetMember("Optional", Parameter.bOptional);
				ParameterJson.SetMember("Description", Parameter.Description);
				ParameterList.AddElement(std::move(ParameterJson));
			}

			FJsonValue CommandJson = FJsonValue::MakeObject();
			CommandJson.SetMember("Name", Description->Name);
			CommandJson.SetMember("Summary", Description->Summary);
			CommandJson.SetMember("ChangesScene", Description->Effect == ECommandEffect::ChangesScene);
			CommandJson.SetMember("Parameters", std::move(ParameterList));
			CommandList.AddElement(std::move(CommandJson));
		}
		return FCommandResult::Success(std::move(CommandList));
	}
}

void FEditorCommands::Register(FCommandRegistry& Registry)
{
	Registry.Register(
		{"Editor.ListCommands",
			"Returns every command of the editor, in the order of their names: its summary, whether it can change the "
			"scene, and its parameters.",
			{},
			ECommandEffect::LeavesScene},
		ListCommands);
}
