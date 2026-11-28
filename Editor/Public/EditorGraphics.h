#pragma once

#include "CoreMinimal.h"

#include <memory>
#include <string>

class FRenderDevice;
struct FRenderDeviceDescription;

/**
 * The editor's graphics card: the one render device that an editor draws with, opened the first time something
 * draws and kept until the editor is destroyed. An editor that never draws, as in a test of its commands or in an
 * agent's session, never opens a graphics card and needs none.
 *
 * Only one render device can exist at a time (FRenderDevice::Create), so whatever draws for an editor draws with
 * this one: its window, and a picture that a command takes.
 */
class FEditorGraphics
{
public:
	EDITOR_API FEditorGraphics();
	EDITOR_API ~FEditorGraphics();

	FEditorGraphics(const FEditorGraphics&) = delete;
	FEditorGraphics& operator=(const FEditorGraphics&) = delete;

	/**
	 * Returns the render device, and opens it with the given description first if it is not open. Once it is open,
	 * the description is not looked at again.
	 *
	 * @param OutError Receives the reason on failure: the machine has no graphics card or driver that draws with
	 * Vulkan 1.3, or another render device exists.
	 * @return The device, or null on failure. A failure is not remembered: the next call tries again.
	 */
	EDITOR_API FRenderDevice* Open(const FRenderDeviceDescription& Description, std::string& OutError);

	/** Returns true once the render device is open. */
	bool IsOpen() const
	{
		return Device != nullptr;
	}

private:
	std::unique_ptr<FRenderDevice> Device;
};
