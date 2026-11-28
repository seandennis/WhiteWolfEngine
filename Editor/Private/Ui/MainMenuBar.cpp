#include "Ui/MainMenuBar.h"

#include "Assets/EditedAssets.h"
#include "Commands/CommandResult.h"
#include "Core/EngineVersion.h"
#include "Editor.h"
#include "Ui/Dialogs/UiDialog.h"
#include "Ui/ImGuiLibrary.h"
#include "Ui/UiCommands.h"
#include "Ui/UiFrame.h"
#include "Ui/UiItems.h"
#include "Ui/UiRequests.h"
#include "Ui/UiWorkspace.h"
#include "Viewport/GizmoMode.h"
#include "Viewport/GizmoSettings.h"

#include <format>
#include <string>
#include <utility>
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

	/**
	 * Like MenuEntry, for one of several entries of which one is in force, and has a tick that says so.
	 *
	 * @return True if the entry was chosen.
	 */
	bool MenuChoice(FUiFrame& Frame, const char* Menu, const char* Label, const char* Shortcut, bool bIsInForce)
	{
		const bool bWasChosen = ImGui::MenuItem(Label, Shortcut, bIsInForce);
		Frame.Items.NameLastItem(std::format("Menu/{}/{}", Menu, Label));
		return bWasChosen;
	}

	/** Returns true if the keys of a shortcut were pressed and nothing that takes the keyboard for itself has it. */
	bool IsShortcutPressed(ImGuiKeyChord Keys)
	{
		return ImGui::Shortcut(Keys, ImGuiInputFlags_RouteGlobal);
	}

	/** Sets what dragging the gizmo of the viewport does. */
	void SetGizmoMode(FEditor& Editor, EGizmoMode Mode)
	{
		FGizmoSettings Settings = Editor.GetGizmoSettings();
		Settings.Mode = Mode;
		Editor.SetGizmoSettings(Settings);
	}

	/**
	 * Carries out the keys of the viewport that are single letters: F to bring the camera to what is selected, and
	 * those that set what dragging the gizmo does: W to move, E to turn, R to size, and Space for the next of the
	 * three.
	 */
	void CarryOutViewportShortcuts(FUiFrame& Frame)
	{
		// The keys are asked for in every frame, also in those in which a mouse button keeps them from doing
		// anything. Dear ImGui gives a key to whoever asked for it in the frame before: a frame without the question
		// would cost the key that goes down in the first frame after it, the one in which the button comes up.
		const bool bWantsFocus = IsShortcutPressed(ImGuiKey_F);
		const bool bWantsMove = IsShortcutPressed(ImGuiKey_W);
		const bool bWantsRotate = IsShortcutPressed(ImGuiKey_E);
		const bool bWantsScale = IsShortcutPressed(ImGuiKey_R);

		// Space also presses the button that the keyboard has been taken to. While the mark of that is shown, the
		// key is the button's, and is not asked for at all: asking would take it from the button.
		const bool bWantsNext = !ImGui::GetIO().NavVisible && IsShortcutPressed(ImGuiKey_Space);

		// While a mouse button is held, these keys are the camera's: W and E fly it, and the others are best left
		// alone until the hand is free again. A camera that went somewhere else in the middle of a drag of the
		// gizmo would leave the drag measuring from where the camera was.
		if (ImGui::IsAnyMouseDown())
		{
			return;
		}

		FEditor& Editor = Frame.Editor;
		if (bWantsFocus)
		{
			FUiCommands::FocusSelection(Editor);
		}
		if (bWantsMove)
		{
			SetGizmoMode(Editor, EGizmoMode::Translate);
		}
		if (bWantsRotate)
		{
			SetGizmoMode(Editor, EGizmoMode::Rotate);
		}
		if (bWantsScale)
		{
			SetGizmoMode(Editor, EGizmoMode::Scale);
		}
		if (bWantsNext)
		{
			SetGizmoMode(Editor, FGizmoSettings::GetNextMode(Editor.GetGizmoSettings().Mode));
		}
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
			if (IsShortcutPressed(ImGuiMod_Ctrl | ImGuiKey_S) || Frame.Requests.bSave)
			{
				Save(Frame);
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
			CarryOutViewportShortcuts(Frame);

			// The windows for projects are also asked for from outside the menu bar: the first when the editor
			// starts, the second by the first, for someone who has no project to open yet.
			if (Frame.Requests.bOpenProject)
			{
				OpenProjectDialog.Open(Frame.Editor);
			}
			if (Frame.Requests.bNewProject)
			{
				NewProjectDialog.Open(Frame.Editor);
			}
		}

		DrawFileMenu(Frame);
		DrawEditMenu(Frame);
		DrawWindowMenu(Frame);
		DrawHelpMenu(Frame);
		ImGui::EndMainMenuBar();
	}
	// A request that came while a window had to be answered is dropped with it, not kept for later.
	Frame.Requests.bSave = false;
	Frame.Requests.bOpenProject = false;
	Frame.Requests.bNewProject = false;

	DrawAboutWindow(Frame);
	OpenProjectDialog.Draw(Frame);
	NewProjectDialog.Draw(Frame);
	// The scene is saved once the user has read what could not be: one window to answer at a time, and a material
	// that cannot be written does not keep the scene from being saved.
	if (NotSavedDialog.Draw(Frame))
	{
		SaveScene(Frame);
	}
	SaveSceneDialog.Draw(Frame);
	OpenSceneDialog.Draw(Frame);
	NewMaterialDialog.Draw(Frame);
}

void FMainMenuBar::Save(FUiFrame& Frame)
{
	// The materials first, and the scene that may refer to them after them.
	std::vector<std::string> Problems;
	for (const std::string& AssetPath : Frame.Editor.GetEditedAssets().GetUnsaved())
	{
		const FCommandResult Result = FUiCommands::SaveAsset(Frame.Editor, AssetPath);
		if (!Result.HasSucceeded())
		{
			Problems.push_back(Result.GetError());
		}
	}

	// What could not be written must not look saved: the user is told at once, and the scene is saved after that.
	if (!Problems.empty())
	{
		NotSavedDialog.Open(std::move(Problems));
		return;
	}
	SaveScene(Frame);
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

	if (MenuEntry(Frame, "File", "New Project...", nullptr))
	{
		NewProjectDialog.Open(Frame.Editor);
	}
	if (MenuEntry(Frame, "File", "Open Project...", nullptr))
	{
		OpenProjectDialog.Open(Frame.Editor);
	}
	ImGui::Separator();
	if (MenuEntry(Frame, "File", "New Scene", "Ctrl+N"))
	{
		FUiCommands::NewScene(Frame.Editor);
	}
	if (MenuEntry(Frame, "File", "Open Scene...", "Ctrl+O"))
	{
		OpenSceneDialog.Open(Frame.Editor);
	}
	ImGui::Separator();
	if (MenuEntry(Frame, "File", "Save", "Ctrl+S"))
	{
		Save(Frame);
	}
	if (MenuEntry(Frame, "File", "Save Scene As...", "Ctrl+Shift+S"))
	{
		SaveSceneDialog.Open(Frame.Editor);
	}
	ImGui::Separator();
	// Assets belong to a project.
	if (MenuEntry(Frame, "File", "New Material...", nullptr, Frame.Editor.GetProject() != nullptr))
	{
		NewMaterialDialog.Open(Frame.Editor);
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

	// What dragging the gizmo of the viewport does.
	ImGui::Separator();
	const EGizmoMode GizmoMode = Frame.Editor.GetGizmoSettings().Mode;
	if (MenuChoice(Frame, "Edit", "Move", "W", GizmoMode == EGizmoMode::Translate))
	{
		SetGizmoMode(Frame.Editor, EGizmoMode::Translate);
	}
	if (MenuChoice(Frame, "Edit", "Rotate", "E", GizmoMode == EGizmoMode::Rotate))
	{
		SetGizmoMode(Frame.Editor, EGizmoMode::Rotate);
	}
	if (MenuChoice(Frame, "Edit", "Scale", "R", GizmoMode == EGizmoMode::Scale))
	{
		SetGizmoMode(Frame.Editor, EGizmoMode::Scale);
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
	MenuToggle(Frame, "Window", "Output Log", Frame.Workspace.bShowOutputLog);
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
		if (bWasCloseClicked || FUiDialog::IsAnswerKeyPressed(ImGuiKey_Escape))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}
