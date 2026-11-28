#pragma once

#include "CoreMinimal.h"

#include "Ui/ImGuiLibrary.h"
#include "Ui/PropertyChange.h"
#include "Ui/PropertyDefaults.h"
#include "Ui/PropertyEditor.h"

#include <cstddef>
#include <optional>
#include <string>

class FProperty;
class WAsset;
class WComponent;
class WEntity;
class WObject;
struct FUiFrame;

/**
 * The panel that shows the entity that was selected last: its name, its transform, and each of its components
 * with the properties that are saved with the scene, under the categories they were declared with. The user
 * changes a value where it is shown, puts one back to what it is in a new object with the arrow beside it, adds
 * components and removes them. A material that is a file of its own is shown below the property that refers to
 * it, and changed there as its own values (FEditedAssets).
 *
 * What a property looks like is decided by its type alone (FPropertyEditor), so a new component class is shown
 * and can be changed without anything being written for it.
 */
class FDetailsPanel
{
public:
	/** What the panel is called on its tab. Dear ImGui also knows the panel by this. */
	static constexpr const char* Title = "Details";

	/** Lays the panel out, and does what the user asked for in it. */
	void Draw(FUiFrame& Frame);

private:
	/**
	 * What the user asked for while the panel was laid out. It is done once everything is laid out, because a
	 * command that adds or removes a component, or makes a list longer or shorter, changes what the rows are laid
	 * out from.
	 */
	struct FPending
	{
		/** The name that was typed for the entity. */
		std::optional<std::string> NewName;

		/** The class of the component that was asked for. */
		std::optional<std::string> AddedComponentClass;

		std::optional<int64> RemovedComponentId;

		/**
		 * What was done to a property: of which entity or component, or of which asset if the path is not empty, to
		 * which property, called what.
		 */
		std::optional<FPropertyChange> Change;
		int64 ChangedObjectId = 0;
		std::string ChangedAssetPath;
		std::string ChangedPropertyName;
		std::string ChangedPropertyLabel;
	};

	/** Lays out what the panel shows of an entity. */
	void DrawEntity(FUiFrame& Frame, const WEntity& Entity, std::size_t SelectedCount);

	/** Lays out the entity's name, the button that adds a component and the search field. */
	void DrawHeading(FUiFrame& Frame, const WEntity& Entity, std::size_t SelectedCount);

	/** Lays out the list of component classes that the button opens. */
	void DrawAddComponentMenu(FUiFrame& Frame);

	/** Lays out the entity's transform under its heading. */
	void DrawTransform(FUiFrame& Frame, const WEntity& Entity);

	/** Lays out a component: its heading, and under it its properties by category. */
	void DrawComponent(FUiFrame& Frame, const WComponent& Component);

	/**
	 * Lays out the heading of a component, with the menu that removes the component.
	 *
	 * @param Label What the component is called.
	 * @return True if the heading is open: what the component holds follows.
	 */
	bool DrawComponentHeading(FUiFrame& Frame, const WComponent& Component, const std::string& Label);

	/**
	 * Lays out the rows of one property of an entity or component, and notes what the user did to it. A reference to
	 * an asset that the editor can change, a material that is a file of its own, is followed by the asset's rows.
	 */
	void DrawProperty(FUiFrame& Frame, const WObject& Object, int64 ObjectId, const FProperty& Property);

	/**
	 * Lays out the rows of an asset that a property refers to, below the property's row, and a row that says so if
	 * the asset has changes that are not saved. Their items are named below the property's:
	 * "Details/<ObjectId>/<Property>/Properties/<AssetProperty>" and "Details/<ObjectId>/<Property>/Unsaved".
	 */
	void DrawAssetRows(FUiFrame& Frame, int64 ObjectId, const FProperty& Property, const WAsset& Asset);

	/** Does what the user asked for while the panel was laid out. */
	void CarryOut(FUiFrame& Frame, int64 EntityId);

	/** Gives the editor the value that the user changed a property to, as a step of the history or part of one. */
	void ApplyChange(FUiFrame& Frame);

	/** Ends the step of the history that a widget began, once the user has let go of the widget. */
	void EndFinishedChange(FUiFrame& Frame);

	FPropertyEditor Properties;
	FPropertyDefaults Defaults;

	/** What the user has typed into the search field; only what contains it is shown. */
	std::string SearchText;

	/** What the user has typed into the search field of the list of component classes. */
	std::string ComponentSearchText;

	/**
	 * True while a step of the history is open for what a widget of this panel is changing, and the widget that
	 * the user holds: a number that is dragged through many values changes the scene in every frame, and all of
	 * it is one step.
	 */
	bool bIsChanging = false;
	ImGuiID ChangingItemId = 0;

	FPending Pending;
};
