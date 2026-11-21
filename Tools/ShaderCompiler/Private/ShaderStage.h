#pragma once

#include "Core/CoreTypes.h"

/** The part of the graphics pipeline a shader runs in. */
enum class EShaderStage : uint8
{
	/** Runs once per vertex and places it on the screen. Source files end in ".vert". */
	Vertex,

	/** Runs once per pixel and gives it its colour. Source files end in ".frag". */
	Fragment,
};
