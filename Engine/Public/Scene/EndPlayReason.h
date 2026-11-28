#pragma once

#include "CoreMinimal.h"

/** Why a component ends play (WComponent::EndPlay). */
enum class EEndPlayReason : uint8
{
	/** The game ended: the scene stopped playing (WScene::EndPlay), or was destroyed while it played. */
	PlayEnded,

	/**
	 * The component left a game that goes on: it was removed or destroyed, or its entity or one of the entity's
	 * ancestors was destroyed.
	 */
	Destroyed,
};
