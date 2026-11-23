#pragma once

#include "CoreMinimal.h"

#include "Rendering/VulkanApi.h"

#include <string>

/**
 * The process's connection to Vulkan: the loader, the shared library through which every Vulkan function is
 * reached (vulkan-1.dll on Windows), and one Vulkan instance.
 *
 * The loader is installed with graphics drivers, not with the engine, so it is loaded at run time and may be
 * absent. It is loaded, and the instance created, when a render device first needs them, and both are kept until
 * the process ends: every render device of the process uses the same instance. Vulkan loads the graphics driver for
 * an instance and unloads it with the instance, and a driver starts threads and keeps memory of its own for as
 * long as it is loaded; nothing is gained by putting it through that for each render device, and a driver that is
 * unloaded while the process goes on takes with it what a leak check needs to see. See docs/DECISIONS.md, D-044.
 *
 * For the main thread.
 */
class FVulkanInstance
{
public:
	/** The Vulkan version the renderer needs: NVRHI uses dynamic rendering and synchronization2, both core in 1.3. */
	static constexpr uint32 RequiredApiVersion = VK_API_VERSION_1_3;

	/**
	 * Makes sure the loader is loaded and the instance exists. After a failure the next call tries again.
	 *
	 * @param OutError Receives the reason on failure: Vulkan is not installed on this machine, is older than
	 * RequiredApiVersion, or could not be started.
	 * @return True if GetHandle and GetInstanceProcAddr can be called.
	 */
	static bool Initialize(std::string& OutError);

	/** Returns the instance. Initialize must have succeeded. */
	static VkInstance GetHandle();

	/** Returns vkGetInstanceProcAddr, from which all other Vulkan functions are obtained. See GetHandle. */
	static PFN_vkGetInstanceProcAddr GetInstanceProcAddr();

	/**
	 * Returns true if pictures can be shown in the windows of the operating system. The instance is created with
	 * what Vulkan needs for that wherever it exists, because it is created once and before anyone asks for a
	 * window. Initialize must have succeeded.
	 */
	static bool CanShowInWindows();

	/**
	 * Returns true if pictures can be shown in windows that exist in memory only, which the platform layer creates
	 * in its headless mode. Not every graphics driver offers that. Initialize must have succeeded.
	 */
	static bool CanShowInHeadlessWindows();

	/** Returns a Vulkan version as text, for messages: "1.3". */
	static std::string FormatVersion(uint32 Version);
};
