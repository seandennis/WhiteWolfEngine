#pragma once

#include "CoreMinimal.h"

class FCommandRegistry;

/**
 * The commands that play the game in a copy of the scene being edited (FEditor::StartPlay): Play.Start, Play.Step,
 * Play.Pause, Play.Resume, Play.Stop and Play.GetState.
 */
class FPlayCommands
{
public:
	/** Adds the commands to a registry. */
	static void Register(FCommandRegistry& Registry);
};
