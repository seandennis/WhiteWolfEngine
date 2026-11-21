#pragma once

#include "CoreMinimal.h"

#include "Platform/PlatformMode.h"

#include <string>

/** Log category of the platform layer: windows and input. */
extern ENGINE_API FLogCategory LogPlatform;

/**
 * The connection to the operating system's window system. Initialize it before creating a window and shut it
 * down after the last window is gone. It is used from the main thread only.
 */
class FPlatform
{
public:
	/**
	 * Starts the platform layer. It must not be initialized already.
	 *
	 * @param OutError Receives the reason on failure, for example that there is no display to open windows on.
	 * @return True if the platform layer is running in the given mode.
	 */
	ENGINE_API static bool Initialize(EPlatformMode Mode, std::string& OutError);

	/**
	 * Stops the platform layer. Every window must have been destroyed; shutting down with windows left is a fatal
	 * error. Does nothing if the platform layer is not initialized.
	 */
	ENGINE_API static void Shutdown();

	ENGINE_API static bool IsInitialized();

	/** Returns the mode the platform layer was initialized with. It must be initialized. */
	ENGINE_API static EPlatformMode GetMode();

	/**
	 * Processes what the operating system has reported since the last call: starts a new input frame for every
	 * window (FInputState::BeginFrame) and then delivers key, mouse, resize and close events to them. Call it once
	 * per frame. It must be initialized.
	 */
	ENGINE_API static void PollEvents();
};
