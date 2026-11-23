#include "Rendering/StaticMeshComponent.h"

#include "Math/Transform.h"
#include "Math/Vector.h"
#include "Rendering/StaticMesh.h"
#include "Scene/Entity.h"

#include <initializer_list>

FBox WStaticMeshComponent::GetWorldBounds() const
{
	FBox WorldBounds;
	const WStaticMesh* const ShownMesh = Mesh.Get();
	const WEntity* const Entity = GetOwner();
	if (ShownMesh == nullptr || Entity == nullptr || ShownMesh->GetBounds().bIsEmpty)
	{
		return WorldBounds;
	}

	// Each corner of the mesh's box goes where the entity puts it, in double precision from there on.
	const FBox3f& MeshBounds = ShownMesh->GetBounds();
	const FTransform WorldTransform = Entity->GetWorldTransform();
	for (const float X : {MeshBounds.Min.x, MeshBounds.Max.x})
	{
		for (const float Y : {MeshBounds.Min.y, MeshBounds.Max.y})
		{
			for (const float Z : {MeshBounds.Min.z, MeshBounds.Max.z})
			{
				WorldBounds.Add(WorldTransform.TransformPosition(FVector(X, Y, Z)));
			}
		}
	}
	return WorldBounds;
}
