#include "EditorWindow.h"

#include "Editor.h"
#include "Platform/Platform.h"
#include "Platform/Window.h"
#include "Platform/WindowDescription.h"
#include "Project/Project.h"
#include "Rendering/RenderDevice.h"
#include "Rendering/RenderDeviceDescription.h"
#include "Rendering/RenderTarget.h"
#include "Rendering/SwapChain.h"
#include "Scene/Scene.h"
#include "Ui/EditorUi.h"

#include <format>
#include <utility>

namespace
{
	/** Returns what the title bar says: the scene, the project if there is one, and what the program is. */
	std::string MakeTitle(const FEditor& Editor)
	{
		const std::string& SceneName = Editor.GetScene().GetName();
		if (const FProject* const Project = Editor.GetProject())
		{
			return std::format("{} - {} - {}", SceneName, Project->GetName(), FEditorWindow::ApplicationName);
		}
		return std::format("{} - {}", SceneName, FEditorWindow::ApplicationName);
	}
}

std::unique_ptr<FEditorWindow> FEditorWindow::Create(
	FEditor& Editor, const FEditorWindowDescription& Description, std::string& OutError)
{
	FWindowDescription WindowDescription;
	WindowDescription.Title = MakeTitle(Editor);
	WindowDescription.Width = Description.Width;
	WindowDescription.Height = Description.Height;
	std::unique_ptr<FWindow> Window = FWindow::Create(WindowDescription, OutError);
	if (Window == nullptr)
	{
		return nullptr;
	}

	FRenderDeviceDescription DeviceDescription;
	DeviceDescription.bEnableValidation = Description.bValidateRendering;
	std::unique_ptr<FRenderDevice> Device = FRenderDevice::Create(DeviceDescription, OutError);
	if (Device == nullptr)
	{
		return nullptr;
	}

	// A window on the desktop has to be able to show what is drawn. One that exists in memory only shows nothing
	// in any case, so a graphics driver that cannot hand pictures to such a window is no obstacle.
	std::unique_ptr<FSwapChain> SwapChain;
	const bool bIsOnDesktop = FPlatform::GetMode() == EPlatformMode::Windowed;
	if (bIsOnDesktop || FSwapChain::IsSupported(*Device, *Window))
	{
		SwapChain = FSwapChain::Create(*Device, *Window, OutError);
		if (SwapChain == nullptr)
		{
			return nullptr;
		}
	}

	std::unique_ptr<FEditorUi> Ui = FEditorUi::Create(Editor, *Device, OutError);
	if (Ui == nullptr)
	{
		return nullptr;
	}

	return std::make_unique<FEditorWindow>(
		Editor, std::move(Window), std::move(Device), std::move(SwapChain), std::move(Ui));
}

FEditorWindow::FEditorWindow(FEditor& InEditor,
	std::unique_ptr<FWindow> InWindow,
	std::unique_ptr<FRenderDevice> InDevice,
	std::unique_ptr<FSwapChain> InSwapChain,
	std::unique_ptr<FEditorUi> InUi)
	: Editor(InEditor)
	, Window(std::move(InWindow))
	, Device(std::move(InDevice))
	, SwapChain(std::move(InSwapChain))
	, Ui(std::move(InUi))
{
}

FEditorWindow::~FEditorWindow() = default;

bool FEditorWindow::RunFrame(double DeltaSeconds, std::string& OutError)
{
	if (!HasArea())
	{
		return true;
	}

	// The title bar says which scene is being edited, and scenes are opened and renamed.
	std::string Title = MakeTitle(Editor);
	if (Title != Window->GetTitle())
	{
		Window->SetTitle(Title);
	}

	const int32 Width = Window->GetWidth();
	const int32 Height = Window->GetHeight();
	const bool bPictureFits = Picture != nullptr && Picture->GetWidth() == Width && Picture->GetHeight() == Height;
	if (!bPictureFits)
	{
		std::unique_ptr<FRenderTarget> NewPicture = Device->CreateRenderTarget(Width, Height, OutError);
		if (NewPicture == nullptr)
		{
			return false;
		}
		Picture = std::move(NewPicture);
	}

	if (!Ui->Draw(*Picture, Window->GetInput(), DeltaSeconds, OutError))
	{
		return false;
	}
	return SwapChain == nullptr || SwapChain->Present(*Picture, OutError);
}

bool FEditorWindow::ShouldClose() const
{
	return Window->ShouldClose();
}

bool FEditorWindow::HasArea() const
{
	return Window->GetWidth() > 0 && Window->GetHeight() > 0;
}

FWindow& FEditorWindow::GetWindow()
{
	return *Window;
}

FEditorUi& FEditorWindow::GetUi()
{
	return *Ui;
}

const FRenderTarget* FEditorWindow::GetPicture() const
{
	return Picture.get();
}

int64 FEditorWindow::GetShownFrameCount() const
{
	return SwapChain != nullptr ? SwapChain->GetPresentedCount() : 0;
}
