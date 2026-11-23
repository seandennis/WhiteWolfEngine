#include "Ui/Dialogs/SaveSceneDialog.h"

#include "Commands/CommandResult.h"
#include "Core/FileHelper.h"
#include "Editor.h"
#include "Scene/Scene.h"
#include "Scene/SceneSerializer.h"
#include "Ui/Dialogs/UiDialog.h"
#include "Ui/ImGuiLibrary.h"
#include "Ui/UiCommands.h"
#include "Ui/UiFrame.h"
#include "Ui/UiItems.h"

#include <filesystem>
#include <string_view>
#include <system_error>
#include <utility>

namespace
{
	constexpr const char* Title = "Save Scene As";

	/** Returns the name of a file with the ending of scene files, which it is given if it does not have it. */
	std::string WithSceneEnding(const std::string& FileName)
	{
		const std::string_view Ending = FSceneSerializer::FileExtension;
		return FileName.ends_with(Ending) ? FileName : FileName + std::string(Ending);
	}
}

void FSaveSceneDialog::Open(const FEditor& Editor, std::string InError)
{
	bWantsToOpen = true;
	bFocusField = true;
	Error = std::move(InError);

	// The file the scene has, as the user reads it, or for a scene without one a file named like the scene.
	const std::filesystem::path& ScenePath = Editor.GetScenePath();
	PathText = ScenePath.empty() ? WithSceneEnding(Editor.GetScene().GetName()) : Editor.MakeProjectPath(ScenePath);
}

void FSaveSceneDialog::Draw(FUiFrame& Frame)
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
	const bool bHasProject = Editor.GetProject() != nullptr;
	ImGui::TextUnformatted(bHasProject ? "The file in the project's content to save the scene to:"
									   : "No project is open. The file to save the scene to, with its whole path:");

	if (bFocusField)
	{
		ImGui::SetKeyboardFocusHere();
		bFocusField = false;
	}
	ImGui::SetNextItemWidth(FUiDialog::GetFieldWidth());
	const bool bWasEntered =
		ImGui::InputText("##Path", &PathText, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
	Frame.Items.NameLastItem("SaveScene/Path");

	// Saving over another file is what was asked for, and is said before it is done.
	const std::string FileName = WithSceneEnding(PathText);
	const std::filesystem::path File = Editor.ResolveFilePath(FFileHelper::PathFromUtf8(FileName));
	std::error_code ErrorCode;
	const bool bReplacesFile =
		!PathText.empty() && File != Editor.GetScenePath() && std::filesystem::exists(File, ErrorCode);
	if (bReplacesFile)
	{
		ImGui::TextDisabled("A file of this name is there already, and will be replaced.");
	}
	FUiDialog::ShowError(Error);

	ImGui::BeginDisabled(PathText.empty());
	const bool bWasSaveClicked = ImGui::Button("Save");
	Frame.Items.NameLastItem("SaveScene/Save");
	ImGui::EndDisabled();
	ImGui::SameLine();
	const bool bWasCancelClicked = ImGui::Button("Cancel");
	Frame.Items.NameLastItem("SaveScene/Cancel");

	const bool bWantsToSave = (bWasSaveClicked || bWasEntered) && !PathText.empty();
	if (bWantsToSave)
	{
		const FCommandResult Result = FUiCommands::SaveSceneAs(Editor, FileName);
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
