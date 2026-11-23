#include "Rendering/SwapChain.h"

#include "Math/LinearColor.h"
#include "Platform/Platform.h"
#include "Platform/ScopedHeadlessPlatform.h"
#include "Platform/Window.h"
#include "Rendering/RenderDevice.h"
#include "Rendering/RenderTarget.h"
#include "Rendering/TestRendering.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>

#include <memory>
#include <ostream>
#include <string>

// The tests run without a display, so the window a swap chain is made for exists in memory only. Whether pictures
// can be shown there is the graphics driver's to say: software drivers such as Lavapipe can, the drivers of many
// graphics cards cannot. Where they can, the swap chain is used as a program uses it. Where they cannot, it has to
// say so, and that is what is tested.

namespace
{
	std::unique_ptr<FWindow> CreateTestWindow(int32 Width, int32 Height)
	{
		FWindowDescription Description;
		Description.Width = Width;
		Description.Height = Height;
		std::string Error;
		std::unique_ptr<FWindow> Window = FWindow::Create(Description, Error);
		INFO("Error: ", Error);
		REQUIRE(Window != nullptr);
		return Window;
	}
}

TEST_SUITE("FSwapChain")
{
	TEST_CASE("A window in memory is shown pictures where the graphics driver can, and refused in words where not")
	{
		const FScopedHeadlessPlatform Platform;
		const FScopedLogCapture Capture;
		const std::unique_ptr<FRenderDevice> Device = CreateValidatedDevice();
		const std::unique_ptr<FWindow> Window = CreateTestWindow(320, 200);
		std::string Error;

		const std::unique_ptr<FSwapChain> SwapChain = FSwapChain::Create(*Device, *Window, Error);

		if (FSwapChain::IsSupported(*Device, *Window))
		{
			INFO("Error: ", Error);
			REQUIRE(SwapChain != nullptr);
			CHECK(SwapChain->GetWidth() == 320);
			CHECK(SwapChain->GetHeight() == 200);
			CHECK(SwapChain->GetPresentedCount() == 0);
		}
		else
		{
			CHECK(SwapChain == nullptr);
			CHECK(Error.find("cannot show pictures in a window that exists in memory only") != std::string::npos);
			CHECK(Error.find(Device->GetAdapterName()) != std::string::npos);
		}
		CHECK(CountLoggedErrors(Capture) == 0);
	}

	TEST_CASE("Pictures are shown one after another, more of them than the window has surfaces")
	{
		const FScopedHeadlessPlatform Platform;
		const FScopedLogCapture Capture;
		const std::unique_ptr<FRenderDevice> Device = CreateValidatedDevice();
		const std::unique_ptr<FWindow> Window = CreateTestWindow(320, 200);
		if (!FSwapChain::IsSupported(*Device, *Window))
		{
			return;
		}
		std::string Error;
		const std::unique_ptr<FSwapChain> SwapChain = FSwapChain::Create(*Device, *Window, Error);
		REQUIRE(SwapChain != nullptr);
		const std::unique_ptr<FRenderTarget> Picture = CreateTestTarget(*Device, 320, 200);

		for (int32 Frame = 0; Frame < 12; ++Frame)
		{
			// Each picture is another one, as in a program that draws.
			Picture->Clear(FLinearColor(static_cast<float>(Frame) / 12.0f, 0.5f, 0.25f));
			const bool bPresented = SwapChain->Present(*Picture, Error);
			INFO("Error: ", Error);
			REQUIRE(bPresented);
		}

		CHECK(SwapChain->GetPresentedCount() == 12);
		CHECK(CountLoggedErrors(Capture) == 0);
	}

	TEST_CASE("The swap chain follows the size of its window")
	{
		const FScopedHeadlessPlatform Platform;
		const FScopedLogCapture Capture;
		const std::unique_ptr<FRenderDevice> Device = CreateValidatedDevice();
		const std::unique_ptr<FWindow> Window = CreateTestWindow(320, 200);
		if (!FSwapChain::IsSupported(*Device, *Window))
		{
			return;
		}
		std::string Error;
		const std::unique_ptr<FSwapChain> SwapChain = FSwapChain::Create(*Device, *Window, Error);
		REQUIRE(SwapChain != nullptr);
		const std::unique_ptr<FRenderTarget> Picture = CreateTestTarget(*Device, 320, 200);
		REQUIRE(SwapChain->Present(*Picture, Error));

		Window->SetSize(640, 360);
		FPlatform::PollEvents();
		REQUIRE(Window->GetWidth() == 640);

		// The picture is still of the old size, as it is for a frame when a window is resized; it is shown in
		// the corner of the larger surface.
		REQUIRE(SwapChain->Present(*Picture, Error));
		CHECK(SwapChain->GetWidth() == 640);
		CHECK(SwapChain->GetHeight() == 360);

		const std::unique_ptr<FRenderTarget> LargerPicture = CreateTestTarget(*Device, 640, 360);
		REQUIRE(SwapChain->Present(*LargerPicture, Error));
		REQUIRE(SwapChain->Present(*Picture, Error));

		Window->SetSize(100, 80);
		FPlatform::PollEvents();
		// Now the picture is larger than the surface, and is cut off.
		REQUIRE(SwapChain->Present(*LargerPicture, Error));
		CHECK(SwapChain->GetWidth() == 100);
		CHECK(SwapChain->GetHeight() == 80);

		CHECK(SwapChain->GetPresentedCount() == 5);
		CHECK(CountLoggedErrors(Capture) == 0);
	}

	TEST_CASE("A swap chain keeps the graphics card alive after the render device is gone")
	{
		const FScopedHeadlessPlatform Platform;
		const FScopedLogCapture Capture;
		std::unique_ptr<FRenderDevice> Device = CreateValidatedDevice();
		const std::unique_ptr<FWindow> Window = CreateTestWindow(320, 200);
		if (!FSwapChain::IsSupported(*Device, *Window))
		{
			return;
		}
		std::string Error;
		std::unique_ptr<FSwapChain> SwapChain = FSwapChain::Create(*Device, *Window, Error);
		REQUIRE(SwapChain != nullptr);
		const std::unique_ptr<FRenderTarget> Picture = CreateTestTarget(*Device, 320, 200);

		Device.reset();

		REQUIRE(SwapChain->Present(*Picture, Error));
		CHECK(SwapChain->GetPresentedCount() == 1);

		// The swap chain goes before its window does.
		SwapChain.reset();
		CHECK(CountLoggedErrors(Capture) == 0);
	}

	TEST_CASE("Two windows can each have a swap chain")
	{
		const FScopedHeadlessPlatform Platform;
		const FScopedLogCapture Capture;
		const std::unique_ptr<FRenderDevice> Device = CreateValidatedDevice();
		const std::unique_ptr<FWindow> First = CreateTestWindow(320, 200);
		const std::unique_ptr<FWindow> Second = CreateTestWindow(200, 320);
		if (!FSwapChain::IsSupported(*Device, *First))
		{
			return;
		}
		std::string Error;
		const std::unique_ptr<FSwapChain> FirstChain = FSwapChain::Create(*Device, *First, Error);
		const std::unique_ptr<FSwapChain> SecondChain = FSwapChain::Create(*Device, *Second, Error);
		REQUIRE(FirstChain != nullptr);
		REQUIRE(SecondChain != nullptr);
		const std::unique_ptr<FRenderTarget> Picture = CreateTestTarget(*Device, 320, 320);

		for (int32 Frame = 0; Frame < 4; ++Frame)
		{
			REQUIRE(FirstChain->Present(*Picture, Error));
			REQUIRE(SecondChain->Present(*Picture, Error));
		}

		CHECK(FirstChain->GetPresentedCount() == 4);
		CHECK(SecondChain->GetPresentedCount() == 4);
		CHECK(SecondChain->GetWidth() == 200);
		CHECK(SecondChain->GetHeight() == 320);
		CHECK(CountLoggedErrors(Capture) == 0);
	}
}
