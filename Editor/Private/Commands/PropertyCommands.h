#pragma once

#include "CoreMinimal.h"

class FCommandRegistry;

/**
 * The commands that read and change the reflected properties of entities and components: Object.GetProperties,
 * Object.GetProperty and Object.SetProperty.
 */
class FPropertyCommands
{
public:
	/** Adds the commands to a registry. */
	static void Register(FCommandRegistry& Registry);
};
