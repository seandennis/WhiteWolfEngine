#pragma once

#include "CoreMinimal.h"

#include "Ui/ImGuiLibrary.h"

/** What the editor's panels have in common: each is a window that is docked into the main window as a tab. */
class FUiPanel
{
public:
	/**
	 * Begins a panel. What the panel shows follows if this returns true, and ImGui::End follows in either case.
	 *
	 * @param Title What the panel is called on its tab. Dear ImGui also knows the panel by this.
	 * @param bIsOpen Set to false when the user closes the panel with the button on its tab.
	 * @return True if the panel can be seen: it is the chosen tab where it is docked.
	 */
	static bool Begin(const char* Title, bool& bIsOpen, ImGuiWindowFlags Flags = 0);
};
