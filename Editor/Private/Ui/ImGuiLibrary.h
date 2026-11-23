#pragma once

// The one place that includes Dear ImGui, the library that lays out the editor's user interface. No Dear ImGui type
// appears in a public header of the editor module.
//
// imgui_internal.h is included for the functions that build a layout of docked windows and that put bars along the
// edges of the main window, which the library has not made part of its public header.
#include <imgui.h>
#include <imgui_internal.h>
