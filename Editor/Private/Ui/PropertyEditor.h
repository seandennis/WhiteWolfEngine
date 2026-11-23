#pragma once

#include "CoreMinimal.h"

#include "Math/Rotator.h"
#include "Math/Transform.h"
#include "Ui/ImGuiLibrary.h"
#include "Ui/PropertyChange.h"

#include <optional>
#include <string>
#include <string_view>

class FArrayProperty;
class FProperty;
class WClass;
class WObject;
struct FLinearColor;
struct FUiFrame;

/**
 * Lays out the rows that show the values of reflected properties and let the user change them: a box to tick for
 * a bool, a number to drag or type for a number, three for a vector, angles for a rotation, a colour to pick, a
 * list to choose from for a reference to an entity or component, and so on for every type that can be a property.
 *
 * The rows go into a table of two columns, what a value is called and the value, begun with BeginRows. A widget
 * does not change the value it shows. When the user has changed what it shows, the function that laid it out
 * returns the value the property is to have, and whoever called it gives the editor the command (FUiCommands).
 *
 * It is one object per panel, because it keeps what is being typed and the angles that are being dragged until the
 * user is done with them.
 */
class FPropertyEditor
{
public:
	/**
	 * Begins the table that rows are laid out in. If it returns true, rows follow and then EndRows.
	 *
	 * @param Id What Dear ImGui knows the table by among the items around it.
	 */
	static bool BeginRows(const char* Id);

	/** Ends the table that BeginRows began. */
	static void EndRows();

	/**
	 * Lays out the rows of one property: a row with its value, or one row for each of its parts if it is a
	 * transform or a list.
	 *
	 * @param Property What kind of value it is.
	 * @param Value The address of the value (FProperty::GetValuePtr).
	 * @param Label What the value is called in the first column.
	 * @param ItemName The name that the widget of the value is given among the named items (FUiItems). The parts
	 * of a value that has several are given names below it: "<ItemName>/X", "<ItemName>/Reset".
	 * @param Default The value that the property has in a new object, which an arrow beside a different value
	 * puts back; null if that is not known, and no arrow is shown.
	 * @return What the user changed in this frame, if anything.
	 */
	std::optional<FPropertyChange> DrawProperty(FUiFrame& Frame,
		const FProperty& Property,
		const void* Value,
		std::string_view Label,
		const std::string& ItemName,
		const FJsonValue* Default);

	/**
	 * Lays out a transform as the rows Location, Rotation and Scale, without a row for the transform itself: the
	 * transform of an entity is shown this way, under a heading of its own.
	 *
	 * @param Default The transform whose parts the arrows put back; null for no arrows.
	 * @param SearchText Only the rows whose names contain this are laid out.
	 */
	std::optional<FPropertyChange> DrawTransform(FUiFrame& Frame,
		const FTransform& Value,
		const FTransform* Default,
		const std::string& ItemName,
		std::string_view SearchText);

	/**
	 * Lays out a line of text that the user can type over. What is typed stays in the field until the user is
	 * done: Enter, or going elsewhere, returns it; Escape puts back what was there.
	 *
	 * @param Id What Dear ImGui knows the field by among the items around it.
	 * @param Text What the field shows while it is not being typed into.
	 * @param Hint What an empty field shows, greyed.
	 * @return The text that was typed, once the user is done and if it is another than Text.
	 */
	std::optional<std::string> EditText(const char* Id, const std::string& Text, const char* Hint);

private:
	/** What the widgets of one value did in a frame. */
	struct FValueEdit
	{
		/** The value that the user changed it to, as JSON; nothing if it is as it was. */
		std::optional<FJsonValue> NewValue;

		/** The widget that the user is still holding; see FPropertyChange. */
		ImGuiID HeldItemId = 0;
	};

	/** What the user did in the rows of one value. */
	struct FRowsEdit
	{
		std::optional<FJsonValue> NewValue;
		ImGuiID HeldItemId = 0;

		/** True if NewValue is the default, asked for with the arrow. */
		bool bIsReset = false;

		/** True if the user asked for the value, an element of a list, to be taken out of its list. */
		bool bWantsRemoval = false;
	};

	/** What stands at the end of a row: an arrow that puts a property back, or a cross that removes an element. */
	enum class ERowEnd : uint8
	{
		ResetArrow,
		RemoveCross,
	};

	/** Lays out the rows of a value of any kind; see DrawProperty. */
	FRowsEdit DrawRows(FUiFrame& Frame,
		const FProperty& Property,
		const void* Value,
		std::string_view Label,
		const std::string& ItemName,
		const FJsonValue* Default,
		ERowEnd RowEnd);

	/** Lays out a list: a row that says how long it is, with a button that makes it longer, and a row per element. */
	FRowsEdit DrawArray(FUiFrame& Frame,
		const FArrayProperty& Property,
		const void* Value,
		std::string_view Label,
		const std::string& ItemName,
		const FJsonValue* Default,
		ERowEnd RowEnd);

	/** Lays out the three rows of a transform; see DrawTransform. */
	FValueEdit DrawTransformParts(FUiFrame& Frame,
		const FTransform& Value,
		const FTransform* Default,
		const std::string& ItemName,
		std::string_view SearchText,
		bool& bOutIsReset);

	/** Lays out the widgets of a value that takes one row, in the width given. */
	FValueEdit EditValue(
		FUiFrame& Frame, const FProperty& Property, const void* Value, const std::string& ItemName, float Width);

	/**
	 * Lays out the three angles of a rotation.
	 *
	 * @param OutHeldItemId Receives the angle that the user is still holding, or 0.
	 * @return The rotation that the user turned it into in this frame, if any.
	 */
	std::optional<FQuat> EditRotation(
		FUiFrame& Frame, const FQuat& Rotation, const std::string& ItemName, float Width, ImGuiID& OutHeldItemId);

	/**
	 * Lays out a colour: a button that shows it, and that opens a window in which to pick it or to type its
	 * amounts of light.
	 */
	static FValueEdit EditColor(FUiFrame& Frame, const FLinearColor& Color, const std::string& ItemName, float Width);

	/**
	 * Lays out a reference to an object of a class: a list of the scene's entities or components to choose from,
	 * or the path of an asset to type.
	 */
	FValueEdit EditReference(FUiFrame& Frame,
		const WClass& ReferencedClass,
		const WObject* Referenced,
		const std::string& ItemName,
		float Width);

	/** The field that is being typed into, when it was last laid out, and what has been typed; see EditText. */
	ImGuiID TypedItemId = 0;
	int32 TypedFrame = 0;
	std::string TypedText;

	/**
	 * The rotation whose angles are being changed, when it was last laid out, and the angles. They are kept while
	 * the user holds them, because the angles read back from a rotation are not always the ones it was made from.
	 */
	ImGuiID TurnedItemId = 0;
	int32 TurnedFrame = 0;
	FRotator TurnedAngles;
};
