#include "Commands/CommandArguments.h"

#include <utility>

FCommandArguments::FCommandArguments(FJsonValue InValues)
	: Values(std::move(InValues))
{
	WW_CHECKF(Values.IsObject(), "The arguments of a command are a JSON object, not {}", Values.GetTypeDescription());
}

bool FCommandArguments::Has(std::string_view Name) const
{
	return Values.FindMember(Name) != nullptr;
}

bool FCommandArguments::GetBoolean(std::string_view Name) const
{
	const FJsonValue& Value = GetJson(Name);
	WW_CHECKF(Value.IsBoolean(), "The argument \"{}\" is {}, not a boolean", Name, Value.GetTypeDescription());
	return Value.GetBoolean();
}

int64 FCommandArguments::GetInteger(std::string_view Name) const
{
	const FJsonValue& Value = GetJson(Name);
	WW_CHECKF(Value.IsInteger(), "The argument \"{}\" is {}, not an integer", Name, Value.GetTypeDescription());
	return Value.GetInteger();
}

double FCommandArguments::GetNumber(std::string_view Name) const
{
	const FJsonValue& Value = GetJson(Name);
	WW_CHECKF(Value.IsNumber(), "The argument \"{}\" is {}, not a number", Name, Value.GetTypeDescription());
	return Value.GetNumber();
}

const std::string& FCommandArguments::GetString(std::string_view Name) const
{
	const FJsonValue& Value = GetJson(Name);
	WW_CHECKF(Value.IsString(), "The argument \"{}\" is {}, not a string", Name, Value.GetTypeDescription());
	return Value.GetString();
}

const FJsonValue& FCommandArguments::GetJson(std::string_view Name) const
{
	const FJsonValue* const Value = Values.FindMember(Name);
	WW_CHECKF(Value != nullptr, "The command was not given the argument \"{}\"", Name);
	return *Value;
}
