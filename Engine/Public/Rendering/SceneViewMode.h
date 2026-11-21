#pragma once

#include "CoreMinimal.h"

/** How FSceneRenderer shades what it draws. */
enum class ESceneViewMode : uint8
{
	/** Surfaces are lit by the lights of the scene, and the result is fitted to the display. The normal picture. */
	Lit,

	/**
	 * Every surface shows the base colour of its material exactly, as if evenly lit, with no lights, exposure or
	 * tone mapping involved. For inspecting a scene, and for tests that check where things are drawn.
	 */
	Unlit,
};
