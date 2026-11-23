#pragma once

#include "CoreMinimal.h"

/** The axes along which the gizmo of the viewport moves things, and around which it turns them. */
enum class EGizmoSpace : uint8
{
	/** The axes of the world, whichever way the thing is turned. */
	World,

	/** The axes of the thing itself: the entity that the gizmo is on. */
	Local,
};
