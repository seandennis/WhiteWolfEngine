#pragma once

#include "CoreMinimal.h"

#include "Object/WeakObjectPtr.h"
#include "Rendering/EnvironmentMap.h"

#include <nvrhi/nvrhi.h>

#include <vector>

/** An environment map on the graphics card, in the two forms the renderer reads it in. */
struct FEnvironmentResource
{
	/** The environment this is a copy of. Null once it is gone, which is when the copy is released. */
	TWeakObjectPtr<WEnvironmentMap> Environment;

	/** The version of the picture that was copied; see WEnvironmentMap::GetImageVersion. */
	uint64 ImageVersion = 0;

	/** The environment's picture, with smaller versions. The sky is drawn from it. */
	nvrhi::TextureHandle Picture;

	/** How many pixels the picture has, and how high it is. */
	float PicturePixelCount = 1.0f;
	int32 PictureHeight = 1;

	/**
	 * A cube map of the environment whose levels are blurred the way ever rougher surfaces mirror it. Reflections
	 * are read from it.
	 */
	nvrhi::TextureHandle Reflections;

	/** True until the picture has been written and the reflections made, which the next drawn picture does. */
	bool bNeedsUpload = true;

	/** What making the reflections needs, kept only until that is done: one target per face of each level. */
	std::vector<nvrhi::FramebufferHandle> ReflectionTargets;
	nvrhi::BindingSetHandle PictureBindings;
};
