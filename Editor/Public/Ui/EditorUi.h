#pragma once

#include "CoreMinimal.h"

#include "Ui/UiRectangle.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

class FEditor;
class FInputState;
class FRenderDevice;
class FRenderTarget;

/**
 * The editor's user interface: the menu bar and the panels through which a person works on the scene that an
 * FEditor holds.
 *
 * It is laid out for a surface and drawn onto it, and does not know about windows. The editor's executable gives it
 * the surface that its window shows and the input of that window. A test gives it a render target and an input state
 * that the test fills in, which is how the user interface is driven and looked at without a person or a window.
 *
 * Whatever the user interface does to the scene, it does by giving the editor a command (FEditor::ExecuteCommand):
 * the same commands that agents and tests give.
 *
 * Create and use it on the main thread. The editor and the render device have to outlive it.
 */
class FEditorUi
{
private:
	struct FImplementation;

public:
	/** The smallest and largest scale of the user interface; see SetScale. */
	static constexpr float MinScale = 0.5f;
	static constexpr float MaxScale = 4.0f;

	/**
	 * Creates the user interface of an editor.
	 *
	 * @param Device The graphics card that the user interface, and the scene in its viewport, are drawn with.
	 * @param OutError Receives the reason on failure.
	 * @return The user interface, or null if the graphics card could not create what drawing it needs.
	 */
	EDITOR_API static std::unique_ptr<FEditorUi> Create(FEditor& Editor, FRenderDevice& Device, std::string& OutError);

	/** Used by Create. Other code cannot call it, because it cannot name the implementation type. */
	explicit FEditorUi(std::unique_ptr<FImplementation> InImplementation);

	EDITOR_API ~FEditorUi();

	FEditorUi(const FEditorUi&) = delete;
	FEditorUi& operator=(const FEditorUi&) = delete;

	/**
	 * Advances the user interface by one frame and draws it: takes in what the user has done since the last frame,
	 * lets the menus and panels act on it, and draws them onto a surface, which they cover completely.
	 *
	 * @param Target The surface to draw onto. The user interface is laid out for its size.
	 * @param Input The keyboard and mouse of whatever shows Target, with positions in the pixels of Target.
	 * @param DeltaSeconds The time since the last frame, with which double clicks and key repeats are timed. A
	 * value of zero or less counts as a very short time.
	 * @param OutError Receives the reason on failure.
	 * @return True if the frame was drawn.
	 */
	EDITOR_API bool Draw(FRenderTarget& Target, const FInputState& Input, double DeltaSeconds, std::string& OutError);

	/**
	 * Makes everything in the user interface larger or smaller, from the next frame on. At 1, the scale it starts
	 * with, it is the size meant for a display of the usual pixel density; on a display that the user has set to
	 * another scale (FWindow::GetContentScale), that scale makes it look the same size there. A scale outside
	 * MinScale to MaxScale is taken as the nearest of the two, and one that is not a number as 1.
	 */
	EDITOR_API void SetScale(float NewScale);

	/** Returns the scale of the user interface. */
	EDITOR_API float GetScale() const;

	/**
	 * Returns where an item of the user interface was drawn in the last frame, or nothing if it was not drawn or
	 * is out of sight. The items that tests and tools point the mouse at have names: "Viewport" is the picture of
	 * the scene, "Outliner/7" the row of the entity with the identifier 7, "Menu/File" a menu of the menu bar and
	 * "Menu/File/New Scene" an entry of that menu while it is open.
	 */
	EDITOR_API std::optional<FUiRectangle> FindItem(std::string_view Name) const;

	/**
	 * Has the window open, in the next frame, that asks which project to work on: the one that File > Open
	 * Project opens, with the projects that were opened last to choose from and a way to make a new one. The
	 * executable asks for it when it starts, because a game is made in a project. Does nothing while another
	 * window is open that has to be answered first.
	 */
	EDITOR_API void AskForProject();

	/**
	 * Returns true if the last frame left the user in the middle of something that needs the mouse to itself and
	 * free to move without end, as steering the viewport's camera with a button held down does. Whoever shows the
	 * user interface in a window takes the mouse for that long (FWindow::CaptureMouse) and gives it back after.
	 */
	EDITOR_API bool WantsMouseCaptured() const;

private:
	std::unique_ptr<FImplementation> Implementation;
};
