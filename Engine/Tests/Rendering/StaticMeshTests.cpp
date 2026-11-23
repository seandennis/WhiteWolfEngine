#include "Rendering/StaticMesh.h"

#include "Object/ObjectGlobals.h"
#include "Object/TestObjects.h"
#include "Rendering/MeshShapes.h"

#include <doctest/doctest.h>

#include <initializer_list>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace
{
	/** A triangle in the XY plane that faces +Z. */
	std::vector<FStaticMeshVertex> MakeTriangleVertices()
	{
		const FVector3f Normal(0.0f, 0.0f, 1.0f);
		return {
			{FVector3f(0.0f, 0.0f, 0.0f), Normal},
			{FVector3f(1.0f, 0.0f, 0.0f), Normal},
			{FVector3f(0.0f, 1.0f, 0.0f), Normal},
		};
	}
}

TEST_SUITE("WStaticMesh")
{
	TEST_CASE("A new mesh has no geometry")
	{
		const FScopedObjectTest ObjectTest;

		const WStaticMesh* const Mesh = NewObject<WStaticMesh>();

		CHECK_FALSE(Mesh->HasGeometry());
		CHECK(Mesh->GetVertices().empty());
		CHECK(Mesh->GetIndices().empty());
		CHECK(Mesh->GetTriangleCount() == 0);
		CHECK(Mesh->GetGeometryVersion() == 0);
		CHECK(Mesh->GetBounds().bIsEmpty);
	}

	TEST_CASE("A mesh keeps the geometry it is given")
	{
		const FScopedObjectTest ObjectTest;
		WStaticMesh* const Mesh = NewObject<WStaticMesh>();
		std::vector<FStaticMeshVertex> Vertices = MakeTriangleVertices();
		Vertices.push_back({FVector3f(1.0f, 1.0f, 0.0f), FVector3f(0.0f, 0.0f, 1.0f)});
		const std::vector<uint32> Indices{0, 1, 2, 2, 1, 3};
		std::string Error;

		REQUIRE(Mesh->SetGeometry(Vertices, Indices, Error));

		CHECK(Mesh->HasGeometry());
		CHECK(Mesh->GetVertices() == Vertices);
		CHECK(Mesh->GetIndices() == Indices);
		CHECK(Mesh->GetTriangleCount() == 2);
	}

	TEST_CASE("The bounds of a mesh are the box that holds its vertices")
	{
		const FScopedObjectTest ObjectTest;
		WStaticMesh* const Mesh = NewObject<WStaticMesh>();
		const FVector3f Normal(0.0f, 0.0f, 1.0f);
		std::string Error;

		REQUIRE(Mesh->SetGeometry({{FVector3f(-1.0f, 0.0f, 2.0f), Normal},
									  {FVector3f(3.0f, -4.0f, 2.0f), Normal},
									  {FVector3f(0.0f, 5.0f, -6.0f), Normal}},
			{0, 1, 2},
			Error));

		CHECK_FALSE(Mesh->GetBounds().bIsEmpty);
		CHECK(Mesh->GetBounds().Min == FVector3f(-1.0f, -4.0f, -6.0f));
		CHECK(Mesh->GetBounds().Max == FVector3f(3.0f, 5.0f, 2.0f));

		// New geometry has bounds of its own: nothing of the old ones is left.
		REQUIRE(Mesh->SetGeometry(MakeTriangleVertices(), {0, 1, 2}, Error));
		CHECK(Mesh->GetBounds().Min == FVector3f(0.0f, 0.0f, 0.0f));
		CHECK(Mesh->GetBounds().Max == FVector3f(1.0f, 1.0f, 0.0f));
	}
	TEST_CASE("The geometry version changes with every new geometry and differs between meshes")
	{
		const FScopedObjectTest ObjectTest;
		WStaticMesh* const First = NewObject<WStaticMesh>();
		WStaticMesh* const Second = NewObject<WStaticMesh>();
		std::string Error;

		REQUIRE(First->SetGeometry(MakeTriangleVertices(), {0, 1, 2}, Error));
		const uint64 FirstVersion = First->GetGeometryVersion();
		REQUIRE(Second->SetGeometry(MakeTriangleVertices(), {0, 1, 2}, Error));
		const uint64 SecondVersion = Second->GetGeometryVersion();
		REQUIRE(First->SetGeometry(MakeTriangleVertices(), {0, 1, 2}, Error));

		CHECK(FirstVersion != 0);
		CHECK(SecondVersion != 0);
		CHECK(SecondVersion != FirstVersion);
		CHECK(First->GetGeometryVersion() != FirstVersion);
		CHECK(First->GetGeometryVersion() != SecondVersion);
		CHECK(Second->GetGeometryVersion() == SecondVersion);
	}

	TEST_CASE("Geometry that is not a list of triangles is refused with the reason")
	{
		const FScopedObjectTest ObjectTest;
		WStaticMesh* const Mesh = NewObject<WStaticMesh>();
		std::string Error;

		CHECK_FALSE(Mesh->SetGeometry({}, {}, Error));
		CHECK(Error == "a mesh needs at least one triangle");

		CHECK_FALSE(Mesh->SetGeometry(MakeTriangleVertices(), {}, Error));
		CHECK(Error == "a mesh needs at least one triangle");

		CHECK_FALSE(Mesh->SetGeometry({}, {0, 1, 2}, Error));
		CHECK(Error == "a mesh needs at least one triangle");

		CHECK_FALSE(Mesh->SetGeometry(MakeTriangleVertices(), {0, 1, 2, 0}, Error));
		CHECK(Error == "a mesh is made of triangles, so the number of indices must be a multiple of three, not 4");

		CHECK_FALSE(Mesh->SetGeometry(MakeTriangleVertices(), {0, 1, 3}, Error));
		CHECK(Error == "index 2 names vertex 3, but the mesh has only 3 vertices");

		CHECK_FALSE(Mesh->HasGeometry());
	}

	TEST_CASE("Geometry with a position, normal or texture coordinate that is not a finite number is refused")
	{
		const FScopedObjectTest ObjectTest;
		WStaticMesh* const Mesh = NewObject<WStaticMesh>();
		std::string Error;

		std::vector<FStaticMeshVertex> Vertices = MakeTriangleVertices();
		Vertices[1].Position.y = std::numeric_limits<float>::infinity();
		CHECK_FALSE(Mesh->SetGeometry(Vertices, {0, 1, 2}, Error));
		CHECK(Error == "vertex 1 has a position, normal or texture coordinate that is not a finite number");

		Vertices = MakeTriangleVertices();
		Vertices[2].Normal.x = std::numeric_limits<float>::quiet_NaN();
		CHECK_FALSE(Mesh->SetGeometry(Vertices, {0, 1, 2}, Error));
		CHECK(Error == "vertex 2 has a position, normal or texture coordinate that is not a finite number");

		Vertices = MakeTriangleVertices();
		Vertices[0].TextureCoordinate.y = std::numeric_limits<float>::infinity();
		CHECK_FALSE(Mesh->SetGeometry(Vertices, {0, 1, 2}, Error));
		CHECK(Error == "vertex 0 has a position, normal or texture coordinate that is not a finite number");

		CHECK_FALSE(Mesh->HasGeometry());
	}

	TEST_CASE("Refused geometry leaves the mesh as it was")
	{
		const FScopedObjectTest ObjectTest;
		WStaticMesh* const Mesh = NewObject<WStaticMesh>();
		std::string Error;
		REQUIRE(Mesh->SetGeometry(MakeTriangleVertices(), {0, 1, 2}, Error));
		const uint64 Version = Mesh->GetGeometryVersion();
		const FBox3f Bounds = Mesh->GetBounds();

		CHECK_FALSE(Mesh->SetGeometry(MakeTriangleVertices(), {0, 1, 7}, Error));

		CHECK(Mesh->GetVertices() == MakeTriangleVertices());
		CHECK(Mesh->GetIndices() == std::vector<uint32>{0, 1, 2});
		CHECK(Mesh->GetGeometryVersion() == Version);
		CHECK(Mesh->GetBounds() == Bounds);
	}

	TEST_CASE("A ray meets the front of a mesh at its distance, in multiples of its direction")
	{
		const FScopedObjectTest ObjectTest;
		WStaticMesh* const Mesh = NewObject<WStaticMesh>();
		std::string Error;
		REQUIRE(Mesh->SetGeometry(MakeTriangleVertices(), {0, 1, 2}, Error));
		const FVector3f Origin(0.25f, 0.25f, 5.0f);

		const std::optional<float> Distance = Mesh->IntersectRay(Origin, FVector3f(0.0f, 0.0f, -1.0f));
		const std::optional<float> InLongerSteps = Mesh->IntersectRay(Origin, FVector3f(0.0f, 0.0f, -2.0f));

		REQUIRE(Distance.has_value());
		CHECK(*Distance == doctest::Approx(5.0f));
		REQUIRE(InLongerSteps.has_value());
		CHECK(*InLongerSteps == doctest::Approx(2.5f));
	}

	TEST_CASE("A ray that misses a mesh, or comes at it from behind, meets nothing")
	{
		const FScopedObjectTest ObjectTest;
		WStaticMesh* const Mesh = NewObject<WStaticMesh>();
		std::string Error;
		REQUIRE(Mesh->SetGeometry(MakeTriangleVertices(), {0, 1, 2}, Error));

		// Beside the triangle but through the box around it, and beside the box.
		CHECK_FALSE(Mesh->IntersectRay(FVector3f(0.9f, 0.9f, 5.0f), FVector3f(0.0f, 0.0f, -1.0f)).has_value());
		CHECK_FALSE(Mesh->IntersectRay(FVector3f(5.0f, 5.0f, 5.0f), FVector3f(0.0f, 0.0f, -1.0f)).has_value());
		// From behind.
		CHECK_FALSE(Mesh->IntersectRay(FVector3f(0.25f, 0.25f, -5.0f), FVector3f(0.0f, 0.0f, 1.0f)).has_value());
	}

	TEST_CASE("Of the triangles that a ray meets, the nearest counts, whatever their order in the mesh")
	{
		const FScopedObjectTest ObjectTest;
		const FVector3f Normal(0.0f, 0.0f, 1.0f);
		// Three sheets that face +Z, at the heights 0, 2 and 1 in that order.
		std::vector<FStaticMeshVertex> Vertices;
		for (const float Height : {0.0f, 2.0f, 1.0f})
		{
			Vertices.push_back({FVector3f(0.0f, 0.0f, Height), Normal});
			Vertices.push_back({FVector3f(1.0f, 0.0f, Height), Normal});
			Vertices.push_back({FVector3f(0.0f, 1.0f, Height), Normal});
		}
		WStaticMesh* const Mesh = NewObject<WStaticMesh>();
		std::string Error;
		REQUIRE(Mesh->SetGeometry(Vertices, {0, 1, 2, 3, 4, 5, 6, 7, 8}, Error));

		const std::optional<float> Distance =
			Mesh->IntersectRay(FVector3f(0.25f, 0.25f, 5.0f), FVector3f(0.0f, 0.0f, -1.0f));

		// The sheet at the height 2 is three below where the ray begins.
		REQUIRE(Distance.has_value());
		CHECK(*Distance == doctest::Approx(3.0f));
	}

	TEST_CASE("A ray that begins inside a closed mesh meets nothing: the insides of its triangles are not drawn")
	{
		const FScopedObjectTest ObjectTest;
		const WStaticMesh* const Box = FMeshShapes::CreateBox(FVector3f(2.0f));
		REQUIRE(Box != nullptr);

		for (const FVector3f& Direction :
			{FVector3f(1.0f, 0.0f, 0.0f), FVector3f(0.0f, -1.0f, 0.0f), FVector3f(0.3f, 0.5f, -0.8f)})
		{
			CHECK_FALSE(Box->IntersectRay(FVector3f(0.0f), Direction).has_value());
		}

		// From outside, the side that faces the ray is met, a metre from the middle of the box.
		const std::optional<float> Distance =
			Box->IntersectRay(FVector3f(0.3f, -0.2f, 5.0f), FVector3f(0.0f, 0.0f, -1.0f));
		REQUIRE(Distance.has_value());
		CHECK(*Distance == doctest::Approx(4.0f));
	}

	TEST_CASE("A mesh without geometry is met by nothing")
	{
		const FScopedObjectTest ObjectTest;
		const WStaticMesh* const Mesh = NewObject<WStaticMesh>();

		CHECK_FALSE(Mesh->IntersectRay(FVector3f(0.0f, 0.0f, 5.0f), FVector3f(0.0f, 0.0f, -1.0f)).has_value());
	}
}
