#pragma once

#include "CoreMinimal.h"

#include "Json/JsonValue.h"

#include <cstddef>
#include <string>
#include <vector>

/**
 * What an editor command returns: either success, with a value, or failure, with the reason. A success can also
 * carry a picture, as the PNG file of a screenshot, which the agent interface hands an agent as an image.
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
	 * Returns a successful result with a picture.
	 *
	 * @param InValue What the command returns besides the picture.
	 * @param InPngFile The bytes of a PNG file. An empty picture is a fatal error.
	 */
	EDITOR_API static FCommandResult SuccessWithPicture(FJsonValue InValue, std::vector<std::byte> InPngFile);

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

	/** Returns the bytes of the PNG file of the picture that the command returned; empty if it returned none. */
	const std::vector<std::byte>& GetPicture() const
	{
		return Picture;
	}

	/**
	 * Returns the result as JSON: {"Succeeded": true, "Value": ...} or {"Succeeded": false, "Error": "..."}. A
	 * picture is not part of it.
	 */
	EDITOR_API FJsonValue ToJson() const;

private:
	FCommandResult() = default;

	bool bSucceeded = false;
	FJsonValue Value;
	std::string Error;
	std::vector<std::byte> Picture;
};
