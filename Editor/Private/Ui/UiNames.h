#pragma once

#include "CoreMinimal.h"

#include <string>
#include <string_view>

struct FUiFrame;

/**
 * Widgets that show a name which is not theirs to know them by.
 *
 * For Dear ImGui the text of a widget is also what the widget is known by, and it ends at the first "##": what
 * follows only tells widgets apart. A name that a user, a file or a module gives may have that in it, and would be
 * shown cut off; it may have no letters at all, which the library does not take for a text. These widgets are
 * known by a text of their own, in the scope that they are laid out in (ImGui::PushID), and show the name whole,
 * whatever is in it.
 */
class FUiNames
{
public:
	/**
	 * Lays out a line of a list that shows a name, and gives the line the name that tests find it by. Every line
	 * of a list needs a scope of its own, which is what tells the lines apart.
	 *
	 * @param ItemName What the line is called among the items of the user interface (FUiItems).
	 * @param ShownName What the line says.
	 * @param bIsSelected True for the line of what is in force, which is marked.
	 * @return True if the line was chosen.
	 */
	static bool Selectable(FUiFrame& Frame, const std::string& ItemName, std::string_view ShownName, bool bIsSelected);

	/**
	 * Writes a name into the field of a list that was begun without a text of its own (ImGui::BeginCombo with a
	 * null preview), as far as the field's arrow. The field has to be the item that was laid out last, which it is
	 * again once its list has been ended.
	 */
	static void WriteInField(std::string_view ShownName);

	/**
	 * Lays out a heading that shows a name, and that opens and closes what follows it: open to begin with, and
	 * with room for other items to lie on it and be clicked. It is the only heading of the scope that it is laid
	 * out in, and the item that was laid out last when this returns.
	 *
	 * @return True if the heading is open.
	 */
	static bool Heading(std::string_view ShownName);
};
