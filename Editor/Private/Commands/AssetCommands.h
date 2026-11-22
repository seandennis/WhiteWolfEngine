#pragma once

#include "CoreMinimal.h"

class FCommandRegistry;

/**
 * The commands that act on the assets of the project: Asset.Import, Asset.List, Asset.Describe and Asset.Place.
 */
class FAssetCommands
{
public:
	/** Adds the commands to a registry. */
	static void Register(FCommandRegistry& Registry);
};
