#include "Rendering/VulkanInstance.h"

#include <nvrhi/vulkan.h>

#include <algorithm>
#include <format>
#include <string_view>
#include <vector>

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

		/** Whether the instance was created with what showing pictures in each kind of window needs. */
		bool bCanShowInWindows = false;
		bool bCanShowInHeadlessWindows = false;
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

	/** Returns the extensions that an instance on this machine can be created with. */
	std::vector<VkExtensionProperties> GetAvailableExtensions()
	{
		std::vector<VkExtensionProperties> Extensions;
		const PFN_vkEnumerateInstanceExtensionProperties Enumerate =
			reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(
				State.GetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties"));
		if (Enumerate == nullptr)
		{
			return Extensions;
		}

		uint32 Count = 0;
		if (Enumerate(nullptr, &Count, nullptr) != VK_SUCCESS || Count == 0)
		{
			return Extensions;
		}
		Extensions.resize(Count);
		// More extensions than were counted a moment ago would leave the list incomplete, not wrong.
		const VkResult Result = Enumerate(nullptr, &Count, Extensions.data());
		if (Result != VK_SUCCESS && Result != VK_INCOMPLETE)
		{
			Count = 0;
		}
		Extensions.resize(Count);
		return Extensions;
	}

	bool HasExtension(const std::vector<VkExtensionProperties>& Extensions, std::string_view Name)
	{
		return std::ranges::any_of(
			Extensions, [Name](const VkExtensionProperties& Extension) { return Name == Extension.extensionName; });
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

	// The instance is the process's only one and is created before anything asks for a window, so it is created
	// with whatever showing pictures in windows needs, where the machine has it. A machine that has none of it
	// can still draw; it can only not show.
	const std::vector<VkExtensionProperties> Available = GetAvailableExtensions();
	const bool bHasSurfaces = HasExtension(Available, VK_KHR_SURFACE_EXTENSION_NAME);
#if defined(VK_USE_PLATFORM_WIN32_KHR)
	const bool bHasWindowSurfaces = bHasSurfaces && HasExtension(Available, VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#else
	// The builds for other systems create windows in memory only (D-025).
	const bool bHasWindowSurfaces = false;
#endif
	const bool bHasHeadlessSurfaces = bHasSurfaces && HasExtension(Available, VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME);

	std::vector<const char*> EnabledExtensions;
	if (bHasWindowSurfaces || bHasHeadlessSurfaces)
	{
		EnabledExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
	}
#if defined(VK_USE_PLATFORM_WIN32_KHR)
	if (bHasWindowSurfaces)
	{
		EnabledExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
	}
#endif
	if (bHasHeadlessSurfaces)
	{
		EnabledExtensions.push_back(VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME);
	}

	VkInstanceCreateInfo CreateInfo{};
	CreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	CreateInfo.pApplicationInfo = &ApplicationInfo;
	CreateInfo.enabledExtensionCount = static_cast<uint32>(EnabledExtensions.size());
	CreateInfo.ppEnabledExtensionNames = EnabledExtensions.data();

	VkInstance NewInstance = VK_NULL_HANDLE;
	const VkResult Result = CreateInstance(&CreateInfo, nullptr, &NewInstance);
	if (Result != VK_SUCCESS)
	{
		OutError = std::format("Vulkan could not be started: {}", nvrhi::vulkan::resultToString(Result));
		return false;
	}

	State.Instance = NewInstance;
	State.bCanShowInWindows = bHasWindowSurfaces;
	State.bCanShowInHeadlessWindows = bHasHeadlessSurfaces;
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

bool FVulkanInstance::CanShowInWindows()
{
	WW_CHECKF(State.Instance != VK_NULL_HANDLE, "Vulkan has not been started");
	return State.bCanShowInWindows;
}

bool FVulkanInstance::CanShowInHeadlessWindows()
{
	WW_CHECKF(State.Instance != VK_NULL_HANDLE, "Vulkan has not been started");
	return State.bCanShowInHeadlessWindows;
}

std::string FVulkanInstance::FormatVersion(uint32 Version)
{
	return std::format("{}.{}", VK_API_VERSION_MAJOR(Version), VK_API_VERSION_MINOR(Version));
}
