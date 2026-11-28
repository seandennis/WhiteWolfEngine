#include "Ui/MainToolbar.h"

#include "Assets/AssetManager.h"
#include "Ui/ImGuiLibrary.h"
#include "Ui/UiCommands.h"
#include "Ui/UiFrame.h"
#include "Ui/UiItems.h"
#include "Ui/UiNewEntity.h"
#include "Ui/UiRequests.h"

#include <array>
#include <format>
#include <string>
#include <string_view>

namespace
{
	/** What Dear ImGui knows the list of things to add by. */
	constexpr const char* AddMenuId = "##AddMenu";

	/** One line of the Add menu: what it says, and the name, the component and the mesh of the entity it makes. */
	struct FAddEntry
	{
		const char* Label;
		std::string_view EntityName;
		std::string_view ComponentClassName;
		std::string_view MeshPath;

		/** How far the new entity is tilted and turned, in degrees. */
		float Pitch = 0.0f;
		float Yaw = 0.0f;
	};

	/** An entity with nothing on it is called what the outliner calls one. */
	constexpr FAddEntry EmptyEntity{"Empty Entity", "Entity", std::string_view(), std::string_view()};

	constexpr std::array<FAddEntry, 3> Shapes{{
		{"Cube", "Cube", "WStaticMeshComponent", FAssetManager::CubeShape},
		{"Sphere", "Sphere", "WStaticMeshComponent", FAssetManager::SphereShape},
		{"Plane", "Plane", "WStaticMeshComponent", FAssetManager::PlaneShape},
	}};

	// A sun that has just been added shines down at a slant, from over the left shoulder of someone who looks
	// along -Z, so that what it lights has a bright side, a dim side and a shadow to be seen.
	constexpr std::array<FAddEntry, 3> Lights{{
		{"Directional Light", "Directional Light", "WDirectionalLightComponent", std::string_view(), -50.0f, -30.0f},
		{"Point Light", "Point Light", "WPointLightComponent", std::string_view()},
		{"Sky Light", "Sky Light", "WSkyLightComponent", std::string_view()},
	}};

	/** Lays out a line of the Add menu, and makes the entity if the line is chosen. */
	void AddMenuEntry(FUiFrame& Frame, const FAddEntry& Entry)
	{
		const bool bWasChosen = ImGui::MenuItem(Entry.Label);
		Frame.Items.NameLastItem(std::format("Toolbar/Add/{}", Entry.Label));
		if (!bWasChosen)
		{
			return;
		}

		FUiNewEntity NewEntity;
		NewEntity.Name = Entry.EntityName;
		NewEntity.ComponentClassName = Entry.ComponentClassName;
		NewEntity.MeshPath = Entry.MeshPath;
		NewEntity.Rotation.Pitch = Entry.Pitch;
		NewEntity.Rotation.Yaw = Entry.Yaw;
		FUiCommands::AddEntity(Frame.Editor, NewEntity);
	}
}

void FMainToolbar::Draw(FUiFrame& Frame)
{
	// As high as a row of buttons with the room of a window around it, and as wide as the main window.
	const float Height = ImGui::GetFrameHeight() + ImGui::GetStyle().WindowPadding.y * 2.0f;
	constexpr ImGuiWindowFlags Flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;
	// The bar shows no title. The name is what the list of windows calls it that Ctrl+Tab goes through.
	if (ImGui::BeginViewportSideBar("Toolbar###MainToolbar", ImGui::GetMainViewport(), ImGuiDir_Up, Height, Flags))
	{
		if (ImGui::Button("Save"))
		{
			Frame.Requests.bSave = true;
		}
		ImGui::SetItemTooltip("Save the scene and the materials that were changed (Ctrl+S)");
		Frame.Items.NameLastItem("Toolbar/Save");

		ImGui::SameLine();
		if (ImGui::Button("Add"))
		{
			ImGui::OpenPopup(AddMenuId);
		}
		Frame.Items.NameLastItem("Toolbar/Add");
		DrawAddMenu(Frame);
	}
	ImGui::End();
}

void FMainToolbar::DrawAddMenu(FUiFrame& Frame)
{
	if (!ImGui::BeginPopup(AddMenuId))
	{
		return;
	}

	AddMenuEntry(Frame, EmptyEntity);

	ImGui::SeparatorText("Shapes");
	for (const FAddEntry& Shape : Shapes)
	{
		AddMenuEntry(Frame, Shape);
	}

	ImGui::SeparatorText("Lights");
	for (const FAddEntry& Light : Lights)
	{
		AddMenuEntry(Frame, Light);
	}
	ImGui::EndPopup();
}
