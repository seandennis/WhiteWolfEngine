#pragma once

#include "CoreMinimal.h"

#include "Commands/CommandArgumentType.h"

#include <string>
#include <utility>

/** One parameter of an editor command: the name an argument is given under, its type and what it means. */
struct FCommandParameter
{
	/** The name under which the argument is given. Unique among the parameters of its command. */
	std::string Name;

	ECommandArgumentType Type = ECommandArgumentType::String;

	/** What the argument means, for the people and agents who read the list of commands. */
	std::string Description;

	/** True if the command can be given without this argument. */
	bool bOptional = false;

	/** Returns a parameter that every call of the command has to give an argument for. */
	static FCommandParameter Required(std::string InName, ECommandArgumentType InType, std::string InDescription)
	{
		return FCommandParameter{std::move(InName), InType, std::move(InDescription), false};
	}

	/** Returns a parameter that a call of the command may leave out. */
	static FCommandParameter Optional(std::string InName, ECommandArgumentType InType, std::string InDescription)
	{
		return FCommandParameter{std::move(InName), InType, std::move(InDescription), true};
	}
};
