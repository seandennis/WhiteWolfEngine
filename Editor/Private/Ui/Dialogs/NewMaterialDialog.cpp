#include "Ui/Dialogs/NewMaterialDialog.h"

#include "Assets/AssetManager.h"
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

	/**
	 * Returns the suggested name of a material file in a folder, with a number from 2 on to tell it from the ones
	 * before.
	 */
	std::string MakeSuggestion(std::string_view Folder, int32 Number)
	{
		const std::string Suffix = Number > 1 ? std::to_string(Number) : std::string();
		return std::format("{}/{}{}{}", Folder, SuggestedName, Suffix, FMaterialSerializer::FileExtension);
	}

	/**
	 * Returns the suggested folder as the content directory spells it: where the file system does not tell capitals
	 * apart, a folder of the same name in other capitals is that folder, and a material is loaded only under the name
	 * that its directory gives it.
	 */
	std::string FindSuggestedFolder(const FEditor& Editor)
	{
		const std::filesystem::path Suggested = Editor.ResolveFilePath(FFileHelper::PathFromUtf8(SuggestedFolder));
		std::error_code ErrorCode;
		for (std::filesystem::directory_iterator Entry(Suggested.parent_path(), ErrorCode);
			 !ErrorCode && Entry != std::filesystem::directory_iterator();
			 Entry.increment(ErrorCode))
		{
			std::error_code EntryError;
			if (std::filesystem::equivalent(Entry->path(), Suggested, EntryError))
			{
				return FFileHelper::PathToUtf8(Entry->path().filename());
			}
		}
		return std::string(SuggestedFolder);
	}

	/**
	 * Returns true if a material can be made under a name: no file has it, and no material that is loaded has it or
	 * the same name in other capitals, as one that is in use although its file is gone.
	 */
	bool IsFree(const FEditor& Editor, const std::string& File)
	{
		std::error_code ErrorCode;
		const bool bHasFile =
			std::filesystem::exists(Editor.ResolveFilePath(FFileHelper::PathFromUtf8(File)), ErrorCode);
		return !bHasFile && FAssetManager::FindLoadedIgnoringCase(File).empty();
	}
}

void FNewMaterialDialog::Open(const FEditor& Editor)
{
	bWantsToOpen = true;
	bFocusField = true;
	Error.clear();

	// The first name that is free in the project. Without a project the command will say what is missing.
	if (Editor.GetProject() == nullptr)
	{
		PathText = MakeSuggestion(SuggestedFolder, 1);
		return;
	}
	const std::string Folder = FindSuggestedFolder(Editor);
	PathText = MakeSuggestion(Folder, 1);
	for (int32 Number = 2; !IsFree(Editor, PathText); ++Number)
	{
		PathText = MakeSuggestion(Folder, Number);
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
	else if (bWasCancelClicked || FUiDialog::IsAnswerKeyPressed(ImGuiKey_Escape))
	{
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();
}
