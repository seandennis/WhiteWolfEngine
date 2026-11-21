#pragma once

#include "CoreMinimal.h"

#include "Rendering/RenderContext.h"

#include <nvrhi/nvrhi.h>

#include <memory>
#include <vector>

/**
 * The shadow maps of a scene renderer: one texture with a layer for every cascade of every directional light that
 * casts shadows, and what is needed to draw into the layers and to read them. Internal to the scene renderer.
 */
class FShadowMaps
{
public:
	/** The format of a shadow map: the depth of what the light sees. */
	static constexpr nvrhi::Format Format = nvrhi::Format::D32;

	/** With ordinary depth, as shadow maps use it, the farthest value is one. */
	static constexpr float FarthestDepth = 1.0f;

	/** What ShadowDepth.vert is told about the shadow map it draws. Must match FShadowPassConstants there. */
	struct FPassConstants
	{
		/** Which of FViewConstants::ShadowMatrices the shadow map uses. */
		int32 MatrixIndex = 0;
		int32 Padding[3] = {0, 0, 0};
	};

	/**
	 * Creates the pipeline that draws into shadow maps, the sampler that reads them, and a first, tiny texture, so
	 * that there is always something to bind.
	 *
	 * @return False if the graphics card could not create them.
	 */
	bool Initialize(std::shared_ptr<FRenderContext> InContext);

	/**
	 * Tells the shadow pass where the data of the picture's meshes and view is. Its bindings are made again only
	 * when one of the buffers is another than before.
	 *
	 * @param DrawBuffer The array of FDrawData.
	 * @param ViewBuffer The FViewConstants.
	 * @return False if the graphics card could not create the bindings.
	 */
	bool BindSceneData(nvrhi::IBuffer* DrawBuffer, nvrhi::IBuffer* ViewBuffer);

	/** Returns what BindSceneData made: the bindings the shadow pass draws with. */
	nvrhi::IBindingSet* GetPassBindings() const
	{
		return PassBindings;
	}

	/**
	 * Makes sure there are at least LayerCount shadow maps of MapSize pixels a side. The texture is made again
	 * only when it has too few layers or another size.
	 *
	 * @return False if the graphics card could not create the texture; the previous one is then kept.
	 */
	bool Prepare(int32 LayerCount, int32 MapSize);

	nvrhi::ITexture* GetTexture() const
	{
		return Texture;
	}

	nvrhi::ISampler* GetSampler() const
	{
		return Sampler;
	}

	nvrhi::IGraphicsPipeline* GetPipeline() const
	{
		return Pipeline;
	}

	/** Returns the target for drawing into one shadow map. */
	nvrhi::IFramebuffer* GetLayerTarget(int32 Layer) const
	{
		return LayerTargets[static_cast<std::size_t>(Layer)];
	}

	/** The width and height of each shadow map, in pixels. */
	int32 GetMapSize() const
	{
		return MapSize;
	}

private:
	/** Creates a texture with its targets. False on failure, with the outputs untouched. */
	bool CreateTexture(int32 LayerCount, int32 InMapSize);

	std::shared_ptr<FRenderContext> Context;

	/**
	 * The shadow pass has bindings of its own: the two buffers of the scene's data that it reads, and the
	 * constants that say which shadow map is being drawn.
	 */
	nvrhi::BindingLayoutHandle PassBindingLayout;
	nvrhi::BindingSetHandle PassBindings;
	nvrhi::IBuffer* BoundDrawBuffer = nullptr;
	nvrhi::IBuffer* BoundViewBuffer = nullptr;

	nvrhi::GraphicsPipelineHandle Pipeline;
	nvrhi::SamplerHandle Sampler;

	nvrhi::TextureHandle Texture;
	std::vector<nvrhi::FramebufferHandle> LayerTargets;
	int32 MapSize = 0;
};
