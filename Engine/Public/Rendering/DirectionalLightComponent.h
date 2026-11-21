#pragma once

#include "CoreMinimal.h"

#include "Math/LinearColor.h"
#include "Object/ObjectMacros.h"
#include "Scene/Component.h"

/**
 * A light that is so far away that its rays are parallel and equally strong everywhere, like the sun.
 *
 * The light shines along its entity's -Z axis, the way the entity faces. Where the entity is does not matter.
 */
WCLASS()
class ENGINE_API WDirectionalLightComponent : public WComponent
{
	WW_DECLARE_CLASS(WDirectionalLightComponent, WComponent)

public:
	/** The colour of the light, in linear space. */
	WPROPERTY(BlueprintReadWrite, Category = "Light")
	FLinearColor Color = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

	/**
	 * How much light arrives at a surface that faces the light: its illuminance, in lux, as in glTF. The default,
	 * pi, lights a white matte surface that faces the light to a brightness of exactly one.
	 */
	WPROPERTY(BlueprintReadWrite, Category = "Light")
	float Intensity = 3.14159274f;

	/**
	 * Whether things that stand in the light cast shadows. Shadows reach as far from the camera as the renderer's
	 * shadow distance.
	 */
	WPROPERTY(BlueprintReadWrite, Category = "Light")
	bool bCastShadows = true;

	/** Whether the light shines at all. */
	WPROPERTY(BlueprintReadWrite, Category = "Light")
	bool bVisible = true;
};
