#pragma once

#include "Core/CoreTypes.h"

#include <glm/gtc/quaternion.hpp>

/**
 * A rotation, stored as a single-precision quaternion. Components are w, x, y and z.
 *
 * Construct one from components in the order (w, x, y, z); the identity rotation is FQuat(1, 0, 0, 0).
 */
using FQuat = glm::quat;
