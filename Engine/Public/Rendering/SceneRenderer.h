#pragma once

#include "CoreMinimal.h"

#include "Math/LinearColor.h"
#include "Rendering/SceneRenderStatistics.h"
#include "Rendering/SceneViewMode.h"

#include <memory>
#include <string>

class FRenderDevice;
class FRenderTarget;
class WScene;
struct FCameraView;

/**
 * Draws scenes: every visible WStaticMeshComponent of a scene, as seen from a camera, onto a render target.
 *
 * Surfaces are lit by the scene's WDirectionalLightComponent and WPointLightComponent lights with a physically
 * based model (the metallic-roughness model of glTF 2.0). Lighting is calculated in high dynamic range: light can
 * be any amount brighter than white, and the camera's exposure and a tone mapper then fit the result to what a
 * display can show.
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

	/** The most directional lights and point lights that light one picture. Lights beyond these are ignored. */
	static constexpr int32 MaxDirectionalLights = 4;
	static constexpr int32 MaxPointLights = 32;

	/**
	 * The colour of the picture wherever no mesh is drawn, in linear space. In the lit view mode it is light like
	 * any other: the camera's exposure and tone mapping apply to it.
	 */
	FLinearColor BackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);

	/** How surfaces are shaded. */
	ESceneViewMode ViewMode = ESceneViewMode::Lit;

	/**
	 * Draws a scene onto a render target, replacing what the target showed.
	 *
	 * A mesh component is drawn if it is visible, its mesh has geometry and it and its entity have not been
	 * destroyed; without a material it is drawn as a white, matte surface. A light shines if it is visible and
	 * not destroyed. When a scene has more lights of a kind than the renderer supports, the first ones found are
	 * used and a warning is logged.
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
