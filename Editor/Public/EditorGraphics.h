#pragma once

#include "CoreMinimal.h"

#include "Rendering/SceneViewMode.h"

#include <memory>
#include <string>

class FImage;
class FRenderDevice;
class FSceneRenderer;
class WScene;
struct FCameraView;
struct FRenderDeviceDescription;

/**
 * The editor's graphics card: the one render device that an editor draws with, opened the first time something
 * draws and kept until the editor is destroyed. An editor that never draws, as in a test of its commands or in an
 * agent's session that takes no picture, never opens a graphics card and needs none.
 *
 * Only one render device can exist at a time (FRenderDevice::Create), so whatever draws for an editor draws with
 * this one: its window, and the pictures that a command takes (DrawPicture).
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

	/**
	 * Draws a scene as the viewport draws it (FViewportPicture), without the marks that the viewport panel draws over
	 * it, onto a picture of the given size. Opens the render device first, without validation, if nothing has yet,
	 * and makes a scene renderer of its own the first time, which it keeps with what it has put on the graphics
	 * card. The surface that it draws onto is made for each picture and not kept, because a large one takes much
	 * memory.
	 *
	 * @param View Where the picture is seen from. Its proportions are those of the picture.
	 * @param Width The width of the picture, from 1 to FRenderTarget::MaxDimension pixels.
	 * @param Height The height of the picture, likewise.
	 * @param OutPicture Receives the picture, its pixels sRGB-encoded. Unchanged on failure.
	 * @param OutError Receives the reason on failure: the render device could not be opened, as on a machine without
	 * Vulkan 1.3 or beside another render device, or the picture could not be drawn or read back.
	 * @return True if the picture was drawn.
	 */
	EDITOR_API bool DrawPicture(const WScene& Scene,
		const FCameraView& View,
		ESceneViewMode ViewMode,
		int32 Width,
		int32 Height,
		FImage& OutPicture,
		std::string& OutError);

private:
	std::unique_ptr<FRenderDevice> Device;

	/** What DrawPicture draws with; made the first time it is needed. */
	std::unique_ptr<FSceneRenderer> PictureRenderer;
};
