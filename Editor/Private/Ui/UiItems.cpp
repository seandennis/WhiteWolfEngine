#include "Ui/UiItems.h"

#include "Ui/ImGuiLibrary.h"

#include <utility>

void FUiItems::BeginFrame()
{
	Rectangles.clear();
}

void FUiItems::NameLastItem(std::string Name)
{
	if (!ImGui::IsItemVisible())
	{
		return;
	}

	// An item at the edge of a scrolled panel is cut off there.
	ImRect Visible(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
	Visible.ClipWith(ImGui::GetCurrentWindowRead()->ClipRect);
	if (Visible.GetWidth() <= 0.0f || Visible.GetHeight() <= 0.0f)
	{
		return;
	}

	FUiRectangle Rectangle;
	Rectangle.Left = Visible.Min.x;
	Rectangle.Top = Visible.Min.y;
	Rectangle.Right = Visible.Max.x;
	Rectangle.Bottom = Visible.Max.y;
	Add(std::move(Name), Rectangle);
}

void FUiItems::Add(std::string Name, const FUiRectangle& Rectangle)
{
	Rectangles.insert_or_assign(std::move(Name), Rectangle);
}

std::optional<FUiRectangle> FUiItems::Find(std::string_view Name) const
{
	const auto Found = Rectangles.find(Name);
	if (Found == Rectangles.end())
	{
		return std::nullopt;
	}
	return Found->second;
}
