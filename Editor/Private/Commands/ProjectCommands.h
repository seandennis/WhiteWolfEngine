#pragma once

#include "CoreMinimal.h"

class FCommandRegistry;

/**
 * The commands that act on the project: Project.New, Project.Open and Project.Get.
 */
class FProjectCommands
{
public:
	/** Adds the commands to a registry. */
	static void Register(FCommandRegistry& Registry);
};
