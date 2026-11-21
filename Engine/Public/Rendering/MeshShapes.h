#pragma once

#include "CoreMinimal.h"

class WStaticMesh;

/**
 * Makes meshes of simple shapes: the box, sphere and plane that scenes are blocked out with and that tests draw.
 *
 * Every shape is centred on its mesh's origin, has normals that face outwards and texture coordinates that put a
 * texture upright on it as seen from outside. A new, unreferenced mesh is returned: hold it, or give it to a
 * component, before the next garbage collection.
 */
class FMeshShapes
{
public:
	/** The fewest and most segments a sphere can have around its equator. */
	static constexpr int32 MinSphereSegments = 4;
	static constexpr int32 MaxSphereSegments = 1024;

	/**
	 * Makes a box with flat faces, each showing the whole of a texture.
	 *
	 * @param Size The length of the box along X, Y and Z, in metres. Each must be greater than zero.
	 * @return The mesh, or null, with a warning in the log, if a size is not a positive finite number.
	 */
	ENGINE_API static WStaticMesh* CreateBox(const FVector3f& Size);

	/**
	 * Makes a sphere with smooth normals. A texture wraps around it once, its top edge at the north pole (+Y).
	 *
	 * @param Radius In metres. Must be greater than zero.
	 * @param SegmentCount How many segments the sphere has around its equator; it has half as many from pole to
	 * pole. More segments make it rounder. Must be between MinSphereSegments and MaxSphereSegments.
	 * @return The mesh, or null, with a warning in the log, if an argument is out of range.
	 */
	ENGINE_API static WStaticMesh* CreateSphere(float Radius, int32 SegmentCount = 32);

	/**
	 * Makes a flat rectangle that lies in the XZ plane and faces up (+Y), showing the whole of a texture with its
	 * top edge towards -Z.
	 *
	 * @param Width Its length along X, in metres. Must be greater than zero.
	 * @param Depth Its length along Z, in metres. Must be greater than zero.
	 * @return The mesh, or null, with a warning in the log, if a size is not a positive finite number.
	 */
	ENGINE_API static WStaticMesh* CreatePlane(float Width, float Depth);
};
