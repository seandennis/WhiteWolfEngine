#pragma once

#include "CoreMinimal.h"

#include "Rendering/AmbientOcclusionTargetBindings.h"
#include "Rendering/OutlineTargetBindings.h"
#include "Rendering/RenderContext.h"
#include "Rendering/RenderTarget.h"

#include <nvrhi/nvrhi.h>

#include <array>
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

	/** The formats of what ambient occlusion is worked out from and into: the way surfaces face, and the result. */
	static constexpr nvrhi::Format NormalFormat = nvrhi::Format::RGBA8_UNORM;
	static constexpr nvrhi::Format OcclusionFormat = nvrhi::Format::R8_UNORM;

	/** The format of the mask that says where the meshes are that get a line around them. */
	static constexpr nvrhi::Format OutlineMaskFormat = nvrhi::Format::R8_UNORM;

	/** The surface itself, on the graphics card. */
	nvrhi::TextureHandle Texture;

	/** The depth of what was drawn at each pixel, which is how nearer surfaces come to hide farther ones. */
	nvrhi::TextureHandle DepthTexture;

	/** The lit scene before it is fitted to the display, the same size as the surface. */
	nvrhi::TextureHandle SceneColorTexture;

	/** The way the visible surfaces face, which the prepass draws for ambient occlusion. */
	nvrhi::TextureHandle NormalTexture;

	/** The ambient occlusion: the result is in the first, and the second is what smoothing it needs in between. */
	std::array<nvrhi::TextureHandle, 2> OcclusionTextures;

	/** The normals and the depth together, for the prepass. */
	nvrhi::FramebufferHandle PrepassFramebuffer;

	/** Each ambient occlusion texture alone, for the passes that draw into them. */
	std::array<nvrhi::FramebufferHandle, 2> OcclusionFramebuffers;

	/** What gives the ambient occlusion passes this target's textures. Made by the scene renderer. */
	FAmbientOcclusionTargetBindings OcclusionBindings;

	/**
	 * What gives the lit pass the ambient occlusion of this target, or a white texture when there is none. The
	 * scene renderer that last drew to the target made it, for the layout and the texture kept beside it.
	 */
	nvrhi::BindingLayoutHandle LitBindingLayout;
	nvrhi::BindingSetHandle LitBindingSet;
	nvrhi::ITexture* LitBoundOcclusion = nullptr;

	/** Where the meshes are that get a line around them, the same size as the surface, and what draws into it. */
	nvrhi::TextureHandle OutlineMaskTexture;
	nvrhi::FramebufferHandle OutlineMaskFramebuffer;

	/** What gives the pass that draws those lines the mask to read. Made by the scene renderer. */
	FOutlineTargetBindings OutlineBindings;

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
