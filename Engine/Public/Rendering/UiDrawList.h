#pragma once

#include "CoreMinimal.h"

#include "Rendering/UiDrawCommand.h"
#include "Rendering/UiVertex.h"

#include <vector>

/**
 * What a user interface looks like at one moment, as the triangles to draw: flat shapes in the pixels of a surface,
 * in colours, covered with pictures, clipped to rectangles. Whatever lays out a user interface produces one of
 * these for every picture of it, and FUiRenderer draws it.
 *
 * The commands are drawn in order, each over what the ones before it drew.
 */
struct FUiDrawList
{
	std::vector<FUiVertex> Vertices;

	/** Every three are the corners of one triangle, each a place in Vertices once its command's offset is added. */
	std::vector<uint32> Indices;

	std::vector<FUiDrawCommand> Commands;
};
