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
#include "Ui/UiWorkspace.h"

#include <algorithm>
#include <cstdint>
#include <format>
#include <string_view>
#include <vector>

namespace
{
	char ToLowerAscii(char Character)
	{
		return Character >= 'A' && Character <= 'Z' ? static_cast<char>(Character - 'A' + 'a') : Character;
	}

	/** Returns true if Text contains Part, taking the letters A to Z as the same in capitals and small. */
	bool ContainsIgnoringCase(std::string_view Text, std::string_view Part)
	{
		const auto Found = std::search(Text.begin(),
			Text.end(),
			Part.begin(),
			Part.end(),
			[](char Left, char Right) { return ToLowerAscii(Left) == ToLowerAscii(Right); });
		return Found != Text.end() || Part.empty();
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
		ImGui::InputTextWithHint("##Search", "Search", SearchText.data(), SearchText.size());
		Frame.Items.NameLastItem("Outliner/Search");

		ClickedEntityId.reset();
		const WScene& Scene = Frame.Editor.GetScene();
		const std::string_view Search(SearchText.data());
		if (Search.empty())
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
				if (ContainsIgnoringCase(Entity->GetName(), Search))
				{
					DrawRow(Frame, *Entity, false);
				}
			}
		}

		if (ClickedEntityId.has_value())
		{
			// With Ctrl held, a click adds an entity to what is selected or takes it out; without, it selects it alone.
			std::vector<int64> Selection;
			if (ImGui::GetIO().KeyCtrl)
			{
				Selection = Frame.Editor.GetSelection();
				const auto Selected = std::ranges::find(Selection, *ClickedEntityId);
				if (Selected == Selection.end())
				{
					Selection.push_back(*ClickedEntityId);
				}
				else
				{
					Selection.erase(Selected);
				}
			}
			else
			{
				Selection.push_back(*ClickedEntityId);
			}
			FUiCommands::SetSelection(Frame.Editor, Selection);
		}
		else
		{
			// A click on the empty part of the list selects nothing.
			const bool bClickedEmptySpace =
				ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
			if (bClickedEmptySpace && !Frame.Editor.GetSelection().empty())
			{
				FUiCommands::SetSelection(Frame.Editor, {});
			}
		}
	}
	ImGui::End();
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
	ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen |
		ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
	if (!bHasChildRows)
	{
		// A row with nothing below it has no arrow, and nothing has to be closed after it.
		Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}
	if (Frame.Editor.IsSelected(Entity.GetId()))
	{
		Flags |= ImGuiTreeNodeFlags_Selected;
	}

	// Dear ImGui tells rows apart by the identifier, so two entities may have the same name.
	const void* const RowId = reinterpret_cast<const void*>(static_cast<std::intptr_t>(Entity.GetId()));
	const bool bIsOpen = ImGui::TreeNodeEx(RowId, Flags, "%s", Entity.GetName().c_str());
	Frame.Items.NameLastItem(std::format("Outliner/{}", Entity.GetId()));

	// A click on the arrow opens or closes the row and selects nothing.
	if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
	{
		ClickedEntityId = Entity.GetId();
	}
	return bIsOpen && bHasChildRows;
}
