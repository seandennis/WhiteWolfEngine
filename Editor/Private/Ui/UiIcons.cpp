#include "Ui/UiIcons.h"

#include "Ui/ImGuiLibrary.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float Pi = 3.14159265f;

	/** How long the arms of a sign are, as a part of the size of its button. */
	constexpr float SignReach = 0.22f;

	/** How thick the lines of a sign are, as a part of the height of the letters. */
	constexpr float LineThickness = 0.1f;

	/** What a sign is drawn with: where, how large and in which colour. */
	struct FSignCanvas
	{
		ImDrawList* DrawList = nullptr;
		ImVec2 Center;
		float Reach = 0.0f;
		float Thickness = 1.0f;
		ImU32 Color = 0;
	};

	/**
	 * Lays out a square button without a sign and says how to draw one on it.
	 *
	 * @return True if the button was clicked.
	 */
	bool SignButton(const char* Id, FSignCanvas& OutCanvas)
	{
		const float Size = ImGui::GetFrameHeight();
		const bool bWasClicked = ImGui::InvisibleButton(Id, ImVec2(Size, Size));
		const bool bIsHovered = ImGui::IsItemHovered();
		const ImVec2 Min = ImGui::GetItemRectMin();
		const ImVec2 Max = ImGui::GetItemRectMax();

		OutCanvas.DrawList = ImGui::GetWindowDrawList();
		if (bIsHovered)
		{
			const ImGuiCol Background = ImGui::IsItemActive() ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered;
			OutCanvas.DrawList->AddRectFilled(
				Min, Max, ImGui::GetColorU32(Background), ImGui::GetStyle().FrameRounding);
		}
		OutCanvas.Center = ImVec2((Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f);
		OutCanvas.Reach = Size * SignReach;
		OutCanvas.Thickness = std::max(1.0f, ImGui::GetFontSize() * LineThickness);
		OutCanvas.Color = ImGui::GetColorU32(bIsHovered ? ImGuiCol_Text : ImGuiCol_TextDisabled);
		return bWasClicked;
	}

	ImVec2 OnCircle(const ImVec2& Center, float Radius, float Angle)
	{
		return ImVec2(Center.x + std::cos(Angle) * Radius, Center.y + std::sin(Angle) * Radius);
	}
}

bool FUiIcons::ResetButton(const char* Id)
{
	FSignCanvas Canvas;
	const bool bWasClicked = SignButton(Id, Canvas);

	// Three quarters of a circle, open at the top, with an arrowhead at its right end that points back across the
	// opening: an arrow that goes around against the clock, as the sign for taking something back does. Angles
	// are measured clockwise from the right, as the library does, because the screen's y axis points down.
	constexpr float HeadEnd = -0.25f * Pi;
	constexpr float TailEnd = 1.25f * Pi;
	constexpr int32 SegmentCount = 16;
	Canvas.DrawList->PathArcTo(Canvas.Center, Canvas.Reach, HeadEnd, TailEnd, SegmentCount);
	Canvas.DrawList->PathStroke(Canvas.Color, Canvas.Thickness);

	// The head points on along the circle, the way the line was going when it got there.
	const ImVec2 End = OnCircle(Canvas.Center, Canvas.Reach, HeadEnd);
	const ImVec2 Along(std::sin(HeadEnd), -std::cos(HeadEnd));
	const ImVec2 Across(std::cos(HeadEnd), std::sin(HeadEnd));
	const float HeadSize = Canvas.Reach * 0.75f;
	const ImVec2 Tip(End.x + Along.x * HeadSize, End.y + Along.y * HeadSize);
	const ImVec2 Inner(End.x - Across.x * HeadSize, End.y - Across.y * HeadSize);
	const ImVec2 Outer(End.x + Across.x * HeadSize, End.y + Across.y * HeadSize);
	Canvas.DrawList->AddTriangleFilled(Tip, Inner, Outer, Canvas.Color);
	return bWasClicked;
}

bool FUiIcons::RemoveButton(const char* Id)
{
	FSignCanvas Canvas;
	const bool bWasClicked = SignButton(Id, Canvas);

	const float Reach = Canvas.Reach;
	const ImVec2& Center = Canvas.Center;
	Canvas.DrawList->AddLine(ImVec2(Center.x - Reach, Center.y - Reach),
		ImVec2(Center.x + Reach, Center.y + Reach),
		Canvas.Color,
		Canvas.Thickness);
	Canvas.DrawList->AddLine(ImVec2(Center.x - Reach, Center.y + Reach),
		ImVec2(Center.x + Reach, Center.y - Reach),
		Canvas.Color,
		Canvas.Thickness);
	return bWasClicked;
}

bool FUiIcons::AddButton(const char* Id)
{
	FSignCanvas Canvas;
	const bool bWasClicked = SignButton(Id, Canvas);

	const float Reach = Canvas.Reach * 1.2f;
	const ImVec2& Center = Canvas.Center;
	Canvas.DrawList->AddLine(
		ImVec2(Center.x - Reach, Center.y), ImVec2(Center.x + Reach, Center.y), Canvas.Color, Canvas.Thickness);
	Canvas.DrawList->AddLine(
		ImVec2(Center.x, Center.y - Reach), ImVec2(Center.x, Center.y + Reach), Canvas.Color, Canvas.Thickness);
	return bWasClicked;
}

float FUiIcons::GetButtonSize()
{
	return ImGui::GetFrameHeight();
}
