#pragma once

#include "CoreMinimal.h"

#include <optional>
#include <string>
#include <vector>

class WEntity;
struct FUiFrame;

/**
 * The panel that lists the entities of the scene being edited as the tree they form. The user selects entities in
 * it, renames them, creates, copies and deletes them, and drags them onto one another to change the tree.
 */
class FOutlinerPanel
{
public:
	/** What the panel is called on its tab. Dear ImGui also knows the panel by this. */
	static constexpr const char* Title = "Outliner";

	/** What a new entity is called until the user gives it a name. */
	static constexpr const char* NewEntityName = "Entity";

	/** Lays the panel out, and does what the user asked for in it. */
	void Draw(FUiFrame& Frame);

private:
	/**
	 * What the user asked for while the rows were laid out. It is done once they all are, because a command that
	 * creates, destroys or moves an entity changes the lists that the rows are laid out from.
	 */
	struct FPending
	{
		/** The entity whose row was clicked. */
		std::optional<int64> ClickedEntityId;

		/** Set if a new entity was asked for, with the entity it is to be attached to, if any. */
		bool bCreateEntity = false;
		std::optional<int64> NewEntityParentId;

		/** Set if the text field of the entity being renamed was confirmed. */
		bool bCommitRename = false;

		bool bDuplicateSelection = false;
		bool bDeleteSelection = false;

		/** The entities that were dropped, and the entity they were dropped on; on none if at the root. */
		std::vector<int64> DroppedEntityIds;
		std::optional<int64> DropParentId;
	};

	/** Starts the renaming that another part of the user interface, or this panel's own menu, asked for. */
	void BeginRenaming(FUiFrame& Frame);

	/** Lays out the row of an entity and, if the row is open, the rows of its children below it. */
	void DrawTree(FUiFrame& Frame, const WEntity& Entity);

	/**
	 * Lays out the row of one entity.
	 *
	 * @param bHasChildRows True if the rows of the entity's children are to follow when the row is open.
	 * @return True if the row is open and has child rows: they follow, and then ImGui::TreePop.
	 */
	bool DrawRow(FUiFrame& Frame, const WEntity& Entity, bool bHasChildRows);

	/** Lays out the menu that a right click on a row opens. */
	void DrawRowMenu(FUiFrame& Frame, const WEntity& Entity);

	/** Lays out the text field that stands in for the name of the entity being renamed. */
	void DrawRenameField(FUiFrame& Frame);

	/** Makes the last item a place where dragged entities can be dropped, to become children of the given entity. */
	void AcceptDroppedEntities(FUiFrame& Frame, std::optional<int64> ParentId);

	/** Lays out the part of the panel below the rows: a place to drop entities and to ask for a new one. */
	void DrawEmptySpace(FUiFrame& Frame);

	/** Does what the user asked for while the panel was laid out. */
	void CarryOut(FUiFrame& Frame);

	/** What the user has typed into the search field; only entities whose names contain it are listed. */
	std::string SearchText;

	/** The entity whose name is being typed, and what has been typed so far. */
	std::optional<int64> RenamedEntityId;
	std::string RenameText;

	/** True until the text field for the name has been given the keyboard, which takes a frame. */
	bool bFocusRenameField = false;

	FPending Pending;
};
