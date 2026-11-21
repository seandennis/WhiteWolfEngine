#pragma once

#include "CoreMinimal.h"

/** How the platform layer runs. */
enum class EPlatformMode : uint8
{
	/** Windows appear on the desktop and receive input from the user. Needs a window system. */
	Windowed,

	/**
	 * Nothing appears anywhere: windows exist in memory only and receive no input from the operating system.
	 * For tests, build servers and tools, which must run unattended and often on machines without a display.
	 */
	Headless,
};
