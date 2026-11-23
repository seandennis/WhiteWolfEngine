#pragma once

// GLFW's access to a window as the operating system's own window system knows it, which is needed for what GLFW
// does not do itself: handing a window to the renderer. On Windows this brings in windows.h, so it is included by
// the one source file that needs it and not by Platform/Glfw.h.
#include "Platform/Glfw.h"

#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif
