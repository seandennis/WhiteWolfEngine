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

	/**
	 * Shows the ambient occlusion by itself, as shades of grey: white where a surface is in the open, darker where
	 * it is hidden from its surroundings in a crease, a corner or under something. For inspecting that effect.
	 */
	AmbientOcclusion,
};
