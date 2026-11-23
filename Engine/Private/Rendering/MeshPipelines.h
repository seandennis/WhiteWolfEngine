#pragma once

#include "CoreMinimal.h"

#include <nvrhi/nvrhi.h>

/**
 * The two pipelines that draw meshes in one way: one for a mesh as it was made, and one for a mesh that its
 * entity mirrors.
 *
 * A scale that is below zero along one axis, or along all three, turns a mesh into its mirror image. Seen from
 * the front of a triangle, its corners then run clockwise instead of counter-clockwise. A pipeline takes one of
 * the two for the front and leaves the backs out, so each needs a pipeline of its own; otherwise a mirrored mesh
 * would show its inside.
 */
struct FMeshPipelines
{
	/** For meshes whose triangles have their corners counter-clockwise, as every mesh is made. */
	nvrhi::GraphicsPipelineHandle AsMade;

	/** For meshes that are mirrored, whose triangles have them clockwise. */
	nvrhi::GraphicsPipelineHandle Mirrored;

	/** Returns true if both pipelines were created. */
	bool IsComplete() const
	{
		return AsMade != nullptr && Mirrored != nullptr;
	}
};
