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
	 * Returns where an item of the user interface was drawn in the last frame, or nothing if it was not drawn or
	 * is out of sight. The items that tests and tools point the mouse at have names: "Viewport" is the picture of
	 * the scene, "Outliner/7" the row of the entity with the identifier 7, "Menu/File" a menu of the menu bar and
	 * "Menu/File/New Scene" an entry of that menu while it is open.
	 */
	EDITOR_API std::optional<FUiRectangle> FindItem(std::string_view Name) const;

private:
	std::unique_ptr<FImplementation> Implementation;
};
