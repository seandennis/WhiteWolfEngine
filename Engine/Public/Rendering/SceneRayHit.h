#pragma once

#include "CoreMinimal.h"

class WStaticMeshComponent;

/** What a ray through a scene meets first; see FSceneRayCast. */
struct FSceneRayHit
{
	/**
	 * The component whose mesh the ray meets. Its entity is the one that a person would say is there. Like every
	 * pointer to an object that nothing registers, it is for now and not for the next frame.
	 */
	const WStaticMeshComponent* Component = nullptr;

	/** How far along the ray that is, in metres from where the ray begins. */
	double Distance = 0.0;
};
