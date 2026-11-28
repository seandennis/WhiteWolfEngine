#include "Ui/Dialogs/NotSavedDialog.h"

#include "Ui/Dialogs/UiDialog.h"
#include "Ui/ImGuiLibrary.h"
#include "Ui/UiFrame.h"
#include "Ui/UiItems.h"

#include <utility>

namespace
{
	constexpr const char* Title = "Not Saved";
}

void FNotSavedDialog::Open(std::vector<std::string> InProblems)
{
	bWantsToOpen = true;
	Problems = std::move(InProblems);
}

bool FNotSavedDialog::Draw(FUiFrame& Frame)
{
	if (bWantsToOpen)
	{
		ImGui::OpenPopup(Title);
		bWantsToOpen = false;
	}
	if (!FUiDialog::Begin(Title))
	{
		return false;
	}

	ImGui::TextUnformatted("These could not be written to their files:");
	for (const std::string& Problem : Problems)
	{
		FUiDialog::ShowError(Problem);
	}

	const bool bWasCloseClicked = ImGui::Button("Close");
	Frame.Items.NameLastItem("NotSaved/Close");
	const bool bWasClosed =
		bWasCloseClicked || ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Escape);
	if (bWasClosed)
	{
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();
	return bWasClosed;
}
