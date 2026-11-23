#pragma once

#include "CoreMinimal.h"

#include "Rendering/SceneRayHit.h"

#include <optional>

class WScene;
struct FRay;

/**
 * Finds what lies along a ray through a scene: what is seen at a point of a picture, when the ray is the one that
 * the camera sends through that point (FCameraView::GetRay), and so what a user means who clicks there.
 *
 * A ray meets what is drawn: the fronts of the triangles of the meshes that visible static mesh components show.
 * Lights, and entities that show nothing, are not met by anything.
 *
 * The meshes are gone through one after the other, and of each that the ray may meet every triangle. That is fast
 * enough for a question now and then, and not meant for many rays in every frame.
 */
class FSceneRayCast
{
public:
	/**
	 * Returns what a ray meets first, or nothing if it meets nothing in the scene.
	 *
	 * @param Scene The scene to look in. The ray is in its world space.
	 */
	ENGINE_API static std::optional<FSceneRayHit> FindFirstHit(const WScene& Scene, const FRay& Ray);
};
