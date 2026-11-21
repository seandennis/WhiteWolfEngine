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

	/** The format lighting is calculated in: wide enough for light far brighter than a display can show. */
	static constexpr nvrhi::Format SceneColorFormat = nvrhi::Format::RGBA16_FLOAT;

	/** The surface itself, on the graphics card. */
	nvrhi::TextureHandle Texture;

	/** The depth of what was drawn at each pixel, which is how nearer surfaces come to hide farther ones. */
	nvrhi::TextureHandle DepthTexture;

	/** The lit scene before it is fitted to the display, the same size as the surface. */
	nvrhi::TextureHandle SceneColorTexture;

	/** The surface and the depth together, for drawing that needs no lighting. */
	nvrhi::FramebufferHandle Framebuffer;

	/** The lit scene and the depth together, for drawing with lighting. */
	nvrhi::FramebufferHandle SceneFramebuffer;

	/** The surface alone, for the pass that fits the lit scene to the display. */
	nvrhi::FramebufferHandle OutputFramebuffer;

	/**
	 * What gives the tone mapping pass the lit scene to read. The scene renderer that last drew to this target
	 * created it, for the layout kept beside it.
	 */
	nvrhi::BindingLayoutHandle TonemapBindingLayout;
	nvrhi::BindingSetHandle TonemapBindingSet;

	/** The copy of the surface that the processor can read. Created the first time the pixels are read. */
	nvrhi::StagingTextureHandle ReadbackTexture;

	int32 Width = 0;
	int32 Height = 0;
};
