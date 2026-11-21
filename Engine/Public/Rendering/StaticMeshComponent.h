#pragma once

#include "CoreMinimal.h"

#include "Object/ObjectMacros.h"
#include "Object/ObjectPtr.h"
#include "Scene/Component.h"

class WMaterial;
class WStaticMesh;

/**
 * Makes its entity visible: the renderer draws the component's mesh at the entity's place in the world, with the
 * entity's rotation and scale.
 */
WCLASS()
class ENGINE_API WStaticMeshComponent : public WComponent
{
	WW_DECLARE_CLASS(WStaticMeshComponent, WComponent)

public:
	/** The shape to draw. Nothing is drawn while this is null or the mesh has no geometry. */
	WPROPERTY(BlueprintReadWrite, Category = "Rendering")
	TObjectPtr<WStaticMesh> Mesh;

	/** What the surface of the mesh looks like. While this is null the mesh is drawn plain white. */
	WPROPERTY(BlueprintReadWrite, Category = "Rendering")
	TObjectPtr<WMaterial> Material;

	/** Whether the mesh casts a shadow in the light of directional lights. An invisible mesh casts none. */
	WPROPERTY(BlueprintReadWrite, Category = "Rendering")
	bool bCastShadows = true;

	/** Whether the mesh is drawn at all. */
	WPROPERTY(BlueprintReadWrite, Category = "Rendering")
	bool bVisible = true;
};
