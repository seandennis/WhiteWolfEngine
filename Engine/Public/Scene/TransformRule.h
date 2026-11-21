#pragma once

#include "CoreMinimal.h"

/** What happens to an entity's transform when it is given another parent. */
enum class ETransformRule : uint8
{
	/** The entity keeps its transform relative to its parent, so it moves in the world with its new parent. */
	KeepRelative,

	/** The entity stays where it is in the world; its relative transform is recalculated for the new parent. */
	KeepWorld,
};
