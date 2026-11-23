#include "Ui/Dialogs/UiDialog.h"

#include "Math/Color.h"
#include "Ui/ImGuiLibrary.h"
#include "Ui/UiText.h"
#include "Ui/UiTheme.h"

#include <string_view>

namespace
{
	/** How wide a field is, in heights of a letter: room for the whole path of a file. */
	constexpr float FieldWidthInLetters = 40.0f;

	constexpr float MaxChannel = 255.0f;
}

bool FUiDialog::Begin(const char* Title)
{
	const ImGuiViewport* const MainViewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(MainViewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	return ImGui::BeginPopupModal(Title, nullptr, ImGuiWindowFlags_AlwaysAutoResize);
}

void FUiDialog::ShowError(const std::string& Error)
{
	if (Error.empty())
	{
		return;
	}

	// The sentences of commands can be long: they are broken at the width of the fields above them.
	const FColor Color = FUiTheme::GetErrorColor();
	ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + GetFieldWidth());
	ImGui::PushStyleColor(ImGuiCol_Text,
		ImVec4(static_cast<float>(Color.R) / MaxChannel,
			static_cast<float>(Color.G) / MaxChannel,
			static_cast<float>(Color.B) / MaxChannel,
			1.0f));
	ImGui::TextUnformatted(Error.c_str());
	ImGui::PopStyleColor();
	ImGui::PopTextWrapPos();
}

float FUiDialog::GetFieldWidth()
{
	return ImGui::GetFontSize() * FieldWidthInLetters;
}

std::string FUiDialog::FitPath(const std::string& Path, float Width)
{
	return FUiText::FitEnd(Path,
		Width,
		[](std::string_view Text) { return ImGui::CalcTextSize(Text.data(), Text.data() + Text.size()).x; });
}
