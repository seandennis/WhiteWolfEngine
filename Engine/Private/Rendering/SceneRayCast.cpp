#include "Rendering/SceneRayCast.h"

#include "Math/Ray.h"
#include "Object/ObjectGlobals.h"
#include "Object/ObjectPtr.h"
#include "Rendering/StaticMeshComponent.h"
#include "Scene/Component.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"

std::optional<FSceneRayHit> FSceneRayCast::FindFirstHit(const WScene& Scene, const FRay& Ray)
{
	std::optional<FSceneRayHit> First;
	for (const WEntity* const Entity : Scene.GetAllEntities())
	{
		for (const TObjectPtr<WComponent>& Component : Entity->GetComponents())
		{
			const WStaticMeshComponent* const Mesh = Cast<WStaticMeshComponent>(Component.Get());
			if (!IsValid(Mesh))
			{
				continue;
			}

			const std::optional<double> Distance = Mesh->IntersectRay(Ray);
			if (Distance.has_value() && (!First.has_value() || *Distance < First->Distance))
			{
				First = FSceneRayHit{Mesh, *Distance};
			}
		}
	}
	return First;
}
