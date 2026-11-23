#pragma once

#include "CoreMinimal.h"

#include <nvrhi/nvrhi.h>

/**
 * What gives the pass that draws outlines the mask of one render target to read. A render target keeps one of
 * these; the scene renderer that last drew outlines onto the target made it.
 */
struct FOutlineTargetBindings
{
	/** The layout the bindings were made for, which says whose they are. Null until they are made. */
	nvrhi::IBindingLayout* Owner = nullptr;

	/** The mask of the render target. */
	nvrhi::BindingSetHandle Mask;
};
