#include "Ui/Panels/ViewportToolbar.h"

#include "Editor.h"
#include "Math/Color.h"
#include "Rendering/SceneViewMode.h"
#include "Ui/ImGuiLibrary.h"
#include "Ui/UiFrame.h"
#include "Ui/UiItems.h"
#include "Ui/UiTheme.h"
#include "Viewport/GizmoMode.h"
#include "Viewport/GizmoSettings.h"
#include "Viewport/GizmoSpace.h"
#include "Viewport/ViewportCamera.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <string>

namespace
{
	/** A button that chooses what dragging the gizmo does. */
	struct FModeButton
	{
		EGizmoMode Mode;
		const char* Label;
		const char* Tooltip;
	};

	constexpr std::array<FModeButton, 3> ModeButtons = {{
		{EGizmoMode::Translate, "Move", "Move what is selected (W, or Space for the next of the three)"},
		{EGizmoMode::Rotate, "Rotate", "Turn what is selected (E, or Space for the next of the three)"},
		{EGizmoMode::Scale, "Scale", "Size what is selected (R, or Space for the next of the three)"},
	}};

	constexpr const char* WorldSpaceLabel = "World";
	constexpr const char* LocalSpaceLabel = "Local";

	/**
	 * A button that says whether things are dragged in steps, and shows the step, with the button beside it that
	 * opens the list of the steps on offer.
	 */
	struct FSnapLabels
	{
		/** What the two are called in the names of their items: "Snap<Name>" and "<Name>Snap". */
		const char* Name;
		const char* ToggleTooltip;
		const char* SizeTooltip;
	};

	constexpr FSnapLabels LocationSnapLabels = {"Location",
		"Move things in steps of this length. Click to switch that on or off.",
		"Choose the length of a step"};
	constexpr FSnapLabels RotationSnapLabels = {"Rotation",
		"Turn things in steps of this angle. Click to switch that on or off.",
		"Choose the angle of a step"};
	constexpr FSnapLabels ScaleSnapLabels = {"Scale",
		"Size things in steps of this part of their size. Click to switch that on or off.",
		"Choose the size of a step"};

	/** What Dear ImGui knows the list of steps by, among the items of one such pair of buttons. */
	constexpr const char* SnapListId = "##Steps";

	// The steps that the lists offer: metres, degrees, and parts of a size.
	constexpr std::array<double, 8> LocationSnaps = {0.01, 0.05, 0.1, 0.25, 0.5, 1.0, 5.0, 10.0};
	constexpr std::array<float, 8> RotationSnaps = {1.0f, 5.0f, 10.0f, 15.0f, 30.0f, 45.0f, 60.0f, 90.0f};
	constexpr std::array<float, 4> ScaleSnaps = {0.1f, 0.25f, 0.5f, 1.0f};

	std::string FormatLength(double Metres)
	{
		return std::format("{:.6g} m", Metres);
	}

	std::string FormatAngle(float Degrees)
	{
		return std::format("{:.6g} deg", Degrees);
	}

	std::string FormatPart(float Part)
	{
		constexpr float Hundredths = 100.0f;
		return std::format("{:.6g}%", Part * Hundredths);
	}

	/** A way of shading the scene, and what the list calls it. */
	struct FViewModeEntry
	{
		ESceneViewMode Mode;
		const char* Label;
	};

	constexpr std::array<FViewModeEntry, 3> ViewModes = {{
		{ESceneViewMode::Lit, "Lit"},
		{ESceneViewMode::Unlit, "Unlit"},
		{ESceneViewMode::AmbientOcclusion, "Ambient Occlusion"},
	}};

	/** The widest that the speed of the camera is written, which its field is made wide enough for. */
	constexpr const char* WidestCameraSpeed = "1000.0 m/s";

	/** Returns how wide a list has to be to show a text of the given width, with the arrow that opens it. */
	float GetListWidth(float TextWidth)
	{
		return TextWidth + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetFrameHeight();
	}

	/**
	 * Lays out a button that shows the step in which things are dragged, in the accent while they are, and that
	 * switches stepping on and off; and beside it a button with an arrow that opens the list of the steps on offer.
	 */
	template <typename NumberType, std::size_t SizeCount>
	void DrawSnap(FUiFrame& Frame,
		const FSnapLabels& Labels,
		bool& bIsOn,
		NumberType& Size,
		const std::array<NumberType, SizeCount>& Sizes,
		std::string (*Format)(NumberType))
	{
		// As wide as the widest step, so that what comes after stays where it is when another step is chosen.
		const std::string Current = Format(Size);
		float TextWidth = ImGui::CalcTextSize(Current.c_str()).x;
		for (const NumberType Offered : Sizes)
		{
			TextWidth = std::max(TextWidth, ImGui::CalcTextSize(Format(Offered).c_str()).x);
		}
		const ImGuiStyle& Style = ImGui::GetStyle();
		const ImVec2 ButtonSize(TextWidth + Style.FramePadding.x * 2.0f, 0.0f);

		// The part after ### is what Dear ImGui knows the button by, whichever step it shows.
		ImGui::PushID(Labels.Name);
		const bool bWasOn = bIsOn;
		if (bWasOn)
		{
			FUiTheme::PushChosenButtonColors();
		}
		if (ImGui::Button(std::format("{}###On", Current).c_str(), ButtonSize))
		{
			bIsOn = !bIsOn;
		}
		if (bWasOn)
		{
			FUiTheme::PopChosenButtonColors();
		}
		ImGui::SetItemTooltip("%s", Labels.ToggleTooltip);
		Frame.Items.NameLastItem(std::format("Viewport/Toolbar/Snap{}", Labels.Name));

		// The list opens below the two buttons, as the list of a field with an arrow does. The arrow is close
		// enough to the step to be seen to belong to it: a quarter of the room that parts of one widget have.
		const ImVec2 BelowButtons(ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y);
		constexpr float ArrowGap = 0.25f;
		ImGui::SameLine(0.0f, Style.ItemInnerSpacing.x * ArrowGap);
		if (ImGui::ArrowButton("##Choose", ImGuiDir_Down))
		{
			ImGui::OpenPopup(SnapListId);
		}
		ImGui::SetItemTooltip("%s", Labels.SizeTooltip);
		Frame.Items.NameLastItem(std::format("Viewport/Toolbar/{}Snap", Labels.Name));

		ImGui::SetNextWindowPos(BelowButtons);
		if (ImGui::BeginPopup(SnapListId))
		{
			for (const NumberType Offered : Sizes)
			{
				if (ImGui::Selectable(Format(Offered).c_str(), Offered == Size))
				{
					Size = Offered;
				}
				Frame.Items.NameLastItem(std::format("Viewport/Toolbar/{}Snap/{:.6g}", Labels.Name, Offered));
			}
			ImGui::EndPopup();
		}
		ImGui::PopID();
	}

	/** Lays out the buttons that choose what dragging the gizmo does, the one that is in force in the accent. */
	void DrawModes(FUiFrame& Frame, FGizmoSettings& Settings)
	{
		bool bIsFirst = true;
		for (const FModeButton& Button : ModeButtons)
		{
			if (!bIsFirst)
			{
				ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
			}
			bIsFirst = false;

			const bool bIsChosen = Settings.Mode == Button.Mode;
			if (bIsChosen)
			{
				FUiTheme::PushChosenButtonColors();
			}
			if (ImGui::Button(Button.Label))
			{
				Settings.Mode = Button.Mode;
			}
			if (bIsChosen)
			{
				FUiTheme::PopChosenButtonColors();
			}
			ImGui::SetItemTooltip("%s", Button.Tooltip);
			Frame.Items.NameLastItem(std::format("Viewport/Toolbar/{}", Button.Label));
		}
	}

	/** Lays out the button that says along which axes things are moved and turned, and changes them when clicked. */
	void DrawSpace(FUiFrame& Frame, FGizmoSettings& Settings)
	{
		// As wide as the wider of its two words, so that what comes after it stays where it is.
		const float TextWidth =
			std::max(ImGui::CalcTextSize(WorldSpaceLabel).x, ImGui::CalcTextSize(LocalSpaceLabel).x);
		const ImVec2 Size(TextWidth + ImGui::GetStyle().FramePadding.x * 2.0f, 0.0f);

		// The part after ### is what Dear ImGui knows the button by, whichever word it shows.
		const bool bIsLocal = Settings.Space == EGizmoSpace::Local;
		if (ImGui::Button(std::format("{}###Space", bIsLocal ? LocalSpaceLabel : WorldSpaceLabel).c_str(), Size))
		{
			Settings.Space = bIsLocal ? EGizmoSpace::World : EGizmoSpace::Local;
		}
		ImGui::SetItemTooltip("%s",
			bIsLocal ? "Things are moved and turned along their own axes. Click for the axes of the world."
					 : "Things are moved and turned along the axes of the world. Click for their own axes.");
		Frame.Items.NameLastItem("Viewport/Toolbar/Space");
	}

	/** Lays out the field that shows how fast the camera flies, and in which that is dragged or typed. */
	void DrawCameraSpeed(FUiFrame& Frame)
	{
		FEditor& Editor = Frame.Editor;
		FViewportCamera Camera = Editor.GetViewportCamera();

		// From a tenth of a metre to a kilometre a second, a speed is found by its order of magnitude first.
		constexpr float DefaultDragSpeed = 0.0f;
		ImGui::SetNextItemWidth(ImGui::CalcTextSize(WidestCameraSpeed).x + ImGui::GetStyle().FramePadding.x * 2.0f);
		ImGui::DragFloat("##CameraSpeed",
			&Camera.FlySpeed,
			DefaultDragSpeed,
			FViewportCamera::MinFlySpeed,
			FViewportCamera::MaxFlySpeed,
			"%.1f m/s",
			ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp);
		ImGui::SetItemTooltip("%s", "How fast the camera flies. During a flight, the wheel changes it.");
		Frame.Items.NameLastItem("Viewport/Toolbar/CameraSpeed");

		// What can be typed into the field need not be a number. Such a speed is not taken.
		if (Camera.IsValid() && !(Camera == Editor.GetViewportCamera()))
		{
			Editor.SetViewportCamera(Camera);
		}
	}

	/** Lays out the list of the ways the scene is shaded, which shows the one that is in force. */
	void DrawViewMode(FUiFrame& Frame)
	{
		FEditor& Editor = Frame.Editor;
		const ESceneViewMode Current = Editor.GetViewMode();
		const char* CurrentLabel = "";
		float TextWidth = 0.0f;
		for (const FViewModeEntry& Entry : ViewModes)
		{
			TextWidth = std::max(TextWidth, ImGui::CalcTextSize(Entry.Label).x);
			if (Entry.Mode == Current)
			{
				CurrentLabel = Entry.Label;
			}
		}

		ImGui::SetNextItemWidth(GetListWidth(TextWidth));
		if (ImGui::BeginCombo("##ViewMode", CurrentLabel))
		{
			for (const FViewModeEntry& Entry : ViewModes)
			{
				if (ImGui::Selectable(Entry.Label, Entry.Mode == Current))
				{
					Editor.SetViewMode(Entry.Mode);
				}
				Frame.Items.NameLastItem(std::format("Viewport/Toolbar/ViewMode/{}", Entry.Label));
			}
			ImGui::EndCombo();
		}
		ImGui::SetItemTooltip("%s", "How the scene is shaded");
		Frame.Items.NameLastItem("Viewport/Toolbar/ViewMode");
	}
}

void FViewportToolbar::Draw(FUiFrame& Frame, const FVector2f& PictureCorner, const FVector2f& PictureSize)
{
	FEditor& Editor = Frame.Editor;
	const ImGuiStyle& Style = ImGui::GetStyle();

	// A little in from the corner of the picture, on a dark ground of its own that the buttons can be read on
	// whatever the picture shows.
	const ImVec2 Margin = Style.ItemSpacing;
	const ImVec2 Padding = Style.FramePadding;
	const float GroupSpacing = Style.ItemSpacing.x * 2.0f;
	const float RowEnd = PictureCorner.x + PictureSize.x - Margin.x - Padding.x;
	ImGui::SetCursorScreenPos(ImVec2(PictureCorner.x + Margin.x, PictureCorner.y + Margin.y));

	const FColor Ground = FUiTheme::GetOverlayColor();
	ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(Ground.R, Ground.G, Ground.B, Ground.A));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Padding);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, Style.FrameRounding);
	constexpr ImGuiChildFlags ChildFlags = ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY |
		ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_AlwaysUseWindowPadding;
	constexpr ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
	const bool bIsVisible = ImGui::BeginChild("##Toolbar", ImVec2(0.0f, 0.0f), ChildFlags, WindowFlags);

	// The look of the toolbar's own window is not that of the lists that open from it.
	constexpr int32 StyleVarCount = 2;
	ImGui::PopStyleVar(StyleVarCount);
	ImGui::PopStyleColor();

	if (bIsVisible)
	{
		// A group goes on the row of the one before it if it was narrow enough for that when it was last laid out,
		// so many pixels after it.
		int32 GroupIndex = 0;
		const auto BeginGroup = [this, &GroupIndex, RowEnd](float Spacing)
		{
			WW_CHECKF(
				GroupIndex < GroupCount, "The viewport's toolbar has more groups than the {} it counts", GroupCount);
			const float Width = GroupWidths[static_cast<std::size_t>(GroupIndex)];
			if (GroupIndex > 0 && ImGui::GetItemRectMax().x + Spacing + Width <= RowEnd)
			{
				ImGui::SameLine(0.0f, Spacing);
			}
			ImGui::BeginGroup();
		};
		const auto EndGroup = [this, &GroupIndex]()
		{
			ImGui::EndGroup();
			GroupWidths[static_cast<std::size_t>(GroupIndex)] = ImGui::GetItemRectSize().x;
			++GroupIndex;
		};

		FGizmoSettings Settings = Editor.GetGizmoSettings();

		BeginGroup(GroupSpacing);
		DrawModes(Frame, Settings);
		EndGroup();

		BeginGroup(GroupSpacing);
		DrawSpace(Frame, Settings);
		EndGroup();

		// Each pair of buttons for steps is a group of its own, so that a picture that is too narrow for the three
		// of them side by side still shows them all. They belong together, and are as close to one another as the
		// buttons of one group are.
		const float PairSpacing = Style.ItemSpacing.x;
		BeginGroup(GroupSpacing);
		DrawSnap(Frame, LocationSnapLabels, Settings.bSnapLocation, Settings.LocationSnap, LocationSnaps, FormatLength);
		EndGroup();

		BeginGroup(PairSpacing);
		DrawSnap(Frame, RotationSnapLabels, Settings.bSnapRotation, Settings.RotationSnap, RotationSnaps, FormatAngle);
		EndGroup();

		BeginGroup(PairSpacing);
		DrawSnap(Frame, ScaleSnapLabels, Settings.bSnapScale, Settings.ScaleSnap, ScaleSnaps, FormatPart);
		EndGroup();

		BeginGroup(GroupSpacing);
		DrawCameraSpeed(Frame);
		EndGroup();

		BeginGroup(GroupSpacing);
		DrawViewMode(Frame);
		EndGroup();

		if (!(Settings == Editor.GetGizmoSettings()))
		{
			Editor.SetGizmoSettings(Settings);
		}
	}
	ImGui::EndChild();
}
