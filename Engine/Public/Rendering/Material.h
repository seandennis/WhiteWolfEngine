#pragma once

#include "CoreMinimal.h"

#include "Math/LinearColor.h"
#include "Object/ObjectMacros.h"

/**
 * What a surface looks like: the properties the renderer shades a mesh with.
 *
 * One material can be used by any number of meshes in any number of scenes; changing it changes all of them.
 */
WCLASS()
class ENGINE_API WMaterial : public WObject
{
	WW_DECLARE_CLASS(WMaterial, WObject)

public:
	/** The colour of the surface itself, in linear space. Alpha is not used yet: surfaces are opaque. */
	WPROPERTY(BlueprintReadWrite, Category = "Material")
	FLinearColor BaseColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
};
