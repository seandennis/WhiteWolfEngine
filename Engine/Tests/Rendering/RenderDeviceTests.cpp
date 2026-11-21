#include "Rendering/RenderDevice.h"

#include "Rendering/Image.h"
#include "Rendering/RenderTarget.h"
#include "Rendering/TestRendering.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>

#include <memory>
#include <ostream>
#include <string>

TEST_SUITE("FRenderDevice")
{
	TEST_CASE("A render device is created without a window and names its graphics card")
	{
		const FScopedLogCapture Capture;

		const std::unique_ptr<FRenderDevice> Device = CreateValidatedDevice();

		CHECK_FALSE(Device->GetAdapterName().empty());
		CHECK(CountLoggedErrors(Capture) == 0);
	}

	TEST_CASE("Only one render device can exist at a time")
	{
		const std::unique_ptr<FRenderDevice> Device = CreateValidatedDevice();
		std::string Error;

		const std::unique_ptr<FRenderDevice> Second = FRenderDevice::Create(FRenderDeviceDescription(), Error);

		CHECK(Second == nullptr);
		CHECK(Error == "a render device already exists, and only one can exist at a time");
	}

	TEST_CASE("A render device can be created again after the previous one is gone")
	{
		for (int32 Round = 0; Round < 3; ++Round)
		{
			const std::unique_ptr<FRenderDevice> Device = CreateValidatedDevice();

			CHECK_FALSE(Device->GetAdapterName().empty());
		}
	}

	TEST_CASE("A render device works without validation too")
	{
		std::string Error;

		const std::unique_ptr<FRenderDevice> Device = FRenderDevice::Create(FRenderDeviceDescription(), Error);

		INFO("Error: ", Error);
		REQUIRE(Device != nullptr);
		const std::unique_ptr<FRenderTarget> Target = CreateTestTarget(*Device, 4, 4);
		Target->Clear(FLinearColor(1.0f, 1.0f, 1.0f));
		CHECK(ReadTestTarget(*Target).GetPixel(3, 3) == FColor(255, 255, 255));
	}
}

TEST_SUITE("FRenderTarget")
{
	TEST_CASE("A render target has the size it was created with")
	{
		const std::unique_ptr<FRenderDevice> Device = CreateValidatedDevice();

		const std::unique_ptr<FRenderTarget> Target = CreateTestTarget(*Device, 64, 32);

		CHECK(Target->GetWidth() == 64);
		CHECK(Target->GetHeight() == 32);
	}

	TEST_CASE("A render target cannot have a side of less than one pixel or more than the maximum")
	{
		const std::unique_ptr<FRenderDevice> Device = CreateValidatedDevice();
		std::string Error;

		CHECK(Device->CreateRenderTarget(0, 32, Error) == nullptr);
		CHECK(Error == "a render target cannot be 0 by 32 pixels; each side must be between 1 and 16384");
		CHECK(Device->CreateRenderTarget(32, -1, Error) == nullptr);
		CHECK(Device->CreateRenderTarget(FRenderTarget::MaxDimension + 1, 32, Error) == nullptr);
		CHECK(Device->CreateRenderTarget(32, FRenderTarget::MaxDimension + 1, Error) == nullptr);
	}

	TEST_CASE("A cleared render target reads back as that colour in every pixel")
	{
		const FScopedLogCapture Capture;
		const std::unique_ptr<FRenderDevice> Device = CreateValidatedDevice();
		const std::unique_ptr<FRenderTarget> Target = CreateTestTarget(*Device, 17, 9);

		Target->Clear(FLinearColor(1.0f, 0.0f, 0.0f, 1.0f));
		const FImage Image = ReadTestTarget(*Target);

		REQUIRE(Image.GetWidth() == 17);
		REQUIRE(Image.GetHeight() == 9);
		CHECK(Image == FImage(17, 9, FColor(255, 0, 0, 255)));
		CHECK(CountLoggedErrors(Capture) == 0);
	}

	TEST_CASE("Each channel of the clear colour arrives in its own channel")
	{
		const std::unique_ptr<FRenderDevice> Device = CreateValidatedDevice();
		const std::unique_ptr<FRenderTarget> Target = CreateTestTarget(*Device, 8, 8);

		Target->Clear(FLinearColor(0.0f, 1.0f, 0.0f, 0.0f));
		CHECK(ReadTestTarget(*Target).GetPixel(4, 4) == FColor(0, 255, 0, 0));

		Target->Clear(FLinearColor(0.0f, 0.0f, 1.0f, 1.0f));
		CHECK(ReadTestTarget(*Target).GetPixel(4, 4) == FColor(0, 0, 255, 255));
	}

	TEST_CASE("Colours are given in linear space and stored sRGB-encoded; alpha is stored as it is")
	{
		const std::unique_ptr<FRenderDevice> Device = CreateValidatedDevice();
		const std::unique_ptr<FRenderTarget> Target = CreateTestTarget(*Device, 8, 8);

		// Linear 0.5 is 188 and linear 0.25 is 137 once encoded as sRGB; alpha 0.5 is simply 128.
		Target->Clear(FLinearColor(0.5f, 0.25f, 0.0f, 0.5f));
		const FColor Pixel = ReadTestTarget(*Target).GetPixel(0, 0);

		INFO("Pixel: ",
			static_cast<int32>(Pixel.R),
			", ",
			static_cast<int32>(Pixel.G),
			", ",
			static_cast<int32>(Pixel.B),
			", ",
			static_cast<int32>(Pixel.A));
		CHECK(IsWithinOne(Pixel, FColor(188, 137, 0, 128)));
	}

	TEST_CASE("A render target can be read more than once and cleared again in between")
	{
		const std::unique_ptr<FRenderDevice> Device = CreateValidatedDevice();
		const std::unique_ptr<FRenderTarget> Target = CreateTestTarget(*Device, 5, 5);

		Target->Clear(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
		const FImage First = ReadTestTarget(*Target);
		const FImage Again = ReadTestTarget(*Target);
		Target->Clear(FLinearColor(0.0f, 0.0f, 0.0f, 1.0f));
		const FImage Second = ReadTestTarget(*Target);

		CHECK(First == FImage(5, 5, FColor(255, 255, 255, 255)));
		CHECK(Again == First);
		CHECK(Second == FImage(5, 5, FColor(0, 0, 0, 255)));
	}

	TEST_CASE("Render targets are independent of each other")
	{
		const std::unique_ptr<FRenderDevice> Device = CreateValidatedDevice();
		const std::unique_ptr<FRenderTarget> Red = CreateTestTarget(*Device, 4, 4);
		const std::unique_ptr<FRenderTarget> Blue = CreateTestTarget(*Device, 6, 2);

		Red->Clear(FLinearColor(1.0f, 0.0f, 0.0f));
		Blue->Clear(FLinearColor(0.0f, 0.0f, 1.0f));

		CHECK(ReadTestTarget(*Red) == FImage(4, 4, FColor(255, 0, 0)));
		CHECK(ReadTestTarget(*Blue) == FImage(6, 2, FColor(0, 0, 255)));
	}

	TEST_CASE("A large render target is read back completely, including its last row and column")
	{
		const std::unique_ptr<FRenderDevice> Device = CreateValidatedDevice();
		const std::unique_ptr<FRenderTarget> Target = CreateTestTarget(*Device, 1021, 613);

		Target->Clear(FLinearColor(0.0f, 1.0f, 1.0f));
		const FImage Image = ReadTestTarget(*Target);

		CHECK(Image.GetPixel(0, 0) == FColor(0, 255, 255));
		CHECK(Image.GetPixel(1020, 0) == FColor(0, 255, 255));
		CHECK(Image.GetPixel(0, 612) == FColor(0, 255, 255));
		CHECK(Image.GetPixel(1020, 612) == FColor(0, 255, 255));
	}

	TEST_CASE("A render target keeps the graphics card alive after its device object is gone")
	{
		const FScopedLogCapture Capture;
		std::unique_ptr<FRenderDevice> Device = CreateValidatedDevice();
		const std::unique_ptr<FRenderTarget> Target = CreateTestTarget(*Device, 4, 4);
		std::string Error;

		Device.reset();
		Target->Clear(FLinearColor(1.0f, 1.0f, 0.0f));

		CHECK(ReadTestTarget(*Target) == FImage(4, 4, FColor(255, 255, 0)));
		// The graphics card is still in use, so a second device cannot be created yet.
		CHECK(FRenderDevice::Create(FRenderDeviceDescription(), Error) == nullptr);
		CHECK(CountLoggedErrors(Capture) == 0);
	}
}
