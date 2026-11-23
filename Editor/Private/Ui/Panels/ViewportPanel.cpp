#include "Ui/Panels/ViewportPanel.h"

#include "Editor.h"
#include "Math/LinearColor.h"
#include "Math/Ray.h"
#include "Rendering/CameraView.h"
#include "Rendering/RenderDevice.h"
#include "Rendering/RenderTarget.h"
#include "Rendering/SceneRayCast.h"
#include "Rendering/SceneRayHit.h"
#include "Rendering/SceneRenderer.h"
#include "Rendering/SceneViewMode.h"
#include "Rendering/StaticMeshComponent.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Ui/ImGuiLibrary.h"
#include "Ui/Panels/UiPanel.h"
#include "Ui/UiCommands.h"
#include "Ui/UiFrame.h"
#include "Ui/UiItems.h"
#include "Ui/UiTextures.h"
#include "Ui/UiTheme.h"
#include "Ui/UiWorkspace.h"
#include "Viewport/ViewportCamera.h"

#include <algorithm>
#include <cmath>
#include <optional>
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

	// How far the camera turns for each pixel that the mouse moves, in degrees: looking around, and going around.
	constexpr float LookSpeed = 0.2f;
	constexpr float OrbitSpeed = 0.3f;

	/** The part of its distance to the focus point that the camera moves for each pixel of a dolly. */
	constexpr double DollySpeed = 0.005;

	/** The part of that distance that it moves for each notch of the wheel. */
	constexpr double WheelDollyStep = 0.1;

	/** By how much a notch of the wheel multiplies the speed of flying. */
	constexpr float FlySpeedStep = 1.25f;
}

bool FViewportPanel::Draw(FUiFrame& Frame, FRenderDevice& Device, std::string& OutError)
{
	bIsPictureShown = false;

	// The camera is steered for as long as the picture is laid out with the mouse button down on it, frame after
	// frame. A frame without the picture ends the steering, whatever takes the picture away.
	const ENavigation Continued = Navigation;
	Navigation = ENavigation::None;
	if (!Frame.Workspace.bShowViewport)
	{
		Gizmo.LetGo(Frame);
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
			const ImVec2 PictureSize(static_cast<float>(Width), static_cast<float>(Height));
			const ImVec2 PictureCorner = ImGui::GetCursorScreenPos();
			ImGui::Image(ImTextureRef(Frame.Textures.AddRenderTarget(*Target)), PictureSize);
			bIsPictureShown = true;

			// What the mouse does on the picture is the panel's to say. A button that has no look of its own lies
			// on the picture and takes all three mouse buttons; while one of them is down, the mouse is its.
			ImGui::SetCursorScreenPos(PictureCorner);
			ImGui::InvisibleButton("##Picture",
				PictureSize,
				ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
					ImGuiButtonFlags_MouseButtonMiddle);
			Frame.Items.NameLastItem("Viewport");

			// On the picture, Alt is a key of the mouse: it turns a press into going around, or a drag into a
			// drag of copies. While the mouse is on the picture or held on it, the key is the picture's, and
			// letting go of it does not hand the keyboard to the menu bar, as Alt by itself does elsewhere.
			ImGui::SetItemKeyOwner(ImGuiMod_Alt);

			// The gizmo comes first: a press that takes one of its handles neither steers the camera nor is a
			// click on what is behind the gizmo.
			const FVector2f Corner(PictureCorner.x, PictureCorner.y);
			const FVector2f Size(PictureSize.x, PictureSize.y);
			Gizmo.Draw(Frame, Corner, Size);
			if (!Gizmo.HadMouse())
			{
				Navigate(Frame, Height, Continued);
				SelectClicked(Frame, Corner, Size, Continued);
			}

			// The toolbar comes last. Everything before it asks what the mouse does on the picture, which is the
			// item that was laid out last until the toolbar is.
			Toolbar.Draw(Frame, Corner, Size);
		}
	}
	ImGui::End();

	// Like the steering of the camera, a drag of the gizmo ends with the first frame that has no picture.
	if (!bIsPictureShown)
	{
		Gizmo.LetGo(Frame);
	}
	return bSucceeded;
}

void FViewportPanel::Navigate(FUiFrame& Frame, int32 PictureHeight, ENavigation Continued)
{
	const ImGuiIO& Io = ImGui::GetIO();
	if (ImGui::IsItemActivated())
	{
		// The button that went down, and whether Alt was held, say how the camera is steered until it comes up.
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
		{
			Navigation = ENavigation::Pan;
		}
		else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
		{
			Navigation = Io.KeyAlt ? ENavigation::Dolly : ENavigation::Fly;
		}
		else if (Io.KeyAlt)
		{
			Navigation = ENavigation::Orbit;
		}
	}
	else if (ImGui::IsItemActive())
	{
		Navigation = Continued;
	}

	FViewportCamera Camera = Frame.Editor.GetViewportCamera();
	const ImVec2& Moved = Io.MouseDelta;
	switch (Navigation)
	{
		case ENavigation::Fly:
		{
			// The mouse moved to the right looks to the right, which is a turn the other way than the yaw counts;
			// moved down, it looks down.
			Camera.Turn(-Moved.x * LookSpeed, -Moved.y * LookSpeed);

			const double Step = static_cast<double>(Camera.FlySpeed) * static_cast<double>(Io.DeltaTime);
			const auto GetAxis = [](ImGuiKey Positive, ImGuiKey Negative)
			{
				return (ImGui::IsKeyDown(Positive) ? 1.0 : 0.0) - (ImGui::IsKeyDown(Negative) ? 1.0 : 0.0);
			};
			Camera.Fly(GetAxis(ImGuiKey_D, ImGuiKey_A) * Step,
				GetAxis(ImGuiKey_E, ImGuiKey_Q) * Step,
				GetAxis(ImGuiKey_W, ImGuiKey_S) * Step);

			// The wheel makes the flight faster or slower, by the same part of the speed for every notch.
			if (Io.MouseWheel != 0.0f)
			{
				Camera.FlySpeed = std::clamp(Camera.FlySpeed * std::pow(FlySpeedStep, Io.MouseWheel),
					FViewportCamera::MinFlySpeed,
					FViewportCamera::MaxFlySpeed);
			}
			break;
		}
		case ENavigation::Orbit:
			Camera.Orbit(-Moved.x * OrbitSpeed, -Moved.y * OrbitSpeed);
			break;
		case ENavigation::Dolly:
		{
			// To the right or upwards is closer, by a part of the distance for every pixel.
			const double Pixels = static_cast<double>(Moved.x) - static_cast<double>(Moved.y);
			Camera.Dolly(Pixels * DollySpeed * Camera.FocusDistance);
			break;
		}
		case ENavigation::Pan:
		{
			// The scene goes with the mouse, so the camera goes the other way: what was under the mouse at the
			// distance of the focus point stays under it.
			const double PixelSize = Camera.GetPixelSizeAtFocus(PictureHeight);
			Camera.Pan(-static_cast<double>(Moved.x) * PixelSize, static_cast<double>(Moved.y) * PixelSize);
			break;
		}
		case ENavigation::None:
		default:
			// The wheel by itself moves the camera closer to what it looks at, or away, a step for every notch.
			if (ImGui::IsItemHovered() && Io.MouseWheel != 0.0f)
			{
				Camera.Dolly(static_cast<double>(Io.MouseWheel) * WheelDollyStep * Camera.FocusDistance);
			}
			break;
	}

	// The camera is the user's eye and not part of the scene: it is set, not changed by a command (D-048).
	if (!(Camera == Frame.Editor.GetViewportCamera()))
	{
		Frame.Editor.SetViewportCamera(Camera);
	}
}

void FViewportPanel::SelectClicked(
	FUiFrame& Frame, const FVector2f& PictureCorner, const FVector2f& PictureSize, ENavigation Continued)
{
	// A click is the left button going down on the picture and coming up again without the mouse having gone
	// anywhere in between. A press that steered the camera, as one with Alt held does, was no click on anything.
	const ImGuiIO& Io = ImGui::GetIO();
	const bool bWasDragged =
		Io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] >= Io.MouseDragThreshold * Io.MouseDragThreshold;
	const bool bWasClicked = ImGui::IsItemDeactivated() && ImGui::IsMouseReleased(ImGuiMouseButton_Left) &&
		!bWasDragged && Continued == ENavigation::None;
	if (!bWasClicked)
	{
		return;
	}

	// What is seen under the mouse is what the ray through that point of the picture meets first. The picture
	// counts from its top left corner and downwards; the camera from the middle, and upwards.
	const FVector2f FromCorner = (FVector2f(Io.MousePos.x, Io.MousePos.y) - PictureCorner) / PictureSize;
	const FVector2f PicturePoint(FromCorner.x * 2.0f - 1.0f, 1.0f - FromCorner.y * 2.0f);
	FEditor& Editor = Frame.Editor;
	const FRay Ray = Editor.GetViewportCamera().GetView().GetRay(PicturePoint, PictureSize.x / PictureSize.y);
	const std::optional<FSceneRayHit> Hit = FSceneRayCast::FindFirstHit(Editor.GetScene(), Ray);
	const WEntity* const Entity = Hit.has_value() ? Hit->Component->GetOwner() : nullptr;

	// With Ctrl held, a click adds an entity to what is selected or takes it out, and a click on nothing changes
	// nothing. Without, it selects the entity alone, or nothing.
	if (Io.KeyCtrl)
	{
		if (Entity != nullptr)
		{
			FUiCommands::ToggleSelection(Editor, Entity->GetId());
		}
	}
	else if (Entity != nullptr)
	{
		FUiCommands::SetSelection(Editor, {Entity->GetId()});
	}
	else if (!Editor.GetSelection().empty())
	{
		FUiCommands::SetSelection(Editor, {});
	}
}

bool FViewportPanel::RenderScene(FSceneRenderer& Renderer, const FEditor& Editor, std::string& OutError)
{
	if (!bIsPictureShown)
	{
		return true;
	}

	Renderer.ViewMode = Editor.GetViewMode();
	Renderer.BackgroundColor = Renderer.ViewMode == ESceneViewMode::Lit ? EmptySpaceLight : EmptySpaceColor;

	// What is selected has a line around it, with everything below it.
	Renderer.OutlinedEntities = Editor.GetSelection();
	Renderer.OutlineColor = FUiTheme::GetSelectionOutlineColor();
	Renderer.OutlineWidth = FUiTheme::GetSelectionOutlineWidth();
	return Renderer.Render(Editor.GetScene(), Editor.GetViewportCamera().GetView(), *Target, OutError);
}
