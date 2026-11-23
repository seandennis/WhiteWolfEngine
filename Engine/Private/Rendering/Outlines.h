#pragma once

#include "CoreMinimal.h"

#include "Rendering/OutlineTargetBindings.h"
#include "Rendering/RenderContext.h"

#include <glm/vec4.hpp>
#include <nvrhi/nvrhi.h>

#include <memory>

/**
 * Lines around meshes: how a picture marks some of what it shows, as an editor marks what is selected. Internal to
 * the scene renderer.
 *
 * It takes two passes. The meshes to outline are drawn into a mask, a surface of the picture's size that says for
 * every pixel whether one of them is there; the scene renderer does that, with the pipeline that this class has
 * for it. Then a pass over the whole picture draws the line wherever a marked pixel is near and the pixel itself is
 * not marked. The mask takes no notice of depth, so the line goes around the whole of a mesh, also where
 * something nearer hides it, and around several meshes that overlap as around one shape.
 */
class FOutlines
{
public:
	/** What the pass that draws the line is told. Must match FOutlineConstants in Outline.frag. */
	struct FConstants
	{
		/** The colour of the line, in linear space; a is how much of the picture below it the line covers. */
		glm::vec4 Color = glm::vec4(0.0f);

		/** x: the width of the line in pixels. */
		glm::vec4 Settings = glm::vec4(0.0f);
	};

	/**
	 * Creates the pipelines of the two passes.
	 *
	 * @param MeshLayout How the vertices of a static mesh are laid out.
	 * @param MeshVertexShader The shader that places them on the screen.
	 * @param SceneLayout The layout of the scene's data, which that shader reads.
	 * @param MaskFormat The format of the masks.
	 * @param OutputFormat The format of the surfaces that lines are drawn on.
	 * @return False if the graphics card could not create them.
	 */
	bool Initialize(std::shared_ptr<FRenderContext> InContext,
		nvrhi::IInputLayout* MeshLayout,
		nvrhi::IShader* MeshVertexShader,
		nvrhi::IBindingLayout* SceneLayout,
		nvrhi::Format MaskFormat,
		nvrhi::Format OutputFormat);

	/**
	 * Makes the bindings of a render target, unless it already has this object's.
	 *
	 * @param Mask The mask of the render target.
	 * @return False if the graphics card could not create the bindings.
	 */
	bool PrepareTarget(FOutlineTargetBindings& Bindings, nvrhi::ITexture* Mask) const;

	/**
	 * Returns the pipeline that marks a mesh in a mask. It draws a static mesh as the scene renderer's other
	 * pipelines do, with the scene's data as its only bindings, both sides of every triangle and no depth.
	 */
	nvrhi::IGraphicsPipeline* GetMaskPipeline() const
	{
		return MaskPipeline;
	}

	/**
	 * Draws the line around what a mask marks onto a surface, over what the surface shows.
	 *
	 * @param Bindings The bindings of the render target whose mask it is.
	 * @param Output Draws onto the surface.
	 * @param Area The whole of the surface.
	 */
	void Draw(nvrhi::ICommandList& CommandList,
		const FOutlineTargetBindings& Bindings,
		nvrhi::IFramebuffer* Output,
		const nvrhi::Viewport& Area,
		const FConstants& Constants) const;

private:
	std::shared_ptr<FRenderContext> Context;

	/** The pass that draws the line reads the mask. */
	nvrhi::BindingLayoutHandle MaskLayout;

	nvrhi::GraphicsPipelineHandle MaskPipeline;
	nvrhi::GraphicsPipelineHandle LinePipeline;
};
