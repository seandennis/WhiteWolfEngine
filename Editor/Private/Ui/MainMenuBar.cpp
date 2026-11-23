#include "Ui/MainMenuBar.h"

#include "Commands/CommandResult.h"
#include "Core/EngineVersion.h"
#include "Editor.h"
#include "Ui/ImGuiLibrary.h"
#include "Ui/UiCommands.h"
#include "Ui/UiFrame.h"
#include "Ui/UiItems.h"
#include "Ui/UiRequests.h"
#include "Ui/UiWorkspace.h"

#include <format>
#include <string>
#include <vector>

namespace
{
	constexpr const char* AboutWindowTitle = "About White Wolf Engine";

	/**
	 * Opens a menu of the menu bar, and gives its title the name "Menu/<Label>".
	 *
	 * @return True if the menu is open: its entries follow, and then ImGui::EndMenu.
	 */
	bool BeginNamedMenu(FUiFrame& Frame, const char* Label)
	{
		// The title is as wide as its text and the spacing around it, and as high as the bar.
		const ImVec2 TitleCorner = ImGui::GetCursorScreenPos();
		FUiRectangle Title;
		Title.Left = TitleCorner.x;
		Title.Top = TitleCorner.y;
		Title.Right = TitleCorner.x + ImGui::CalcTextSize(Label).x + ImGui::GetStyle().ItemSpacing.x;
		Title.Bottom = TitleCorner.y + ImGui::GetFrameHeight();
		Frame.Items.Add(std::format("Menu/{}", Label), Title);

		return ImGui::BeginMenu(Label);
	}

	/**
	 * Lays out an entry of the menu that is open, and gives it the name "Menu/<Menu>/<Label>".
	 *
	 * @param Shortcut The keys that do the same, as the entry shows them; null if there are none.
	 * @return True if the entry was chosen.
	 */
	bool MenuEntry(FUiFrame& Frame, const char* Menu, const char* Label, const char* Shortcut, bool bIsEnabled = true)
	{
		const bool bWasChosen = ImGui::MenuItem(Label, Shortcut, false, bIsEnabled);
		Frame.Items.NameLastItem(std::format("Menu/{}/{}", Menu, Label));
		return bWasChosen;
	}

	/** Like MenuEntry, for an entry with a tick that says whether something is on; choosing it flips that. */
	void MenuToggle(FUiFrame& Frame, const char* Menu, const char* Label, bool& bIsOn)
	{
		ImGui::MenuItem(Label, nullptr, &bIsOn);
		Frame.Items.NameLastItem(std::format("Menu/{}/{}", Menu, Label));
	}

	/** Returns true if the keys of a shortcut were pressed and nothing that takes the keyboard for itself has it. */
	bool IsShortcutPressed(ImGuiKeyChord Keys)
	{
		return ImGui::Shortcut(Keys, ImGuiInputFlags_RouteGlobal);
	}

	/** Asks the outliner to let the user type a new name for the entity that was selected last. */
	void RenameSelection(FUiFrame& Frame)
	{
		const std::vector<int64> Selection = Frame.Editor.GetSelection();
		if (!Selection.empty())
		{
			Frame.Requests.EntityToRename = Selection.back();
		}
	}
}

void FMainMenuBar::Draw(FUiFrame& Frame)
{
	if (ImGui::BeginMainMenuBar())
	{
		// While a window is open that has to be answered first, the shortcuts must not change the scene behind it.
		const bool bIsBlocked = ImGui::GetTopMostPopupModal() != nullptr;
		if (!bIsBlocked)
		{
			if (IsShortcutPressed(ImGuiMod_Ctrl | ImGuiKey_N))
			{
				FUiCommands::NewScene(Frame.Editor);
			}
			if (IsShortcutPressed(ImGuiMod_Ctrl | ImGuiKey_O))
			{
				OpenSceneDialog.Open(Frame.Editor);
			}
			if (IsShortcutPressed(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S))
			{
				SaveSceneDialog.Open(Frame.Editor);
			}
			// What another part of the user interface asked for, such as the toolbar's button, is done here too.
			if (IsShortcutPressed(ImGuiMod_Ctrl | ImGuiKey_S) || Frame.Requests.bSaveScene)
			{
				SaveScene(Frame);
			}
			if (IsShortcutPressed(ImGuiMod_Ctrl | ImGuiKey_Z))
			{
				FUiCommands::Undo(Frame.Editor);
			}
			if (IsShortcutPressed(ImGuiMod_Ctrl | ImGuiKey_Y) ||
				IsShortcutPressed(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z))
			{
				FUiCommands::Redo(Frame.Editor);
			}
			if (IsShortcutPressed(ImGuiMod_Ctrl | ImGuiKey_D))
			{
				FUiCommands::DuplicateEntities(Frame.Editor, Frame.Editor.GetSelection());
			}
			if (IsShortcutPressed(ImGuiKey_Delete))
			{
				FUiCommands::DeleteEntities(Frame.Editor, Frame.Editor.GetSelection());
			}
			if (IsShortcutPressed(ImGuiKey_F2))
			{
				RenameSelection(Frame);
			}
			if (IsShortcutPressed(ImGuiKey_F))
			{
				FUiCommands::FocusSelection(Frame.Editor);
			}
		}

		DrawFileMenu(Frame);
		DrawEditMenu(Frame);
		DrawWindowMenu(Frame);
		DrawHelpMenu(Frame);
		ImGui::EndMainMenuBar();
	}
	// A request that came while a window had to be answered is dropped with it, not kept for later.
	Frame.Requests.bSaveScene = false;

	DrawAboutWindow(Frame);
	SaveSceneDialog.Draw(Frame);
	OpenSceneDialog.Draw(Frame);
}

void FMainMenuBar::SaveScene(FUiFrame& Frame)
{
	if (Frame.Editor.GetScenePath().empty())
	{
		SaveSceneDialog.Open(Frame.Editor);
		return;
	}

	// A scene that cannot be saved must not look saved: the user is told, and asked for another place.
	const FCommandResult Result = FUiCommands::SaveScene(Frame.Editor);
	if (!Result.HasSucceeded())
	{
		SaveSceneDialog.Open(Frame.Editor, Result.GetError());
	}
}

void FMainMenuBar::DrawFileMenu(FUiFrame& Frame)
{
	if (!BeginNamedMenu(Frame, "File"))
	{
		return;
	}

	if (MenuEntry(Frame, "File", "New Scene", "Ctrl+N"))
	{
		FUiCommands::NewScene(Frame.Editor);
	}
	if (MenuEntry(Frame, "File", "Open Scene...", "Ctrl+O"))
	{
		OpenSceneDialog.Open(Frame.Editor);
	}
	ImGui::Separator();
	if (MenuEntry(Frame, "File", "Save Scene", "Ctrl+S"))
	{
		SaveScene(Frame);
	}
	if (MenuEntry(Frame, "File", "Save Scene As...", "Ctrl+Shift+S"))
	{
		SaveSceneDialog.Open(Frame.Editor);
	}
	ImGui::EndMenu();
}

void FMainMenuBar::DrawEditMenu(FUiFrame& Frame)
{
	if (!BeginNamedMenu(Frame, "Edit"))
	{
		return;
	}

	// Each entry says what it would undo or redo, as the history calls it.
	const FUndoHistory& History = Frame.Editor.GetHistory();
	const std::vector<std::string> UndoSteps = History.GetUndoStepNames();
	const std::vector<std::string> RedoSteps = History.GetRedoStepNames();
	const std::string UndoLabel = UndoSteps.empty() ? "Undo" : std::format("Undo {}", UndoSteps.front());
	const std::string RedoLabel = RedoSteps.empty() ? "Redo" : std::format("Redo {}", RedoSteps.front());

	// The part after ### is what Dear ImGui knows the entry by, so that it stays the same entry as its text changes.
	if (ImGui::MenuItem(std::format("{}###Undo", UndoLabel).c_str(), "Ctrl+Z", false, !UndoSteps.empty()))
	{
		FUiCommands::Undo(Frame.Editor);
	}
	Frame.Items.NameLastItem("Menu/Edit/Undo");

	if (ImGui::MenuItem(std::format("{}###Redo", RedoLabel).c_str(), "Ctrl+Y", false, !RedoSteps.empty()))
	{
		FUiCommands::Redo(Frame.Editor);
	}
	Frame.Items.NameLastItem("Menu/Edit/Redo");

	// What follows is about the entities that are selected.
	ImGui::Separator();
	const std::vector<int64> Selection = Frame.Editor.GetSelection();
	const bool bHasSelection = !Selection.empty();
	if (MenuEntry(Frame, "Edit", "Duplicate", "Ctrl+D", bHasSelection))
	{
		FUiCommands::DuplicateEntities(Frame.Editor, Selection);
	}
	if (MenuEntry(Frame, "Edit", "Delete", "Del", bHasSelection))
	{
		FUiCommands::DeleteEntities(Frame.Editor, Selection);
	}
	if (MenuEntry(Frame, "Edit", "Rename", "F2", bHasSelection))
	{
		RenameSelection(Frame);
	}
	if (MenuEntry(Frame, "Edit", "Focus", "F", bHasSelection))
	{
		FUiCommands::FocusSelection(Frame.Editor);
	}
	ImGui::EndMenu();
}

void FMainMenuBar::DrawWindowMenu(FUiFrame& Frame)
{
	if (!BeginNamedMenu(Frame, "Window"))
	{
		return;
	}

	MenuToggle(Frame, "Window", "Viewport", Frame.Workspace.bShowViewport);
	MenuToggle(Frame, "Window", "Outliner", Frame.Workspace.bShowOutliner);
	MenuToggle(Frame, "Window", "Details", Frame.Workspace.bShowDetails);
	ImGui::Separator();
	if (MenuEntry(Frame, "Window", "Load Default Layout", nullptr))
	{
		Frame.Workspace = FUiWorkspace();
	}
	ImGui::EndMenu();
}

void FMainMenuBar::DrawHelpMenu(FUiFrame& Frame)
{
	if (!BeginNamedMenu(Frame, "Help"))
	{
		return;
	}

	if (MenuEntry(Frame, "Help", AboutWindowTitle, nullptr))
	{
		bWantsAboutWindow = true;
	}
	ImGui::EndMenu();
}

void FMainMenuBar::DrawAboutWindow(FUiFrame& Frame)
{
	// A window is opened where it is laid out, which is here and not inside the menu that asked for it.
	if (bWantsAboutWindow)
	{
		ImGui::OpenPopup(AboutWindowTitle);
		bWantsAboutWindow = false;
	}

	const ImGuiViewport* const MainViewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(MainViewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (ImGui::BeginPopupModal(AboutWindowTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextUnformatted("White Wolf Engine");
		ImGui::TextUnformatted(std::format("Version {}", FEngineVersion::Current().ToString()).c_str());
		ImGui::Spacing();
		ImGui::TextUnformatted("A simple, production-grade 3D game engine for Windows.");
		ImGui::Spacing();
		const bool bWasCloseClicked = ImGui::Button("Close");
		Frame.Items.NameLastItem("About/Close");
		if (bWasCloseClicked || ImGui::IsKeyPressed(ImGuiKey_Escape))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}
