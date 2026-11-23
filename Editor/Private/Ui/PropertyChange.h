#pragma once

#include "CoreMinimal.h"

#include "Json/JsonValue.h"
#include "Ui/ImGuiLibrary.h"

/** What the user did to the value of a property in one frame, with the widgets that show it. */
struct FPropertyChange
{
	/** The value that the property is to have now, in the form the command Object.SetProperty takes it. */
	FJsonValue Value;

	/** True if that is the value the property has in a new object, asked for with the arrow beside the value. */
	bool bIsReset = false;

	/**
	 * The widget that the user is still holding, such as a number in the middle of being dragged, or 0 if the
	 * change is complete. Everything that a widget changes while it is held is one step of the history.
	 */
	ImGuiID HeldItemId = 0;
};
