#include "Ui/Dialogs/UiDialog.h"

#include "Math/Color.h"
#include "Ui/ImGuiLibrary.h"
#include "Ui/UiText.h"
#include "Ui/UiTheme.h"

#include <initializer_list>
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
	if (!ImGui::BeginPopupModal(Title, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		return false;
	}

	// A key that is down when the window appears was pressed for what was there before, as the Escape that closed
	// another window or the Enter that chose the entry of a menu. Until it is let go, nothing reads it: neither an
	// answer to the window nor a field that is told Enter, also when the key repeats while it is held.
	if (ImGui::IsWindowAppearing())
	{
		const ImGuiID HeldFromBefore = ImGui::GetID("##KeyHeldFromBefore");
		for (const ImGuiKey Key : {ImGuiKey_Enter, ImGuiKey_KeypadEnter, ImGuiKey_Escape})
		{
			if (ImGui::IsKeyDown(Key))
			{
				ImGui::SetKeyOwner(Key, HeldFromBefore, ImGuiInputFlags_LockUntilRelease);
			}
		}
	}
	return true;
}

bool FUiDialog::IsAnswerKeyPressed(ImGuiKey Key)
{
	// Pressed, not repeated: a key that is held is one answer. One held from before the window is not read (Begin).
	return ImGui::IsKeyPressed(Key, false);
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
