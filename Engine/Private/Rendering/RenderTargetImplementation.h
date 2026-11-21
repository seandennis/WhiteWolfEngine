#pragma once

#include "CoreMinimal.h"

#include "Rendering/RenderContext.h"
#include "Rendering/RenderTarget.h"

#include <nvrhi/nvrhi.h>

#include <memory>

/** Everything a render target holds. It lives here so that no NVRHI type appears in an engine header. */
struct FRenderTarget::FImplementation
{
	/** Keeps the graphics card alive for as long as this render target exists. */
	std::shared_ptr<FRenderContext> Context;

	/** The formats of every render target. The renderer's pipelines are built for exactly these. */
	static constexpr nvrhi::Format ColorFormat = nvrhi::Format::SRGBA8_UNORM;
	static constexpr nvrhi::Format DepthFormat = nvrhi::Format::D32;

	/** The surface itself, on the graphics card. */
	nvrhi::TextureHandle Texture;

	/** The depth of what was drawn at each pixel, which is how nearer surfaces come to hide farther ones. */
	nvrhi::TextureHandle DepthTexture;

	/** The surface and its depth together, as the target of drawing. */
	nvrhi::FramebufferHandle Framebuffer;

	/** The copy of the surface that the processor can read. Created the first time the pixels are read. */
	nvrhi::StagingTextureHandle ReadbackTexture;

	int32 Width = 0;
	int32 Height = 0;
};
