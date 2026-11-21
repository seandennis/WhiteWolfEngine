#include "Rendering/RenderContext.h"

#include "Rendering/RenderDevice.h"

#include <nvrhi/validation.h>

#include <format>
#include <utility>
#include <vector>

// The table of Vulkan function pointers that NVRHI's Vulkan backend calls through. vulkan.hpp requires exactly one
// definition of it in the program, and NVRHI, built as a static library, leaves that to the application.
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

constinit FLogCategory LogRendering("LogRendering", ELogVerbosity::Log);

namespace
{
	/** The Vulkan version the renderer needs: NVRHI uses dynamic rendering and synchronization2, both core in 1.3. */
	constexpr uint32 RequiredApiVersion = VK_API_VERSION_1_3;

	/** True while a render context exists. There can be only one; see FRenderContext. */
	bool bContextExists = false;

	vk::detail::DispatchLoaderDynamic& GetFunctions()
	{
		return VULKAN_HPP_DEFAULT_DISPATCHER;
	}

	std::string FormatVersion(uint32 Version)
	{
		return std::format("{}.{}", VK_API_VERSION_MAJOR(Version), VK_API_VERSION_MINOR(Version));
	}

	/** Higher is better: a dedicated graphics card, then one built into the processor, then software. */
	int32 ScoreDeviceType(VkPhysicalDeviceType Type)
	{
		switch (Type)
		{
			case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
				return 3;
			case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
				return 2;
			case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
				return 1;
			default:
				return 0;
		}
	}

	/** The Vulkan features the renderer cannot work without, as a chain for querying or enabling them. */
	struct FRequiredFeatures
	{
		VkPhysicalDeviceVulkan13Features Vulkan13{};
		VkPhysicalDeviceVulkan12Features Vulkan12{};
		VkPhysicalDeviceFeatures2 Features{};

		FRequiredFeatures()
		{
			Vulkan13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
			Vulkan12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
			Vulkan12.pNext = &Vulkan13;
			Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
			Features.pNext = &Vulkan12;
		}

		FRequiredFeatures(const FRequiredFeatures&) = delete;
		FRequiredFeatures& operator=(const FRequiredFeatures&) = delete;

		/** Returns true if every required feature is set, as after a query of what a graphics card supports. */
		bool AreAllSupported() const
		{
			return Vulkan12.timelineSemaphore == VK_TRUE && Vulkan13.dynamicRendering == VK_TRUE &&
				Vulkan13.synchronization2 == VK_TRUE;
		}

		/** Switches on the required features and nothing else, for creating a device. */
		void EnableRequired()
		{
			Vulkan12.timelineSemaphore = VK_TRUE;
			Vulkan13.dynamicRendering = VK_TRUE;
			Vulkan13.synchronization2 = VK_TRUE;
		}
	};
}

void FRenderContext::FMessageCallback::message(nvrhi::MessageSeverity Severity, const char* MessageText)
{
	const char* const Text = MessageText != nullptr ? MessageText : "";
	switch (Severity)
	{
		case nvrhi::MessageSeverity::Info:
			WW_LOG(LogRendering, Verbose, "{}", Text);
			break;
		case nvrhi::MessageSeverity::Warning:
			WW_LOG(LogRendering, Warning, "{}", Text);
			break;
		case nvrhi::MessageSeverity::Error:
		case nvrhi::MessageSeverity::Fatal:
		default:
			// NVRHI carries on after reporting; the engine decides what a rendering error means.
			WW_LOG(LogRendering, Error, "{}", Text);
			break;
	}
}

std::shared_ptr<FRenderContext> FRenderContext::Create(
	const FRenderDeviceDescription& Description, std::string& OutError)
{
	if (bContextExists)
	{
		OutError = "a render device already exists, and only one can exist at a time";
		return nullptr;
	}

	std::shared_ptr<FRenderContext> Context = std::make_shared<FRenderContext>();
	Context->bOwnsFunctionTable = true;
	bContextExists = true;

	if (!Context->Initialize(Description, OutError))
	{
		return nullptr;
	}

	return Context;
}

FRenderContext::FRenderContext() = default;

FRenderContext::~FRenderContext()
{
	if (Device != nullptr)
	{
		// Let the graphics card finish and let NVRHI free what it was keeping for commands in flight.
		Device->waitForIdle();
		Device->runGarbageCollection();
	}
	ImmediateCommandList = nullptr;
	Device = nullptr;

	if (LogicalDevice != VK_NULL_HANDLE)
	{
		GetFunctions().vkDestroyDevice(LogicalDevice, nullptr);
	}
	if (Instance != VK_NULL_HANDLE)
	{
		GetFunctions().vkDestroyInstance(Instance, nullptr);
	}

	if (bOwnsFunctionTable)
	{
		bContextExists = false;
	}
}

bool FRenderContext::Initialize(const FRenderDeviceDescription& Description, std::string& OutError)
{
	if (!Library.Load(OutError))
	{
		return false;
	}

	// The functions that exist before there is an instance, such as vkCreateInstance.
	GetFunctions().init(Library.GetInstanceProcAddr());

	return CreateInstance(OutError) && SelectPhysicalDevice(OutError) && CreateLogicalDevice(OutError) &&
		CreateRenderInterface(Description, OutError);
}

bool FRenderContext::CreateInstance(std::string& OutError)
{
	// vkEnumerateInstanceVersion does not exist in Vulkan 1.0, which is how 1.0 is recognised.
	uint32 LoaderVersion = VK_API_VERSION_1_0;
	if (GetFunctions().vkEnumerateInstanceVersion != nullptr)
	{
		GetFunctions().vkEnumerateInstanceVersion(&LoaderVersion);
	}
	if (LoaderVersion < RequiredApiVersion)
	{
		OutError = std::format("the installed Vulkan is version {}, but version {} or later is needed",
			FormatVersion(LoaderVersion),
			FormatVersion(RequiredApiVersion));
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

	const VkResult Result = GetFunctions().vkCreateInstance(&CreateInfo, nullptr, &Instance);
	if (Result != VK_SUCCESS)
	{
		Instance = VK_NULL_HANDLE;
		OutError = std::format("Vulkan could not be started: {}", nvrhi::vulkan::resultToString(Result));
		return false;
	}

	// The functions that belong to the instance, such as vkEnumeratePhysicalDevices.
	GetFunctions().init(vk::Instance(Instance));
	return true;
}

bool FRenderContext::SelectPhysicalDevice(std::string& OutError)
{
	uint32 DeviceCount = 0;
	GetFunctions().vkEnumeratePhysicalDevices(Instance, &DeviceCount, nullptr);
	std::vector<VkPhysicalDevice> Devices(DeviceCount);
	if (DeviceCount > 0)
	{
		GetFunctions().vkEnumeratePhysicalDevices(Instance, &DeviceCount, Devices.data());
		Devices.resize(DeviceCount);
	}

	int32 BestScore = -1;
	std::string Rejections;

	for (const VkPhysicalDevice Candidate : Devices)
	{
		VkPhysicalDeviceProperties Properties{};
		GetFunctions().vkGetPhysicalDeviceProperties(Candidate, &Properties);

		if (Properties.apiVersion < RequiredApiVersion)
		{
			Rejections += std::format(
				" {} supports only Vulkan {}.", Properties.deviceName, FormatVersion(Properties.apiVersion));
			continue;
		}

		FRequiredFeatures Supported;
		GetFunctions().vkGetPhysicalDeviceFeatures2(Candidate, &Supported.Features);
		if (!Supported.AreAllSupported())
		{
			Rejections +=
				std::format(" {} lacks a feature of Vulkan 1.3 that the renderer needs.", Properties.deviceName);
			continue;
		}

		uint32 FamilyCount = 0;
		GetFunctions().vkGetPhysicalDeviceQueueFamilyProperties(Candidate, &FamilyCount, nullptr);
		std::vector<VkQueueFamilyProperties> Families(FamilyCount);
		GetFunctions().vkGetPhysicalDeviceQueueFamilyProperties(Candidate, &FamilyCount, Families.data());

		bool bHasGraphicsQueue = false;
		uint32 GraphicsFamily = 0;
		for (uint32 Index = 0; Index < FamilyCount && !bHasGraphicsQueue; ++Index)
		{
			if ((Families[Index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 && Families[Index].queueCount > 0)
			{
				bHasGraphicsQueue = true;
				GraphicsFamily = Index;
			}
		}
		if (!bHasGraphicsQueue)
		{
			Rejections += std::format(" {} cannot draw.", Properties.deviceName);
			continue;
		}

		const int32 Score = ScoreDeviceType(Properties.deviceType);
		if (Score > BestScore)
		{
			BestScore = Score;
			PhysicalDevice = Candidate;
			GraphicsQueueFamily = GraphicsFamily;
			AdapterName = Properties.deviceName;
			bIsSoftwareAdapter = Properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU;
		}
	}

	if (PhysicalDevice == VK_NULL_HANDLE)
	{
		OutError = Devices.empty() ? "Vulkan found no graphics card on this machine"
								   : "no graphics card on this machine can be used:" + Rejections;
		return false;
	}

	return true;
}

bool FRenderContext::CreateLogicalDevice(std::string& OutError)
{
	const float QueuePriority = 1.0f;
	VkDeviceQueueCreateInfo QueueInfo{};
	QueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	QueueInfo.queueFamilyIndex = GraphicsQueueFamily;
	QueueInfo.queueCount = 1;
	QueueInfo.pQueuePriorities = &QueuePriority;

	FRequiredFeatures Enabled;
	Enabled.EnableRequired();

	VkDeviceCreateInfo CreateInfo{};
	CreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	CreateInfo.pNext = &Enabled.Features;
	CreateInfo.queueCreateInfoCount = 1;
	CreateInfo.pQueueCreateInfos = &QueueInfo;

	const VkResult Result = GetFunctions().vkCreateDevice(PhysicalDevice, &CreateInfo, nullptr, &LogicalDevice);
	if (Result != VK_SUCCESS)
	{
		LogicalDevice = VK_NULL_HANDLE;
		OutError = std::format(
			"the graphics card {} could not be opened: {}", AdapterName, nvrhi::vulkan::resultToString(Result));
		return false;
	}

	// The functions that belong to the device; from here on calls go straight to this graphics card's driver.
	GetFunctions().init(vk::Device(LogicalDevice));
	GetFunctions().vkGetDeviceQueue(LogicalDevice, GraphicsQueueFamily, 0, &GraphicsQueue);
	return true;
}

bool FRenderContext::CreateRenderInterface(const FRenderDeviceDescription& Description, std::string& OutError)
{
	nvrhi::vulkan::DeviceDesc DeviceDescription;
	DeviceDescription.errorCB = &MessageCallback;
	DeviceDescription.instance = Instance;
	DeviceDescription.physicalDevice = PhysicalDevice;
	DeviceDescription.device = LogicalDevice;
	DeviceDescription.graphicsQueue = GraphicsQueue;
	DeviceDescription.graphicsQueueIndex = static_cast<int>(GraphicsQueueFamily);
	DeviceDescription.transferQueue = VK_NULL_HANDLE;
	DeviceDescription.computeQueue = VK_NULL_HANDLE;

	const nvrhi::vulkan::DeviceHandle VulkanDevice = nvrhi::vulkan::createDevice(DeviceDescription);
	if (VulkanDevice == nullptr)
	{
		OutError = "the rendering interface could not be created";
		return false;
	}

	Device = VulkanDevice;
	if (Description.bEnableValidation)
	{
		Device = nvrhi::validation::createValidationLayer(VulkanDevice);
	}

	ImmediateCommandList = Device->createCommandList();
	if (ImmediateCommandList == nullptr)
	{
		OutError = "the rendering interface could not create a command list";
		return false;
	}

	WW_LOG(LogRendering, Log, "Rendering with Vulkan on {}{}", AdapterName, bIsSoftwareAdapter ? " (software)" : "");
	return true;
}
