#pragma once

// The one place that includes Vulkan.
//
// NVRHI's Vulkan backend calls Vulkan through the "default dynamic dispatcher" of vulkan.hpp, a table of function
// pointers that the application has to define and fill in. The engine includes vulkan.hpp configured exactly as
// NVRHI does, so that both sides agree on that table, and otherwise calls plain C Vulkan functions through it.
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
