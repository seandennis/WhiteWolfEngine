#include "Rendering/SwapChain.h"

#include "Platform/Window.h"
#include "Rendering/RenderContext.h"
#include "Rendering/RenderDevice.h"
#include "Rendering/RenderTarget.h"
#include "Rendering/RenderTargetImplementation.h"
#include "Rendering/ShaderLibrary.h"
#include "Rendering/VulkanApi.h"
#include "Rendering/VulkanInstance.h"

#include <nvrhi/nvrhi.h>
#include <nvrhi/vulkan.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <limits>
#include <utility>
#include <vector>

// NVRHI draws; it leaves showing what was drawn in a window to its user. This file is that part: the window's
// surface and its swap chain are made with Vulkan itself, the surfaces of the swap chain are then given to NVRHI
// as textures to draw to, and the two are kept in step with semaphores.

namespace
{
	vk::detail::DispatchLoaderDynamic& GetFunctions()
	{
		return VULKAN_HPP_DEFAULT_DISPATCHER;
	}

	/**
	 * How many pictures may be on their way to the display at once. The processor waits for the oldest of them to
	 * leave the graphics card before it starts on one more.
	 */
	constexpr std::size_t FrameSlotCount = 3;

	/** A format that the surface of a window can have, and what NVRHI calls it. */
	struct FSurfaceFormat
	{
		VkFormat Format;
		nvrhi::Format RenderFormat;

		/** True if the graphics card encodes what is written to the surface as sRGB, as a display expects. */
		bool bEncodesSrgb;
	};

	/**
	 * The formats the engine shows pictures in, the preferred one first: eight bits per channel, encoded as sRGB
	 * by the graphics card, which is what a render target's own pixels are, or else by the copying shader.
	 */
	constexpr std::array<FSurfaceFormat, 4> SurfaceFormats{{
		{VK_FORMAT_B8G8R8A8_SRGB, nvrhi::Format::SBGRA8_UNORM, true},
		{VK_FORMAT_R8G8B8A8_SRGB, nvrhi::Format::SRGBA8_UNORM, true},
		{VK_FORMAT_B8G8R8A8_UNORM, nvrhi::Format::BGRA8_UNORM, false},
		{VK_FORMAT_R8G8B8A8_UNORM, nvrhi::Format::RGBA8_UNORM, false},
	}};

	/** What the copying shader is told. Declared again in Engine/Shaders/Present.frag; keep the two identical. */
	struct FPresentConstants
	{
		int32 EncodeSrgb = 0;
	};

	/** The ways a window's surface can be laid over what is behind it; the first that the window offers is used. */
	constexpr std::array<VkCompositeAlphaFlagBitsKHR, 4> CompositeModes{
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
		VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
	};
}

/** Everything a swap chain holds. It lives here so that no Vulkan or NVRHI type appears in an engine header. */
struct FSwapChain::FImplementation
{
	/** One of the surfaces that the window shows in turn, as NVRHI draws to it. */
	struct FImage
	{
		nvrhi::TextureHandle Texture;
		nvrhi::FramebufferHandle Framebuffer;
	};

	/** What one picture needs while it is on its way to the display. */
	struct FFrameSlot
	{
		/** Signalled by the window system when the surface that was asked for can be drawn to. */
		VkSemaphore ImageAcquired = VK_NULL_HANDLE;

		/** Tells when the graphics card has finished the picture, after which the slot can be used again. */
		nvrhi::EventQueryHandle Finished;

		bool bIsInFlight = false;
	};

	FImplementation() = default;
	~FImplementation();

	FImplementation(const FImplementation&) = delete;
	FImplementation& operator=(const FImplementation&) = delete;

	bool Initialize(std::string& OutError);
	bool CreateSurface(std::string& OutError);
	bool ChooseSurfaceFormat(std::string& OutError);
	bool CreatePipeline(std::string& OutError);
	bool CreateSemaphore(VkSemaphore& OutSemaphore, std::string& OutError) const;

	/**
	 * Replaces the swap chain with one that fits the window as it is now. Afterwards there is no swap chain if the
	 * window has no area.
	 */
	bool RecreateSwapChain(std::string& OutError);

	/** Lets go of the surfaces of the swap chain. Nothing may be on its way to them. */
	void ReleaseImages();

	/** Returns what gives the copying shader a picture to read, or null if the graphics card cannot provide it. */
	nvrhi::IBindingSet* GetPictureBindings(nvrhi::ITexture* PictureTexture);

	/** Keeps the graphics card alive for as long as this swap chain exists. */
	std::shared_ptr<FRenderContext> Context;

	FWindow* Window = nullptr;

	VkSurfaceKHR Surface = VK_NULL_HANDLE;
	FSurfaceFormat SurfaceFormat{VK_FORMAT_UNDEFINED, nvrhi::Format::UNKNOWN, false};

	/** Null while the window has no area. */
	VkSwapchainKHR SwapChain = VK_NULL_HANDLE;
	std::vector<FImage> Images;
	int32 Width = 0;
	int32 Height = 0;

	/** The size the window had when the swap chain was made for it; another size means another swap chain. */
	int32 WindowWidth = 0;
	int32 WindowHeight = 0;

	/** Set when the window system says that the swap chain no longer fits the window. */
	bool bIsOutOfDate = false;

	/**
	 * One for each surface of the swap chain: signalled when the picture has been copied onto the surface, which
	 * is what the window system waits for before it shows it. They are kept when the swap chain is replaced.
	 */
	std::vector<VkSemaphore> PictureReady;

	std::array<FFrameSlot, FrameSlotCount> FrameSlots;
	std::size_t NextFrameSlot = 0;

	nvrhi::BindingLayoutHandle BindingLayout;
	nvrhi::GraphicsPipelineHandle Pipeline;
	nvrhi::CommandListHandle CommandList;

	/** What gives the shader the picture that was shown last, kept because it is usually shown again. */
	nvrhi::BindingSetHandle PictureBindings;
	nvrhi::ITexture* BoundPicture = nullptr;

	int64 PresentedCount = 0;
};

FSwapChain::FImplementation::~FImplementation()
{
	if (Context == nullptr)
	{
		return;
	}

	nvrhi::IDevice& Device = *Context->GetDevice();
	const VkDevice LogicalDevice = Context->GetLogicalDevice();

	// What is on its way to the window has to arrive before its surfaces are taken away.
	Device.waitForIdle();
	ReleaseImages();
	PictureBindings = nullptr;
	Pipeline = nullptr;
	CommandList = nullptr;
	for (FFrameSlot& Slot : FrameSlots)
	{
		Slot.Finished = nullptr;
	}
	Device.runGarbageCollection();

	if (SwapChain != VK_NULL_HANDLE)
	{
		GetFunctions().vkDestroySwapchainKHR(LogicalDevice, SwapChain, nullptr);
	}
	for (const VkSemaphore Semaphore : PictureReady)
	{
		GetFunctions().vkDestroySemaphore(LogicalDevice, Semaphore, nullptr);
	}
	for (const FFrameSlot& Slot : FrameSlots)
	{
		if (Slot.ImageAcquired != VK_NULL_HANDLE)
		{
			GetFunctions().vkDestroySemaphore(LogicalDevice, Slot.ImageAcquired, nullptr);
		}
	}
	if (Surface != VK_NULL_HANDLE)
	{
		GetFunctions().vkDestroySurfaceKHR(FVulkanInstance::GetHandle(), Surface, nullptr);
	}
}

bool FSwapChain::FImplementation::Initialize(std::string& OutError)
{
	if (!CreateSurface(OutError) || !ChooseSurfaceFormat(OutError) || !CreatePipeline(OutError))
	{
		return false;
	}

	nvrhi::IDevice& Device = *Context->GetDevice();
	CommandList = Device.createCommandList();
	if (CommandList == nullptr)
	{
		OutError = "the rendering interface could not create a command list for showing pictures";
		return false;
	}
	for (FFrameSlot& Slot : FrameSlots)
	{
		Slot.Finished = Device.createEventQuery();
		if (Slot.Finished == nullptr)
		{
			OutError = "the rendering interface could not create what tells when a picture is finished";
			return false;
		}
		if (!CreateSemaphore(Slot.ImageAcquired, OutError))
		{
			return false;
		}
	}

	return RecreateSwapChain(OutError);
}

bool FSwapChain::FImplementation::CreateSurface(std::string& OutError)
{
	const VkInstance Instance = FVulkanInstance::GetHandle();
	VkResult Result = VK_ERROR_INITIALIZATION_FAILED;

	void* const NativeWindow = Window->GetNativeHandle();
	if (NativeWindow == nullptr)
	{
		// A window that exists in memory only. Vulkan has a surface for that, which shows nothing anywhere.
		VkHeadlessSurfaceCreateInfoEXT CreateInfo{};
		CreateInfo.sType = VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT;
		Result = GetFunctions().vkCreateHeadlessSurfaceEXT(Instance, &CreateInfo, nullptr, &Surface);
	}
	else
	{
#if defined(VK_USE_PLATFORM_WIN32_KHR)
		VkWin32SurfaceCreateInfoKHR CreateInfo{};
		CreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		CreateInfo.hinstance = GetModuleHandleW(nullptr);
		CreateInfo.hwnd = static_cast<HWND>(NativeWindow);
		Result = GetFunctions().vkCreateWin32SurfaceKHR(Instance, &CreateInfo, nullptr, &Surface);
#endif
	}

	if (Result != VK_SUCCESS)
	{
		Surface = VK_NULL_HANDLE;
		OutError = std::format(
			"the window could not be given a surface to show pictures on: {}", nvrhi::vulkan::resultToString(Result));
		return false;
	}

	VkBool32 bCanPresent = VK_FALSE;
	Result = GetFunctions().vkGetPhysicalDeviceSurfaceSupportKHR(
		Context->GetPhysicalDevice(), Context->GetGraphicsQueueFamily(), Surface, &bCanPresent);
	if (Result != VK_SUCCESS || bCanPresent != VK_TRUE)
	{
		OutError = std::format("the graphics card {} cannot show pictures in this window", Context->GetAdapterName());
		return false;
	}
	return true;
}

bool FSwapChain::FImplementation::ChooseSurfaceFormat(std::string& OutError)
{
	const VkPhysicalDevice PhysicalDevice = Context->GetPhysicalDevice();

	uint32 Count = 0;
	GetFunctions().vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, Surface, &Count, nullptr);
	std::vector<VkSurfaceFormatKHR> Offered(Count);
	if (Count > 0)
	{
		GetFunctions().vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, Surface, &Count, Offered.data());
		Offered.resize(Count);
	}

	for (const FSurfaceFormat& Wanted : SurfaceFormats)
	{
		const bool bIsOffered = std::ranges::any_of(Offered,
			[&Wanted](const VkSurfaceFormatKHR& Format)
			{ return Format.format == Wanted.Format && Format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR; });
		if (bIsOffered)
		{
			SurfaceFormat = Wanted;
			return true;
		}
	}

	OutError = "the window offers no surface with eight bits per channel, which is what the engine shows pictures in";
	return false;
}

bool FSwapChain::FImplementation::CreatePipeline(std::string& OutError)
{
	nvrhi::IDevice& Device = *Context->GetDevice();

	const nvrhi::ShaderHandle VertexShader =
		FShaderLibrary::CreateShader(Device, "FullScreen.vert", nvrhi::ShaderType::Vertex);
	const nvrhi::ShaderHandle FragmentShader =
		FShaderLibrary::CreateShader(Device, "Present.frag", nvrhi::ShaderType::Pixel);
	if (VertexShader == nullptr || FragmentShader == nullptr)
	{
		OutError = "the graphics card did not accept the engine's shaders for showing pictures";
		return false;
	}

	// Slot 0 of the textures is the picture.
	BindingLayout =
		Device.createBindingLayout(nvrhi::BindingLayoutDesc()
									   .setVisibility(nvrhi::ShaderType::Pixel)
									   .addItem(nvrhi::BindingLayoutItem::Texture_SRV(0))
									   .addItem(nvrhi::BindingLayoutItem::PushConstants(0, sizeof(FPresentConstants))));
	if (BindingLayout == nullptr)
	{
		OutError = "the graphics card could not create the layout for showing pictures";
		return false;
	}

	nvrhi::GraphicsPipelineDesc Description;
	Description.primType = nvrhi::PrimitiveType::TriangleList;
	Description.VS = VertexShader;
	Description.PS = FragmentShader;
	Description.bindingLayouts.push_back(BindingLayout);
	Description.renderState.rasterState.cullMode = nvrhi::RasterCullMode::None;
	Description.renderState.depthStencilState.depthTestEnable = false;
	Description.renderState.depthStencilState.depthWriteEnable = false;

	nvrhi::FramebufferInfo TargetFormats;
	TargetFormats.addColorFormat(SurfaceFormat.RenderFormat);
	Pipeline = Device.createGraphicsPipeline(Description, TargetFormats);
	if (Pipeline == nullptr)
	{
		OutError = "the graphics card could not create the pipeline for showing pictures";
		return false;
	}
	return true;
}

bool FSwapChain::FImplementation::CreateSemaphore(VkSemaphore& OutSemaphore, std::string& OutError) const
{
	VkSemaphoreCreateInfo CreateInfo{};
	CreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	const VkResult Result =
		GetFunctions().vkCreateSemaphore(Context->GetLogicalDevice(), &CreateInfo, nullptr, &OutSemaphore);
	if (Result != VK_SUCCESS)
	{
		OutSemaphore = VK_NULL_HANDLE;
		OutError = std::format("the graphics card could not create a semaphore for showing pictures: {}",
			nvrhi::vulkan::resultToString(Result));
		return false;
	}
	return true;
}

void FSwapChain::FImplementation::ReleaseImages()
{
	Images.clear();
	Width = 0;
	Height = 0;
}

bool FSwapChain::FImplementation::RecreateSwapChain(std::string& OutError)
{
	nvrhi::IDevice& Device = *Context->GetDevice();
	const VkDevice LogicalDevice = Context->GetLogicalDevice();

	VkSurfaceCapabilitiesKHR Capabilities{};
	VkResult Result =
		GetFunctions().vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Context->GetPhysicalDevice(), Surface, &Capabilities);
	if (Result != VK_SUCCESS)
	{
		OutError = std::format("the window system did not say what the surface of the window can be: {}",
			nvrhi::vulkan::resultToString(Result));
		return false;
	}

	WindowWidth = Window->GetWidth();
	WindowHeight = Window->GetHeight();

	// The window system either says how large the surface is, or leaves it to whoever shows pictures on it.
	VkExtent2D Extent = Capabilities.currentExtent;
	const bool bIsSizeLeftOpen = Extent.width == std::numeric_limits<uint32>::max();
	if (bIsSizeLeftOpen)
	{
		Extent.width = std::clamp(static_cast<uint32>(std::max(WindowWidth, 0)),
			Capabilities.minImageExtent.width,
			Capabilities.maxImageExtent.width);
		Extent.height = std::clamp(static_cast<uint32>(std::max(WindowHeight, 0)),
			Capabilities.minImageExtent.height,
			Capabilities.maxImageExtent.height);
	}

	// What is on its way to the old surfaces has to arrive before they are taken away.
	Device.waitForIdle();
	ReleaseImages();
	Device.runGarbageCollection();
	for (FFrameSlot& Slot : FrameSlots)
	{
		if (Slot.bIsInFlight)
		{
			Device.resetEventQuery(Slot.Finished);
			Slot.bIsInFlight = false;
		}
	}

	const VkSwapchainKHR OldSwapChain = SwapChain;
	SwapChain = VK_NULL_HANDLE;
	bIsOutOfDate = false;

	const bool bHasArea = Extent.width > 0 && Extent.height > 0;
	if (bHasArea)
	{
		// One more than the window system needs, so that there is a surface to draw to while it holds the others.
		uint32 ImageCount = Capabilities.minImageCount + 1;
		if (Capabilities.maxImageCount > 0)
		{
			ImageCount = std::min(ImageCount, Capabilities.maxImageCount);
		}

		VkCompositeAlphaFlagBitsKHR CompositeMode = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		for (const VkCompositeAlphaFlagBitsKHR Mode : CompositeModes)
		{
			if ((Capabilities.supportedCompositeAlpha & Mode) != 0)
			{
				CompositeMode = Mode;
				break;
			}
		}

		VkSwapchainCreateInfoKHR CreateInfo{};
		CreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		CreateInfo.surface = Surface;
		CreateInfo.minImageCount = ImageCount;
		CreateInfo.imageFormat = SurfaceFormat.Format;
		CreateInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		CreateInfo.imageExtent = Extent;
		CreateInfo.imageArrayLayers = 1;
		CreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		CreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		CreateInfo.preTransform = Capabilities.currentTransform;
		CreateInfo.compositeAlpha = CompositeMode;
		// One picture for each refresh of the display, in order: the mode every window system has.
		CreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
		CreateInfo.clipped = VK_TRUE;
		CreateInfo.oldSwapchain = OldSwapChain;
		Result = GetFunctions().vkCreateSwapchainKHR(LogicalDevice, &CreateInfo, nullptr, &SwapChain);
	}

	if (OldSwapChain != VK_NULL_HANDLE)
	{
		GetFunctions().vkDestroySwapchainKHR(LogicalDevice, OldSwapChain, nullptr);
	}
	if (!bHasArea)
	{
		return true;
	}
	if (Result != VK_SUCCESS)
	{
		SwapChain = VK_NULL_HANDLE;
		OutError = std::format(
			"the window could not be given surfaces to show pictures on: {}", nvrhi::vulkan::resultToString(Result));
		return false;
	}

	uint32 ImageCount = 0;
	GetFunctions().vkGetSwapchainImagesKHR(LogicalDevice, SwapChain, &ImageCount, nullptr);
	std::vector<VkImage> VulkanImages(ImageCount);
	Result = GetFunctions().vkGetSwapchainImagesKHR(LogicalDevice, SwapChain, &ImageCount, VulkanImages.data());
	if (Result != VK_SUCCESS || ImageCount == 0)
	{
		OutError =
			std::format("the surfaces of the window could not be reached: {}", nvrhi::vulkan::resultToString(Result));
		return false;
	}
	VulkanImages.resize(ImageCount);

	// NVRHI draws to the surfaces as to textures of its own, and hands each back in the state the window system
	// expects to show it in.
	nvrhi::TextureDesc Description;
	Description.width = Extent.width;
	Description.height = Extent.height;
	Description.format = SurfaceFormat.RenderFormat;
	Description.dimension = nvrhi::TextureDimension::Texture2D;
	Description.isRenderTarget = true;
	Description.debugName = "WindowSurface";
	Description.enableAutomaticStateTracking(nvrhi::ResourceStates::Present);

	for (const VkImage VulkanImage : VulkanImages)
	{
		FImage Image;
		Image.Texture =
			Device.createHandleForNativeTexture(nvrhi::ObjectTypes::VK_Image, nvrhi::Object(VulkanImage), Description);
		if (Image.Texture != nullptr)
		{
			Image.Framebuffer = Device.createFramebuffer(nvrhi::FramebufferDesc().addColorAttachment(Image.Texture));
		}
		if (Image.Framebuffer == nullptr)
		{
			OutError = "the rendering interface could not draw to the surfaces of the window";
			return false;
		}
		Images.push_back(std::move(Image));
	}

	while (PictureReady.size() < Images.size())
	{
		VkSemaphore Semaphore = VK_NULL_HANDLE;
		if (!CreateSemaphore(Semaphore, OutError))
		{
			return false;
		}
		PictureReady.push_back(Semaphore);
	}

	Width = static_cast<int32>(Extent.width);
	Height = static_cast<int32>(Extent.height);
	return true;
}

nvrhi::IBindingSet* FSwapChain::FImplementation::GetPictureBindings(nvrhi::ITexture* PictureTexture)
{
	if (BoundPicture != PictureTexture || PictureBindings == nullptr)
	{
		// The set that is replaced kept the picture it was made for alive, so a new picture cannot be mistaken
		// for it by having its address.
		PictureBindings = Context->GetDevice()->createBindingSet(
			nvrhi::BindingSetDesc()
				.addItem(nvrhi::BindingSetItem::Texture_SRV(0, PictureTexture))
				.addItem(nvrhi::BindingSetItem::PushConstants(0, sizeof(FPresentConstants))),
			BindingLayout);
		BoundPicture = PictureBindings != nullptr ? PictureTexture : nullptr;
	}
	return PictureBindings;
}

bool FSwapChain::IsSupported(const FRenderDevice& Device, const FWindow& Window)
{
	if (!Device.Context->CanShowPictures())
	{
		return false;
	}
	const bool bIsWindowOfTheDesktop = Window.GetNativeHandle() != nullptr;
	return bIsWindowOfTheDesktop ? FVulkanInstance::CanShowInWindows() : FVulkanInstance::CanShowInHeadlessWindows();
}

std::unique_ptr<FSwapChain> FSwapChain::Create(FRenderDevice& Device, FWindow& Window, std::string& OutError)
{
	if (!IsSupported(Device, Window))
	{
		OutError = Window.GetNativeHandle() != nullptr
			? std::format("the graphics card {} has no way to hand pictures to a window", Device.GetAdapterName())
			: std::format("the driver of the graphics card {} cannot show pictures in a window that exists in "
						  "memory only",
				  Device.GetAdapterName());
		return nullptr;
	}

	std::unique_ptr<FImplementation> NewImplementation = std::make_unique<FImplementation>();
	NewImplementation->Context = Device.Context;
	NewImplementation->Window = &Window;
	if (!NewImplementation->Initialize(OutError))
	{
		return nullptr;
	}
	return std::make_unique<FSwapChain>(std::move(NewImplementation));
}

FSwapChain::FSwapChain(std::unique_ptr<FImplementation> InImplementation)
	: Implementation(std::move(InImplementation))
{
}

FSwapChain::~FSwapChain() = default;

bool FSwapChain::Present(const FRenderTarget& Picture, std::string& OutError)
{
	FImplementation& Chain = *Implementation;
	const FRenderTarget::FImplementation& PictureData = *Picture.Implementation;
	WW_CHECKF(PictureData.Context == Chain.Context,
		"A swap chain can only show render targets of the render device that created it");

	const int32 WindowWidth = Chain.Window->GetWidth();
	const int32 WindowHeight = Chain.Window->GetHeight();
	if (WindowWidth <= 0 || WindowHeight <= 0)
	{
		// A minimised window. There is nothing to show a picture on.
		return true;
	}

	const bool bFitsWindow = Chain.SwapChain != VK_NULL_HANDLE && !Chain.bIsOutOfDate &&
		Chain.WindowWidth == WindowWidth && Chain.WindowHeight == WindowHeight;
	if (!bFitsWindow && !Chain.RecreateSwapChain(OutError))
	{
		return false;
	}
	if (Chain.SwapChain == VK_NULL_HANDLE)
	{
		return true;
	}

	// Everything that can fail is done before a surface is asked for, so that one that was got is always shown.
	nvrhi::IDevice& Device = *Chain.Context->GetDevice();
	nvrhi::IBindingSet* const PictureBindings = Chain.GetPictureBindings(PictureData.Texture);
	if (PictureBindings == nullptr)
	{
		OutError = "the graphics card could not bind the picture to show";
		return false;
	}

	FImplementation::FFrameSlot& Slot = Chain.FrameSlots[Chain.NextFrameSlot];
	if (Slot.bIsInFlight)
	{
		// The picture that used this slot has to have left the graphics card before its semaphore is used again.
		Device.waitEventQuery(Slot.Finished);
		Device.resetEventQuery(Slot.Finished);
		Slot.bIsInFlight = false;
	}

	const VkDevice LogicalDevice = Chain.Context->GetLogicalDevice();
	uint32 ImageIndex = 0;
	VkResult Result = GetFunctions().vkAcquireNextImageKHR(LogicalDevice,
		Chain.SwapChain,
		std::numeric_limits<uint64>::max(),
		Slot.ImageAcquired,
		VK_NULL_HANDLE,
		&ImageIndex);
	if (Result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		// The window changed between the check above and now. The next call makes a swap chain that fits.
		Chain.bIsOutOfDate = true;
		return true;
	}
	if (Result != VK_SUCCESS && Result != VK_SUBOPTIMAL_KHR)
	{
		OutError = std::format(
			"the window did not provide a surface to show the picture on: {}", nvrhi::vulkan::resultToString(Result));
		return false;
	}
	// A surface that fits the window less than well is still shown; the next call makes a swap chain that fits.
	Chain.bIsOutOfDate = Result == VK_SUBOPTIMAL_KHR;

	const FImplementation::FImage& Image = Chain.Images[ImageIndex];
	nvrhi::ICommandList& CommandList = *Chain.CommandList;
	CommandList.open();

	nvrhi::GraphicsState State;
	State.pipeline = Chain.Pipeline;
	State.framebuffer = Image.Framebuffer;
	State.bindings.push_back(PictureBindings);
	State.viewport = nvrhi::ViewportState().addViewportAndScissorRect(
		nvrhi::Viewport(static_cast<float>(Chain.Width), static_cast<float>(Chain.Height)));
	CommandList.setGraphicsState(State);

	FPresentConstants Constants;
	Constants.EncodeSrgb = Chain.SurfaceFormat.bEncodesSrgb ? 0 : 1;
	CommandList.setPushConstants(&Constants, sizeof(FPresentConstants));
	CommandList.draw(nvrhi::DrawArguments().setVertexCount(3));
	CommandList.close();

	// The copy waits for the surface to be free, and the window system waits for the copy.
	nvrhi::vulkan::IDevice& VulkanDevice = *Chain.Context->GetVulkanDevice();
	const VkSemaphore PictureReady = Chain.PictureReady[ImageIndex];
	VulkanDevice.queueWaitForSemaphore(nvrhi::CommandQueue::Graphics, Slot.ImageAcquired, 0);
	VulkanDevice.queueSignalSemaphore(nvrhi::CommandQueue::Graphics, PictureReady, 0);
	Device.executeCommandList(&CommandList);
	Device.setEventQuery(Slot.Finished, nvrhi::CommandQueue::Graphics);
	Slot.bIsInFlight = true;
	Chain.NextFrameSlot = (Chain.NextFrameSlot + 1) % FrameSlotCount;

	VkPresentInfoKHR PresentInfo{};
	PresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	PresentInfo.waitSemaphoreCount = 1;
	PresentInfo.pWaitSemaphores = &PictureReady;
	PresentInfo.swapchainCount = 1;
	PresentInfo.pSwapchains = &Chain.SwapChain;
	PresentInfo.pImageIndices = &ImageIndex;
	Result = GetFunctions().vkQueuePresentKHR(Chain.Context->GetGraphicsQueue(), &PresentInfo);
	Device.runGarbageCollection();

	if (Result == VK_ERROR_OUT_OF_DATE_KHR || Result == VK_SUBOPTIMAL_KHR)
	{
		Chain.bIsOutOfDate = true;
	}
	else if (Result != VK_SUCCESS)
	{
		OutError = std::format("the window did not take the picture: {}", nvrhi::vulkan::resultToString(Result));
		return false;
	}

	++Chain.PresentedCount;
	return true;
}

int64 FSwapChain::GetPresentedCount() const
{
	return Implementation->PresentedCount;
}

int32 FSwapChain::GetWidth() const
{
	return Implementation->Width;
}

int32 FSwapChain::GetHeight() const
{
	return Implementation->Height;
}
