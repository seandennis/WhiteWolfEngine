#pragma once

#include "CoreMinimal.h"

#include "Input/InputState.h"
#include "Platform/WindowDescription.h"

#include <memory>
#include <string>

/**
 * A window on the desktop, or, when the platform layer runs headless, a window that exists in memory only. Each
 * window has its own input state, which FPlatform::PollEvents keeps up to date.
 *
 * A window is destroyed with its FWindow object, and must be destroyed before FPlatform::Shutdown.
 */
class FWindow
{
private:
	struct FImplementation;

public:
	/**
	 * Creates a window.
	 *
	 * @param OutError Receives the reason on failure.
	 * @return The window, or null if the platform layer is not initialized, the description is invalid or the
	 * window system refused.
	 */
	ENGINE_API static std::unique_ptr<FWindow> Create(const FWindowDescription& Description, std::string& OutError);

	/** Used by Create. Other code cannot call it, because it cannot name the implementation type. */
	explicit FWindow(std::unique_ptr<FImplementation> InImplementation);

	ENGINE_API ~FWindow();

	FWindow(const FWindow&) = delete;
	FWindow& operator=(const FWindow&) = delete;

	ENGINE_API const std::string& GetTitle() const;
	ENGINE_API void SetTitle(const std::string& NewTitle);

	/** Returns the width of the area that is rendered to, in pixels. */
	ENGINE_API int32 GetWidth() const;

	/** Returns the height of the area that is rendered to, in pixels. */
	ENGINE_API int32 GetHeight() const;

	/**
	 * Returns how much larger than on a display of the usual pixel density things have to be drawn in this window
	 * to look the same size: 1 on such a display, 1.5 on one with half again as many pixels to the centimetre. It
	 * is the scale the user has set for the display the window is on, and follows the window to another display.
	 * A user interface multiplies its sizes by it. Always greater than zero.
	 */
	ENGINE_API float GetContentScale() const;

	/**
	 * Asks the window system to resize the window's content. Both values must be positive; anything else is
	 * ignored with a warning. The new size is reported by GetWidth and GetHeight once the window system has
	 * applied it.
	 */
	ENGINE_API void SetSize(int32 NewWidth, int32 NewHeight);

	/** Returns true once the user has asked to close the window, or RequestClose has been called. */
	ENGINE_API bool ShouldClose() const;

	/** Marks the window as wanting to close, as if the user had clicked its close button. */
	ENGINE_API void RequestClose();

	/**
	 * Returns the window as the operating system's window system knows it: an HWND on Windows. Null for a window
	 * that exists in memory only. It is for handing the window to something that is not the engine's, as the
	 * renderer hands it to the graphics card; everything else about a window goes through this class.
	 */
	ENGINE_API void* GetNativeHandle() const;

	/** Returns the keyboard and mouse state of this window. */
	ENGINE_API FInputState& GetInput();
	ENGINE_API const FInputState& GetInput() const;

private:
	std::unique_ptr<FImplementation> Implementation;
};
