#pragma once

#include "CoreMinimal.h"

class FCommandRegistry;

/**
 * The commands of the Edit menu that act on the history: Edit.Undo, Edit.Redo and Edit.GetHistory.
 */
class FEditCommands
{
public:
	/** Adds the commands to a registry. */
	static void Register(FCommandRegistry& Registry);
};
