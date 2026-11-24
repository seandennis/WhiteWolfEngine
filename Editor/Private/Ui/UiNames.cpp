#include "Ui/UiNames.h"

#include "Ui/ImGuiLibrary.h"
#include "Ui/UiFrame.h"
#include "Ui/UiItems.h"

#include <algorithm>

bool FUiNames::Selectable(FUiFrame& Frame, const std::string& ItemName, std::string_view ShownName, bool bIsSelected)
{
	// The line has no text of its own. The name follows it as text that is no label: it begins where the text of
	// the line would, is written whole, and counts for how wide the list is.
	const bool bWasChosen = ImGui::Selectable("##Line", bIsSelected);
	Frame.Items.NameLastItem(ItemName);
	ImGui::SameLine(0.0f, 0.0f);
	ImGui::TextUnformatted(ShownName.data(), ShownName.data() + ShownName.size());
	return bWasChosen;
}

void FUiNames::WriteInField(std::string_view ShownName)
{
	// Where the library writes the text of such a field, and cut off where it cuts that text off: at the arrow.
	const ImVec2 FieldMin = ImGui::GetItemRectMin();
	const ImVec2 FieldMax = ImGui::GetItemRectMax();
	const ImVec2 Padding = ImGui::GetStyle().FramePadding;
	const float ArrowLeft = std::max(FieldMin.x, FieldMax.x - ImGui::GetFrameHeight());
	const ImVec4 TextArea(FieldMin.x, FieldMin.y, ArrowLeft, FieldMax.y);

	// No font and a size of zero are the font and the size that are being drawn with.
	constexpr float NoWrapping = 0.0f;
	ImGui::GetWindowDrawList()->AddText(nullptr,
		0.0f,
		ImVec2(FieldMin.x + Padding.x, FieldMin.y + Padding.y),
		ImGui::GetColorU32(ImGuiCol_Text),
		ShownName.data(),
		ShownName.data() + ShownName.size(),
		NoWrapping,
		&TextArea);
}

bool FUiNames::Heading(std::string_view ShownName)
{
	// The heading has no text of its own, and the name is written where the library writes the text of a heading:
	// after the arrow, which is as wide as a letter is high, with the room that such a heading leaves around it.
	const ImVec2 Corner = ImGui::GetCursorScreenPos();
	const bool bIsOpen =
		ImGui::CollapsingHeader("##Heading", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);

	const ImVec2 Padding = ImGui::GetStyle().FramePadding;
	constexpr float PaddingCount = 3.0f;
	const ImVec2 TextCorner(Corner.x + ImGui::GetFontSize() + Padding.x * PaddingCount, Corner.y + Padding.y);
	const ImVec2 HeadingMin = ImGui::GetItemRectMin();
	const ImVec2 HeadingMax = ImGui::GetItemRectMax();
	const ImVec4 TextArea(HeadingMin.x, HeadingMin.y, HeadingMax.x, HeadingMax.y);
	constexpr float NoWrapping = 0.0f;
	ImGui::GetWindowDrawList()->AddText(nullptr,
		0.0f,
		TextCorner,
		ImGui::GetColorU32(ImGuiCol_Text),
		ShownName.data(),
		ShownName.data() + ShownName.size(),
		NoWrapping,
		&TextArea);
	return bIsOpen;
}
