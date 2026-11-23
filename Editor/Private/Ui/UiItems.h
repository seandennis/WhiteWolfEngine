#pragma once

#include "CoreMinimal.h"

#include "Ui/UiRectangle.h"

#include <map>
#include <optional>
#include <string>
#include <string_view>

/**
 * The items of the user interface that have a name, and where each was drawn in the current frame.
 *
 * A name says what an item is, not where: "Outliner/7" is the row of the entity with the identifier 7 wherever the
 * outliner is docked and however far it is scrolled. Tests and tools find an item by its name and point the mouse
 * at it, as a person finds it by looking.
 */
class FUiItems
{
public:
	/** Forgets the items of the last frame. */
	void BeginFrame();

	/**
	 * Gives a name to the item that Dear ImGui laid out last, if any of it can be seen. What is recorded is the
	 * part that can be seen, so that its middle is a place where the mouse reaches the item.
	 */
	void NameLastItem(std::string Name);

	/** Gives a name to a rectangle that the caller has worked out, for an item that is not the last one laid out. */
	void Add(std::string Name, const FUiRectangle& Rectangle);

	/** Returns where the item with the given name is, or nothing if no item of the current frame has that name. */
	std::optional<FUiRectangle> Find(std::string_view Name) const;

private:
	std::map<std::string, FUiRectangle, std::less<>> Rectangles;
};
