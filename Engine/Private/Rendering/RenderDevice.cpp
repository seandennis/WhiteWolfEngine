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

	// The lit scene is drawn to and then read by the pass that fits it to the display.
	nvrhi::TextureDesc SceneColorDescription = TextureDescription;
	SceneColorDescription.format = FRenderTarget::FImplementation::SceneColorFormat;
	SceneColorDescription.debugName = "RenderTargetSceneColor";

	// What ambient occlusion is worked out from and into: drawn to, then read by the next pass.
	nvrhi::TextureDesc NormalDescription = TextureDescription;
	NormalDescription.format = FRenderTarget::FImplementation::NormalFormat;
	NormalDescription.debugName = "RenderTargetNormals";

	nvrhi::TextureDesc OcclusionDescription = TextureDescription;
	OcclusionDescription.format = FRenderTarget::FImplementation::OcclusionFormat;
	OcclusionDescription.debugName = "RenderTargetOcclusion";

	nvrhi::IDevice* const Device = Context->GetDevice();
	std::unique_ptr<FRenderTarget::FImplementation> Implementation = std::make_unique<FRenderTarget::FImplementation>();
	Implementation->Context = Context;
	Implementation->Width = Width;
	Implementation->Height = Height;
	Implementation->Texture = Device->createTexture(TextureDescription);
	Implementation->DepthTexture = Device->createTexture(DepthDescription);
	Implementation->SceneColorTexture = Device->createTexture(SceneColorDescription);
	Implementation->NormalTexture = Device->createTexture(NormalDescription);
	Implementation->OcclusionTextures[0] = Device->createTexture(OcclusionDescription);
	Implementation->OcclusionTextures[1] = Device->createTexture(OcclusionDescription);

	const bool bHasTextures = Implementation->Texture != nullptr && Implementation->DepthTexture != nullptr &&
		Implementation->SceneColorTexture != nullptr && Implementation->NormalTexture != nullptr &&
		Implementation->OcclusionTextures[0] != nullptr && Implementation->OcclusionTextures[1] != nullptr;
	if (bHasTextures)
	{
		nvrhi::ITexture* const Depth = Implementation->DepthTexture;
		Implementation->Framebuffer = Device->createFramebuffer(
			nvrhi::FramebufferDesc().addColorAttachment(Implementation->Texture).setDepthAttachment(Depth));
		Implementation->SceneFramebuffer = Device->createFramebuffer(
			nvrhi::FramebufferDesc().addColorAttachment(Implementation->SceneColorTexture).setDepthAttachment(Depth));
		Implementation->OutputFramebuffer =
			Device->createFramebuffer(nvrhi::FramebufferDesc().addColorAttachment(Implementation->Texture));
		Implementation->PrepassFramebuffer = Device->createFramebuffer(
			nvrhi::FramebufferDesc().addColorAttachment(Implementation->NormalTexture).setDepthAttachment(Depth));
		Implementation->OcclusionFramebuffers[0] = Device->createFramebuffer(
			nvrhi::FramebufferDesc().addColorAttachment(Implementation->OcclusionTextures[0]));
		Implementation->OcclusionFramebuffers[1] = Device->createFramebuffer(
			nvrhi::FramebufferDesc().addColorAttachment(Implementation->OcclusionTextures[1]));
	}

	const bool bHasFramebuffers = Implementation->Framebuffer != nullptr &&
		Implementation->SceneFramebuffer != nullptr && Implementation->OutputFramebuffer != nullptr &&
		Implementation->PrepassFramebuffer != nullptr && Implementation->OcclusionFramebuffers[0] != nullptr &&
		Implementation->OcclusionFramebuffers[1] != nullptr;
	if (!bHasFramebuffers)
	{
		OutError = std::format("the graphics card could not create a render target of {} by {} pixels", Width, Height);
		return nullptr;
	}

	return std::make_unique<FRenderTarget>(std::move(Implementation));
}
