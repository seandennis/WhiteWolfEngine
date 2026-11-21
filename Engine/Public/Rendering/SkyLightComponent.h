#pragma once

#include "CoreMinimal.h"

#include "Object/ObjectMacros.h"
#include "Object/ObjectPtr.h"
#include "Scene/Component.h"

class WEnvironmentMap;

/**
 * Lights a scene from all around with the light of an environment map, and shows that environment behind the
 * scene as its sky. This is what fills the shadows and gives metals something to reflect.
 *
 * Where the entity is does not matter. Its rotation turns the environment: an entity turned to the left turns the
 * whole sky to the left. A scene is lit by one sky light, the first that is visible and has an environment with a
 * picture.
 */
WCLASS()
class ENGINE_API WSkyLightComponent : public WComponent
{
	WW_DECLARE_CLASS(WSkyLightComponent, WComponent)

public:
	/** The surrounding whose light this is. Nothing is lit while this is null or has no picture. */
	WPROPERTY(BlueprintReadWrite, Category = "Light")
	TObjectPtr<WEnvironmentMap> Environment;

	/** Multiplies the light of the environment, for the scene and for the sky that is shown. */
	WPROPERTY(BlueprintReadWrite, Category = "Light")
	float Intensity = 1.0f;

	/** Whether the light shines and the sky is shown at all. */
	WPROPERTY(BlueprintReadWrite, Category = "Light")
	bool bVisible = true;
};
