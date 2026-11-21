#pragma once

#include "CoreMinimal.h"

/** How a render device is created. */
struct FRenderDeviceDescription
{
	/**
	 * Checks every call the engine makes to the rendering interface and reports misuse as an error in the log.
	 * It costs time on every call, so it is for tests and for debugging the renderer, not for shipping.
	 */
	bool bEnableValidation = false;
};
