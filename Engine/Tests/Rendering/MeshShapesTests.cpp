#include "Rendering/MeshShapes.h"

#include "Object/TestObjects.h"
#include "Rendering/StaticMesh.h"
#include "Rendering/StaticMeshVertex.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace
{
	/** The smallest box around a mesh's vertices. */
	struct FBounds
	{
		FVector3f Min = FVector3f(std::numeric_limits<float>::max());
		FVector3f Max = FVector3f(std::numeric_limits<float>::lowest());
	};

	FBounds GetBounds(const WStaticMesh& Mesh)
	{
		FBounds Bounds;
		for (const FStaticMeshVertex& Vertex : Mesh.GetVertices())
		{
			Bounds.Min = glm::min(Bounds.Min, Vertex.Position);
			Bounds.Max = glm::max(Bounds.Max, Vertex.Position);
		}
		return Bounds;
	}

	bool IsNearVector(const FVector3f& Actual, const FVector3f& Expected, float Tolerance = 1.0e-5f)
	{
		return glm::length(Actual - Expected) <= Tolerance;
	}

	/**
	 * Checks what every shape promises: unit normals, texture coordinates from 0 to 1, and triangles that have
	 * area and whose front, where the corners run counter-clockwise, is the side their normals point to.
	 */
	void CheckShapeIsWellFormed(const WStaticMesh& Mesh)
	{
		const std::vector<FStaticMeshVertex>& Vertices = Mesh.GetVertices();
		const std::vector<uint32>& Indices = Mesh.GetIndices();

		int32 BadVertexCount = 0;
		for (const FStaticMeshVertex& Vertex : Vertices)
		{
			const bool bHasUnitNormal = std::abs(glm::length(Vertex.Normal) - 1.0f) < 1.0e-4f;
			const bool bHasCoordinateInRange = Vertex.TextureCoordinate.x >= 0.0f &&
				Vertex.TextureCoordinate.x <= 1.0f && Vertex.TextureCoordinate.y >= 0.0f &&
				Vertex.TextureCoordinate.y <= 1.0f;
			BadVertexCount += bHasUnitNormal && bHasCoordinateInRange ? 0 : 1;
		}
		CHECK(BadVertexCount == 0);

		int32 BadTriangleCount = 0;
		for (std::size_t First = 0; First < Indices.size(); First += 3)
		{
			const FStaticMeshVertex& A = Vertices[Indices[First]];
			const FStaticMeshVertex& B = Vertices[Indices[First + 1]];
			const FStaticMeshVertex& C = Vertices[Indices[First + 2]];
			const FVector3f Facing = glm::cross(B.Position - A.Position, C.Position - A.Position);
			const bool bHasArea = glm::length(Facing) > 1.0e-8f;
			const bool bFacesItsNormals = glm::dot(Facing, A.Normal) > 0.0f && glm::dot(Facing, B.Normal) > 0.0f &&
				glm::dot(Facing, C.Normal) > 0.0f;
			BadTriangleCount += bHasArea && bFacesItsNormals ? 0 : 1;
		}
		CHECK(BadTriangleCount == 0);
	}

	/** Checks that the normals of a closed shape around its origin point away from the origin. */
	void CheckNormalsPointOutwards(const WStaticMesh& Mesh)
	{
		int32 InwardCount = 0;
		for (const FStaticMeshVertex& Vertex : Mesh.GetVertices())
		{
			InwardCount += glm::dot(Vertex.Normal, Vertex.Position) > 0.0f ? 0 : 1;
		}
		CHECK(InwardCount == 0);
	}
}

TEST_SUITE("FMeshShapes")
{
	TEST_CASE("A box has the size it was asked for, six flat faces and normals that point outwards")
	{
		const FScopedObjectTest ObjectTest;

		const WStaticMesh* const Box = FMeshShapes::CreateBox(FVector3f(2.0f, 4.0f, 6.0f));

		REQUIRE(Box != nullptr);
		CHECK(Box->GetVertices().size() == 24);
		CHECK(Box->GetTriangleCount() == 12);
		const FBounds Bounds = GetBounds(*Box);
		CHECK(Bounds.Min == FVector3f(-1.0f, -2.0f, -3.0f));
		CHECK(Bounds.Max == FVector3f(1.0f, 2.0f, 3.0f));
		CheckShapeIsWellFormed(*Box);
		CheckNormalsPointOutwards(*Box);

		// Every normal is along an axis, and each of the six directions has its four vertices.
		for (const FVector3f& Direction : {FVector3f(1.0f, 0.0f, 0.0f),
				 FVector3f(-1.0f, 0.0f, 0.0f),
				 FVector3f(0.0f, 1.0f, 0.0f),
				 FVector3f(0.0f, -1.0f, 0.0f),
				 FVector3f(0.0f, 0.0f, 1.0f),
				 FVector3f(0.0f, 0.0f, -1.0f)})
		{
			const std::ptrdiff_t Count = std::ranges::count_if(Box->GetVertices(),
				[&Direction](const FStaticMeshVertex& Vertex) { return IsNearVector(Vertex.Normal, Direction); });
			CHECK(Count == 4);
		}
	}

	TEST_CASE("A texture stands upright on the sides of a box, as seen from outside")
	{
		const FScopedObjectTest ObjectTest;
		const WStaticMesh* const Box = FMeshShapes::CreateBox(FVector3f(2.0f));

		// On the face towards +Z, the top left corner of the texture is at the top left as seen from +Z.
		// On the face towards +X, seen from +X, left is towards +Z.
		int32 FoundCount = 0;
		for (const FStaticMeshVertex& Vertex : Box->GetVertices())
		{
			const bool bIsTopLeftOfTexture = Vertex.TextureCoordinate == FVector2f(0.0f, 0.0f);
			if (bIsTopLeftOfTexture && IsNearVector(Vertex.Normal, FVector3f(0.0f, 0.0f, 1.0f)))
			{
				CHECK(Vertex.Position == FVector3f(-1.0f, 1.0f, 1.0f));
				++FoundCount;
			}
			if (bIsTopLeftOfTexture && IsNearVector(Vertex.Normal, FVector3f(1.0f, 0.0f, 0.0f)))
			{
				CHECK(Vertex.Position == FVector3f(1.0f, 1.0f, 1.0f));
				++FoundCount;
			}
		}
		CHECK(FoundCount == 2);
	}

	TEST_CASE("A sphere has the radius it was asked for and normals that point straight outwards")
	{
		const FScopedObjectTest ObjectTest;

		const WStaticMesh* const Sphere = FMeshShapes::CreateSphere(2.5f, 16);

		REQUIRE(Sphere != nullptr);
		// 16 segments around and 8 from pole to pole: 9 rings of 17 vertices, and two triangles for each of the
		// 8 by 16 patches except at the poles, where a patch is a single triangle.
		CHECK(Sphere->GetVertices().size() == 9 * 17);
		CHECK(Sphere->GetTriangleCount() == 2 * 8 * 16 - 2 * 16);
		CheckShapeIsWellFormed(*Sphere);

		int32 BadVertexCount = 0;
		for (const FStaticMeshVertex& Vertex : Sphere->GetVertices())
		{
			const bool bIsOnSphere = std::abs(glm::length(Vertex.Position) - 2.5f) < 1.0e-4f;
			const bool bFacesOutwards = IsNearVector(Vertex.Normal * 2.5f, Vertex.Position, 1.0e-4f);
			BadVertexCount += bIsOnSphere && bFacesOutwards ? 0 : 1;
		}
		CHECK(BadVertexCount == 0);

		const FBounds Bounds = GetBounds(*Sphere);
		CHECK(IsNearVector(Bounds.Min, FVector3f(-2.5f), 1.0e-4f));
		CHECK(IsNearVector(Bounds.Max, FVector3f(2.5f), 1.0e-4f));
	}

	TEST_CASE("A texture wraps around a sphere from left to right with its top at the north pole")
	{
		const FScopedObjectTest ObjectTest;
		const WStaticMesh* const Sphere = FMeshShapes::CreateSphere(1.0f, 8);

		for (const FStaticMeshVertex& Vertex : Sphere->GetVertices())
		{
			// The top edge of the texture is the north pole and its bottom edge the south pole.
			if (Vertex.TextureCoordinate.y == 0.0f)
			{
				CHECK(IsNearVector(Vertex.Position, FVector3f(0.0f, 1.0f, 0.0f)));
			}
			if (Vertex.TextureCoordinate.y == 1.0f)
			{
				CHECK(IsNearVector(Vertex.Position, FVector3f(0.0f, -1.0f, 0.0f)));
			}
			// Half way down, three quarters of the way across the texture faces +Z, and the texture continues to
			// the right of it, towards +X, as seen from there.
			if (Vertex.TextureCoordinate == FVector2f(0.75f, 0.5f))
			{
				CHECK(IsNearVector(Vertex.Position, FVector3f(0.0f, 0.0f, 1.0f)));
			}
			if (Vertex.TextureCoordinate == FVector2f(1.0f, 0.5f))
			{
				CHECK(IsNearVector(Vertex.Position, FVector3f(1.0f, 0.0f, 0.0f)));
			}
		}
	}

	TEST_CASE("More segments make a sphere of more triangles")
	{
		const FScopedObjectTest ObjectTest;

		const WStaticMesh* const Coarse = FMeshShapes::CreateSphere(1.0f, FMeshShapes::MinSphereSegments);
		const WStaticMesh* const Default = FMeshShapes::CreateSphere(1.0f);
		const WStaticMesh* const Odd = FMeshShapes::CreateSphere(1.0f, 7);

		REQUIRE(Coarse != nullptr);
		REQUIRE(Default != nullptr);
		REQUIRE(Odd != nullptr);
		CHECK(Coarse->GetTriangleCount() == 8);
		CHECK(Default->GetTriangleCount() == 2 * 16 * 32 - 2 * 32);
		CheckShapeIsWellFormed(*Coarse);
		CheckShapeIsWellFormed(*Default);
		CheckShapeIsWellFormed(*Odd);
	}

	TEST_CASE("A plane lies flat, faces up and shows a texture with its top edge away from the viewer")
	{
		const FScopedObjectTest ObjectTest;

		const WStaticMesh* const Plane = FMeshShapes::CreatePlane(4.0f, 2.0f);

		REQUIRE(Plane != nullptr);
		CHECK(Plane->GetVertices().size() == 4);
		CHECK(Plane->GetTriangleCount() == 2);
		CheckShapeIsWellFormed(*Plane);
		const FBounds Bounds = GetBounds(*Plane);
		CHECK(Bounds.Min == FVector3f(-2.0f, 0.0f, -1.0f));
		CHECK(Bounds.Max == FVector3f(2.0f, 0.0f, 1.0f));
		for (const FStaticMeshVertex& Vertex : Plane->GetVertices())
		{
			CHECK(IsNearVector(Vertex.Normal, FVector3f(0.0f, 1.0f, 0.0f)));
			if (Vertex.TextureCoordinate == FVector2f(0.0f, 0.0f))
			{
				CHECK(Vertex.Position == FVector3f(-2.0f, 0.0f, -1.0f));
			}
		}
	}

	TEST_CASE("Shapes of impossible sizes are refused with a warning")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedLogCapture Capture;
		const float Infinity = std::numeric_limits<float>::infinity();
		const float NotANumber = std::numeric_limits<float>::quiet_NaN();

		CHECK(FMeshShapes::CreateBox(FVector3f(0.0f, 1.0f, 1.0f)) == nullptr);
		CHECK(FMeshShapes::CreateBox(FVector3f(1.0f, -1.0f, 1.0f)) == nullptr);
		CHECK(FMeshShapes::CreateBox(FVector3f(1.0f, 1.0f, Infinity)) == nullptr);
		CHECK(FMeshShapes::CreateSphere(0.0f) == nullptr);
		CHECK(FMeshShapes::CreateSphere(NotANumber) == nullptr);
		CHECK(FMeshShapes::CreateSphere(1.0f, FMeshShapes::MinSphereSegments - 1) == nullptr);
		CHECK(FMeshShapes::CreateSphere(1.0f, FMeshShapes::MaxSphereSegments + 1) == nullptr);
		CHECK(FMeshShapes::CreatePlane(0.0f, 1.0f) == nullptr);
		CHECK(FMeshShapes::CreatePlane(1.0f, -2.0f) == nullptr);

		CHECK(Capture.GetMessages().size() == 9);
	}
}
