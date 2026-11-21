#pragma once

#include "CoreMinimal.h"

#include "Logging/LogMessage.h"
#include "Math/Color.h"
#include "Rendering/Image.h"
#include "Rendering/RenderDevice.h"
#include "Rendering/RenderTarget.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>

#include <cstdlib>
#include <memory>
#include <string>

/** Creates the render device with validation on, which a test expects to work. */
inline std::unique_ptr<FRenderDevice> CreateValidatedDevice()
{
	std::string Error;
	std::unique_ptr<FRenderDevice> Device =
		FRenderDevice::Create(FRenderDeviceDescription{.bEnableValidation = true}, Error);
	INFO("Error: ", Error);
	REQUIRE(Device != nullptr);
	return Device;
}

/** Creates a render target, which a test expects to work. */
inline std::unique_ptr<FRenderTarget> CreateTestTarget(FRenderDevice& Device, int32 Width, int32 Height)
{
	std::string Error;
	std::unique_ptr<FRenderTarget> Target = Device.CreateRenderTarget(Width, Height, Error);
	INFO("Error: ", Error);
	REQUIRE(Target != nullptr);
	return Target;
}

/** Reads back what a render target shows, which a test expects to work. */
inline FImage ReadTestTarget(const FRenderTarget& Target)
{
	FImage Image;
	std::string Error;
	const bool bRead = Target.ReadPixels(Image, Error);
	INFO("Error: ", Error);
	REQUIRE(bRead);
	return Image;
}

/** Graphics drivers may round a colour to either neighbouring byte value when they encode it. */
inline bool IsWithinOne(FColor Actual, FColor Expected)
{
	return std::abs(Actual.R - Expected.R) <= 1 && std::abs(Actual.G - Expected.G) <= 1 &&
		std::abs(Actual.B - Expected.B) <= 1 && std::abs(Actual.A - Expected.A) <= 1;
}

/**
 * Returns the number of errors in the log, which is where the validation of rendering calls reports, and fails the
 * test for each of them.
 */
inline int32 CountLoggedErrors(const FScopedLogCapture& Capture)
{
	int32 ErrorCount = 0;
	for (const FLogMessage& Message : Capture.GetMessages())
	{
		if (Message.Verbosity == ELogVerbosity::Error)
		{
			INFO("Logged error: ", Message.Text);
			CHECK(false);
			++ErrorCount;
		}
	}
	return ErrorCount;
}
