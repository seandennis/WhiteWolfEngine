#include "Editor.h"

#include "Commands/CommandResult.h"
#include "Core/FileHelper.h"
#include "EditorGraphics.h"
#include "Json/JsonValue.h"
#include "Math/Color.h"
#include "Math/LinearColor.h"
#include "Math/Srgb.h"
#include "Rendering/Image.h"
#include "Rendering/ImageDifference.h"
#include "Rendering/ImageFile.h"
#include "Rendering/RenderDevice.h"
#include "Rendering/SceneViewMode.h"
#include "Rendering/TestRendering.h"
#include "TestEditorGraphics.h"
#include "TestEditorObjects.h"
#include "TestSupport/ScopedLogCapture.h"
#include "TestSupport/ScopedTempDirectory.h"
#include "Ui/TestEditorUi.h"
#include "Ui/UiRectangle.h"
#include "Viewport/ViewportCamera.h"
#include "Viewport/ViewportPicture.h"

#include <doctest/doctest.h>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	/** Gives the editor a command with arguments written as JSON text, which the test expects to succeed. */
	FCommandResult ExecuteOrFail(FEditor& Editor, std::string_view Name, std::string_view ArgumentText = "{}")
	{
		FCommandResult Result = Editor.ExecuteCommand(Name, ParseTestJson(ArgumentText));
		INFO("Command ", std::string(Name), ": ", Result.GetError());
		REQUIRE(Result.HasSucceeded());
		return Result;
	}

	/**
	 * Puts a cube that the engine has built in, white and a metre wide, at the origin, and has the viewport look at
	 * it from five metres in front, unlit. Returns the identifier of the cube's entity.
	 */
	int64 SetUpCube(FEditor& Editor)
	{
		const FCommandResult Created = ExecuteOrFail(Editor, "Entity.Create", R"({"Name": "Cube"})");
		const int64 CubeId = Created.GetValue().FindMember("Id")->GetInteger();
		const FCommandResult Added = ExecuteOrFail(
			Editor, "Component.Add", std::format(R"({{"Entity": {}, "Class": "WStaticMeshComponent"}})", CubeId));
		ExecuteOrFail(Editor,
			"Object.SetProperty",
			std::format(R"({{"Object": {}, "Property": "Mesh", "Value": "Engine/Shapes#Cube"}})",
				Added.GetValue().FindMember("Id")->GetInteger()));
		ExecuteOrFail(Editor, "Viewport.SetCamera", R"({"Location": [0, 0, 5], "LookAt": [0, 0, 0]})");
		ExecuteOrFail(Editor, "Viewport.SetViewMode", R"({"Mode": "Unlit"})");
		return CubeId;
	}

	/** Returns the picture that a capture returned, which the test expects it to have. */
	FImage ReadReturnedPicture(const FCommandResult& Result)
	{
		REQUIRE_FALSE(Result.GetPicture().empty());
		FImage Picture;
		std::string Error;
		const bool bIsRead = FImageFile::LoadFromMemory(Result.GetPicture(), Picture, Error);
		INFO("Error: ", Error);
		REQUIRE(bIsRead);
		return Picture;
	}

	/** Returns the byte that what the viewport shows where the scene has nothing comes out as, unlit. */
	uint8 GetEmptySpaceByte()
	{
		const float Encoded = FSrgb::Encode(FViewportPicture::GetEmptySpaceColor(ESceneViewMode::Unlit).R);
		return static_cast<uint8>(std::lround(Encoded * 255.0f));
	}

	/** Returns true if each channel of a colour, but alpha, is within one of a grey of the given byte. */
	bool IsGrey(const FColor& Color, uint8 Byte)
	{
		return std::abs(Color.R - Byte) <= 1 && std::abs(Color.G - Byte) <= 1 && std::abs(Color.B - Byte) <= 1;
	}

	/** An editor whose render device is open, with validation, and the cube of SetUpCube. */
	struct FCaptureTest
	{
		FCaptureTest()
		{
			OpenValidatedGraphics(Editor);
			CubeId = SetUpCube(Editor);
		}

		/** Takes a picture with arguments written as JSON text, which the test expects to be taken. */
		FCommandResult Capture(std::string_view ArgumentText = "{}")
		{
			return ExecuteOrFail(Editor, "Viewport.Capture", ArgumentText);
		}

		FScopedEditorTest EditorTest;
		FScopedLogCapture LogCapture;
		FEditor Editor;
		int64 CubeId = 0;
	};
}

TEST_SUITE("Viewport commands capture")
{
	TEST_CASE("Viewport.Capture returns a picture of the scene as the viewport's camera sees it")
	{
		FCaptureTest Test;

		const FCommandResult Result = Test.Capture(R"({"Width": 64, "Height": 36})");

		CHECK(WriteTestJson(Result.GetValue()) == R"({"Width":64,"Height":36,"Playing":false})");
		const FImage Picture = ReadReturnedPicture(Result);
		REQUIRE(Picture.GetWidth() == 64);
		REQUIRE(Picture.GetHeight() == 36);
		CHECK(IsGrey(Picture.GetPixel(32, 18), 255));
		CHECK(IsGrey(Picture.GetPixel(0, 0), GetEmptySpaceByte()));
		CHECK(IsGrey(Picture.GetPixel(63, 35), GetEmptySpaceByte()));
		CHECK(CountLoggedErrors(Test.LogCapture) == 0);
	}

	TEST_CASE("Viewport.Capture takes a picture of 960 by 540 pixels unless it is given a size")
	{
		FCaptureTest Test;

		const FCommandResult Result = Test.Capture();

		CHECK(WriteTestJson(Result.GetValue()) == R"({"Width":960,"Height":540,"Playing":false})");
		const FImage Picture = ReadReturnedPicture(Result);
		CHECK(Picture.GetWidth() == 960);
		CHECK(Picture.GetHeight() == 540);
	}

	TEST_CASE("Viewport.Capture draws in the viewport's view mode")
	{
		FCaptureTest Test;
		ExecuteOrFail(Test.Editor, "Viewport.SetViewMode", R"({"Mode": "Lit"})");

		const FImage Picture = ReadReturnedPicture(Test.Capture(R"({"Width": 64, "Height": 36})"));

		// The scene has no light, so the lit cube is dark.
		CHECK_FALSE(IsGrey(Picture.GetPixel(32, 18), 255));
		CHECK(CountLoggedErrors(Test.LogCapture) == 0);
	}

	TEST_CASE("Viewport.Capture draws no line around what is selected")
	{
		FCaptureTest Test;
		Test.Editor.SetSelection({Test.CubeId});

		const FImage Picture = ReadReturnedPicture(Test.Capture(R"({"Width": 256, "Height": 144})"));

		// The cube is white and the background grey: an orange line would be the only colour that is not grey.
		int32 ColouredCount = 0;
		for (const FColor& Pixel : Picture.GetPixels())
		{
			if (std::abs(Pixel.R - Pixel.B) > 8)
			{
				++ColouredCount;
			}
		}
		CHECK(ColouredCount == 0);
		CHECK(IsGrey(Picture.GetPixel(128, 72), 255));
	}

	TEST_CASE("During a game Viewport.Capture draws the game's copy, and says at which frame")
	{
		FCaptureTest Test;
		ExecuteOrFail(Test.Editor,
			"Component.Add",
			std::format(R"({{"Entity": {}, "Class": "WEditorTestMoverComponent"}})", Test.CubeId));
		ExecuteOrFail(Test.Editor, "Play.Start");

		const FCommandResult AtStart = Test.Capture(R"({"Width": 64, "Height": 36})");
		CHECK(WriteTestJson(AtStart.GetValue()) == R"({"Width":64,"Height":36,"Playing":true,"Frame":0})");
		CHECK(IsGrey(ReadReturnedPicture(AtStart).GetPixel(32, 18), 255));

		// Three frames move the cube three metres to the side, out of the middle of the picture.
		ExecuteOrFail(Test.Editor, "Play.Step", R"({"Frames": 3})");
		const FCommandResult Moved = Test.Capture(R"({"Width": 64, "Height": 36})");
		CHECK(WriteTestJson(Moved.GetValue()) == R"({"Width":64,"Height":36,"Playing":true,"Frame":3})");
		CHECK(IsGrey(ReadReturnedPicture(Moved).GetPixel(32, 18), GetEmptySpaceByte()));

		ExecuteOrFail(Test.Editor, "Play.Stop");
		const FCommandResult After = Test.Capture(R"({"Width": 64, "Height": 36})");
		CHECK(WriteTestJson(After.GetValue()) == R"({"Width":64,"Height":36,"Playing":false})");
		CHECK(IsGrey(ReadReturnedPicture(After).GetPixel(32, 18), 255));
		CHECK(CountLoggedErrors(Test.LogCapture) == 0);
	}

	TEST_CASE("Viewport.Capture with a Path writes the picture there and returns none")
	{
		FCaptureTest Test;
		const FScopedTempDirectory Directory;
		const std::filesystem::path File = Directory.GetPath() / "Pictures" / "Shot.PNG";

		const FCommandResult Written = Test.Capture(std::format(
			R"({{"Width": 64, "Height": 36, "Path": {}}})", WriteTestJson(FJsonValue(FFileHelper::PathToUtf8(File)))));

		CHECK(Written.GetPicture().empty());
		CHECK(*Written.GetValue().FindMember("Path") == FJsonValue(FFileHelper::PathToUtf8(File)));
		FImage FromFile;
		std::string Error;
		REQUIRE(FImageFile::Load(File, FromFile, Error));
		const FImage Returned = ReadReturnedPicture(Test.Capture(R"({"Width": 64, "Height": 36})"));
		CHECK(FromFile.GetPixels() == Returned.GetPixels());
	}

	TEST_CASE("Viewport.Capture is no step, and leaves the camera, the view mode and the selection as they were")
	{
		FCaptureTest Test;
		Test.Editor.SetSelection({Test.CubeId});
		const std::vector<std::string> UndoSteps = Test.Editor.GetHistory().GetUndoStepNames();
		const FViewportCamera Camera = Test.Editor.GetViewportCamera();

		Test.Capture(R"({"Width": 64, "Height": 36})");

		CHECK(Test.Editor.GetHistory().GetUndoStepNames() == UndoSteps);
		CHECK(Test.Editor.GetViewportCamera().Location == Camera.Location);
		CHECK(Test.Editor.GetViewportCamera().Yaw == Camera.Yaw);
		CHECK(Test.Editor.GetViewportCamera().Pitch == Camera.Pitch);
		CHECK(Test.Editor.GetViewMode() == ESceneViewMode::Unlit);
		CHECK(Test.Editor.GetSelection() == std::vector<int64>{Test.CubeId});
	}

	TEST_CASE(
		"Viewport.Capture refuses a size out of range, a relative path, a path that is no PNG file, and a path it "
		"cannot write, and writes nothing")
	{
		FCaptureTest Test;
		const FScopedTempDirectory Directory;
		std::string ArgumentText;
		std::string ExpectedError;

		SUBCASE("A width of nothing")
		{
			ArgumentText = R"({"Width": 0})";
			ExpectedError = "Width must be from 1 to 4096 pixels, and 0 was given.";
		}
		SUBCASE("A width too large")
		{
			ArgumentText = R"({"Width": 4097})";
			ExpectedError = "Width must be from 1 to 4096 pixels, and 4097 was given.";
		}
		SUBCASE("A height of nothing")
		{
			ArgumentText = R"({"Height": 0})";
			ExpectedError = "Height must be from 1 to 4096 pixels, and 0 was given.";
		}
		SUBCASE("A height too large")
		{
			ArgumentText = R"({"Height": 4097})";
			ExpectedError = "Height must be from 1 to 4096 pixels, and 4097 was given.";
		}
		SUBCASE("A relative path")
		{
			ArgumentText = R"({"Path": "Shot.png"})";
			ExpectedError = "Path must be a whole path, such as C:/Pictures/Level1.png: a picture is no file of the "
							"project, and a relative path would be taken as one.";
		}
		SUBCASE("No PNG file")
		{
			ArgumentText = std::format(R"({{"Path": {}}})",
				WriteTestJson(FJsonValue(FFileHelper::PathToUtf8(Directory.GetPath() / "Shot.jpg"))));
			ExpectedError = "Path must name a PNG file, whose name ends in \".png\".";
		}
		SUBCASE("A folder in the file's place")
		{
			const std::filesystem::path Folder = Directory.GetPath() / "Shot.png";
			std::filesystem::create_directories(Folder);
			ArgumentText = std::format(R"({{"Width": 8, "Height": 8, "Path": {}}})",
				WriteTestJson(FJsonValue(FFileHelper::PathToUtf8(Folder))));
			ExpectedError = "The picture was drawn and could not be written: ";
		}
		CAPTURE(ArgumentText);

		const FCommandResult Result = Test.Editor.ExecuteCommand("Viewport.Capture", ParseTestJson(ArgumentText));

		REQUIRE_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError().starts_with(ExpectedError));
		if (!ExpectedError.ends_with(": "))
		{
			CHECK(Result.GetError() == ExpectedError);
		}
		// Nothing was written beside what the test made.
		int32 EntryCount = 0;
		for ([[maybe_unused]] const std::filesystem::directory_entry& Entry :
			std::filesystem::recursive_directory_iterator(Directory.GetPath()))
		{
			++EntryCount;
		}
		CHECK(EntryCount <= 1);
	}

	TEST_CASE("Viewport.Capture fails with a sentence while it cannot open a render device, and works once it can")
	{
		const FScopedEditorTest EditorTest;
		const FScopedLogCapture LogCapture;
		FEditor Editor;
		SetUpCube(Editor);
		std::unique_ptr<FRenderDevice> Other = CreateValidatedDevice();

		const FCommandResult Refused = Editor.ExecuteCommand("Viewport.Capture", ParseTestJson(R"({"Width": 8})"));

		REQUIRE_FALSE(Refused.HasSucceeded());
		CHECK(Refused.GetError() ==
			"No picture could be drawn: a render device already exists, and only one can exist at a time.");
		CHECK_FALSE(Editor.GetGraphics().IsOpen());
		Other.reset();
		const FCommandResult Taken = ExecuteOrFail(Editor, "Viewport.Capture", R"({"Width": 64, "Height": 36})");
		CHECK(IsGrey(ReadReturnedPicture(Taken).GetPixel(32, 18), 255));
		CHECK(Editor.GetGraphics().IsOpen());
	}

	TEST_CASE("A picture at the size of the viewport is the viewport's own picture, but for its marks")
	{
		FEditorUiTest Test;
		SetUpCube(Test.Editor);
		Test.DrawFrames(3);
		const FUiRectangle Viewport = Test.RequireItem("Viewport");
		const int32 Left = static_cast<int32>(Viewport.Left);
		const int32 Top = static_cast<int32>(Viewport.Top);
		const int32 Width = static_cast<int32>(Viewport.Right) - Left;
		const int32 Height = static_cast<int32>(Viewport.Bottom) - Top;
		const FImage Window = Test.ReadPicture();

		const FImage Captured = ReadReturnedPicture(ExecuteOrFail(
			Test.Editor, "Viewport.Capture", std::format(R"({{"Width": {}, "Height": {}}})", Width, Height)));

		// The lower half of the viewport, below the toolbar that lies over its top.
		const int32 HalfHeight = Height / 2;
		FImage FromViewport(Width, HalfHeight);
		FImage FromCapture(Width, HalfHeight);
		for (int32 Y = 0; Y < HalfHeight; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				FromViewport.SetPixel(X, Y, Window.GetPixel(Left + X, Top + HalfHeight + Y));
				FromCapture.SetPixel(X, Y, Captured.GetPixel(X, HalfHeight + Y));
			}
		}
		const FImageDifference Difference = FImageDifference::Measure(FromCapture, FromViewport, 2);
		INFO("Difference: ", Difference.Describe());
		CHECK(Difference.IsWithin(0.001));
		CHECK(CountLoggedErrors(Test.Capture) == 0);
	}
}
