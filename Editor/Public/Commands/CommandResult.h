#pragma once

#include "CoreMinimal.h"

#include "Json/JsonValue.h"

#include <string>

/**
 * What an editor command returns: either success, with a value, or failure, with the reason.
 *
 * Every command returns one, whether a person, a test or an agent gave it, so that whoever gave a command can check
 * what it did.
 */
class FCommandResult
{
public:
	/**
	 * Returns a successful result.
	 *
	 * @param InValue What the command returns; JSON null for a command that has nothing to return.
	 */
	EDITOR_API static FCommandResult Success(FJsonValue InValue = FJsonValue());

	/**
	 * Returns a failed result.
	 *
	 * @param InError Why the command failed, as a sentence for whoever gave it. Must not be empty.
	 */
	EDITOR_API static FCommandResult Failure(std::string InError);

	bool HasSucceeded() const
	{
		return bSucceeded;
	}

	/** Returns what the command returned. JSON null after a failure. */
	const FJsonValue& GetValue() const
	{
		return Value;
	}

	/** Returns why the command failed. Empty after a success. */
	const std::string& GetError() const
	{
		return Error;
	}

	/**
	 * Returns the result as JSON, the form in which the agent interface hands it out: {"Succeeded": true, "Value": ...}
	 * or {"Succeeded": false, "Error": "..."}.
	 */
	EDITOR_API FJsonValue ToJson() const;

private:
	FCommandResult() = default;

	bool bSucceeded = false;
	FJsonValue Value;
	std::string Error;
};
