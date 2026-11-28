#include "EditorGraphics.h"

#include "Rendering/CameraView.h"
#include "Rendering/Image.h"
#include "Rendering/RenderDevice.h"
#include "Rendering/RenderDeviceDescription.h"
#include "Rendering/RenderTarget.h"
#include "Rendering/SceneRenderer.h"
#include "Scene/Scene.h"
#include "Viewport/ViewportPicture.h"

#include <utility>

FEditorGraphics::FEditorGraphics() = default;

FEditorGraphics::~FEditorGraphics() = default;

FRenderDevice* FEditorGraphics::Open(const FRenderDeviceDescription& Description, std::string& OutError)
{
	if (Device == nullptr)
	{
		Device = FRenderDevice::Create(Description, OutError);
	}
	return Device.get();
}

bool FEditorGraphics::DrawPicture(const WScene& Scene,
	const FCameraView& View,
	ESceneViewMode ViewMode,
	int32 Width,
	int32 Height,
	FImage& OutPicture,
	std::string& OutError)
{
	FRenderDevice* const OpenDevice = Open(FRenderDeviceDescription(), OutError);
	if (OpenDevice == nullptr)
	{
		return false;
	}
	if (PictureRenderer == nullptr)
	{
		PictureRenderer = FSceneRenderer::Create(*OpenDevice, OutError);
		if (PictureRenderer == nullptr)
		{
			return false;
		}
	}

	const std::unique_ptr<FRenderTarget> Target = OpenDevice->CreateRenderTarget(Width, Height, OutError);
	if (Target == nullptr)
	{
		return false;
	}

	FViewportPicture::Configure(*PictureRenderer, ViewMode);
	FImage Picture;
	if (!PictureRenderer->Render(Scene, View, *Target, OutError) || !Target->ReadPixels(Picture, OutError))
	{
		return false;
	}
	OutPicture = std::move(Picture);
	return true;
}
