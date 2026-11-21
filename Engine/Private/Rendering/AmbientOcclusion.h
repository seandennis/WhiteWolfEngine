#pragma once

#include "CoreMinimal.h"

#include "Math/Matrix.h"
#include "Rendering/AmbientOcclusionTargetBindings.h"
#include "Rendering/RenderContext.h"

#include <glm/vec4.hpp>
#include <nvrhi/nvrhi.h>

#include <memory>

/**
 * Screen-space ambient occlusion: works out, from the depth and the normals of a picture, how much of its
 * surrounding each visible surface point can see, and smooths the result. Internal to the scene renderer, which
 * draws the depth and the normals first and dims the sky light with the result afterwards.
 */
class FAmbientOcclusion
{
public:
	/** What the passes are told about the picture. Must match FAmbientOcclusionConstants in AmbientOcclusionData.glsl.
	 */
	struct FConstants
	{
		/** Turns a camera-relative direction into the camera's own space. */
		FMatrix4f RelativeToView = FMatrix4f(1.0f);

		/**
		 * x: how far to the side the edge of the picture is per metre of distance, y: the same upwards, z: the
		 * near plane, w: how many pixels a metre covers at a distance of one metre.
		 */
		glm::vec4 Projection = glm::vec4(1.0f);

		/** x: the radius in metres, y: the strength, z and w: the width and height of the picture in pixels. */
		glm::vec4 Settings = glm::vec4(0.0f);

		/** x, y: the direction the blur pass works in. Filled in by Draw. */
		glm::vec4 Blur = glm::vec4(0.0f);
	};

	/**
	 * Creates the pipelines of the passes.
	 *
	 * @param OcclusionFormat The format of the textures the occlusion is drawn into.
	 * @param ShowFormat The format of the surface that Show draws on.
	 * @return False if the graphics card could not create them.
	 */
	bool Initialize(std::shared_ptr<FRenderContext> InContext, nvrhi::Format OcclusionFormat, nvrhi::Format ShowFormat);

	/**
	 * Makes the bindings of a render target, unless it already has this object's.
	 *
	 * @param FirstOcclusion The texture the result ends up in.
	 * @param SecondOcclusion The texture the blur uses in between.
	 * @return False if the graphics card could not create the bindings.
	 */
	bool PrepareTarget(FAmbientOcclusionTargetBindings& Bindings,
		nvrhi::ITexture* Depth,
		nvrhi::ITexture* Normals,
		nvrhi::ITexture* FirstOcclusion,
		nvrhi::ITexture* SecondOcclusion) const;

	/**
	 * Works out the ambient occlusion of a picture whose depth and normals have been drawn, and smooths it. The
	 * result is in the first occlusion texture.
	 *
	 * @param FirstTarget Draws into the first occlusion texture.
	 * @param SecondTarget Draws into the second.
	 */
	void Draw(nvrhi::ICommandList& CommandList,
		const FAmbientOcclusionTargetBindings& Bindings,
		nvrhi::IFramebuffer* FirstTarget,
		nvrhi::IFramebuffer* SecondTarget,
		const FConstants& Constants) const;

	/** Draws the ambient occlusion that Draw made as shades of grey onto a surface. */
	void Show(nvrhi::ICommandList& CommandList,
		const FAmbientOcclusionTargetBindings& Bindings,
		nvrhi::IFramebuffer* Output,
		const FConstants& Constants) const;

private:
	/** Draws one triangle over the whole of a target with a pipeline, bindings and constants. */
	static void DrawFullScreen(nvrhi::ICommandList& CommandList,
		nvrhi::IGraphicsPipeline* Pipeline,
		nvrhi::IBindingSet* BindingSet,
		nvrhi::IFramebuffer* Target,
		const FConstants& Constants);

	std::shared_ptr<FRenderContext> Context;

	/** The occlusion pass reads depth and normals; the blur and show passes read an occlusion texture as well. */
	nvrhi::BindingLayoutHandle InputLayout;
	nvrhi::BindingLayoutHandle BlurLayout;

	nvrhi::GraphicsPipelineHandle OcclusionPipeline;
	nvrhi::GraphicsPipelineHandle BlurPipeline;
	nvrhi::GraphicsPipelineHandle ShowPipeline;
};
