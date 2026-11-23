#pragma once

#include "CoreMinimal.h"

#include "Rendering/RenderDeviceDescription.h"

#include <memory>
#include <string>

class FRenderContext;
class FRenderTarget;

/** Log category of the renderer. */
extern ENGINE_API FLogCategory LogRendering;

/**
 * The graphics card as the engine uses it: the object everything that is drawn goes through.
 *
 * It draws with Vulkan and needs no window, so it works the same on a desktop, in a test and on a build server;
 * what is drawn goes to render targets, which can be read back as images. A machine needs Vulkan 1.3 and a
 * graphics card, or a software implementation of Vulkan, to create one.
 *
 * Only one render device can exist at a time. It is used from the main thread.
 */
class FRenderDevice
{
public:
	/**
	 * Creates the render device on the most capable graphics card of the machine: a dedicated card is preferred to
	 * one that is built into the processor, and either to a software implementation.
	 *
	 * @param OutError Receives the reason on failure, for example that Vulkan is not installed or that no graphics
	 * card supports Vulkan 1.3.
	 * @return The device, or null on failure or if a render device already exists.
	 */
	ENGINE_API static std::unique_ptr<FRenderDevice> Create(
		const FRenderDeviceDescription& Description, std::string& OutError);

	/** Used by Create. Other code cannot call it, because it cannot create a render context. */
	explicit FRenderDevice(std::shared_ptr<FRenderContext> InContext);

	ENGINE_API ~FRenderDevice();

	FRenderDevice(const FRenderDevice&) = delete;
	FRenderDevice& operator=(const FRenderDevice&) = delete;

	/** Returns the name of the graphics card in use, as its driver reports it. */
	ENGINE_API const std::string& GetAdapterName() const;

	/** Returns true if the device draws on the processor instead of on a graphics card. */
	ENGINE_API bool IsSoftwareAdapter() const;

	/**
	 * Creates a surface to draw on, of the given size in pixels.
	 *
	 * @param OutError Receives the reason on failure.
	 * @return The render target, or null if a dimension is not between 1 and FRenderTarget::MaxDimension.
	 */
	ENGINE_API std::unique_ptr<FRenderTarget> CreateRenderTarget(int32 Width, int32 Height, std::string& OutError);

private:
	friend class FSceneRenderer;
	friend class FSwapChain;
	friend class FUiRenderer;

	/** Shared with everything created from this device, so that the graphics card outlives all of it. */
	std::shared_ptr<FRenderContext> Context;
};
