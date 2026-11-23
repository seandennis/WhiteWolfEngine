#pragma once

#include "CoreMinimal.h"

#include "Math/Rotator.h"

#include <string>

/** Something that the Add menu makes: an entity, and what it has to begin with. */
struct FUiNewEntity
{
	/** What the entity is called. The step of the history that makes it is called "Add <Name>". */
	std::string Name;

	/** The class of the component that the entity is given; empty for an entity without components. */
	std::string ComponentClassName;

	/** For a WStaticMeshComponent, the asset path of the mesh that it shows; empty for none. */
	std::string MeshPath;

	/** How the entity is turned: a sun, for one, should not begin by shining along the ground. */
	FRotator Rotation;
};
