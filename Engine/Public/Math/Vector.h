#pragma once

#include "Core/CoreTypes.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/**
 * A position in world space, in double precision.
 *
 * Every world-space position in the engine uses this type, so that worlds can be very large without losing
 * precision far from the origin. Components are x, y and z.
 */
using FVector = glm::dvec3;

/**
 * A direction, scale or mesh-local position, in single precision. Components are x, y and z.
 *
 * Use FVector, not this type, for anything measured from the world origin.
 */
using FVector3f = glm::vec3;

/** A position or size in two dimensions, in double precision: a point in a window, a scroll amount. Components are x
 * and y. */
using FVector2D = glm::dvec2;
