#include "EditorGraphics.h"

#include "Editor.h"
#include "Json/JsonValue.h"
#include "Rendering/RenderDevice.h"
#include "Rendering/RenderDeviceDescription.h"
#include "Rendering/TestRendering.h"
#include "TestEditorGraphics.h"
#include "TestEditorObjects.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>

#include <memory>
#include <ostream>
#include <string>

TEST_SUITE("FEditorGraphics")
{
	TEST_CASE("A new editor has no render device, so that a test can create one beside it")
	{
		const FScopedEditorTest EditorTest;
		const FScopedLogCapture Capture;
		const FEditor Editor;

		CHECK_FALSE(Editor.GetGraphics().IsOpen());
		const std::unique_ptr<FRenderDevice> Beside = CreateValidatedDevice();
		CHECK(Beside != nullptr);
		CHECK(CountLoggedErrors(Capture) == 0);
	}

	TEST_CASE("Open opens the render device once, and gives the same one after")
	{
		const FScopedEditorTest EditorTest;
		const FScopedLogCapture Capture;
		FEditor Editor;

		FRenderDevice& First = OpenValidatedGraphics(Editor);
		std::string Error;
		FRenderDevice* const Second = Editor.GetGraphics().Open(FRenderDeviceDescription(), Error);

		CHECK(Editor.GetGraphics().IsOpen());
		CHECK(Second == &First);
		CHECK(Error.empty());
		CHECK(CountLoggedErrors(Capture) == 0);
	}

	TEST_CASE("Open fails while another render device exists, opens nothing, and opens once that one is gone")
	{
		const FScopedEditorTest EditorTest;
		const FScopedLogCapture Capture;
		FEditor Editor;
		std::unique_ptr<FRenderDevice> Other = CreateValidatedDevice();
		std::string Error;

		CHECK(Editor.GetGraphics().Open(FRenderDeviceDescription(), Error) == nullptr);

		CHECK(Error == "a render device already exists, and only one can exist at a time");
		CHECK_FALSE(Editor.GetGraphics().IsOpen());
		Other.reset();
		OpenValidatedGraphics(Editor);
		CHECK(Editor.GetGraphics().IsOpen());
		CHECK(CountLoggedErrors(Capture) == 0);
	}

	TEST_CASE("The render device goes with the editor, so that another can be created after it")
	{
		const FScopedEditorTest EditorTest;
		const FScopedLogCapture Capture;
		{
			FEditor Editor;
			OpenValidatedGraphics(Editor);
		}

		const std::unique_ptr<FRenderDevice> After = CreateValidatedDevice();
		CHECK(After != nullptr);
		CHECK(CountLoggedErrors(Capture) == 0);
	}

	TEST_CASE("Commands that do not draw open no render device, a game that is played included")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;

		for (const char* const Name : {"Scene.GetHierarchy", "Play.Start", "Play.Step", "Play.Stop"})
		{
			CAPTURE(Name);
			REQUIRE(Editor.ExecuteCommand(Name, FJsonValue()).HasSucceeded());
		}
		REQUIRE(Editor.ExecuteCommand("Entity.Create", ParseTestJson(R"({"Name": "Player"})")).HasSucceeded());

		CHECK_FALSE(Editor.GetGraphics().IsOpen());
	}
}
