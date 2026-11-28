#include "Viewport/ViewportPicture.h"

#include "Rendering/SceneRenderer.h"

namespace
{
	/** What the viewport shows where the scene has nothing: a dark grey, a little darker than the panels around it. */
	constexpr FLinearColor EmptySpaceColor(0.012f, 0.012f, 0.012f);

	/**
	 * The amount of light that the lit view mode shows as about that grey when the camera's exposure is one. There
	 * the background is light like any other, which the exposure scales and the tone mapper bends: it darkens what
	 * is dim, so that more light is needed for the same grey.
	 */
	constexpr FLinearColor EmptySpaceLight(0.044f, 0.044f, 0.044f);
}

FLinearColor FViewportPicture::GetEmptySpaceColor(ESceneViewMode ViewMode)
{
	return ViewMode == ESceneViewMode::Lit ? EmptySpaceLight : EmptySpaceColor;
}

void FViewportPicture::Configure(FSceneRenderer& Renderer, ESceneViewMode ViewMode)
{
	Renderer.ViewMode = ViewMode;
	Renderer.BackgroundColor = GetEmptySpaceColor(ViewMode);
}
