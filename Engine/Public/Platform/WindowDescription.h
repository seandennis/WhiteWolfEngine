#pragma once

#include "CoreMinimal.h"

#include <string>

/** What a new window should look like. */
struct FWindowDescription
{
	/** The text in the window's title bar, in UTF-8. */
	std::string Title = "White Wolf Engine";

	/** The size of the window's content, in screen coordinates. Both must be positive. */
	int32 Width = 1280;
	int32 Height = 720;

	/** Whether the user can resize the window by dragging its border. */
	bool bResizable = true;
};
