#pragma once

// The one place that includes GLFW. Left to itself, glfw3.h includes the system's OpenGL header, which the engine
// does not use and which a machine without OpenGL development files does not have.
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
