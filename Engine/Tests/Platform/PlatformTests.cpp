#include "Platform/Platform.h"

#include "Platform/ScopedHeadlessPlatform.h"
#include "Platform/Window.h"
#include "TestSupport/FatalErrorException.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>

#include <memory>
#include <string>

TEST_SUITE("FPlatform")
{
	TEST_CASE("The platform layer is not initialized until asked")
	{
		CHECK_FALSE(FPlatform::IsInitialized());
	}

	TEST_CASE("Headless mode starts without a display and stops again")
	{
		std::string Error;

		REQUIRE(FPlatform::Initialize(EPlatformMode::Headless, Error));
		CHECK(FPlatform::IsInitialized());
		CHECK(FPlatform::GetMode() == EPlatformMode::Headless);
		CHECK(Error.empty());

		FPlatform::Shutdown();
		CHECK_FALSE(FPlatform::IsInitialized());
	}

	TEST_CASE("The platform layer can be started again after a shutdown")
	{
		for (int32 Round = 0; Round < 3; ++Round)
		{
			const FScopedHeadlessPlatform Platform;

			CHECK(FPlatform::IsInitialized());
		}

		CHECK_FALSE(FPlatform::IsInitialized());
	}

	TEST_CASE("Shutting down a platform layer that is not running does nothing")
	{
		FPlatform::Shutdown();
		FPlatform::Shutdown();

		CHECK_FALSE(FPlatform::IsInitialized());
	}

	TEST_CASE("PollEvents can be called every frame with and without windows")
	{
		const FScopedHeadlessPlatform Platform;
		std::string Error;

		FPlatform::PollEvents();
		const std::unique_ptr<FWindow> Window = FWindow::Create(FWindowDescription(), Error);
		REQUIRE(Window != nullptr);
		FPlatform::PollEvents();
		FPlatform::PollEvents();

		CHECK_FALSE(Window->ShouldClose());
	}

	TEST_CASE("PollEvents starts a new input frame for every window")
	{
		const FScopedHeadlessPlatform Platform;
		std::string Error;
		const std::unique_ptr<FWindow> First = FWindow::Create(FWindowDescription(), Error);
		const std::unique_ptr<FWindow> Second = FWindow::Create(FWindowDescription(), Error);
		REQUIRE(First != nullptr);
		REQUIRE(Second != nullptr);
		First->GetInput().HandleKeyEvent(EKey::W, EInputEvent::Pressed);
		Second->GetInput().HandleScroll(FVector2D(0.0, 3.0));

		FPlatform::PollEvents();

		// The key is still held, but it is no longer newly pressed, and the scroll movement is over.
		CHECK(First->GetInput().IsKeyDown(EKey::W));
		CHECK_FALSE(First->GetInput().WasKeyPressed(EKey::W));
		CHECK(Second->GetInput().GetScrollDelta() == FVector2D(0.0));
	}

	TEST_CASE("Using the platform layer in the wrong state is a fatal error")
	{
		const FScopedLogCapture Capture;
		std::string Error;

		CHECK_THROWS_AS(FPlatform::GetMode(), FFatalErrorException);
		CHECK_THROWS_AS(FPlatform::PollEvents(), FFatalErrorException);

		{
			const FScopedHeadlessPlatform Platform;
			CHECK_THROWS_AS(FPlatform::Initialize(EPlatformMode::Headless, Error), FFatalErrorException);

			const std::unique_ptr<FWindow> Window = FWindow::Create(FWindowDescription(), Error);
			REQUIRE(Window != nullptr);
			CHECK_THROWS_AS(FPlatform::Shutdown(), FFatalErrorException);
			CHECK(FPlatform::IsInitialized());
		}

		CHECK_FALSE(FPlatform::IsInitialized());
	}
}
