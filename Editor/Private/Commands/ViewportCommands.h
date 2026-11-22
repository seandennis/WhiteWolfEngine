#pragma once

#include "CoreMinimal.h"

class FCommandRegistry;

/**
 * The commands that say from where and how the viewport shows the scene: Viewport.GetCamera, Viewport.SetCamera,
 * Viewport.GetViewMode and Viewport.SetViewMode.
 */
class FViewportCommands
{
public:
	/** Adds the commands to a registry. */
	static void Register(FCommandRegistry& Registry);
};
