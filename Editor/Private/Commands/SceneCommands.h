#pragma once

#include "CoreMinimal.h"

class FCommandRegistry;

/**
 * The commands that act on the scene as a whole: Scene.New, Scene.Open, Scene.Save and Scene.GetHierarchy.
 */
class FSceneCommands
{
public:
	/** Adds the commands to a registry. */
	static void Register(FCommandRegistry& Registry);
};
