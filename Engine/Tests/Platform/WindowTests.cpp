#include "Platform/Window.h"

#include "Platform/Platform.h"
#include "Platform/ScopedHeadlessPlatform.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>

#include <memory>
#include <ostream>
#include <string>

TEST_SUITE("FWindow")
{
	TEST_CASE("A window is created with the title and size of its description")
	{
		const FScopedHeadlessPlatform Platform;
		FWindowDescription Description;
		Description.Title = "Test Window";
		Description.Width = 640;
		Description.Height = 360;
		std::string Error;

		const std::unique_ptr<FWindow> Window = FWindow::Create(Description, Error);

		REQUIRE(Window != nullptr);
		CHECK(Error.empty());
		CHECK(Window->GetTitle() == "Test Window");
		CHECK(Window->GetWidth() == 640);
		CHECK(Window->GetHeight() == 360);
		CHECK_FALSE(Window->ShouldClose());
	}

	TEST_CASE("The default description gives a usable window")
	{
		const FScopedHeadlessPlatform Platform;
		std::string Error;

		const std::unique_ptr<FWindow> Window = FWindow::Create(FWindowDescription(), Error);

		REQUIRE(Window != nullptr);
		CHECK(Window->GetTitle() == "White Wolf Engine");
		CHECK(Window->GetWidth() == 1280);
		CHECK(Window->GetHeight() == 720);
	}

	TEST_CASE("Creating a window without the platform layer fails with an explanation")
	{
		std::string Error;

		const std::unique_ptr<FWindow> Window = FWindow::Create(FWindowDescription(), Error);

		CHECK(Window == nullptr);
		CHECK(Error.find("FPlatform::Initialize") != std::string::npos);
	}

	TEST_CASE("A window cannot have a size of zero or less")
	{
		const FScopedHeadlessPlatform Platform;
		FWindowDescription ZeroWidth;
		ZeroWidth.Width = 0;
		FWindowDescription NegativeHeight;
		NegativeHeight.Height = -720;
		std::string Error;

		CHECK(FWindow::Create(ZeroWidth, Error) == nullptr);
		CHECK(Error == "a window cannot be 0 by 720 in size");
		CHECK(FWindow::Create(NegativeHeight, Error) == nullptr);
		CHECK(Error == "a window cannot be 1280 by -720 in size");
	}

	TEST_CASE("The title can be changed, including to text outside ASCII")
	{
		const FScopedHeadlessPlatform Platform;
		std::string Error;
		const std::unique_ptr<FWindow> Window = FWindow::Create(FWindowDescription(), Error);
		REQUIRE(Window != nullptr);

		Window->SetTitle("Sc\xC3\xA8ne 1");

		CHECK(Window->GetTitle() == "Sc\xC3\xA8ne 1");
	}

	TEST_CASE("SetSize resizes the window")
	{
		const FScopedHeadlessPlatform Platform;
		std::string Error;
		const std::unique_ptr<FWindow> Window = FWindow::Create(FWindowDescription(), Error);
		REQUIRE(Window != nullptr);

		Window->SetSize(800, 600);
		FPlatform::PollEvents();

		CHECK(Window->GetWidth() == 800);
		CHECK(Window->GetHeight() == 600);
	}

	TEST_CASE("SetSize ignores a size of zero or less, with a warning")
	{
		const FScopedHeadlessPlatform Platform;
		const FScopedLogCapture Capture;
		std::string Error;
		const std::unique_ptr<FWindow> Window = FWindow::Create(FWindowDescription(), Error);
		REQUIRE(Window != nullptr);

		Window->SetSize(0, 600);
		Window->SetSize(800, -1);
		FPlatform::PollEvents();

		CHECK(Window->GetWidth() == 1280);
		CHECK(Window->GetHeight() == 720);
		CHECK(Capture.GetMessages().size() == 2);
	}

	TEST_CASE("RequestClose marks the window as wanting to close")
	{
		const FScopedHeadlessPlatform Platform;
		std::string Error;
		const std::unique_ptr<FWindow> Window = FWindow::Create(FWindowDescription(), Error);
		REQUIRE(Window != nullptr);

		Window->RequestClose();

		CHECK(Window->ShouldClose());
	}

	TEST_CASE("A window that exists in memory only has no handle in the operating system's window system")
	{
		const FScopedHeadlessPlatform Platform;
		std::string Error;
		const std::unique_ptr<FWindow> Window = FWindow::Create(FWindowDescription(), Error);
		REQUIRE(Window != nullptr);

		CHECK(Window->GetNativeHandle() == nullptr);
	}

	TEST_CASE("Each window has its own input state")
	{
		const FScopedHeadlessPlatform Platform;
		std::string Error;
		const std::unique_ptr<FWindow> First = FWindow::Create(FWindowDescription(), Error);
		const std::unique_ptr<FWindow> Second = FWindow::Create(FWindowDescription(), Error);
		REQUIRE(First != nullptr);
		REQUIRE(Second != nullptr);

		First->GetInput().HandleKeyEvent(EKey::Space, EInputEvent::Pressed);

		CHECK(First->GetInput().IsKeyDown(EKey::Space));
		CHECK_FALSE(Second->GetInput().IsKeyDown(EKey::Space));
		const FWindow& ConstFirst = *First;
		CHECK(ConstFirst.GetInput().IsKeyDown(EKey::Space));
	}

	TEST_CASE("Windows can be destroyed in any order")
	{
		const FScopedHeadlessPlatform Platform;
		std::string Error;
		std::unique_ptr<FWindow> First = FWindow::Create(FWindowDescription(), Error);
		std::unique_ptr<FWindow> Second = FWindow::Create(FWindowDescription(), Error);
		std::unique_ptr<FWindow> Third = FWindow::Create(FWindowDescription(), Error);
		REQUIRE(Third != nullptr);

		Second.reset();
		FPlatform::PollEvents();
		First.reset();
		FPlatform::PollEvents();

		CHECK_FALSE(Third->ShouldClose());
	}
}
