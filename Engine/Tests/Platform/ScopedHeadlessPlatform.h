#pragma once

#include "Platform/Platform.h"

#include <doctest/doctest.h>

#include <string>

/**
 * Runs the platform layer in headless mode for the duration of a test, so that the test can create windows
 * without anything appearing on a screen, and on a machine that has no screen.
 */
class FScopedHeadlessPlatform
{
public:
	FScopedHeadlessPlatform()
	{
		std::string Error;
		const bool bInitialized = FPlatform::Initialize(EPlatformMode::Headless, Error);
		INFO("Error: ", Error);
		REQUIRE(bInitialized);
	}

	~FScopedHeadlessPlatform()
	{
		FPlatform::Shutdown();
	}

	FScopedHeadlessPlatform(const FScopedHeadlessPlatform&) = delete;
	FScopedHeadlessPlatform& operator=(const FScopedHeadlessPlatform&) = delete;
};
