#include "Ui/Dialogs/OpenSceneDialog.h"

#include "Commands/CommandResult.h"
#include "Editor.h"
#include "Ui/Dialogs/UiDialog.h"
#include "Ui/ImGuiLibrary.h"
#include "Ui/UiCommands.h"
#include "Ui/UiFrame.h"
#include "Ui/UiItems.h"

#include <format>

namespace
{
	constexpr const char* Title = "Open Scene";

	/** How many scenes of the list can be seen before it has to be scrolled. */
	constexpr float ListLineCount = 10.0f;
}

void FOpenSceneDialog::Open(FEditor& Editor)
{
	bWantsToOpen = true;
	bFocusField = true;
	Error.clear();
	PathText.clear();
	Scenes = FUiCommands::ListScenes(Editor);
}

void FOpenSceneDialog::Draw(FUiFrame& Frame)
{
	if (bWantsToOpen)
	{
		ImGui::OpenPopup(Title);
		bWantsToOpen = false;
	}
	if (!FUiDialog::Begin(Title))
	{
		return;
	}

	FEditor& Editor = Frame.Editor;
	bool bWasSceneDoubleClicked = false;
	if (Editor.GetProject() != nullptr)
	{
		ImGui::TextUnformatted("The scenes of the project:");
		const ImVec2 ListSize(FUiDialog::GetFieldWidth(), ImGui::GetTextLineHeightWithSpacing() * ListLineCount);
		if (ImGui::BeginChild("##Scenes", ListSize, ImGuiChildFlags_Borders))
		{
			if (Scenes.empty())
			{
				ImGui::TextDisabled("The project has no scenes yet.");
			}
			for (const std::string& Scene : Scenes)
			{
				// A click chooses a scene, which is then in the field; a double click opens it.
				constexpr ImGuiSelectableFlags Flags =
					ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_NoAutoClosePopups;
				if (ImGui::Selectable(Scene.c_str(), Scene == PathText, Flags))
				{
					PathText = Scene;
					bWasSceneDoubleClicked = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
				}
				Frame.Items.NameLastItem(std::format("OpenScene/{}", Scene));
			}
		}
		ImGui::EndChild();
		ImGui::TextUnformatted("The scene file to open:");
	}
	else
	{
		ImGui::TextUnformatted("No project is open. The scene file to open, with its whole path:");
	}

	if (bFocusField)
	{
		ImGui::SetKeyboardFocusHere();
		bFocusField = false;
	}
	ImGui::SetNextItemWidth(FUiDialog::GetFieldWidth());
	const bool bWasEntered = ImGui::InputText("##Path", &PathText, ImGuiInputTextFlags_EnterReturnsTrue);
	Frame.Items.NameLastItem("OpenScene/Path");
	FUiDialog::ShowError(Error);

	ImGui::BeginDisabled(PathText.empty());
	const bool bWasOpenClicked = ImGui::Button("Open");
	Frame.Items.NameLastItem("OpenScene/Open");
	ImGui::EndDisabled();
	ImGui::SameLine();
	const bool bWasCancelClicked = ImGui::Button("Cancel");
	Frame.Items.NameLastItem("OpenScene/Cancel");

	const bool bWantsToOpenScene = (bWasOpenClicked || bWasEntered || bWasSceneDoubleClicked) && !PathText.empty();
	if (bWantsToOpenScene)
	{
		const FCommandResult Result = FUiCommands::OpenScene(Editor, PathText);
		if (Result.HasSucceeded())
		{
			ImGui::CloseCurrentPopup();
		}
		else
		{
			Error = Result.GetError();
		}
	}
	else if (bWasCancelClicked || ImGui::IsKeyPressed(ImGuiKey_Escape))
	{
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();
}
