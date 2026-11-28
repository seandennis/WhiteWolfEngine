#pragma once

#include "CoreMinimal.h"

#include "Ui/Dialogs/NewMaterialDialog.h"
#include "Ui/Dialogs/NewProjectDialog.h"
#include "Ui/Dialogs/NotSavedDialog.h"
#include "Ui/Dialogs/OpenProjectDialog.h"
#include "Ui/Dialogs/OpenSceneDialog.h"
#include "Ui/Dialogs/SaveSceneDialog.h"

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

	/**
	 * Saves what was changed: each material whose changes are not saved, and then the scene (SaveScene). If a
	 * material cannot be written, the user is told which and why, and the scene is saved when the window that says
	 * so is closed.
	 */
	void Save(FUiFrame& Frame);

	/**
	 * Saves the scene to its file. A scene that has no file yet, or that could not be saved to its file, has the
	 * user asked where to save it.
	 */
	void SaveScene(FUiFrame& Frame);

	/** Set by Help > About: the window that says what the editor is opens in the same frame. */
	bool bWantsAboutWindow = false;

	FNewProjectDialog NewProjectDialog;
	FOpenProjectDialog OpenProjectDialog;
	FSaveSceneDialog SaveSceneDialog;
	FOpenSceneDialog OpenSceneDialog;
	FNewMaterialDialog NewMaterialDialog;
	FNotSavedDialog NotSavedDialog;
};
