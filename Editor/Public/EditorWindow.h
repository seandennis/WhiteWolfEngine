#pragma once

#include "CoreMinimal.h"

#include "EditorWindowDescription.h"

#include <memory>
#include <string>

class FEditor;
class FEditorUi;
class FRenderDevice;
class FRenderTarget;
class FSwapChain;
class FWindow;

/**
 * The editor's main window: a window of the platform layer, the graphics card, and the editor's user interface
 * drawn into the window, frame after frame.
 *
 * The platform layer has to be running (FPlatform::Initialize) for as long as an editor window exists. In its
 * headless mode the window exists in memory only, and everything happens as it does on a desktop: the window has a
 * size and input, and each frame is drawn. The picture is then shown nowhere, which is how tests run the editor's
 * window; GetPicture is what a window on the desktop would show.
 *
 * It draws with the editor's render device (FEditor::GetGraphics), which it opens if nothing has yet. Create and
 * use it on the main thread. The editor has to outlive it.
 */
class FEditorWindow
{
public:
	/** What the window is called in its title bar, after the name of the scene. */
	static constexpr const char* ApplicationName = "White Wolf Editor";

	/**
	 * Opens the editor's main window.
	 *
	 * @param Editor The editor whose scene the window shows and whose commands its user interface gives.
	 * @param OutError Receives the reason on failure.
	 * @return The window, or null if the platform layer is not running, the window could not be opened, the
	 * editor's render device could not be opened, as on a machine without a graphics card that the engine can draw
	 * with, or pictures cannot be shown in the window.
	 */
	EDITOR_API static std::unique_ptr<FEditorWindow> Create(
		FEditor& Editor, const FEditorWindowDescription& Description, std::string& OutError);

	/** Used by Create. */
	FEditorWindow(FEditor& InEditor,
		std::unique_ptr<FWindow> InWindow,
		FRenderDevice& InDevice,
		std::unique_ptr<FSwapChain> InSwapChain,
		std::unique_ptr<FEditorUi> InUi);

	EDITOR_API ~FEditorWindow();

	FEditorWindow(const FEditorWindow&) = delete;
	FEditorWindow& operator=(const FEditorWindow&) = delete;

	/**
	 * Draws one frame of the editor into the window: lays the user interface out for the window's size, with what
	 * the window's keyboard and mouse did since the last frame, draws it and shows the picture. Call
	 * FPlatform::PollEvents before it, once per frame. A window without an area, as when it is minimised, is
	 * drawn nothing.
	 *
	 * @param DeltaSeconds The time since the last frame.
	 * @param OutError Receives the reason on failure.
	 * @return True if the frame was drawn and shown, or the window has no area.
	 */
	EDITOR_API bool RunFrame(double DeltaSeconds, std::string& OutError);

	/** Returns true once the user has asked to close the window. */
	EDITOR_API bool ShouldClose() const;

	/** Returns true if the window has an area to draw into: it is not minimised. */
	EDITOR_API bool HasArea() const;

	/** Returns the window, for its size and its input. */
	EDITOR_API FWindow& GetWindow();

	/** Returns the user interface that is drawn into the window. */
	EDITOR_API FEditorUi& GetUi();

	/** Returns the picture of the last frame: what the window shows. Null before the first frame is drawn. */
	EDITOR_API const FRenderTarget* GetPicture() const;

	/**
	 * Returns how many frames have been shown in the window. In the headless mode of the platform layer that is
	 * none, unless the graphics driver has a way to show pictures in windows that exist in memory only.
	 */
	EDITOR_API int64 GetShownFrameCount() const;

private:
	FEditor& Editor;

	// In the order they are created, which is the opposite of the order they have to go in: the swap chain before
	// the window it shows in.

	std::unique_ptr<FWindow> Window;

	/** The editor's render device, which the editor keeps. */
	FRenderDevice& Device;

	/** Null for a window that exists in memory only, on a machine whose graphics driver cannot show pictures there. */
	std::unique_ptr<FSwapChain> SwapChain;

	std::unique_ptr<FEditorUi> Ui;

	/** The surface a frame is drawn onto before it is shown: as large as the window. */
	std::unique_ptr<FRenderTarget> Picture;
};
