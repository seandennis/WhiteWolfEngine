#pragma once

#include "CoreMinimal.h"

#include <nvrhi/nvrhi.h>

#include <array>

/**
 * What gives the ambient occlusion passes the textures of one render target to read. A render target keeps one of
 * these; the scene renderer that last drew to the target made it.
 */
struct FAmbientOcclusionTargetBindings
{
	/** The layout the bindings were made for, which says whose they are. Null until they are made. */
	nvrhi::IBindingLayout* Owner = nullptr;

	/** For the occlusion pass: the depth and the normals of the picture. */
	nvrhi::BindingSetHandle Input;

	/**
	 * For the two blur passes: the depth, the normals, and the occlusion texture each pass reads. The first reads
	 * the first occlusion texture and the second the second.
	 */
	std::array<nvrhi::BindingSetHandle, 2> Blur;
};
