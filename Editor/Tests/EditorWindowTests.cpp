#include "EditorWindow.h"

#include "Core/FileHelper.h"
#include "Editor.h"
#include "Input/InputEvent.h"
#include "Input/InputState.h"
#include "Input/MouseButton.h"
#include "Math/Color.h"
#include "Platform/Platform.h"
#include "Platform/ScopedHeadlessPlatform.h"
#include "Platform/Window.h"
#include "Rendering/Image.h"
#include "Rendering/RenderTarget.h"
#include "Rendering/TestRendering.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "TestEditorObjects.h"
#include "TestSupport/ScopedLogCapture.h"
#include "TestSupport/ScopedTempDirectory.h"
#include "Ui/EditorUi.h"
#include "Ui/UiRectangle.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <format>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

namespace
{
	/** The editor's window, in memory, for one test. Rendering is validated. */
	class FEditorWindowTest
	{
	public:
		static constexpr double FrameSeconds = 1.0 / 60.0;

		explicit FEditorWindowTest(int32 Width = 800, int32 Height = 600)
		{
			FEditorWindowDescription Description;
			Description.Width = Width;
			Description.Height = Height;
			Description.bValidateRendering = true;
			std::string Error;
			Window = FEditorWindow::Create(Editor, Description, Error);
			INFO("Error: ", Error);
			REQUIRE(Window != nullptr);
		}

		FEditorWindowTest(const FEditorWindowTest&) = delete;
		FEditorWindowTest& operator=(const FEditorWindowTest&) = delete;

		/** Runs one frame as the editor's executable does: what the window system reports, then the frame. */
		void RunFrame()
		{
			FPlatform::PollEvents();
			DrawFrame();
		}

		/** Draws a frame with the input that the test has given the window since the events were last polled. */
		void DrawFrame()
		{
			std::string Error;
			const bool bDrawn = Window->RunFrame(FrameSeconds, Error);
			INFO("Error: ", Error);
			REQUIRE(bDrawn);
		}

		void RunFrames(int32 Count)
		{
			for (int32 Index = 0; Index < Count; ++Index)
			{
				RunFrame();
			}
		}

	private:
		FScopedEditorTest EditorTest;
		FScopedHeadlessPlatform Platform;

	public:
		FScopedLogCapture Capture;
		FEditor Editor;
		std::unique_ptr<FEditorWindow> Window;
	};
}

TEST_SUITE("FEditorWindow")
{
	TEST_CASE("The editor's window opens at the size it is asked for, with the scene in its title")
	{
		FEditorWindowTest Test(800, 600);

		CHECK(Test.Window->GetWindow().GetWidth() == 800);
		CHECK(Test.Window->GetWindow().GetHeight() == 600);
		CHECK(Test.Window->GetWindow().GetTitle() == "Untitled - White Wolf Editor");
		CHECK(Test.Window->HasArea());
		CHECK_FALSE(Test.Window->ShouldClose());
		CHECK(Test.Window->GetPicture() == nullptr);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("A frame draws the editor's user interface at the size of the window")
	{
		FEditorWindowTest Test(800, 600);

		Test.RunFrames(3);

		const FRenderTarget* const Picture = Test.Window->GetPicture();
		REQUIRE(Picture != nullptr);
		CHECK(Picture->GetWidth() == 800);
		CHECK(Picture->GetHeight() == 600);

		// The panels are there, inside the picture, and the picture is of more than one colour.
		const std::optional<FUiRectangle> Viewport = Test.Window->GetUi().FindItem("Viewport");
		REQUIRE(Viewport.has_value());
		CHECK(Viewport->Right <= 800.0f);
		CHECK(Viewport->Bottom <= 600.0f);
		const FImage Image = ReadTestTarget(*Picture);
		const std::vector<FColor>& Pixels = Image.GetPixels();
		CHECK(std::ranges::any_of(Pixels, [&Pixels](const FColor& Pixel) { return Pixel != Pixels.front(); }));
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("The picture follows the size of the window")
	{
		FEditorWindowTest Test(800, 600);
		Test.RunFrame();

		Test.Window->GetWindow().SetSize(1024, 500);
		Test.RunFrames(2);

		REQUIRE(Test.Window->GetPicture() != nullptr);
		CHECK(Test.Window->GetPicture()->GetWidth() == 1024);
		CHECK(Test.Window->GetPicture()->GetHeight() == 500);
		const std::optional<FUiRectangle> Viewport = Test.Window->GetUi().FindItem("Viewport");
		REQUIRE(Viewport.has_value());
		CHECK(Viewport->Bottom > 490.0f);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("The title follows the name of the scene and of the project")
	{
		FEditorWindowTest Test;
		const FScopedTempDirectory Directory;

		Test.Editor.GetScene().SetName("Level 1");
		Test.RunFrame();
		CHECK(Test.Window->GetWindow().GetTitle() == "Level 1 - White Wolf Editor");

		FJsonValue Arguments = FJsonValue::MakeObject();
		Arguments.SetMember("Directory", FFileHelper::PathToUtf8(Directory.GetPath()));
		Arguments.SetMember("Name", "Tetris");
		REQUIRE(Test.Editor.ExecuteCommand("Project.New", Arguments).HasSucceeded());
		Test.RunFrame();
		CHECK(Test.Window->GetWindow().GetTitle() == "Untitled - Tetris - White Wolf Editor");
	}

	TEST_CASE("What the mouse of the window does reaches the user interface")
	{
		FEditorWindowTest Test;
		const int64 Player = Test.Editor.GetScene().CreateEntity("Player")->GetId();
		Test.RunFrames(3);
		const std::optional<FUiRectangle> Row = Test.Window->GetUi().FindItem(std::format("Outliner/{}", Player));
		REQUIRE(Row.has_value());
		FInputState& Input = Test.Window->GetWindow().GetInput();

		// Each step is what the window system would report between two frames.
		FPlatform::PollEvents();
		Input.HandleMouseMove(Row->GetCenter());
		Test.DrawFrame();
		FPlatform::PollEvents();
		Input.HandleMouseButtonEvent(EMouseButton::Left, EInputEvent::Pressed);
		Test.DrawFrame();
		FPlatform::PollEvents();
		Input.HandleMouseButtonEvent(EMouseButton::Left, EInputEvent::Released);
		Test.DrawFrame();
		Test.RunFrames(2);

		CHECK(Test.Editor.GetSelection() == std::vector<int64>{Player});
	}

	TEST_CASE("Frames are shown in a window in memory only where the graphics driver can show pictures there")
	{
		FEditorWindowTest Test;

		Test.RunFrames(5);

		// Five frames were drawn either way. Where they cannot be shown, none was.
		const int64 ShownFrameCount = Test.Window->GetShownFrameCount();
		const bool bIsEitherAllOrNone = ShownFrameCount == 5 || ShownFrameCount == 0;
		CHECK(bIsEitherAllOrNone);
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}

	TEST_CASE("The user interface has the scale of the display the window is on")
	{
		FEditorWindowTest Test;
		Test.RunFrame();
		// A window in memory is on no display, and its scale is one.
		REQUIRE(Test.Window->GetWindow().GetContentScale() == 1.0f);
		CHECK(Test.Window->GetUi().GetScale() == 1.0f);

		// Whatever scale the user interface was given, a frame gives it the window's.
		Test.Window->GetUi().SetScale(2.0f);
		Test.RunFrame();

		CHECK(Test.Window->GetUi().GetScale() == 1.0f);
	}

	TEST_CASE("The editor's window can open maximised")
	{
		const FScopedEditorTest EditorTest;
		const FScopedHeadlessPlatform Platform;
		FEditor Editor;
		FEditorWindowDescription Description;
		Description.Width = 640;
		Description.Height = 360;
		Description.bMaximized = true;
		std::string Error;

		const std::unique_ptr<FEditorWindow> Window = FEditorWindow::Create(Editor, Description, Error);

		INFO("Error: ", Error);
		REQUIRE(Window != nullptr);
		CHECK(Window->GetWindow().GetWidth() >= 640);
		CHECK(Window->GetWindow().GetHeight() >= 360);
		FPlatform::PollEvents();
		CHECK(Window->RunFrame(1.0 / 60.0, Error));
		REQUIRE(Window->GetPicture() != nullptr);
		CHECK(Window->GetPicture()->GetWidth() == Window->GetWindow().GetWidth());
	}

	TEST_CASE("A window that was asked to close says so")
	{
		FEditorWindowTest Test;
		Test.RunFrame();

		Test.Window->GetWindow().RequestClose();

		CHECK(Test.Window->ShouldClose());
	}

	TEST_CASE("The editor's window cannot be opened while the platform layer is not running")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		std::string Error;

		const std::unique_ptr<FEditorWindow> Window = FEditorWindow::Create(Editor, FEditorWindowDescription(), Error);

		CHECK(Window == nullptr);
		CHECK(Error.find("FPlatform::Initialize") != std::string::npos);
	}

	TEST_CASE("The editor's window cannot be opened with a size of nothing")
	{
		const FScopedEditorTest EditorTest;
		const FScopedHeadlessPlatform Platform;
		FEditor Editor;
		FEditorWindowDescription Description;
		Description.Width = 0;
		std::string Error;

		const std::unique_ptr<FEditorWindow> Window = FEditorWindow::Create(Editor, Description, Error);

		CHECK(Window == nullptr);
		CHECK(Error == "a window cannot be 0 by 900 in size");
	}

	TEST_CASE("The editor's window can be opened again after it was closed")
	{
		{
			FEditorWindowTest First;
			First.RunFrame();
		}

		FEditorWindowTest Second;
		Second.RunFrame();

		CHECK(Second.Window->GetPicture() != nullptr);
		CHECK(CountLoggedErrors(Second.Capture) == 0);
	}
}
