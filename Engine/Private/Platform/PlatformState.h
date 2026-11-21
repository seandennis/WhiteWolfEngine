#pragma once

#include "CoreMinimal.h"

#include "Platform/PlatformMode.h"

#include <string>
#include <vector>

class FWindow;

/** What the platform layer keeps between calls. Shared by FPlatform and FWindow; internal to the Engine module. */
struct FPlatformState
{
	bool bIsInitialized = false;
	EPlatformMode Mode = EPlatformMode::Headless;

	/** Every window that exists, in creation order. */
	std::vector<FWindow*> Windows;

	/** The description of the most recent error reported by the window system. */
	std::string LastError;

	/** Returns the one platform state. */
	static FPlatformState& Get();
};
