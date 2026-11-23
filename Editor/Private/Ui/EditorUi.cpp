#include "Ui/EditorUi.h"

#include "Editor.h"
#include "Math/LinearColor.h"
#include "Rendering/RenderDevice.h"
#include "Rendering/RenderTarget.h"
#include "Rendering/SceneRenderer.h"
#include "Rendering/UiDrawList.h"
#include "Rendering/UiRenderer.h"
#include "Ui/ImGuiLibrary.h"
#include "Ui/MainMenuBar.h"
#include "Ui/Panels/OutlinerPanel.h"
#include "Ui/Panels/ViewportPanel.h"
#include "Ui/UiDrawData.h"
#include "Ui/UiFrame.h"
#include "Ui/UiInput.h"
#include "Ui/UiItems.h"
#include "Ui/UiRequests.h"
#include "Ui/UiTextures.h"
#include "Ui/UiTheme.h"
#include "Ui/UiWorkspace.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
	/** The part of the main window's width that the panels to the right of the viewport have in the default layout. */
	constexpr float RightColumnFraction = 0.22f;

	/** The shortest time a frame counts as taking: Dear ImGui cannot divide by no time at all. */
	constexpr float MinDeltaSeconds = 1.0e-5f;

	/** What a surface shows where the user interface leaves it uncovered, which it never does. */
	constexpr FLinearColor UncoveredColor(0.0f, 0.0f, 0.0f);
}

/** Everything the user interface holds. It lives here so that no Dear ImGui type appears in a public header. */
struct FEditorUi::FImplementation
{
	FImplementation(FEditor& InEditor, FRenderDevice& InDevice)
		: Editor(InEditor)
		, Device(InDevice)
	{
	}

	~FImplementation()
	{
		if (Context != nullptr)
		{
			ImGui::SetCurrentContext(Context);
			Textures.ReleaseAll(ImGui::GetPlatformIO().Textures);
			ImGui::DestroyContext(Context);
		}
	}

	FImplementation(const FImplementation&) = delete;
	FImplementation& operator=(const FImplementation&) = delete;

	/** Puts the panels where they are when the editor is first started. */
	void BuildDefaultLayout(ImGuiID DockSpaceId, const ImVec2& Size) const
	{
		ImGui::DockBuilderRemoveNode(DockSpaceId);
		ImGui::DockBuilderAddNode(DockSpaceId, ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(DockSpaceId, Size);

		// The viewport has the middle; what is split off around it takes the other panels.
		ImGuiID MiddleId = DockSpaceId;
		const ImGuiID RightId =
			ImGui::DockBuilderSplitNode(MiddleId, ImGuiDir_Right, RightColumnFraction, nullptr, &MiddleId);
		ImGui::DockBuilderDockWindow(FViewportPanel::Title, MiddleId);
		ImGui::DockBuilderDockWindow(FOutlinerPanel::Title, RightId);
		ImGui::DockBuilderFinish(DockSpaceId);
	}

	/**
	 * Lays out one frame of the user interface: the menu bar, the space the panels are docked in, and the panels.
	 *
	 * @return False, with the reason in OutError, if a panel could not get what it draws with.
	 */
	bool LayOut(std::string& OutError)
	{
		FUiFrame Frame{Editor, Items, Textures, Workspace, Requests};
		MenuBar.Draw(Frame);

		const ImGuiViewport* const MainViewport = ImGui::GetMainViewport();
		const ImGuiID DockSpaceId = ImGui::GetID("EditorDockSpace");
		if (Workspace.bWantsDefaultLayout)
		{
			BuildDefaultLayout(DockSpaceId, MainViewport->WorkSize);
			Workspace.bWantsDefaultLayout = false;
		}
		ImGui::DockSpaceOverViewport(DockSpaceId, MainViewport);

		const bool bViewportSucceeded = Viewport.Draw(Frame, Device, OutError);
		Outliner.Draw(Frame);
		return bViewportSucceeded;
	}

	FEditor& Editor;
	FRenderDevice& Device;

	/** The state of Dear ImGui for this user interface. Made current at the start of everything that uses it. */
	ImGuiContext* Context = nullptr;

	std::unique_ptr<FUiRenderer> UiRenderer;
	std::unique_ptr<FSceneRenderer> SceneRenderer;

	FUiTextures Textures;
	FUiItems Items;
	FUiWorkspace Workspace;
	FUiRequests Requests;

	/** By how much the sizes of the user interface are multiplied; see FEditorUi::SetScale. */
	float Scale = 1.0f;

	FMainMenuBar MenuBar;
	FViewportPanel Viewport;
	FOutlinerPanel Outliner;

	/** The triangles of the frame being drawn. Kept between frames so that its memory is used again. */
	FUiDrawList DrawList;
};

std::unique_ptr<FEditorUi> FEditorUi::Create(FEditor& Editor, FRenderDevice& Device, std::string& OutError)
{
	std::unique_ptr<FImplementation> NewImplementation = std::make_unique<FImplementation>(Editor, Device);

	NewImplementation->UiRenderer = FUiRenderer::Create(Device, OutError);
	if (NewImplementation->UiRenderer == nullptr)
	{
		return nullptr;
	}
	NewImplementation->SceneRenderer = FSceneRenderer::Create(Device, OutError);
	if (NewImplementation->SceneRenderer == nullptr)
	{
		return nullptr;
	}

	IMGUI_CHECKVERSION();
	NewImplementation->Context = ImGui::CreateContext();
	ImGui::SetCurrentContext(NewImplementation->Context);

	ImGuiIO& Io = ImGui::GetIO();
	// The editor reads and writes its own files; Dear ImGui is left none to write by itself.
	Io.IniFilename = nullptr;
	Io.LogFilename = nullptr;
	Io.BackendPlatformName = "White Wolf Engine";
	Io.BackendRendererName = "White Wolf Engine";
	// Dear ImGui may create, update and destroy textures as it goes: FUiTextures does what it asks.
	Io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
	// The keyboard moves through menus and panels as it does in other programs: arrows, Enter, and Escape to close.
	Io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
	// A panel is moved by its tab; dragging inside it is for what the panel shows.
	Io.ConfigWindowsMoveFromTitleBarOnly = true;

	FUiTheme::AddFonts();
	FUiTheme::Apply(NewImplementation->Scale);
	return std::make_unique<FEditorUi>(std::move(NewImplementation));
}

FEditorUi::FEditorUi(std::unique_ptr<FImplementation> InImplementation)
	: Implementation(std::move(InImplementation))
{
}

FEditorUi::~FEditorUi() = default;

bool FEditorUi::Draw(FRenderTarget& Target, const FInputState& Input, double DeltaSeconds, std::string& OutError)
{
	FImplementation& Ui = *Implementation;
	ImGui::SetCurrentContext(Ui.Context);

	ImGuiIO& Io = ImGui::GetIO();
	Io.DisplaySize = ImVec2(static_cast<float>(Target.GetWidth()), static_cast<float>(Target.GetHeight()));
	Io.DeltaTime = std::max(static_cast<float>(DeltaSeconds), MinDeltaSeconds);
	FUiInput::Feed(Input, Io);

	Ui.Textures.BeginFrame();
	Ui.Items.BeginFrame();
	ImGui::NewFrame();
	const bool bWasLaidOut = Ui.LayOut(OutError);
	// A frame that was begun is ended whatever happened in it.
	ImGui::Render();
	if (!bWasLaidOut)
	{
		return false;
	}

	// The scene first: the user interface that is drawn next shows that picture.
	if (!Ui.Viewport.RenderScene(*Ui.SceneRenderer, Ui.Editor, OutError))
	{
		return false;
	}

	ImDrawData* const DrawData = ImGui::GetDrawData();
	if (DrawData->Textures != nullptr && !Ui.Textures.Update(*DrawData->Textures, OutError))
	{
		return false;
	}
	FUiDrawData::Convert(*DrawData, Ui.Textures, Ui.DrawList);

	Target.Clear(UncoveredColor);
	return Ui.UiRenderer->Draw(Ui.DrawList, Target, OutError);
}

void FEditorUi::SetScale(float NewScale)
{
	const float AcceptedScale = std::isfinite(NewScale) ? std::clamp(NewScale, MinScale, MaxScale) : 1.0f;
	if (AcceptedScale != Implementation->Scale)
	{
		Implementation->Scale = AcceptedScale;
		ImGui::SetCurrentContext(Implementation->Context);
		FUiTheme::Apply(AcceptedScale);
	}
}

float FEditorUi::GetScale() const
{
	return Implementation->Scale;
}

std::optional<FUiRectangle> FEditorUi::FindItem(std::string_view Name) const
{
	return Implementation->Items.Find(Name);
}
