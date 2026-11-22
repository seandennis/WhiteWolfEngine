#pragma once

#include "CoreMinimal.h"

#include "Commands/CommandEffect.h"
#include "Commands/CommandParameter.h"

#include <string>
#include <vector>

/** What an editor command is called, what it does and which arguments it takes. */
struct FCommandDescription
{
	/** The name the command is given by, for example "Entity.Create". Unique among the commands of a registry. */
	std::string Name;

	/** A sentence that says what the command does, for the people and agents who read the list of commands. */
	std::string Summary;

	/** The arguments the command takes. A command is refused if it is given any other argument. */
	std::vector<FCommandParameter> Parameters;

	/** Whether the command can change the scene; see ECommandEffect. */
	ECommandEffect Effect = ECommandEffect::ChangesScene;
};
