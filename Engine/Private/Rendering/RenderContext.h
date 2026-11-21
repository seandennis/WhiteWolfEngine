#pragma once

#include "CoreMinimal.h"

#include "Rendering/RenderDeviceDescription.h"
#include "Rendering/VulkanApi.h"
#include "Rendering/VulkanLibrary.h"

#include <nvrhi/nvrhi.h>
#include <nvrhi/vulkan.h>

#include <memory>
#include <string>

/**
 * The graphics card and everything needed to use it: the Vulkan instance and device and the NVRHI device built on
 * them. Internal to the Engine module; FRenderDevice and every resource created from it share one context, which
 * is released with the last of them.
 *
 * Only one context can exist at a time, because NVRHI reaches Vulkan through one global table of functions that
 * belongs to one Vulkan device.
 */
class FRenderContext
{
public:
	/**
	 * Creates the context on the most capable graphics card.
	 *
	 * @return The context, or null with the reason in OutError.
	 */
	static std::shared_ptr<FRenderContext> Create(const FRenderDeviceDescription& Description, std::string& OutError);

	/** Use Create. The constructor is public only so that std::make_shared can reach it. */
	FRenderContext();
	~FRenderContext();

	FRenderContext(const FRenderContext&) = delete;
	FRenderContext& operator=(const FRenderContext&) = delete;

	/** Returns the NVRHI device that all rendering goes through. */
	nvrhi::IDevice* GetDevice() const
	{
		return Device;
	}

	/**
	 * Returns a command list for commands that are executed at once: open it, record, close it and execute it
	 * before anything else uses it.
	 */
	nvrhi::ICommandList* GetImmediateCommandList() const
	{
		return ImmediateCommandList;
	}

	const std::string& GetAdapterName() const
	{
		return AdapterName;
	}

	bool IsSoftwareAdapter() const
	{
		return bIsSoftwareAdapter;
	}

private:
	/** Passes what NVRHI has to say on to the engine log. */
	class FMessageCallback final : public nvrhi::IMessageCallback
	{
	public:
		void message(nvrhi::MessageSeverity Severity, const char* MessageText) override;
	};

	bool Initialize(const FRenderDeviceDescription& Description, std::string& OutError);
	bool CreateInstance(std::string& OutError);
	bool SelectPhysicalDevice(std::string& OutError);
	bool CreateLogicalDevice(std::string& OutError);
	bool CreateRenderInterface(const FRenderDeviceDescription& Description, std::string& OutError);

	// Declared first so that it is destroyed last: everything below calls into the library.
	FVulkanLibrary Library;

	VkInstance Instance = VK_NULL_HANDLE;
	VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;
	VkDevice LogicalDevice = VK_NULL_HANDLE;
	VkQueue GraphicsQueue = VK_NULL_HANDLE;
	uint32 GraphicsQueueFamily = 0;

	FMessageCallback MessageCallback;

	/** The device used for rendering: the Vulkan device, or the validation layer wrapped around it. */
	nvrhi::DeviceHandle Device;
	nvrhi::CommandListHandle ImmediateCommandList;

	std::string AdapterName;
	bool bIsSoftwareAdapter = false;

	/** True if this context is the one that currently owns the global Vulkan function table. */
	bool bOwnsFunctionTable = false;
};
