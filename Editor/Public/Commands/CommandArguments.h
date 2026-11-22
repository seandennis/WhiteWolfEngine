#pragma once

#include "CoreMinimal.h"

#include "Json/JsonValue.h"

#include <string>
#include <string_view>

/**
 * The arguments a command was given, by parameter name.
 *
 * FCommandRegistry checks the arguments against the command's parameters before the command runs: every required
 * argument is there, every argument has the type of its parameter, and there is no argument without a parameter.
 * A command therefore reads its arguments without checking them again. Asking for an argument that is not there,
 * or as another type than its parameter's, is a mistake in the command and a fatal error; ask Has first for a
 * parameter that is optional.
 */
class FCommandArguments
{
public:
	/** @param InValues The arguments: a JSON object with one member per argument. */
	EDITOR_API explicit FCommandArguments(FJsonValue InValues);

	/** Returns true if the argument was given. */
	EDITOR_API bool Has(std::string_view Name) const;

	/** Returns the argument of a Boolean parameter. */
	EDITOR_API bool GetBoolean(std::string_view Name) const;

	/** Returns the argument of an Integer parameter. */
	EDITOR_API int64 GetInteger(std::string_view Name) const;

	/** Returns the argument of a Number or Integer parameter. */
	EDITOR_API double GetNumber(std::string_view Name) const;

	/** Returns the argument of a String parameter. */
	EDITOR_API const std::string& GetString(std::string_view Name) const;

	/** Returns the argument of a parameter of any type, as JSON. */
	EDITOR_API const FJsonValue& GetJson(std::string_view Name) const;

private:
	FJsonValue Values;
};
