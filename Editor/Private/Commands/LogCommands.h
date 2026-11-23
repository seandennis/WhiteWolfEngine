#pragma once

#include "CoreMinimal.h"

class FCommandRegistry;

/**
 * The commands that read what the editor and the engine have logged: Log.Read and Log.Clear.
 */
class FLogCommands
{
public:
	/** Adds the commands to a registry. */
	static void Register(FCommandRegistry& Registry);
};
