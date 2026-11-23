#include "Ui/Dialogs/NewProjectDialog.h"

#include "Commands/CommandResult.h"
#include "Core/FileHelper.h"
#include "Editor.h"
#include "Project/Project.h"
#include "Ui/Dialogs/UiDialog.h"
#include "Ui/ImGuiLibrary.h"
#include "Ui/UiCommands.h"
#include "Ui/UiFrame.h"
#include "Ui/UiItems.h"

#include <filesystem>
#include <vector>

namespace
{
	constexpr const char* Title = "New Project";

	/** The name that is in the field when the window opens, to be typed over. */
	constexpr const char* SuggestedName = "My Game";

	/**
	 * Returns the directory to suggest for a new project's own directory to be made in: where the project that
	 * was opened last has its own, so that a person's projects end up side by side, and for a first project the
	 * place that the editor suggests for all of them.
	 */
	std::filesystem::path SuggestLocation(const FEditor& Editor)
	{
		const std::vector<FProject> RecentProjects = Editor.FindRecentProjects();
		return RecentProjects.empty() ? FProject::GetDefaultLocation()
									  : RecentProjects.front().GetDirectory().parent_path();
	}
}

void FNewProjectDialog::Open(const FEditor& Editor)
{
	bWantsToOpen = true;
	bFocusField = true;
	Error.clear();
	NameText = SuggestedName;
	LocationText = FFileHelper::PathToUtf8(SuggestLocation(Editor));
}

void FNewProjectDialog::Draw(FUiFrame& Frame)
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

	// What is in a field is selected when the field is first clicked into, so that typing replaces it.
	constexpr ImGuiInputTextFlags FieldFlags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll;

	ImGui::TextUnformatted("The name of the project:");
	if (bFocusField)
	{
		ImGui::SetKeyboardFocusHere();
		bFocusField = false;
	}
	ImGui::SetNextItemWidth(FUiDialog::GetFieldWidth());
	const bool bWasNameEntered = ImGui::InputText("##Name", &NameText, FieldFlags);
	Frame.Items.NameLastItem("NewProject/Name");

	ImGui::TextUnformatted("The directory to make the project's own directory in:");
	ImGui::SetNextItemWidth(FUiDialog::GetFieldWidth());
	const bool bWasLocationEntered = ImGui::InputText("##Location", &LocationText, FieldFlags);
	Frame.Items.NameLastItem("NewProject/Location");

	// The project's own directory is called like the project. Where that puts it is said before it is made.
	const bool bIsComplete = !NameText.empty() && !LocationText.empty();
	const std::string Directory =
		FFileHelper::PathToUtf8(FFileHelper::PathFromUtf8(LocationText) / FFileHelper::PathFromUtf8(NameText));
	ImGui::TextDisabled("The project will be in:");
	if (bIsComplete)
	{
		ImGui::TextDisabled("%s", FUiDialog::FitPath(Directory, FUiDialog::GetFieldWidth()).c_str());
	}
	else
	{
		ImGui::TextDisabled("A project needs a name and a directory.");
	}
	FUiDialog::ShowError(Error);

	ImGui::BeginDisabled(!bIsComplete);
	const bool bWasCreateClicked = ImGui::Button("Create");
	Frame.Items.NameLastItem("NewProject/Create");
	ImGui::EndDisabled();
	ImGui::SameLine();
	const bool bWasCancelClicked = ImGui::Button("Cancel");
	Frame.Items.NameLastItem("NewProject/Cancel");

	const bool bWantsToCreate = (bWasCreateClicked || bWasNameEntered || bWasLocationEntered) && bIsComplete;
	if (bWantsToCreate)
	{
		const FCommandResult Result = FUiCommands::NewProject(Frame.Editor, Directory, NameText);
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
