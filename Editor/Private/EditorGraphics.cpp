#include "EditorGraphics.h"

#include "Rendering/RenderDevice.h"
#include "Rendering/RenderDeviceDescription.h"

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
