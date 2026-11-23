#pragma once

#include "CoreMinimal.h"

#include <memory>
#include <string>

class FEditor;
class FRenderDevice;
class FRenderTarget;
class FSceneRenderer;
struct FUiFrame;

/**
 * The panel that shows the scene being edited, as the editor's viewport camera sees it.
 *
 * The scene is drawn onto a render target of the panel's own, as large as the panel, and the user interface shows
 * that surface as a picture.
 */
class FViewportPanel
{
public:
	/** What the panel is called on its tab. Dear ImGui also knows the panel by this. */
	static constexpr const char* Title = "Viewport";

	/**
	 * Lays the panel out: a picture of the scene that fills it. The picture itself is drawn afterwards, by
	 * RenderScene.
	 *
	 * @param Device Where the surface for the picture is created when the panel has changed its size.
	 * @param OutError Receives the reason on failure.
	 * @return False if the graphics card could not create the surface.
	 */
	bool Draw(FUiFrame& Frame, FRenderDevice& Device, std::string& OutError);

	/**
	 * Draws the scene onto the surface that the picture shows, if the last call of Draw laid a picture out. Call
	 * it after the frame is laid out and before the user interface is drawn.
	 *
	 * @param OutError Receives the reason on failure.
	 * @return False if the scene could not be drawn.
	 */
	bool RenderScene(FSceneRenderer& Renderer, const FEditor& Editor, std::string& OutError);

private:
	/** The surface the scene is drawn onto. Null until the panel is first shown. */
	std::unique_ptr<FRenderTarget> Target;

	/** True if the frame that was laid out last shows the picture. */
	bool bIsPictureShown = false;
};
