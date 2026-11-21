#pragma once

#include "CoreMinimal.h"

#include "Math/LinearColor.h"
#include "Math/Matrix.h"

/**
 * What changes from one drawn mesh to the next, as the shaders receive it.
 * The layout must match FDrawConstants in Engine/Shaders/DrawConstants.glsl.
 */
struct FDrawConstants
{
	/** Takes a position in the mesh's own space to clip space. */
	FMatrix4f LocalToClip = FMatrix4f(1.0f);

	/** The base colour of the material, in linear space. */
	FLinearColor BaseColor;
};

static_assert(sizeof(FDrawConstants) == 80, "FDrawConstants must be laid out as the shaders declare it");
