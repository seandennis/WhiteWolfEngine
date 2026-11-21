#pragma once

#include "Core/CoreTypes.h"

#include <glm/mat4x4.hpp>

/**
 * A 4 by 4 matrix in single precision, as the graphics card takes it.
 *
 * It is stored column by column: Matrix[Column][Row]. A matrix transforms a column vector written to its right,
 * so in A * B * Vector, B is applied first.
 *
 * Matrices are for the renderer. Positions in the world are FVector and transforms are FTransform; a matrix never
 * holds a position measured from the world origin, because single precision cannot.
 */
using FMatrix4f = glm::mat4;
