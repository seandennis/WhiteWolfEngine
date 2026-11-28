#pragma once

#include "CoreMinimal.h"

#include "Math/LinearColor.h"
#include "Rendering/SceneViewMode.h"

class FSceneRenderer;

/**
 * How the viewport's picture of a scene is drawn: what the viewport panel shows, and what a picture that a command
 * takes of the scene shows (Viewport.Capture), which are the same but for the marks that the panel draws over it.
 */
class FViewportPicture
{
public:
	/**
	 * Returns what is drawn where the scene has nothing, in a view mode: a dark grey, a little darker than the
	 * panels around the viewport. In the lit view mode it is an amount of light that the camera's exposure scales
	 * and the tone mapper bends, which at an exposure of one comes out as about that grey.
	 */
	EDITOR_API static FLinearColor GetEmptySpaceColor(ESceneViewMode ViewMode);

	/**
	 * Sets a scene renderer up to draw the viewport's picture in a view mode: the view mode and what empty space
	 * shows. The line around what is selected is the viewport panel's to add.
	 */
	EDITOR_API static void Configure(FSceneRenderer& Renderer, ESceneViewMode ViewMode);
};
