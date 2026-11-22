#pragma once

#include "CoreMinimal.h"

#include "Commands/CommandArguments.h"
#include "Commands/CommandDescription.h"
#include "Commands/CommandResult.h"

#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

class FEditor;

/**
 * The function that carries out a command. It is called only with arguments that fit the command's parameters, does
 * what the command says to the editor it is given, and returns what happened. A command that cannot do what it was
 * asked returns a failure and leaves the editor as it was.
 */
using FCommandFunction = std::function<FCommandResult(FEditor& Editor, const FCommandArguments& Arguments)>;

/**
 * The commands of an editor, by name.
 *
 * Everything the editor can do is a command: a name, typed parameters and a function. The user interface, the tests
 * and the agent interface all act by giving commands to the same registry, so what one of them can do, all can.
 * The registry checks the arguments of every command it is given before the command runs.
 */
class FCommandRegistry
{
public:
	/**
	 * Adds a command.
	 *
	 * The description must have a name that no command of this registry has yet, and parameters with names that
	 * are not empty and differ from each other; Function must not be empty. Anything else is a mistake in the code
	 * that registers the command and a fatal error.
	 */
	EDITOR_API void Register(FCommandDescription Description, FCommandFunction Function);

	/** Returns the description of the command with the given name, or null if there is none. */
	EDITOR_API const FCommandDescription* Find(std::string_view Name) const;

	/** Returns the description of every command, in the order of their names. */
	EDITOR_API std::vector<const FCommandDescription*> GetCommands() const;

	/**
	 * Carries out a command.
	 *
	 * @param Editor The editor the command acts on.
	 * @param Name The name of the command.
	 * @param Arguments A JSON object with one member per argument, named after the parameter. JSON null stands for
	 * no arguments.
	 * @return What the command returned. A failure, with nothing done, if there is no command of that name or the
	 * arguments do not fit its parameters: one is missing, has the wrong type, or has no parameter.
	 */
	EDITOR_API FCommandResult Execute(FEditor& Editor, std::string_view Name, const FJsonValue& Arguments) const;

private:
	struct FEntry
	{
		FCommandDescription Description;
		FCommandFunction Function;
	};

	/** The commands by name. Ordered, so that the list of commands is the same on every run. */
	std::map<std::string, FEntry, std::less<>> Commands;
};
