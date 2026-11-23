#pragma once

#include "CoreMinimal.h"

class FCommandRegistry;

/**
 * The commands that act on entities: Entity.Create, Entity.Delete, Entity.Duplicate, Entity.Rename and
 * Entity.SetParent.
 */
class FEntityCommands
{
public:
	/** Adds the commands to a registry. */
	static void Register(FCommandRegistry& Registry);
};
