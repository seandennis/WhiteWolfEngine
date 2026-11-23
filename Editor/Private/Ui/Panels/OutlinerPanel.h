#pragma once

#include "CoreMinimal.h"

#include <array>
#include <optional>

class WEntity;
struct FUiFrame;

/**
 * The panel that lists the entities of the scene being edited as the tree they form, in which the user selects
 * them.
 */
class FOutlinerPanel
{
public:
	/** What the panel is called on its tab. Dear ImGui also knows the panel by this. */
	static constexpr const char* Title = "Outliner";

	/** Lays the panel out, and selects what the user clicks on. */
	void Draw(FUiFrame& Frame);

private:
	/** Lays out the row of an entity and, if the row is open, the rows of its children below it. */
	void DrawTree(FUiFrame& Frame, const WEntity& Entity);

	/**
	 * Lays out the row of one entity.
	 *
	 * @param bHasChildRows True if the rows of the entity's children are to follow when the row is open.
	 * @return True if the row is open and has child rows: they follow, and then ImGui::TreePop.
	 */
	bool DrawRow(FUiFrame& Frame, const WEntity& Entity, bool bHasChildRows);

	/** What the user has typed into the search field; only entities whose names contain it are listed. */
	std::array<char, 128> SearchText{};

	/** The identifier of the entity whose row was clicked in this frame. Acted on once all rows are laid out. */
	std::optional<int64> ClickedEntityId;
};
