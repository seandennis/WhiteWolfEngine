#pragma once

#include "CoreMinimal.h"

#include <string>

class FEditor;
struct FUiFrame;

/**
 * The window in which a material is made: a field for its file in the project's content, which opens with a name
 * that is free, in the materials folder as the project spells it, and Create and Cancel. It has to be answered
 * before anything else is done.
 *
 * A name without the ending of material files is given it. The material that is made has the values of a new one,
 * and is chosen for a mesh as any other is. If it cannot be made there, the window stays and says why.
 */
class FNewMaterialDialog
{
public:
	/**
	 * Has the window open when it is next laid out, with a name in its field that is free: no file of the project
	 * has it, and no material that is loaded has it or the same name in other capitals.
	 */
	void Open(const FEditor& Editor);

	/** Lays the window out if it is open, and makes the material if the user asks for it. */
	void Draw(FUiFrame& Frame);

private:
	/** True from Open until the window has been laid out. */
	bool bWantsToOpen = false;

	/** True until the field has been given the keyboard, which takes a frame after the window has opened. */
	bool bFocusField = false;

	/** What is in the field. */
	std::string PathText;

	/** Why the last attempt to make the material failed; empty if none has. */
	std::string Error;
};
