#include "Rendering/VulkanLibrary.h"

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
}

FVulkanLibrary::~FVulkanLibrary()
{
	if (Handle == nullptr)
	{
		return;
	}

#if defined(_WIN32)
	FreeLibrary(static_cast<HMODULE>(Handle));
#else
	dlclose(Handle);
#endif
}

bool FVulkanLibrary::Load(std::string& OutError)
{
	WW_CHECKF(Handle == nullptr, "The Vulkan library was loaded twice");

#if defined(_WIN32)
	const HMODULE Module = LoadLibraryA(LibraryName);
	void* const LoadedHandle = Module;
	void* const EntryPoint =
		Module != nullptr ? reinterpret_cast<void*>(GetProcAddress(Module, EntryPointName)) : nullptr;
#else
	void* const LoadedHandle = dlopen(LibraryName, RTLD_NOW | RTLD_LOCAL);
	void* const EntryPoint = LoadedHandle != nullptr ? dlsym(LoadedHandle, EntryPointName) : nullptr;
#endif

	if (LoadedHandle == nullptr)
	{
		OutError = std::format(
			"Vulkan is not installed on this machine: {} could not be loaded. It comes with the graphics driver",
			LibraryName);
		return false;
	}

	Handle = LoadedHandle;
	if (EntryPoint == nullptr)
	{
		OutError = std::format("{} does not provide {}", LibraryName, EntryPointName);
		return false;
	}

	InstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(EntryPoint);
	return true;
}

PFN_vkGetInstanceProcAddr FVulkanLibrary::GetInstanceProcAddr() const
{
	WW_CHECKF(InstanceProcAddr != nullptr, "The Vulkan library has not been loaded");
	return InstanceProcAddr;
}
