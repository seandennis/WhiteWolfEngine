#pragma once

#include "CoreMinimal.h"

#include "Object/WeakObjectPtr.h"
#include "Rendering/StaticMesh.h"

#include <nvrhi/nvrhi.h>

/** The copy of a static mesh's geometry on the graphics card. */
struct FStaticMeshBuffers
{
	/** The mesh this is a copy of. Null once the mesh is gone, which is when the copy is released. */
	TWeakObjectPtr<WStaticMesh> Mesh;

	/** The version of the geometry that was copied; see WStaticMesh::GetGeometryVersion. */
	uint64 GeometryVersion = 0;

	nvrhi::BufferHandle VertexBuffer;
	nvrhi::BufferHandle IndexBuffer;
	uint32 IndexCount = 0;
};
