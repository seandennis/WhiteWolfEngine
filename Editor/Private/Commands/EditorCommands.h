#pragma once

#include "CoreMinimal.h"

class FCommandRegistry;

/**
 * The commands about the editor itself: Editor.ListCommands, which is how an agent learns what the editor can do.
 */
class FEditorCommands
{
public:
	/** Adds the commands to a registry. */
	static void Register(FCommandRegistry& Registry);
};
