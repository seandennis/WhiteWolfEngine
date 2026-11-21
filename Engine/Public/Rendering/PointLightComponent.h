#pragma once

#include "CoreMinimal.h"

#include "Math/LinearColor.h"
#include "Object/ObjectMacros.h"
#include "Scene/Component.h"

/**
 * A light that shines equally in all directions from its entity's location, like a bare bulb. It gets weaker with
 * the square of the distance, as light does.
 */
WCLASS()
class ENGINE_API WPointLightComponent : public WComponent
{
	WW_DECLARE_CLASS(WPointLightComponent, WComponent)

public:
	/** The colour of the light, in linear space. */
	WPROPERTY(BlueprintReadWrite, Category = "Light")
	FLinearColor Color = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

	/**
	 * How strongly the light shines in each direction: its luminous intensity, in candela, as in glTF. A surface
	 * one metre away that faces the light receives this much light in lux; at two metres, a quarter of it.
	 */
	WPROPERTY(BlueprintReadWrite, Category = "Light")
	float Intensity = 3.14159274f;

	/**
	 * The distance, in metres, beyond which the light lights nothing; it fades out smoothly towards it. Zero means
	 * no limit. A limit lets the renderer skip the light for everything farther away.
	 */
	WPROPERTY(BlueprintReadWrite, Category = "Light")
	float Range = 0.0f;

	/** Whether the light shines at all. */
	WPROPERTY(BlueprintReadWrite, Category = "Light")
	bool bVisible = true;
};
