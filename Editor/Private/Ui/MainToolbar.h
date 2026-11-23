#pragma once

#include "CoreMinimal.h"

struct FUiFrame;

/**
 * The toolbar below the menu bar of the editor's main window: what is done all the time is one click away.
 *
 * Add opens a list of things to put into the scene, an empty entity, the engine's shapes and the lights, each of
 * which becomes an entity where the viewport's camera looks.
 */
class FMainToolbar
{
public:
	/** Lays the toolbar out, across the main window, and does what the user asked for in it. */
	void Draw(FUiFrame& Frame);

private:
	/** Lays out the list that the Add button opens. */
	void DrawAddMenu(FUiFrame& Frame);
};
