#pragma once

#include "CoreMinimal.h"

struct FUiFrame;

/**
 * The menu bar along the top of the editor's main window, and the keyboard shortcuts of its entries, which work
 * whether or not a menu is open.
 */
class FMainMenuBar
{
public:
	/** Lays out the menu bar and whatever window one of its entries has opened, and carries out the shortcuts. */
	void Draw(FUiFrame& Frame);

private:
	void DrawFileMenu(FUiFrame& Frame);
	void DrawEditMenu(FUiFrame& Frame);
	void DrawWindowMenu(FUiFrame& Frame);
	void DrawHelpMenu(FUiFrame& Frame);
	void DrawAboutWindow(FUiFrame& Frame);

	/** Set by Help > About: the window that says what the editor is opens in the same frame. */
	bool bWantsAboutWindow = false;
};
