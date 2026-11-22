#include "Commands/CommandRegistry.h"

#include "Commands/CommandArgumentType.h"
#include "Commands/CommandParameter.h"
#include "Json/JsonValue.h"

#include <algorithm>
#include <cstddef>
#include <format>
#include <utility>

namespace
{
	/** Returns true if Value is of the type that a parameter of the given type takes. */
	bool IsOfType(const FJsonValue& Value, ECommandArgumentType Type)
	{
		switch (Type)
		{
			case ECommandArgumentType::Boolean:
				return Value.IsBoolean();
			case ECommandArgumentType::Integer:
				return Value.IsInteger();
			case ECommandArgumentType::Number:
				return Value.IsNumber();
			case ECommandArgumentType::String:
				return Value.IsString();
			case ECommandArgumentType::Json:
				return true;
			default:
				return false;
		}
	}

	/** Returns a type in words that fit into a sentence: "a boolean", "an integer". */
	std::string_view DescribeType(ECommandArgumentType Type)
	{
		switch (Type)
		{
			case ECommandArgumentType::Boolean:
				return "a boolean";
			case ECommandArgumentType::Integer:
				return "an integer";
			case ECommandArgumentType::Number:
				return "a number";
			case ECommandArgumentType::String:
				return "a string";
			case ECommandArgumentType::Json:
				return "a JSON value";
			default:
				return "a value of an unknown type";
		}
	}

	/**
	 * Returns what an argument of the wrong type is, in words that fit into a sentence. For an Integer parameter
	 * that was given a number, it says what kind of number, because "a number" would not say what is wrong.
	 */
	std::string_view DescribeMismatch(const FJsonValue& Value, ECommandArgumentType ExpectedType)
	{
		if (ExpectedType == ECommandArgumentType::Integer && Value.IsNumber())
		{
			return Value.IsUnsignedInteger() ? "a number too large for an integer"
											 : "a number that is not written as an integer";
		}
		return Value.GetTypeDescription();
	}

	/**
	 * Checks the arguments of a command against its parameters.
	 *
	 * @param Arguments A JSON object.
	 * @param OutError Receives what is wrong with the first argument that does not fit.
	 * @return True if the command can be given these arguments.
	 */
	bool CheckArguments(const FCommandDescription& Description, const FJsonValue& Arguments, std::string& OutError)
	{
		for (const FJsonValue::FMember& Argument : Arguments.GetMembers())
		{
			const bool bHasParameter = std::ranges::any_of(Description.Parameters,
				[&Argument](const FCommandParameter& Parameter) { return Parameter.Name == Argument.first; });
			if (!bHasParameter)
			{
				OutError = std::format("The command \"{}\" has no parameter \"{}\".", Description.Name, Argument.first);
				return false;
			}
		}

		for (const FCommandParameter& Parameter : Description.Parameters)
		{
			const FJsonValue* const Argument = Arguments.FindMember(Parameter.Name);
			if (Argument == nullptr)
			{
				if (Parameter.bOptional)
				{
					continue;
				}
				OutError =
					std::format("The command \"{}\" needs the argument \"{}\".", Description.Name, Parameter.Name);
				return false;
			}
			if (!IsOfType(*Argument, Parameter.Type))
			{
				OutError = std::format("The argument \"{}\" of the command \"{}\" must be {}, but it is {}.",
					Parameter.Name,
					Description.Name,
					DescribeType(Parameter.Type),
					DescribeMismatch(*Argument, Parameter.Type));
				return false;
			}
		}
		return true;
	}
}

void FCommandRegistry::Register(FCommandDescription Description, FCommandFunction Function)
{
	WW_CHECKF(!Description.Name.empty(), "A command needs a name");
	WW_CHECKF(!Commands.contains(Description.Name), "The command \"{}\" is registered twice", Description.Name);
	WW_CHECKF(Function != nullptr, "The command \"{}\" has no function", Description.Name);
	for (const FCommandParameter& Parameter : Description.Parameters)
	{
		WW_CHECKF(!Parameter.Name.empty(), "A parameter of the command \"{}\" has no name", Description.Name);
		const std::ptrdiff_t ParameterCount = std::ranges::count_if(Description.Parameters,
			[&Parameter](const FCommandParameter& Other) { return Other.Name == Parameter.Name; });
		WW_CHECKF(ParameterCount == 1,
			"The command \"{}\" has more than one parameter \"{}\"",
			Description.Name,
			Parameter.Name);
	}

	std::string Name = Description.Name;
	Commands.emplace(std::move(Name), FEntry{std::move(Description), std::move(Function)});
}

const FCommandDescription* FCommandRegistry::Find(std::string_view Name) const
{
	const auto Found = Commands.find(Name);
	return Found != Commands.end() ? &Found->second.Description : nullptr;
}

std::vector<const FCommandDescription*> FCommandRegistry::GetCommands() const
{
	std::vector<const FCommandDescription*> Descriptions;
	Descriptions.reserve(Commands.size());
	for (const std::pair<const std::string, FEntry>& Command : Commands)
	{
		Descriptions.push_back(&Command.second.Description);
	}
	return Descriptions;
}

FCommandResult FCommandRegistry::Execute(FEditor& Editor, std::string_view Name, const FJsonValue& Arguments) const
{
	const auto Found = Commands.find(Name);
	if (Found == Commands.end())
	{
		return FCommandResult::Failure(std::format("There is no command \"{}\".", Name));
	}
	const FEntry& Entry = Found->second;

	if (!Arguments.IsNull() && !Arguments.IsObject())
	{
		return FCommandResult::Failure(
			std::format("The arguments of the command \"{}\" must be a JSON object, but they are {}.",
				Name,
				Arguments.GetTypeDescription()));
	}
	FJsonValue ArgumentObject = Arguments.IsNull() ? FJsonValue::MakeObject() : Arguments;

	std::string Error;
	if (!CheckArguments(Entry.Description, ArgumentObject, Error))
	{
		return FCommandResult::Failure(std::move(Error));
	}

	return Entry.Function(Editor, FCommandArguments(std::move(ArgumentObject)));
}
