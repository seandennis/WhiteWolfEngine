#pragma once

#include "CoreMinimal.h"

#include <memory>
#include <string>

class FRenderDevice;
class FRenderTarget;
class FWindow;

/**
 * Shows pictures in a window: what the renderer has drawn onto a render target is copied onto the surface of the
 * window, where the user sees it.
 *
 * Everything the engine draws, it draws onto render targets, without a window. A swap chain is the last step, and
 * the only one that needs a window: once per frame, the program gives it the render target that holds the finished
 * picture.
 *
 * A swap chain belongs to one window, which must outlive it. Like everything created from a render device it keeps
 * the graphics card alive. It is used on the main thread.
 */
class FSwapChain
{
private:
	struct FImplementation;

public:
	/**
	 * Returns true if pictures can be shown in a window with a render device. That takes a graphics card that can
	 * hand pictures to the window system. A window that exists in memory only, as the platform layer makes in its
	 * headless mode, takes more: a graphics driver that has a surface for such windows, which shows nothing
	 * anywhere but lets everything else happen as it does on a desktop. Not every driver has one.
	 */
	ENGINE_API static bool IsSupported(const FRenderDevice& Device, const FWindow& Window);

	/**
	 * Creates the swap chain of a window.
	 *
	 * @param OutError Receives the reason on failure.
	 * @return The swap chain, or null if pictures cannot be shown in this window (see IsSupported) or the window
	 * system refused.
	 */
	ENGINE_API static std::unique_ptr<FSwapChain> Create(FRenderDevice& Device, FWindow& Window, std::string& OutError);

	/** Used by Create. Other code cannot call it, because it cannot name the implementation type. */
	explicit FSwapChain(std::unique_ptr<FImplementation> InImplementation);

	ENGINE_API ~FSwapChain();

	FSwapChain(const FSwapChain&) = delete;
	FSwapChain& operator=(const FSwapChain&) = delete;

	/**
	 * Shows a picture in the window, pixel for pixel from the top left corner; give it a render target of the
	 * window's size (FWindow::GetWidth, GetHeight). Where the window is larger than the picture it is black.
	 *
	 * The call returns when the display takes the picture, which it does at its own rate: a program that draws in
	 * a loop is paced by this. When the window has changed its size the swap chain follows. A window without an
	 * area, as when it is minimised, is shown nothing, and the call returns at once.
	 *
	 * @param OutError Receives the reason on failure.
	 * @return True if the picture was handed to the window, or the window has no area.
	 */
	ENGINE_API bool Present(const FRenderTarget& Picture, std::string& OutError);

	/** Returns how many pictures have been handed to the window since the swap chain was created. */
	ENGINE_API int64 GetPresentedCount() const;

	/**
	 * Returns the width of the surface that pictures are shown on, in pixels: the window's, as of the last call of
	 * Present or of Create. Zero while the window has no area.
	 */
	ENGINE_API int32 GetWidth() const;

	/** Returns the height of the surface that pictures are shown on; see GetWidth. */
	ENGINE_API int32 GetHeight() const;

private:
	std::unique_ptr<FImplementation> Implementation;
};
