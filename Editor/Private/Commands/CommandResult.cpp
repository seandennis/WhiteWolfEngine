#include "Commands/CommandResult.h"

#include <utility>

FCommandResult FCommandResult::Success(FJsonValue InValue)
{
	FCommandResult Result;
	Result.bSucceeded = true;
	Result.Value = std::move(InValue);
	return Result;
}

FCommandResult FCommandResult::Failure(std::string InError)
{
	WW_CHECKF(!InError.empty(), "A failed command result needs a reason");

	FCommandResult Result;
	Result.Error = std::move(InError);
	return Result;
}

FJsonValue FCommandResult::ToJson() const
{
	FJsonValue Json = FJsonValue::MakeObject();
	Json.SetMember("Succeeded", bSucceeded);
	if (bSucceeded)
	{
		Json.SetMember("Value", Value);
	}
	else
	{
		Json.SetMember("Error", Error);
	}
	return Json;
}
