#include "Ui/Panels/UiPanel.h"

bool FUiPanel::Begin(const char* Title, bool& bIsOpen, ImGuiWindowFlags Flags)
{
	// Each tab has a button that closes it, so the row of tabs needs no second one for the tab that is chosen.
	ImGuiWindowClass PanelClass;
	PanelClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoCloseButton;
	ImGui::SetNextWindowClass(&PanelClass);
	return ImGui::Begin(Title, &bIsOpen, Flags);
}
