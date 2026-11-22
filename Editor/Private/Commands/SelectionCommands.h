#pragma once

#include "CoreMinimal.h"

class FCommandRegistry;

/**
 * The commands that say which entities are selected: Selection.Set and Selection.Get.
 */
class FSelectionCommands
{
public:
	/** Adds the commands to a registry. */
	static void Register(FCommandRegistry& Registry);
};
