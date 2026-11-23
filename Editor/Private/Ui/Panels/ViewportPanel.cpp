#include "Ui/Panels/ViewportPanel.h"

#include "Editor.h"
#include "Math/LinearColor.h"
#include "Rendering/RenderDevice.h"
#include "Rendering/RenderTarget.h"
#include "Rendering/SceneRenderer.h"
#include "Rendering/SceneViewMode.h"
#include "Ui/ImGuiLibrary.h"
#include "Ui/Panels/UiPanel.h"
#include "Ui/UiFrame.h"
#include "Ui/UiItems.h"
#include "Ui/UiTextures.h"
#include "Ui/UiWorkspace.h"

#include <algorithm>
#include <utility>

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

bool FViewportPanel::Draw(FUiFrame& Frame, FRenderDevice& Device, std::string& OutError)
{
	bIsPictureShown = false;
	if (!Frame.Workspace.bShowViewport)
	{
		return true;
	}

	// The picture reaches the edges of the panel.
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	const bool bIsVisible = FUiPanel::Begin(
		Title, Frame.Workspace.bShowViewport, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	ImGui::PopStyleVar();

	bool bSucceeded = true;
	if (bIsVisible)
	{
		const ImVec2 Available = ImGui::GetContentRegionAvail();
		const int32 Width = std::clamp(static_cast<int32>(Available.x), 1, FRenderTarget::MaxDimension);
		const int32 Height = std::clamp(static_cast<int32>(Available.y), 1, FRenderTarget::MaxDimension);

		const bool bHasRightSize = Target != nullptr && Target->GetWidth() == Width && Target->GetHeight() == Height;
		if (!bHasRightSize)
		{
			std::unique_ptr<FRenderTarget> NewTarget = Device.CreateRenderTarget(Width, Height, OutError);
			bSucceeded = NewTarget != nullptr;
			if (bSucceeded)
			{
				Target = std::move(NewTarget);
			}
		}

		if (bSucceeded)
		{
			ImGui::Image(ImTextureRef(Frame.Textures.AddRenderTarget(*Target)),
				ImVec2(static_cast<float>(Width), static_cast<float>(Height)));
			Frame.Items.NameLastItem("Viewport");
			bIsPictureShown = true;
		}
	}
	ImGui::End();
	return bSucceeded;
}

bool FViewportPanel::RenderScene(FSceneRenderer& Renderer, const FEditor& Editor, std::string& OutError)
{
	if (!bIsPictureShown)
	{
		return true;
	}

	Renderer.ViewMode = Editor.GetViewMode();
	Renderer.BackgroundColor = Renderer.ViewMode == ESceneViewMode::Lit ? EmptySpaceLight : EmptySpaceColor;
	return Renderer.Render(Editor.GetScene(), Editor.GetViewportCamera().GetView(), *Target, OutError);
}
