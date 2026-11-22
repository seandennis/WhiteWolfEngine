#pragma once

#include "CoreMinimal.h"

class FCommandRegistry;

/**
 * The commands that act on components: Component.Add and Component.Remove.
 */
class FComponentCommands
{
public:
	/** Adds the commands to a registry. */
	static void Register(FCommandRegistry& Registry);
};
