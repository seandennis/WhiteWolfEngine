#pragma once

#include "CoreMinimal.h"

#include "Math/LinearColor.h"
#include "Rendering/SceneRenderStatistics.h"

#include <memory>
#include <string>

class FRenderDevice;
class FRenderTarget;
class WScene;
struct FCameraView;

/**
 * Draws scenes: every visible WStaticMeshComponent of a scene, as seen from a camera, onto a render target.
 *
 * The picture does not depend on where in the world the scene is: everything is drawn relative to the camera, so a
 * scene ten billion kilometres from the origin looks exactly as it does at the origin. Depth has no far plane, so
 * something a metre away and something a light-year away are drawn together in the right order.
 *
 * A scene renderer keeps a copy of each mesh it has drawn on the graphics card, for as long as the mesh exists.
 * Like everything created from a render device it keeps the graphics card alive. It is used on the main thread.
 */
class FSceneRenderer
{
private:
	struct FImplementation;

public:
	/**
	 * Creates a scene renderer for a render device.
	 *
	 * @param OutError Receives the reason on failure.
	 * @return The renderer, or null if the graphics card could not create what the renderer needs.
	 */
	ENGINE_API static std::unique_ptr<FSceneRenderer> Create(FRenderDevice& Device, std::string& OutError);

	/** Used by Create. Other code cannot call it, because it cannot name the implementation type. */
	explicit FSceneRenderer(std::unique_ptr<FImplementation> InImplementation);

	ENGINE_API ~FSceneRenderer();

	FSceneRenderer(const FSceneRenderer&) = delete;
	FSceneRenderer& operator=(const FSceneRenderer&) = delete;

	/** The colour of the picture wherever no mesh is drawn, in linear space. */
	FLinearColor BackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);

	/**
	 * Draws a scene onto a render target, replacing what the target showed.
	 *
	 * A component is drawn if it is visible, its mesh has geometry and it and its entity have not been destroyed.
	 * A component without a material is drawn plain white.
	 *
	 * @param View Where the picture is taken from. An invalid view (see FCameraView::IsValid) is refused.
	 * @param OutError Receives the reason on failure.
	 * @return True if the scene was drawn. On failure the target is unchanged.
	 */
	ENGINE_API bool Render(const WScene& Scene, const FCameraView& View, FRenderTarget& Target, std::string& OutError);

	/** Returns what the last call of Render did. All zero before the first call. */
	ENGINE_API const FSceneRenderStatistics& GetStatistics() const;

private:
	std::unique_ptr<FImplementation> Implementation;
};
