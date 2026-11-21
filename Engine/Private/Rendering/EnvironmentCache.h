#pragma once

#include "CoreMinimal.h"

#include "Rendering/EnvironmentResource.h"
#include "Rendering/RenderContext.h"

#include <nvrhi/nvrhi.h>

#include <memory>
#include <unordered_map>

class WEnvironmentMap;

/**
 * Keeps the environment maps that scenes are lit with on the graphics card. Internal to the scene renderer.
 *
 * An environment is copied the first time it is asked for and again when its picture changes, and released when
 * the environment itself is collected. Copying means writing the picture with smaller versions of itself, and
 * drawing a cube map whose levels show the environment as surfaces of increasing roughness mirror it.
 */
class FEnvironmentCache
{
public:
	/** The width of a face of a reflection cube map, and the number of its levels, one per step of roughness. */
	static constexpr int32 ReflectionSize = 256;
	static constexpr int32 ReflectionLevelCount = 6;

	/**
	 * Creates what every environment needs: the pipeline that makes reflections, the samplers, and the black
	 * environment that stands in when a scene has no sky light.
	 *
	 * @return False if the graphics card could not create them.
	 */
	bool Initialize(std::shared_ptr<FRenderContext> InContext);

	/**
	 * Returns the copy of an environment on the graphics card, creating it if it is not there or not current.
	 * An environment that is null, destroyed or without a picture gives the black environment.
	 *
	 * @return Null if the graphics card could not create the copy.
	 */
	const FEnvironmentResource* FindOrCreate(WEnvironmentMap* Environment);

	/** Returns the environment that gives no light. */
	const FEnvironmentResource& GetBlack() const
	{
		return Black;
	}

	/** Releases the copies of environments that no longer exist. */
	void ReleaseUnused();

	/**
	 * Writes the picture and makes the reflections of every environment that was created since the last call.
	 *
	 * @return How many environments were processed.
	 */
	int32 ProcessPending(nvrhi::ICommandList& CommandList);

	/** Returns how many environments have a copy on the graphics card. */
	int32 GetCount() const
	{
		return static_cast<int32>(Resources.size());
	}

	/** The sampler reflections are read with, and the one the picture is read with. */
	nvrhi::ISampler* GetReflectionsSampler() const
	{
		return ReflectionsSampler;
	}

	nvrhi::ISampler* GetPictureSampler() const
	{
		return PictureSampler;
	}

private:
	/** Creates the textures of an environment and what making its reflections needs. False on failure. */
	bool CreateResource(const FHdrImage& Image, FEnvironmentResource& OutResource) const;

	std::shared_ptr<FRenderContext> Context;

	nvrhi::BindingLayoutHandle PrefilterBindingLayout;
	nvrhi::GraphicsPipelineHandle PrefilterPipeline;
	nvrhi::SamplerHandle ReflectionsSampler;
	nvrhi::SamplerHandle PictureSampler;

	FEnvironmentResource Black;

	/** Keyed by address only: an entry is used only while its picture version is that of the environment there. */
	std::unordered_map<const WEnvironmentMap*, FEnvironmentResource> Resources;
};
