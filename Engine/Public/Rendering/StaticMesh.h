#pragma once

#include "CoreMinimal.h"

#include "Object/ObjectMacros.h"
#include "Rendering/StaticMeshVertex.h"

#include <string>
#include <vector>

/**
 * A shape made of triangles that does not change its form: a rock, a wall, a spaceship hull.
 *
 * A mesh is a list of vertices and a list of indices; every three indices name the corners of one triangle,
 * counter-clockwise as seen from its front. Positions are in the mesh's own space, in single precision; where the
 * mesh is in the world is decided by the entity that shows it.
 *
 * The mesh holds its geometry in main memory. The renderer copies it to the graphics card the first time it draws
 * the mesh, and again whenever the geometry is replaced.
 */
WCLASS()
class ENGINE_API WStaticMesh : public WObject
{
	WW_DECLARE_CLASS(WStaticMesh, WObject)

public:
	/** The largest number of vertices, and of indices, a mesh can have. */
	static constexpr int32 MaxElementCount = 100'000'000;

	/**
	 * Replaces the geometry of the mesh.
	 *
	 * The geometry is refused, and the mesh left as it was, unless there is at least one triangle, the number of
	 * indices is a multiple of three, every index names an existing vertex, every position, normal and texture
	 * coordinate is a finite number, and neither list is longer than MaxElementCount.
	 *
	 * @param OutError Receives the reason when the geometry is refused.
	 * @return True if the mesh now has the given geometry.
	 */
	bool SetGeometry(std::vector<FStaticMeshVertex> InVertices, std::vector<uint32> InIndices, std::string& OutError);

	/** Returns true once the mesh has been given geometry. A mesh without geometry is not drawn. */
	bool HasGeometry() const;

	const std::vector<FStaticMeshVertex>& GetVertices() const;

	/** Returns the indices; every three are the corners of one triangle. */
	const std::vector<uint32>& GetIndices() const;

	int32 GetTriangleCount() const;

	/**
	 * Returns a number that identifies the geometry the mesh has now. It changes every time SetGeometry succeeds
	 * and is never the same for two meshes, so whoever keeps a copy of the geometry, as the renderer does on the
	 * graphics card, can tell whether the copy is current. Zero means the mesh has no geometry.
	 */
	uint64 GetGeometryVersion() const;

private:
	std::vector<FStaticMeshVertex> Vertices;
	std::vector<uint32> Indices;
	uint64 GeometryVersion = 0;
};
