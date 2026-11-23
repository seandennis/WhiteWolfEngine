#include "Ui/Panels/OutlinerPanel.h"

#include "Editor.h"
#include "Object/ObjectPtr.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Ui/ImGuiLibrary.h"
#include "Ui/Panels/UiPanel.h"
#include "Ui/UiCommands.h"
#include "Ui/UiFrame.h"
#include "Ui/UiItems.h"
#include "Ui/UiRequests.h"
#include "Ui/UiText.h"
#include "Ui/UiWorkspace.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <format>
#include <utility>

namespace
{
	/** What Dear ImGui calls the thing that is dragged when a row is: the identifier of the row's entity. */
	constexpr const char* EntityPayloadType = "WhiteWolfEntity";

	/**
	 * Returns the entities that go where a dragged row goes: everything that is selected if the row is among it,
	 * and the row's entity alone if it is not.
	 */
	std::vector<int64> GetDraggedEntities(const FEditor& Editor, int64 DraggedEntityId)
	{
		std::vector<int64> Selection = Editor.GetSelection();
		if (std::ranges::find(Selection, DraggedEntityId) != Selection.end())
		{
			return Selection;
		}
		return {DraggedEntityId};
	}

	/** Lays out an entry of a menu of the outliner, and gives it the name "Outliner/Menu/<Label>". */
	bool MenuEntry(FUiFrame& Frame, const char* Label, const char* Shortcut = nullptr)
	{
		const bool bWasChosen = ImGui::MenuItem(Label, Shortcut);
		Frame.Items.NameLastItem(std::format("Outliner/Menu/{}", Label));
		return bWasChosen;
	}
}

void FOutlinerPanel::Draw(FUiFrame& Frame)
{
	if (!Frame.Workspace.bShowOutliner)
	{
		return;
	}

	if (FUiPanel::Begin(Title, Frame.Workspace.bShowOutliner))
	{
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##Search", "Search", &SearchText);
		Frame.Items.NameLastItem("Outliner/Search");

		Pending = FPending();
		BeginRenaming(Frame);

		const WScene& Scene = Frame.Editor.GetScene();
		if (SearchText.empty())
		{
			for (const TObjectPtr<WEntity>& Root : Scene.GetRootEntities())
			{
				DrawTree(Frame, *Root);
			}
		}
		else
		{
			// What is searched for is listed by itself, without the entities around it.
			for (const WEntity* const Entity : Scene.GetAllEntities())
			{
				if (FUiText::ContainsIgnoringCase(Entity->GetName(), SearchText))
				{
					DrawRow(Frame, *Entity, false);
				}
			}
		}

		DrawEmptySpace(Frame);
		CarryOut(Frame);
	}
	ImGui::End();
}

void FOutlinerPanel::BeginRenaming(FUiFrame& Frame)
{
	const WScene& Scene = Frame.Editor.GetScene();
	if (Frame.Requests.EntityToRename.has_value())
	{
		if (const WEntity* const Entity = Scene.FindEntityById(*Frame.Requests.EntityToRename))
		{
			RenamedEntityId = Entity->GetId();
			RenameText = Entity->GetName();
			bFocusRenameField = true;
		}
		Frame.Requests.EntityToRename.reset();
	}

	// An undo, or an agent, can take the entity away while its name is being typed.
	if (RenamedEntityId.has_value() && Scene.FindEntityById(*RenamedEntityId) == nullptr)
	{
		RenamedEntityId.reset();
	}
}

void FOutlinerPanel::DrawTree(FUiFrame& Frame, const WEntity& Entity)
{
	const std::vector<TObjectPtr<WEntity>>& Children = Entity.GetChildren();
	if (DrawRow(Frame, Entity, !Children.empty()))
	{
		for (const TObjectPtr<WEntity>& Child : Children)
		{
			DrawTree(Frame, *Child);
		}
		ImGui::TreePop();
	}
}

bool FOutlinerPanel::DrawRow(FUiFrame& Frame, const WEntity& Entity, bool bHasChildRows)
{
	const int64 EntityId = Entity.GetId();
	const bool bIsBeingRenamed = RenamedEntityId == EntityId;

	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen |
		ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
	if (!bHasChildRows)
	{
		// A row with nothing below it has no arrow, and nothing has to be closed after it.
		Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}
	if (Frame.Editor.IsSelected(EntityId))
	{
		Flags |= ImGuiTreeNodeFlags_Selected;
	}
	if (bIsBeingRenamed)
	{
		// The text field lies on the row, where the name was.
		Flags |= ImGuiTreeNodeFlags_AllowOverlap;
	}

	// Dear ImGui tells rows apart by the identifier, so two entities may have the same name.
	const void* const RowId = reinterpret_cast<const void*>(static_cast<std::intptr_t>(EntityId));
	const bool bIsOpen = ImGui::TreeNodeEx(RowId, Flags, "%s", bIsBeingRenamed ? "" : Entity.GetName().c_str());
	Frame.Items.NameLastItem(std::format("Outliner/{}", EntityId));

	// A click on the arrow opens or closes the row and selects nothing. Pressing the button on a row that is
	// selected with others may be the start of dragging them all, so it selects that row alone only once the
	// button comes up again without the mouse having gone anywhere.
	const ImGuiIO& Io = ImGui::GetIO();
	const bool bIsOneOfSeveral = Frame.Editor.IsSelected(EntityId) && !Io.KeyCtrl;
	if (bIsOneOfSeveral)
	{
		const bool bWasDragged =
			Io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] >= Io.MouseDragThreshold * Io.MouseDragThreshold;
		if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !bWasDragged)
		{
			Pending.ClickedEntityId = EntityId;
		}
	}
	else if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
	{
		Pending.ClickedEntityId = EntityId;
	}

	if (ImGui::BeginDragDropSource())
	{
		ImGui::SetDragDropPayload(EntityPayloadType, &EntityId, sizeof(EntityId));
		const std::size_t DraggedCount = GetDraggedEntities(Frame.Editor, EntityId).size();
		if (DraggedCount == 1)
		{
			ImGui::TextUnformatted(Entity.GetName().c_str());
		}
		else
		{
			ImGui::TextUnformatted(std::format("{} entities", DraggedCount).c_str());
		}
		ImGui::EndDragDropSource();
	}
	AcceptDroppedEntities(Frame, EntityId);

	DrawRowMenu(Frame, Entity);
	if (bIsBeingRenamed)
	{
		DrawRenameField(Frame);
	}
	return bIsOpen && bHasChildRows;
}

void FOutlinerPanel::DrawRowMenu(FUiFrame& Frame, const WEntity& Entity)
{
	const int64 EntityId = Entity.GetId();

	// A right click on a row that is not selected selects it first: the menu is about what is selected.
	if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && !Frame.Editor.IsSelected(EntityId))
	{
		Pending.ClickedEntityId = EntityId;
	}

	if (ImGui::BeginPopupContextItem())
	{
		if (MenuEntry(Frame, "Create Child Entity"))
		{
			Pending.bCreateEntity = true;
			Pending.NewEntityParentId = EntityId;
		}
		ImGui::Separator();
		if (MenuEntry(Frame, "Rename", "F2"))
		{
			Frame.Requests.EntityToRename = EntityId;
		}
		if (MenuEntry(Frame, "Duplicate", "Ctrl+D"))
		{
			Pending.bDuplicateSelection = true;
		}
		if (MenuEntry(Frame, "Delete", "Del"))
		{
			Pending.bDeleteSelection = true;
		}
		ImGui::EndPopup();
	}
}

void FOutlinerPanel::DrawRenameField(FUiFrame& Frame)
{
	ImGui::SameLine();
	if (bFocusRenameField)
	{
		ImGui::SetKeyboardFocusHere();
		bFocusRenameField = false;
	}
	ImGui::SetNextItemWidth(-1.0f);
	const bool bWasEntered = ImGui::InputText(
		"##Rename", &RenameText, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
	Frame.Items.NameLastItem("Outliner/Rename");

	// Enter gives the entity the name, and so does going elsewhere; Escape leaves it as it was.
	const bool bWasLeft = ImGui::IsItemDeactivated();
	const bool bWasCancelled = bWasLeft && ImGui::IsKeyPressed(ImGuiKey_Escape);
	if (bWasCancelled)
	{
		RenamedEntityId.reset();
	}
	else if (bWasEntered || bWasLeft)
	{
		Pending.bCommitRename = true;
	}
}

void FOutlinerPanel::AcceptDroppedEntities(FUiFrame& Frame, std::optional<int64> ParentId)
{
	if (!ImGui::BeginDragDropTarget())
	{
		return;
	}

	// What is being dragged is looked at before it is accepted, so that a place where it cannot go does not
	// light up as one where it can.
	const ImGuiPayload* const Dragged = ImGui::GetDragDropPayload();
	const bool bIsEntity =
		Dragged != nullptr && Dragged->IsDataType(EntityPayloadType) && Dragged->DataSize == sizeof(int64);
	if (bIsEntity)
	{
		int64 DraggedEntityId = 0;
		std::memcpy(&DraggedEntityId, Dragged->Data, sizeof(DraggedEntityId));
		std::vector<int64> EntityIds = GetDraggedEntities(Frame.Editor, DraggedEntityId);
		const bool bCanDrop = FUiCommands::CanAttachEntities(Frame.Editor, EntityIds, ParentId);
		if (bCanDrop && ImGui::AcceptDragDropPayload(EntityPayloadType) != nullptr)
		{
			Pending.DroppedEntityIds = std::move(EntityIds);
			Pending.DropParentId = ParentId;
		}
	}
	ImGui::EndDragDropTarget();
}

void FOutlinerPanel::DrawEmptySpace(FUiFrame& Frame)
{
	// Whatever room the rows leave, and at least a row's worth, so that there is always a place to drop on.
	const ImVec2 Available = ImGui::GetContentRegionAvail();
	ImGui::Dummy(ImVec2(Available.x, std::max(Available.y, ImGui::GetFrameHeight())));
	Frame.Items.NameLastItem("Outliner/Empty");
	AcceptDroppedEntities(Frame, std::nullopt);

	if (ImGui::BeginPopupContextWindow(
			"##EmptySpaceMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
	{
		if (MenuEntry(Frame, "Create Entity"))
		{
			Pending.bCreateEntity = true;
		}
		ImGui::EndPopup();
	}
}

void FOutlinerPanel::CarryOut(FUiFrame& Frame)
{
	FEditor& Editor = Frame.Editor;

	if (Pending.ClickedEntityId.has_value())
	{
		// With Ctrl held, a click adds an entity to what is selected or takes it out; without, it selects it alone.
		const int64 ClickedEntityId = *Pending.ClickedEntityId;
		std::vector<int64> Selection;
		if (ImGui::GetIO().KeyCtrl)
		{
			Selection = Editor.GetSelection();
			const auto Selected = std::ranges::find(Selection, ClickedEntityId);
			if (Selected == Selection.end())
			{
				Selection.push_back(ClickedEntityId);
			}
			else
			{
				Selection.erase(Selected);
			}
		}
		else
		{
			Selection.push_back(ClickedEntityId);
		}
		FUiCommands::SetSelection(Editor, Selection);
	}
	else
	{
		// A click on the empty part of the list selects nothing.
		const bool bClickedEmptySpace =
			ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
		if (bClickedEmptySpace && !Editor.GetSelection().empty())
		{
			FUiCommands::SetSelection(Editor, {});
		}
	}

	if (Pending.bCommitRename && RenamedEntityId.has_value())
	{
		const WEntity* const Entity = Editor.GetScene().FindEntityById(*RenamedEntityId);
		if (Entity != nullptr && Entity->GetName() != RenameText)
		{
			FUiCommands::RenameEntity(Editor, *RenamedEntityId, RenameText);
		}
		RenamedEntityId.reset();
	}

	if (Pending.bCreateEntity)
	{
		FUiCommands::CreateEntity(Editor, NewEntityName, Pending.NewEntityParentId);
	}
	if (Pending.bDuplicateSelection)
	{
		FUiCommands::DuplicateEntities(Editor, Editor.GetSelection());
	}
	if (Pending.bDeleteSelection)
	{
		FUiCommands::DeleteEntities(Editor, Editor.GetSelection());
	}
	if (!Pending.DroppedEntityIds.empty())
	{
		FUiCommands::AttachEntities(Editor, Pending.DroppedEntityIds, Pending.DropParentId);
	}
}
