#include "Rendering/VulkanInstance.h"

#include <nvrhi/vulkan.h>

#include <format>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace
{
#if defined(_WIN32)
	constexpr const char* LibraryName = "vulkan-1.dll";
#else
	constexpr const char* LibraryName = "libvulkan.so.1";
#endif

	constexpr const char* EntryPointName = "vkGetInstanceProcAddr";

	/**
	 * What the process holds of Vulkan. None of it is released: the operating system takes the library and the
	 * instance back when the process ends.
	 */
	struct FVulkanInstanceState
	{
		/** The operating system's handle of the loader library, or null while it is not loaded. */
		void* Library = nullptr;

		PFN_vkGetInstanceProcAddr GetInstanceProcAddr = nullptr;
		VkInstance Instance = VK_NULL_HANDLE;
	};

	constinit FVulkanInstanceState State;

	/** Loads the loader library and finds its entry point. */
	bool LoadLibraryOnce(std::string& OutError)
	{
		if (State.GetInstanceProcAddr != nullptr)
		{
			return true;
		}

		if (State.Library == nullptr)
		{
#if defined(_WIN32)
			State.Library = LoadLibraryA(LibraryName);
#else
			State.Library = dlopen(LibraryName, RTLD_NOW | RTLD_LOCAL);
#endif
		}
		if (State.Library == nullptr)
		{
			OutError = std::format(
				"Vulkan is not installed on this machine: {} could not be loaded. It comes with the graphics driver",
				LibraryName);
			return false;
		}

#if defined(_WIN32)
		void* const EntryPoint =
			reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(State.Library), EntryPointName));
#else
		void* const EntryPoint = dlsym(State.Library, EntryPointName);
#endif
		if (EntryPoint == nullptr)
		{
			OutError = std::format("{} does not provide {}", LibraryName, EntryPointName);
			return false;
		}

		State.GetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(EntryPoint);
		return true;
	}
}

bool FVulkanInstance::Initialize(std::string& OutError)
{
	if (State.Instance != VK_NULL_HANDLE)
	{
		return true;
	}
	if (!LoadLibraryOnce(OutError))
	{
		return false;
	}

	// The functions that exist before there is an instance are asked for with a null instance.
	// vkEnumerateInstanceVersion does not exist in Vulkan 1.0, which is how 1.0 is recognised.
	uint32 LoaderVersion = VK_API_VERSION_1_0;
	const PFN_vkEnumerateInstanceVersion EnumerateInstanceVersion = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
		State.GetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"));
	if (EnumerateInstanceVersion != nullptr)
	{
		EnumerateInstanceVersion(&LoaderVersion);
	}
	if (LoaderVersion < RequiredApiVersion)
	{
		OutError = std::format("the installed Vulkan is version {}, but version {} or later is needed",
			FormatVersion(LoaderVersion),
			FormatVersion(RequiredApiVersion));
		return false;
	}

	const PFN_vkCreateInstance CreateInstance =
		reinterpret_cast<PFN_vkCreateInstance>(State.GetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance"));
	if (CreateInstance == nullptr)
	{
		OutError = std::format("{} does not provide vkCreateInstance", LibraryName);
		return false;
	}

	VkApplicationInfo ApplicationInfo{};
	ApplicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	ApplicationInfo.pApplicationName = "White Wolf Engine";
	ApplicationInfo.pEngineName = "White Wolf Engine";
	ApplicationInfo.apiVersion = RequiredApiVersion;

	VkInstanceCreateInfo CreateInfo{};
	CreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	CreateInfo.pApplicationInfo = &ApplicationInfo;

	VkInstance NewInstance = VK_NULL_HANDLE;
	const VkResult Result = CreateInstance(&CreateInfo, nullptr, &NewInstance);
	if (Result != VK_SUCCESS)
	{
		OutError = std::format("Vulkan could not be started: {}", nvrhi::vulkan::resultToString(Result));
		return false;
	}

	State.Instance = NewInstance;
	return true;
}

VkInstance FVulkanInstance::GetHandle()
{
	WW_CHECKF(State.Instance != VK_NULL_HANDLE, "Vulkan has not been started");
	return State.Instance;
}

PFN_vkGetInstanceProcAddr FVulkanInstance::GetInstanceProcAddr()
{
	WW_CHECKF(State.GetInstanceProcAddr != nullptr, "The Vulkan library has not been loaded");
	return State.GetInstanceProcAddr;
}

std::string FVulkanInstance::FormatVersion(uint32 Version)
{
	return std::format("{}.{}", VK_API_VERSION_MAJOR(Version), VK_API_VERSION_MINOR(Version));
}
