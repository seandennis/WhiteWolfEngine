#include "Rendering/StaticMesh.h"

#include "Math/Intersection.h"

#include <cmath>
#include <cstddef>
#include <format>
#include <utility>

namespace
{
	/** The version given to the geometry set last. Objects are used on the main thread only. */
	constinit uint64 LastGeometryVersion = 0;

	bool IsFinite(const FVector3f& Vector)
	{
		return std::isfinite(Vector.x) && std::isfinite(Vector.y) && std::isfinite(Vector.z);
	}

	bool IsFinite(const FVector2f& Vector)
	{
		return std::isfinite(Vector.x) && std::isfinite(Vector.y);
	}
}

bool WStaticMesh::SetGeometry(
	std::vector<FStaticMeshVertex> InVertices, std::vector<uint32> InIndices, std::string& OutError)
{
	constexpr std::size_t MaxCount = static_cast<std::size_t>(MaxElementCount);
	if (InVertices.empty() || InIndices.empty())
	{
		OutError = "a mesh needs at least one triangle";
		return false;
	}
	if (InVertices.size() > MaxCount || InIndices.size() > MaxCount)
	{
		OutError = std::format("a mesh cannot have {} vertices and {} indices; the most of either is {}",
			InVertices.size(),
			InIndices.size(),
			MaxElementCount);
		return false;
	}
	if (InIndices.size() % 3 != 0)
	{
		OutError =
			std::format("a mesh is made of triangles, so the number of indices must be a multiple of three, not {}",
				InIndices.size());
		return false;
	}

	for (std::size_t Position = 0; Position < InIndices.size(); ++Position)
	{
		if (InIndices[Position] >= InVertices.size())
		{
			OutError = std::format("index {} names vertex {}, but the mesh has only {} vertices",
				Position,
				InIndices[Position],
				InVertices.size());
			return false;
		}
	}

	for (std::size_t VertexIndex = 0; VertexIndex < InVertices.size(); ++VertexIndex)
	{
		const FStaticMeshVertex& Vertex = InVertices[VertexIndex];
		if (!IsFinite(Vertex.Position) || !IsFinite(Vertex.Normal) || !IsFinite(Vertex.TextureCoordinate))
		{
			OutError = std::format(
				"vertex {} has a position, normal or texture coordinate that is not a finite number", VertexIndex);
			return false;
		}
	}

	Vertices = std::move(InVertices);
	Indices = std::move(InIndices);
	Bounds = FBox3f();
	for (const FStaticMeshVertex& Vertex : Vertices)
	{
		Bounds.Add(Vertex.Position);
	}
	++LastGeometryVersion;
	GeometryVersion = LastGeometryVersion;
	return true;
}

bool WStaticMesh::HasGeometry() const
{
	return GeometryVersion != 0;
}

const std::vector<FStaticMeshVertex>& WStaticMesh::GetVertices() const
{
	return Vertices;
}

const std::vector<uint32>& WStaticMesh::GetIndices() const
{
	return Indices;
}

int32 WStaticMesh::GetTriangleCount() const
{
	return static_cast<int32>(Indices.size() / 3);
}

const FBox3f& WStaticMesh::GetBounds() const
{
	return Bounds;
}

std::optional<float> WStaticMesh::IntersectRay(const FVector3f& Origin, const FVector3f& Direction) const
{
	std::optional<float> Nearest;
	// Most rays miss most meshes, which the box of a mesh says without a look at its triangles.
	if (!FIntersection::RayBox(Origin, Direction, Bounds))
	{
		return Nearest;
	}

	for (std::size_t Corner = 0; Corner + 2 < Indices.size(); Corner += 3)
	{
		const std::optional<float> Distance = FIntersection::RayTriangle(Origin,
			Direction,
			Vertices[Indices[Corner]].Position,
			Vertices[Indices[Corner + 1]].Position,
			Vertices[Indices[Corner + 2]].Position);
		if (Distance.has_value() && (!Nearest.has_value() || *Distance < *Nearest))
		{
			Nearest = Distance;
		}
	}
	return Nearest;
}

uint64 WStaticMesh::GetGeometryVersion() const
{
	return GeometryVersion;
}
