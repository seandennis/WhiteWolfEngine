#include "Ui/Panels/DetailsPanel.h"

#include "Commands/ComponentClasses.h"
#include "Commands/SavedProperties.h"
#include "Editor.h"
#include "Math/Transform.h"
#include "Object/Class.h"
#include "Object/ObjectGlobals.h"
#include "Object/ObjectPtr.h"
#include "Reflection/Property.h"
#include "Scene/Component.h"
#include "Scene/Entity.h"
#include "Scene/MissingComponent.h"
#include "Scene/Scene.h"
#include "Ui/Panels/UiPanel.h"
#include "Ui/UiCommands.h"
#include "Ui/UiFrame.h"
#include "Ui/UiItems.h"
#include "Ui/UiNames.h"
#include "Ui/UiText.h"
#include "Ui/UiTheme.h"
#include "Ui/UiWorkspace.h"

#include <algorithm>
#include <cfloat>
#include <cstdint>
#include <format>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
	/** What Dear ImGui knows the list of component classes by, and the menu of a component. */
	constexpr const char* AddComponentMenuId = "##AddComponent";
	constexpr const char* ComponentMenuId = "##ComponentMenu";

	/** The property of an entity that is its transform, and what the steps that change it are called after. */
	constexpr const char* TransformPropertyName = "RelativeTransform";
	constexpr const char* TransformLabel = "Transform";

	/** How many lines of the list of component classes can be seen before it has to be scrolled. */
	constexpr float ComponentListLineCount = 16.0f;

	/** Makes the width of the next widget all that is left of the line. */
	void FillLine()
	{
		ImGui::SetNextItemWidth(-FLT_MIN);
	}

	/** What the button at the end of a component's heading shows: there is more to do with the component. */
	constexpr const char* MenuButtonLabel = "...";

	/** What the panel shows of a component, given what the user searches for. */
	struct FShownProperties
	{
		/** False if neither the component nor anything it holds is searched for: it is then left out. */
		bool bIsComponentShown = false;

		std::vector<const FProperty*> Properties;

		/** The categories of those properties, each once: first none, then in the order they were declared. */
		std::vector<std::string_view> Categories;
	};

	/**
	 * Returns the properties of a component that are shown: all of them if the component itself is searched for,
	 * and otherwise those whose names or categories are.
	 *
	 * @param Label What the component is called in its heading.
	 */
	FShownProperties FindShownProperties(
		const WComponent& Component, std::string_view Label, std::string_view SearchText)
	{
		FShownProperties Shown;
		const bool bComponentIsSearchedFor = FUiText::ContainsIgnoringCase(Label, SearchText);
		for (const FProperty* const Property : FSavedProperties::GetAll(*Component.GetClass()))
		{
			const std::string_view Category = Property->GetCategory();
			const bool bIsSearchedFor = bComponentIsSearchedFor ||
				FUiText::ContainsIgnoringCase(FUiText::MakePropertyLabel(Property->GetName()), SearchText) ||
				FUiText::ContainsIgnoringCase(Category, SearchText);
			if (!bIsSearchedFor)
			{
				continue;
			}
			Shown.Properties.push_back(Property);
			if (std::ranges::find(Shown.Categories, Category) == Shown.Categories.end())
			{
				Shown.Categories.push_back(Category);
			}
		}
		// Properties that were declared without a category come first, under the heading of the component itself.
		std::ranges::stable_partition(Shown.Categories, [](std::string_view Category) { return Category.empty(); });

		Shown.bIsComponentShown = bComponentIsSearchedFor || !Shown.Properties.empty();
		return Shown;
	}

	/** Returns what a component is called in its heading. */
	std::string MakeComponentLabel(const WComponent& Component)
	{
		if (const WMissingComponent* const Missing = Cast<WMissingComponent>(&Component))
		{
			return std::format("{} (class not found)", FUiText::MakeClassLabel(Missing->GetMissingClassName()));
		}
		return FUiText::MakeClassLabel(Component.GetClass()->GetName());
	}
}

void FDetailsPanel::Draw(FUiFrame& Frame)
{
	if (Frame.Workspace.bShowDetails)
	{
		if (FUiPanel::Begin(Title, Frame.Workspace.bShowDetails))
		{
			Pending = FPending();
			const std::vector<int64> Selection = Frame.Editor.GetSelection();
			const WEntity* const Entity =
				Selection.empty() ? nullptr : Frame.Editor.GetScene().FindEntityById(Selection.back());
			if (Entity != nullptr)
			{
				const int64 EntityId = Entity->GetId();
				DrawEntity(Frame, *Entity, Selection.size());
				CarryOut(Frame, EntityId);
			}
			else
			{
				ImGui::TextDisabled("Select an entity to see its details.");
				Frame.Items.NameLastItem("Details/NothingSelected");
			}
		}
		ImGui::End();
	}

	// Also when the panel is closed or behind another tab: the widget that began a step is then gone.
	EndFinishedChange(Frame);
}

void FDetailsPanel::DrawEntity(FUiFrame& Frame, const WEntity& Entity, std::size_t SelectedCount)
{
	// Everything in the panel belongs to this entity. Another entity's widgets are other widgets, so that what
	// is being typed for one can never end up in another.
	ImGui::PushID(reinterpret_cast<const void*>(static_cast<std::intptr_t>(Entity.GetId())));

	DrawHeading(Frame, Entity, SelectedCount);
	DrawTransform(Frame, Entity);
	for (const TObjectPtr<WComponent>& Component : Entity.GetComponents())
	{
		DrawComponent(Frame, *Component);
	}

	ImGui::PopID();
}

void FDetailsPanel::DrawHeading(FUiFrame& Frame, const WEntity& Entity, std::size_t SelectedCount)
{
	FillLine();
	if (std::optional<std::string> NewName = Properties.EditText("##Name", Entity.GetName(), "Name"))
	{
		Pending.NewName = std::move(NewName);
	}
	Frame.Items.NameLastItem("Details/Name");

	if (SelectedCount > 1)
	{
		// The panel shows one entity, the one that was selected last, and says that there are others.
		const std::size_t OtherCount = SelectedCount - 1;
		ImGui::TextDisabled("%s", std::format("and {} more selected", OtherCount).c_str());
	}

	if (ImGui::Button("Add Component"))
	{
		ImGui::OpenPopup(AddComponentMenuId);
	}
	Frame.Items.NameLastItem("Details/AddComponent");
	DrawAddComponentMenu(Frame);

	ImGui::SameLine();
	FillLine();
	ImGui::InputTextWithHint("##Search", "Search", &SearchText);
	Frame.Items.NameLastItem("Details/Search");
}

void FDetailsPanel::DrawAddComponentMenu(FUiFrame& Frame)
{
	// A list of many classes is scrolled rather than as long as the window.
	const float MaxHeight = ImGui::GetTextLineHeightWithSpacing() * ComponentListLineCount;
	ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(FLT_MAX, MaxHeight));
	if (!ImGui::BeginPopup(AddComponentMenuId))
	{
		return;
	}

	if (ImGui::IsWindowAppearing())
	{
		// The list opens ready for the first letters of what is wanted.
		ComponentSearchText.clear();
		ImGui::SetKeyboardFocusHere();
	}
	ImGui::InputTextWithHint("##Search", "Search", &ComponentSearchText);
	Frame.Items.NameLastItem("Details/AddComponent/Search");

	for (const WClass* const Class : FComponentClasses::GetAddable())
	{
		const std::string& ClassName = Class->GetName();
		const std::string Label = FUiText::MakeClassLabel(ClassName);
		if (!FUiText::ContainsIgnoringCase(Label, ComponentSearchText))
		{
			continue;
		}

		ImGui::PushID(ClassName.c_str());
		if (ImGui::Selectable(Label.c_str()))
		{
			Pending.AddedComponentClass = ClassName;
		}
		Frame.Items.NameLastItem(std::format("Details/AddComponent/{}", ClassName));
		ImGui::PopID();
	}
	ImGui::EndPopup();
}

void FDetailsPanel::DrawTransform(FUiFrame& Frame, const WEntity& Entity)
{
	// Searching for the heading finds all three rows; otherwise the rows are searched for themselves.
	const bool bHeadingIsSearchedFor = FUiText::ContainsIgnoringCase(TransformLabel, SearchText);
	const std::string_view RowSearchText = bHeadingIsSearchedFor ? std::string_view() : std::string_view(SearchText);
	const bool bHasRows = FUiText::ContainsIgnoringCase("Location", RowSearchText) ||
		FUiText::ContainsIgnoringCase("Rotation", RowSearchText) ||
		FUiText::ContainsIgnoringCase("Scale", RowSearchText);
	if (!bHasRows)
	{
		return;
	}

	if (!SearchText.empty())
	{
		// What was searched for is not hidden under a closed heading.
		ImGui::SetNextItemOpen(true);
	}
	FUiTheme::PushHeadingColors();
	const bool bIsOpen = ImGui::CollapsingHeader(TransformLabel, ImGuiTreeNodeFlags_DefaultOpen);
	FUiTheme::PopHeadingColors();
	Frame.Items.NameLastItem("Details/Transform");
	if (!bIsOpen || !FPropertyEditor::BeginRows("##TransformRows"))
	{
		return;
	}

	// A new entity is where its parent is, turned as it is and of its size: that is what the arrows put back.
	const FTransform Identity;
	const int64 EntityId = Entity.GetId();
	const std::string ItemName = std::format("Details/{}/{}", EntityId, TransformPropertyName);
	if (std::optional<FPropertyChange> Change =
			Properties.DrawTransform(Frame, Entity.GetRelativeTransform(), &Identity, ItemName, RowSearchText))
	{
		Pending.Change = std::move(Change);
		Pending.ChangedObjectId = EntityId;
		Pending.ChangedPropertyName = TransformPropertyName;
		Pending.ChangedPropertyLabel = TransformLabel;
	}
	FPropertyEditor::EndRows();
}

void FDetailsPanel::DrawComponent(FUiFrame& Frame, const WComponent& Component)
{
	const int64 ComponentId = Component.GetId();
	const std::string Label = MakeComponentLabel(Component);
	const FShownProperties Shown = FindShownProperties(Component, Label, SearchText);
	if (!Shown.bIsComponentShown)
	{
		return;
	}

	ImGui::PushID(reinterpret_cast<const void*>(static_cast<std::intptr_t>(ComponentId)));
	if (DrawComponentHeading(Frame, Component, Label))
	{
		if (Cast<WMissingComponent>(&Component) != nullptr)
		{
			ImGui::PushTextWrapPos(0.0f);
			ImGui::TextDisabled("The class of this component is not in this build of the editor. What it holds is "
								"kept and saved as it was.");
			ImGui::PopTextWrapPos();
		}

		for (const std::string_view Category : Shown.Categories)
		{
			// The properties of a category are under a heading that can be closed; those without one are not.
			bool bIsCategoryOpen = true;
			if (!Category.empty())
			{
				if (!SearchText.empty())
				{
					ImGui::SetNextItemOpen(true);
				}
				const std::string CategoryLabel(Category);
				bIsCategoryOpen = ImGui::TreeNodeEx(
					CategoryLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth);
				Frame.Items.NameLastItem(std::format("Details/{}/Category/{}", ComponentId, Category));
			}
			if (!bIsCategoryOpen)
			{
				continue;
			}

			if (FPropertyEditor::BeginRows("##Rows"))
			{
				for (const FProperty* const Property : Shown.Properties)
				{
					if (Property->GetCategory() == Category)
					{
						DrawProperty(Frame, Component, ComponentId, *Property);
					}
				}
				FPropertyEditor::EndRows();
			}
			if (!Category.empty())
			{
				ImGui::TreePop();
			}
		}
	}
	ImGui::PopID();
}

bool FDetailsPanel::DrawComponentHeading(FUiFrame& Frame, const WComponent& Component, const std::string& Label)
{
	const int64 ComponentId = Component.GetId();
	if (!SearchText.empty())
	{
		// What was searched for is not hidden under a closed heading.
		ImGui::SetNextItemOpen(true);
	}

	// The heading lets what lies on it be clicked: the button at its end.
	const float HeadingRight = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
	FUiTheme::PushHeadingColors();
	// What the heading says is the name of a class, which for a component whose class is missing is whatever the
	// scene file calls it: it is written by FUiNames, and is not the heading's own text.
	const bool bIsOpen = FUiNames::Heading(Label);
	FUiTheme::PopHeadingColors();
	const float HeadingTop = ImGui::GetItemRectMin().y;
	Frame.Items.NameLastItem(std::format("Details/{}", ComponentId));

	// The menu of the component opens with the right mouse button on the heading, and with the button at its end.
	if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
	{
		ImGui::OpenPopup(ComponentMenuId);
	}
	const float MenuButtonWidth = ImGui::CalcTextSize(MenuButtonLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f;
	ImGui::SameLine();
	ImGui::SetCursorScreenPos(ImVec2(HeadingRight - MenuButtonWidth, HeadingTop));
	if (ImGui::Button(MenuButtonLabel))
	{
		ImGui::OpenPopup(ComponentMenuId);
	}
	Frame.Items.NameLastItem(std::format("Details/{}/Menu", ComponentId));

	if (ImGui::BeginPopup(ComponentMenuId))
	{
		if (ImGui::MenuItem("Remove Component"))
		{
			Pending.RemovedComponentId = ComponentId;
		}
		Frame.Items.NameLastItem("Details/Menu/Remove Component");
		ImGui::EndPopup();
	}
	return bIsOpen;
}
void FDetailsPanel::DrawProperty(FUiFrame& Frame, const WObject& Object, int64 ObjectId, const FProperty& Property)
{
	const std::string Label = FUiText::MakePropertyLabel(Property.GetName());
	const std::string ItemName = std::format("Details/{}/{}", ObjectId, Property.GetName());
	const FJsonValue* const Default = Defaults.Find(*Object.GetClass(), Property, Frame.Editor.GetScene());

	if (std::optional<FPropertyChange> Change =
			Properties.DrawProperty(Frame, Property, Property.GetValuePtr(&Object), Label, ItemName, Default))
	{
		Pending.Change = std::move(Change);
		Pending.ChangedObjectId = ObjectId;
		Pending.ChangedPropertyName = Property.GetName();
		Pending.ChangedPropertyLabel = Label;
	}
}

void FDetailsPanel::CarryOut(FUiFrame& Frame, int64 EntityId)
{
	FEditor& Editor = Frame.Editor;

	if (Pending.Change.has_value())
	{
		ApplyChange(Frame);
	}
	if (Pending.NewName.has_value())
	{
		FUiCommands::RenameEntity(Editor, EntityId, *Pending.NewName);
	}
	if (Pending.AddedComponentClass.has_value())
	{
		FUiCommands::AddComponent(Editor, EntityId, *Pending.AddedComponentClass);
	}
	if (Pending.RemovedComponentId.has_value())
	{
		FUiCommands::RemoveComponent(Editor, *Pending.RemovedComponentId);
	}
}

void FDetailsPanel::ApplyChange(FUiFrame& Frame)
{
	FEditor& Editor = Frame.Editor;
	const FPropertyChange& Change = *Pending.Change;

	// What the widget that the user still holds changes goes into the step that the widget began. Anything else
	// begins a step: another widget, or the same one after an undo has ended its step.
	const bool bContinuesChange =
		bIsChanging && Editor.IsStepOpen() && Change.HeldItemId != 0 && Change.HeldItemId == ChangingItemId;
	if (!bContinuesChange)
	{
		FUiCommands::BeginChange(
			Editor, std::format("{} {}", Change.bIsReset ? "Reset" : "Set", Pending.ChangedPropertyLabel));
		bIsChanging = true;
		ChangingItemId = Change.HeldItemId;
	}
	FUiCommands::SetProperty(Editor, Pending.ChangedObjectId, Pending.ChangedPropertyName, Change.Value);
}

void FDetailsPanel::EndFinishedChange(FUiFrame& Frame)
{
	if (!bIsChanging)
	{
		return;
	}

	// Held means that the user has not let go of the widget and that it was laid out in this frame. The library
	// goes on calling a widget active for a frame after it has gone, which is a frame too long for a step to
	// stay open: whatever was done in that frame would become part of it.
	const ImGuiContext& Context = *ImGui::GetCurrentContext();
	const bool bIsStillHeld =
		ChangingItemId != 0 && Context.ActiveId == ChangingItemId && Context.ActiveIdIsAlive == ChangingItemId;
	if (!bIsStillHeld)
	{
		FUiCommands::EndChange(Frame.Editor);
		bIsChanging = false;
		ChangingItemId = 0;
	}
}
