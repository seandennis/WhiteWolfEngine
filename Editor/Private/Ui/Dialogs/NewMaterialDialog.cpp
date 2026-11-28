#include "Ui/Dialogs/NewMaterialDialog.h"

#include "Commands/CommandResult.h"
#include "Core/FileHelper.h"
#include "Editor.h"
#include "Rendering/MaterialSerializer.h"
#include "Ui/Dialogs/UiDialog.h"
#include "Ui/ImGuiLibrary.h"
#include "Ui/UiCommands.h"
#include "Ui/UiFrame.h"
#include "Ui/UiItems.h"

#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <system_error>

namespace
{
	constexpr const char* Title = "New Material";

	/** Where a new material is suggested to go, and what it is called if that name is free. */
	constexpr std::string_view SuggestedFolder = "Materials";
	constexpr std::string_view SuggestedName = "NewMaterial";

	/** Returns the name of a file with the ending of material files, which it is given if it does not have it. */
	std::string WithMaterialEnding(const std::string& FileName)
	{
		return FMaterialSerializer::IsMaterialFile(FFileHelper::PathFromUtf8(FileName))
			? FileName
			: FileName + std::string(FMaterialSerializer::FileExtension);
	}

	/** Returns the suggested name of a material file, with a number from 2 on to tell it from the ones before. */
	std::string MakeSuggestion(int32 Number)
	{
		const std::string Suffix = Number > 1 ? std::to_string(Number) : std::string();
		return std::format("{}/{}{}{}", SuggestedFolder, SuggestedName, Suffix, FMaterialSerializer::FileExtension);
	}
}

void FNewMaterialDialog::Open(const FEditor& Editor)
{
	bWantsToOpen = true;
	bFocusField = true;
	Error.clear();

	// The first name that no file of the project has. Without a project the command will say what is missing.
	PathText = MakeSuggestion(1);
	if (Editor.GetProject() != nullptr)
	{
		std::error_code ErrorCode;
		for (int32 Number = 2;
			 std::filesystem::exists(Editor.ResolveFilePath(FFileHelper::PathFromUtf8(PathText)), ErrorCode);
			 ++Number)
		{
			PathText = MakeSuggestion(Number);
		}
	}
}

void FNewMaterialDialog::Draw(FUiFrame& Frame)
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

	ImGui::TextUnformatted("The file in the project's content to keep the material in:");
	if (bFocusField)
	{
		ImGui::SetKeyboardFocusHere();
		bFocusField = false;
	}
	ImGui::SetNextItemWidth(FUiDialog::GetFieldWidth());
	const bool bWasEntered =
		ImGui::InputText("##Path", &PathText, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
	Frame.Items.NameLastItem("NewMaterial/Path");
	FUiDialog::ShowError(Error);

	ImGui::BeginDisabled(PathText.empty());
	const bool bWasCreateClicked = ImGui::Button("Create");
	Frame.Items.NameLastItem("NewMaterial/Create");
	ImGui::EndDisabled();
	ImGui::SameLine();
	const bool bWasCancelClicked = ImGui::Button("Cancel");
	Frame.Items.NameLastItem("NewMaterial/Cancel");

	const bool bWantsToCreate = (bWasCreateClicked || bWasEntered) && !PathText.empty();
	if (bWantsToCreate)
	{
		const FCommandResult Result = FUiCommands::CreateMaterial(Frame.Editor, WithMaterialEnding(PathText));
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
