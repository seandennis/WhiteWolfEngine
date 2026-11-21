#pragma once

#include "CoreMinimal.h"

#include "Rendering/VulkanApi.h"

#include <string>

/**
 * The Vulkan loader: the shared library through which every Vulkan function is reached (vulkan-1.dll on Windows).
 * It is installed with graphics drivers, not with the engine, so it is loaded at run time and may be absent.
 */
class FVulkanLibrary
{
public:
	FVulkanLibrary() = default;
	~FVulkanLibrary();

	FVulkanLibrary(const FVulkanLibrary&) = delete;
	FVulkanLibrary& operator=(const FVulkanLibrary&) = delete;

	/**
	 * Loads the library. It must not be loaded already.
	 *
	 * @param OutError Receives the reason on failure: Vulkan is not installed on this machine.
	 * @return True if the library is loaded and GetInstanceProcAddr can be called.
	 */
	bool Load(std::string& OutError);

	/** Returns vkGetInstanceProcAddr, the function from which all other Vulkan functions are obtained. */
	PFN_vkGetInstanceProcAddr GetInstanceProcAddr() const;

private:
	/** The operating system's handle of the loaded library, or null. */
	void* Handle = nullptr;

	PFN_vkGetInstanceProcAddr InstanceProcAddr = nullptr;
};
