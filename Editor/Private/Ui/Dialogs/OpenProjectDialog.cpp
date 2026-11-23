#include "Ui/Dialogs/OpenProjectDialog.h"

#include "Commands/CommandResult.h"
#include "Core/FileHelper.h"
#include "Editor.h"
#include "Ui/Dialogs/UiDialog.h"
#include "Ui/ImGuiLibrary.h"
#include "Ui/UiCommands.h"
#include "Ui/UiFrame.h"
#include "Ui/UiItems.h"
#include "Ui/UiRequests.h"

#include <cstddef>
#include <format>

namespace
{
	constexpr const char* Title = "Open Project";

	/** How many projects of the list can be seen before it has to be scrolled. */
	constexpr float VisibleProjectCount = 4.0f;

	/** How many lines a project of the list takes: its name, and below it where it is. */
	constexpr float LinesPerProject = 2.0f;
}

void FOpenProjectDialog::Open(const FEditor& Editor)
{
	bWantsToOpen = true;
	bFocusField = true;
	Error.clear();
	PathText.clear();
	RecentProjects = Editor.FindRecentProjects();
}

void FOpenProjectDialog::Draw(FUiFrame& Frame)
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

	const ImGuiStyle& Style = ImGui::GetStyle();
	const float RowHeight = ImGui::GetTextLineHeight() * LinesPerProject + Style.ItemSpacing.y;
	const ImVec2 ListSize(FUiDialog::GetFieldWidth(),
		(RowHeight + Style.ItemSpacing.y) * VisibleProjectCount + Style.WindowPadding.y * 2.0f);

	bool bWasProjectDoubleClicked = false;
	ImGui::TextUnformatted("The projects that were opened last:");
	if (ImGui::BeginChild("##Projects", ListSize, ImGuiChildFlags_Borders))
	{
		if (RecentProjects.empty())
		{
			ImGui::TextDisabled("No project has been opened yet.");
		}
		for (std::size_t Index = 0; Index < RecentProjects.size(); ++Index)
		{
			// A click chooses a project, whose file is then in the field; a double click opens it. Dear ImGui
			// knows the row by its place in the list, which stays what it is while the window is open: a name
			// may be that of two projects, and in a path there may be the signs that mean something to it.
			const FProject& Project = RecentProjects[Index];
			const std::string File = FFileHelper::PathToUtf8(Project.GetFile());
			const ImVec2 RowCorner = ImGui::GetCursorScreenPos();
			const float RowWidth = ImGui::GetContentRegionAvail().x;
			constexpr ImGuiSelectableFlags Flags =
				ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_NoAutoClosePopups;
			ImGui::PushID(static_cast<int>(Index));
			if (ImGui::Selectable("##Project", File == PathText, Flags, ImVec2(0.0f, RowHeight)))
			{
				PathText = File;
				bWasProjectDoubleClicked = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
			}
			ImGui::PopID();
			Frame.Items.NameLastItem(std::format("OpenProject/{}", File));

			// What the row says is drawn over it: the name, which is what a person knows a project by, and below
			// it the directory, which tells two projects of one name apart.
			const std::string Directory = FUiDialog::FitPath(FFileHelper::PathToUtf8(Project.GetDirectory()), RowWidth);
			const ImVec2 DirectoryCorner(RowCorner.x, RowCorner.y + ImGui::GetTextLineHeightWithSpacing());
			ImDrawList* const DrawList = ImGui::GetWindowDrawList();
			DrawList->AddText(RowCorner, ImGui::GetColorU32(ImGuiCol_Text), Project.GetName().c_str());
			DrawList->AddText(DirectoryCorner, ImGui::GetColorU32(ImGuiCol_TextDisabled), Directory.c_str());
		}
	}
	ImGui::EndChild();

	ImGui::TextUnformatted("The project file, or the directory of the project, to open:");
	if (bFocusField)
	{
		ImGui::SetKeyboardFocusHere();
		bFocusField = false;
	}
	ImGui::SetNextItemWidth(FUiDialog::GetFieldWidth());
	const bool bWasEntered = ImGui::InputText("##Path", &PathText, ImGuiInputTextFlags_EnterReturnsTrue);
	Frame.Items.NameLastItem("OpenProject/Path");
	FUiDialog::ShowError(Error);

	ImGui::BeginDisabled(PathText.empty());
	const bool bWasOpenClicked = ImGui::Button("Open");
	Frame.Items.NameLastItem("OpenProject/Open");
	ImGui::EndDisabled();
	ImGui::SameLine();
	const bool bWasNewClicked = ImGui::Button("New Project...");
	Frame.Items.NameLastItem("OpenProject/New");
	ImGui::SameLine();
	const bool bWasCancelClicked = ImGui::Button("Cancel");
	Frame.Items.NameLastItem("OpenProject/Cancel");

	const bool bWantsToOpenProject = (bWasOpenClicked || bWasEntered || bWasProjectDoubleClicked) && !PathText.empty();
	if (bWantsToOpenProject)
	{
		const FCommandResult Result = FUiCommands::OpenProject(Frame.Editor, PathText);
		if (Result.HasSucceeded())
		{
			ImGui::CloseCurrentPopup();
		}
		else
		{
			Error = Result.GetError();
		}
	}
	else if (bWasNewClicked)
	{
		// A project is made in a window of its own, which opens in place of this one.
		Frame.Requests.bNewProject = true;
		ImGui::CloseCurrentPopup();
	}
	else if (bWasCancelClicked || ImGui::IsKeyPressed(ImGuiKey_Escape))
	{
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();
}
