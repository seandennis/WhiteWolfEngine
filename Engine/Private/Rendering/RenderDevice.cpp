#include "Rendering/RenderDevice.h"

#include "Rendering/RenderContext.h"
#include "Rendering/RenderTarget.h"
#include "Rendering/RenderTargetImplementation.h"

#include <nvrhi/nvrhi.h>

#include <format>
#include <utility>

std::unique_ptr<FRenderDevice> FRenderDevice::Create(const FRenderDeviceDescription& Description, std::string& OutError)
{
	std::shared_ptr<FRenderContext> NewContext = FRenderContext::Create(Description, OutError);
	if (NewContext == nullptr)
	{
		return nullptr;
	}

	return std::make_unique<FRenderDevice>(std::move(NewContext));
}

FRenderDevice::FRenderDevice(std::shared_ptr<FRenderContext> InContext)
	: Context(std::move(InContext))
{
}

FRenderDevice::~FRenderDevice() = default;

const std::string& FRenderDevice::GetAdapterName() const
{
	return Context->GetAdapterName();
}

bool FRenderDevice::IsSoftwareAdapter() const
{
	return Context->IsSoftwareAdapter();
}

std::unique_ptr<FRenderTarget> FRenderDevice::CreateRenderTarget(int32 Width, int32 Height, std::string& OutError)
{
	const bool bIsValidSize =
		Width >= 1 && Width <= FRenderTarget::MaxDimension && Height >= 1 && Height <= FRenderTarget::MaxDimension;
	if (!bIsValidSize)
	{
		OutError = std::format("a render target cannot be {} by {} pixels; each side must be between 1 and {}",
			Width,
			Height,
			FRenderTarget::MaxDimension);
		return nullptr;
	}

	// The surface is drawn to and read from, and NVRHI moves it between those uses by itself.
	nvrhi::TextureDesc TextureDescription;
	TextureDescription.width = static_cast<uint32>(Width);
	TextureDescription.height = static_cast<uint32>(Height);
	TextureDescription.format = FRenderTarget::FImplementation::ColorFormat;
	TextureDescription.dimension = nvrhi::TextureDimension::Texture2D;
	TextureDescription.isRenderTarget = true;
	TextureDescription.debugName = "RenderTarget";
	TextureDescription.enableAutomaticStateTracking(nvrhi::ResourceStates::RenderTarget);

	nvrhi::TextureDesc DepthDescription;
	DepthDescription.width = TextureDescription.width;
	DepthDescription.height = TextureDescription.height;
	DepthDescription.format = FRenderTarget::FImplementation::DepthFormat;
	DepthDescription.dimension = nvrhi::TextureDimension::Texture2D;
	DepthDescription.isRenderTarget = true;
	DepthDescription.debugName = "RenderTargetDepth";
	DepthDescription.enableAutomaticStateTracking(nvrhi::ResourceStates::DepthWrite);

	nvrhi::IDevice* const Device = Context->GetDevice();
	nvrhi::TextureHandle Texture = Device->createTexture(TextureDescription);
	nvrhi::TextureHandle DepthTexture = Device->createTexture(DepthDescription);
	nvrhi::FramebufferHandle Framebuffer;
	if (Texture != nullptr && DepthTexture != nullptr)
	{
		Framebuffer = Device->createFramebuffer(
			nvrhi::FramebufferDesc().addColorAttachment(Texture).setDepthAttachment(DepthTexture));
	}
	if (Framebuffer == nullptr)
	{
		OutError = std::format("the graphics card could not create a render target of {} by {} pixels", Width, Height);
		return nullptr;
	}

	std::unique_ptr<FRenderTarget::FImplementation> Implementation = std::make_unique<FRenderTarget::FImplementation>();
	Implementation->Context = Context;
	Implementation->Texture = std::move(Texture);
	Implementation->DepthTexture = std::move(DepthTexture);
	Implementation->Framebuffer = std::move(Framebuffer);
	Implementation->Width = Width;
	Implementation->Height = Height;
	return std::make_unique<FRenderTarget>(std::move(Implementation));
}
