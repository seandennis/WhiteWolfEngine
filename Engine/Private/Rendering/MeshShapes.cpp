#include "Rendering/MeshShapes.h"

#include "Object/ObjectGlobals.h"
#include "Rendering/RenderDevice.h"
#include "Rendering/StaticMesh.h"
#include "Rendering/StaticMeshVertex.h"

#include <glm/geometric.hpp>

#include <cmath>
#include <cstddef>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

namespace
{
	bool IsPositive(float Value)
	{
		return std::isfinite(Value) && Value > 0.0f;
	}

	/**
	 * Appends a flat rectangle of four vertices and two triangles.
	 *
	 * @param Centre The middle of the rectangle.
	 * @param HalfRight From the middle to the middle of its right edge, as seen from the side it faces.
	 * @param HalfUp From the middle to the middle of its top edge. The rectangle faces along HalfRight x HalfUp.
	 */
	void AppendRectangle(const FVector3f& Centre,
		const FVector3f& HalfRight,
		const FVector3f& HalfUp,
		std::vector<FStaticMeshVertex>& Vertices,
		std::vector<uint32>& Indices)
	{
		const FVector3f Normal = glm::normalize(glm::cross(HalfRight, HalfUp));
		const uint32 First = static_cast<uint32>(Vertices.size());

		// Bottom left, bottom right, top right, top left; a texture's top left corner is its (0, 0).
		Vertices.emplace_back(Centre - HalfRight - HalfUp, Normal, FVector2f(0.0f, 1.0f));
		Vertices.emplace_back(Centre + HalfRight - HalfUp, Normal, FVector2f(1.0f, 1.0f));
		Vertices.emplace_back(Centre + HalfRight + HalfUp, Normal, FVector2f(1.0f, 0.0f));
		Vertices.emplace_back(Centre - HalfRight + HalfUp, Normal, FVector2f(0.0f, 0.0f));

		for (const uint32 Corner : {0u, 1u, 2u, 0u, 2u, 3u})
		{
			Indices.push_back(First + Corner);
		}
	}

	/** Makes a mesh of geometry that a shape function built, which is valid by construction. */
	WStaticMesh* MakeMesh(std::vector<FStaticMeshVertex> Vertices, std::vector<uint32> Indices)
	{
		WStaticMesh* const Mesh = NewObject<WStaticMesh>();
		std::string Error;
		const bool bAccepted = Mesh->SetGeometry(std::move(Vertices), std::move(Indices), Error);
		WW_CHECKF(bAccepted, "A shape was built with invalid geometry: {}", Error);
		return Mesh;
	}
}

WStaticMesh* FMeshShapes::CreateBox(const FVector3f& Size)
{
	if (!IsPositive(Size.x) || !IsPositive(Size.y) || !IsPositive(Size.z))
	{
		WW_LOG(LogRendering,
			Warning,
			"A box cannot be {} by {} by {} metres; each size must be greater than zero",
			Size.x,
			Size.y,
			Size.z);
		return nullptr;
	}

	const FVector3f Right(Size.x * 0.5f, 0.0f, 0.0f);
	const FVector3f Up(0.0f, Size.y * 0.5f, 0.0f);
	const FVector3f Front(0.0f, 0.0f, Size.z * 0.5f);

	// One rectangle per face, each given by its centre and what is right and up as seen from outside.
	std::vector<FStaticMeshVertex> Vertices;
	std::vector<uint32> Indices;
	AppendRectangle(Front, Right, Up, Vertices, Indices);
	AppendRectangle(-Front, -Right, Up, Vertices, Indices);
	AppendRectangle(Right, -Front, Up, Vertices, Indices);
	AppendRectangle(-Right, Front, Up, Vertices, Indices);
	AppendRectangle(Up, Right, -Front, Vertices, Indices);
	AppendRectangle(-Up, Right, Front, Vertices, Indices);
	return MakeMesh(std::move(Vertices), std::move(Indices));
}

WStaticMesh* FMeshShapes::CreateSphere(float Radius, int32 SegmentCount)
{
	const bool bIsValid = IsPositive(Radius) && SegmentCount >= MinSphereSegments && SegmentCount <= MaxSphereSegments;
	if (!bIsValid)
	{
		WW_LOG(LogRendering,
			Warning,
			"A sphere cannot have a radius of {} metres and {} segments; the radius must be greater than zero and the "
			"segments between {} and {}",
			Radius,
			SegmentCount,
			MinSphereSegments,
			MaxSphereSegments);
		return nullptr;
	}

	// Rings of vertices from the north pole to the south pole. The first and the last vertex of each ring are in
	// the same place, where the texture's left and right edges meet.
	const int32 RingCount = SegmentCount / 2;
	std::vector<FStaticMeshVertex> Vertices;
	Vertices.reserve(static_cast<std::size_t>((RingCount + 1) * (SegmentCount + 1)));
	for (int32 Ring = 0; Ring <= RingCount; ++Ring)
	{
		const float Down = static_cast<float>(Ring) / static_cast<float>(RingCount);
		const float Polar = Down * std::numbers::pi_v<float>;
		for (int32 Segment = 0; Segment <= SegmentCount; ++Segment)
		{
			// Around the sphere so that the texture reads from left to right as seen from outside.
			const float Around = static_cast<float>(Segment) / static_cast<float>(SegmentCount);
			const float Azimuth = Around * 2.0f * std::numbers::pi_v<float>;
			const FVector3f Normal(
				std::sin(Polar) * std::cos(Azimuth), std::cos(Polar), -std::sin(Polar) * std::sin(Azimuth));
			Vertices.emplace_back(Normal * Radius, Normal, FVector2f(Around, Down));
		}
	}

	std::vector<uint32> Indices;
	Indices.reserve(static_cast<std::size_t>(RingCount * SegmentCount * 6));
	for (int32 Ring = 0; Ring < RingCount; ++Ring)
	{
		for (int32 Segment = 0; Segment < SegmentCount; ++Segment)
		{
			const uint32 TopLeft = static_cast<uint32>(Ring * (SegmentCount + 1) + Segment);
			const uint32 TopRight = TopLeft + 1;
			const uint32 BottomLeft = TopLeft + static_cast<uint32>(SegmentCount + 1);
			const uint32 BottomRight = BottomLeft + 1;

			// At a pole the two top or the two bottom corners are one point, and that triangle has no area.
			if (Ring != RingCount - 1)
			{
				Indices.insert(Indices.end(), {TopLeft, BottomLeft, BottomRight});
			}
			if (Ring != 0)
			{
				Indices.insert(Indices.end(), {TopLeft, BottomRight, TopRight});
			}
		}
	}

	return MakeMesh(std::move(Vertices), std::move(Indices));
}

WStaticMesh* FMeshShapes::CreatePlane(float Width, float Depth)
{
	if (!IsPositive(Width) || !IsPositive(Depth))
	{
		WW_LOG(LogRendering,
			Warning,
			"A plane cannot be {} by {} metres; each size must be greater than zero",
			Width,
			Depth);
		return nullptr;
	}

	std::vector<FStaticMeshVertex> Vertices;
	std::vector<uint32> Indices;
	AppendRectangle(
		FVector3f(0.0f), FVector3f(Width * 0.5f, 0.0f, 0.0f), FVector3f(0.0f, 0.0f, -Depth * 0.5f), Vertices, Indices);
	return MakeMesh(std::move(Vertices), std::move(Indices));
}
