#pragma once

#include "CoreMinimal.h"

#include "Math/Vector.h"

/** One corner of the triangles of a static mesh. */
struct FStaticMeshVertex
{
	FStaticMeshVertex() = default;

	FStaticMeshVertex(
		const FVector3f& InPosition, const FVector3f& InNormal, const FVector2f& InTextureCoordinate = FVector2f(0.0f))
		: Position(InPosition)
		, Normal(InNormal)
		, TextureCoordinate(InTextureCoordinate)
	{
	}

	/** Where the corner is, in the mesh's own space, in metres. */
	FVector3f Position = FVector3f(0.0f);

	/** The direction the surface faces at the corner. A unit vector. */
	FVector3f Normal = FVector3f(0.0f, 1.0f, 0.0f);

	/**
	 * Where on the material's textures the corner is. (0, 0) is the top left corner of a texture and (1, 1) its
	 * bottom right corner, as in glTF; values outside that range repeat or clamp as the texture says.
	 */
	FVector2f TextureCoordinate = FVector2f(0.0f);

	bool operator==(const FStaticMeshVertex& Other) const = default;
};
