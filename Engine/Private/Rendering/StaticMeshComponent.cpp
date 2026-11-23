#include "Rendering/StaticMeshComponent.h"

#include "Math/Ray.h"
#include "Math/Transform.h"
#include "Math/Vector.h"
#include "Object/ObjectGlobals.h"
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

std::optional<double> WStaticMeshComponent::IntersectRay(const FRay& Ray) const
{
	// A mesh that has been destroyed is no longer drawn, though it is there until it is collected.
	const WStaticMesh* const ShownMesh = Mesh.Get();
	const WEntity* const Entity = GetOwner();
	if (!bVisible || !IsValid(ShownMesh) || Entity == nullptr)
	{
		return std::nullopt;
	}

	// A mesh that is scaled to nothing along an axis is flat, and a ray cannot be taken into its space.
	const FTransform WorldTransform = Entity->GetWorldTransform();
	if (!WorldTransform.IsInvertible())
	{
		return std::nullopt;
	}

	// The ray goes to the mesh rather than the mesh to the ray. Where it begins is measured from the entity in
	// double precision, so that it is exact however far both are from the world origin. Its direction is no
	// longer of unit length there, and stands for a metre of the world all the same: a multiple of it in the space
	// of the mesh is that many metres along the ray.
	const FVector3f LocalOrigin = FVector3f(WorldTransform.InverseTransformPosition(Ray.Origin));
	const FVector3f LocalDirection = WorldTransform.InverseTransformVector(Ray.Direction);
	const std::optional<float> Distance = ShownMesh->IntersectRay(LocalOrigin, LocalDirection);
	if (!Distance.has_value())
	{
		return std::nullopt;
	}
	return static_cast<double>(*Distance);
}
