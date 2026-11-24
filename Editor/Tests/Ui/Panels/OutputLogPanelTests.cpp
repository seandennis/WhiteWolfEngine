#include "Assets/TestAssets.h"
#include "Logging/Log.h"
#include "Logging/LogCategory.h"
#include "Logging/LogVerbosity.h"
#include "Logging/MemoryLogSink.h"
#include "Math/Color.h"
#include "Rendering/Image.h"
#include "Ui/TestEditorUi.h"

#include <doctest/doctest.h>

#include <format>
#include <initializer_list>
#include <ostream>
#include <string>
#include <string_view>

// The Output Log is used here as a person uses it, through FEditorUi. Which messages a filter wants is tested with
// FLogFilter; these tests are about the panel showing the messages that the editor keeps, and its widgets choosing
// among them. What a line says cannot be read off the surface, so the tests count the lines and look at their
// colours.

namespace
{
	// A surface high enough for the panel to show more than a dozen lines at once: the lines of the few messages
	// that most of these tests log are then all in sight, and can be counted.
	constexpr int32 SurfaceWidth = 1280;
	constexpr int32 SurfaceHeight = 1400;

	/** Returns the name of the line that shows a message: the first, the second, and so on, of those that are shown. */
	std::string Line(int32 Index)
	{
		return std::format("OutputLog/Line/{}", Index);
	}

	/** Returns how many lines the panel shows, counted from the first, which has to be in sight. */
	int32 CountLines(const FEditorUiTest& Test)
	{
		int32 Count = 0;
		while (Test.HasItem(Line(Count)))
		{
			++Count;
		}
		return Count;
	}

	/** Returns the first of the lines that are in sight, or -1 if none is. */
	int32 FindFirstLineInSight(const FEditorUiTest& Test, int32 LineCount)
	{
		for (int32 Index = 0; Index < LineCount; ++Index)
		{
			if (Test.HasItem(Line(Index)))
			{
				return Index;
			}
		}
		return -1;
	}

	/** An editor with its user interface, and a log that holds nothing yet. */
	struct FLogTest : FEditorUiTest
	{
		FLogTest()
			: FEditorUiTest(SurfaceWidth, SurfaceHeight)
		{
			Editor.ClearLog();
			DrawFrame();
		}
	};

	/** Chooses an entry of one of the two lists above the messages. */
	void Choose(FEditorUiTest& Test, std::string_view List, std::string_view Entry)
	{
		Test.Click(std::format("OutputLog/{}", List));
		Test.Click(std::format("OutputLog/{}/{}", List, Entry));
	}

	/** Returns how many pixels of an item are of a colour that a test of pixels says. */
	template <typename PixelTestType>
	int32 CountPixels(const FEditorUiTest& Test, const std::string& ItemName, PixelTestType IsWanted)
	{
		const FUiRectangle Rectangle = Test.RequireItem(ItemName);
		const FImage Picture = Test.ReadPicture();
		int32 Count = 0;
		for (int32 Y = static_cast<int32>(Rectangle.Top); Y < static_cast<int32>(Rectangle.Bottom); ++Y)
		{
			for (int32 X = static_cast<int32>(Rectangle.Left); X < static_cast<int32>(Rectangle.Right); ++X)
			{
				Count += IsWanted(Picture.GetPixel(X, Y)) ? 1 : 0;
			}
		}
		return Count;
	}

	// The colours that errors and warnings are written in: a red, and a yellow.

	bool IsRed(FColor Pixel)
	{
		return Pixel.R > 200 && Pixel.G < 130 && Pixel.B < 130;
	}

	bool IsYellow(FColor Pixel)
	{
		return Pixel.R > 200 && Pixel.G > 160 && Pixel.B < 110;
	}

	/** As light as letters are: what is written, on a ground that is dark. */
	bool IsInk(FColor Pixel)
	{
		return Pixel.R > 150;
	}

	// Categories as a module might name them: an ordinary name, one with what the library that lays the list out
	// takes for the end of a text, and one of no letters.
	constinit FLogCategory LogGame("Game", ELogVerbosity::Log);
	constinit FLogCategory LogGameNet("Game##Net", ELogVerbosity::Log);
	constinit FLogCategory LogNameless("", ELogVerbosity::Log);
}

TEST_SUITE("FOutputLogPanel")
{
	TEST_CASE("The Output Log is below the viewport, with what chooses among its messages above them")
	{
		FLogTest Test;
		const FUiRectangle Viewport = Test.RequireItem("Viewport");

		for (const char* const Name : {"Verbosity", "Category", "Search", "Clear"})
		{
			CAPTURE(Name);
			const FUiRectangle Item = Test.RequireItem(std::format("OutputLog/{}", Name));
			CHECK(Item.Top > Viewport.Bottom);
			CHECK(Item.Right <= Viewport.Right);
		}
		CHECK(CountLines(Test) == 0);
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("Every message that is logged is a line, the newest last")
	{
		FLogTest Test;

		WW_LOG(LogEditor, Log, "The first thing");
		Test.DrawFrame();
		CHECK(CountLines(Test) == 1);

		WW_LOG(LogCore, Display, "The second thing");
		WW_LOG(LogEditor, Log, "The third thing");
		Test.DrawFrame();

		REQUIRE(CountLines(Test) == 3);
		const FUiRectangle First = Test.RequireItem(Line(0));
		const FUiRectangle Second = Test.RequireItem(Line(1));
		const FUiRectangle Third = Test.RequireItem(Line(2));
		CHECK(Second.Top >= First.Bottom);
		CHECK(Third.Top >= Second.Bottom);
		// Below what chooses among them.
		CHECK(First.Top >= Test.RequireItem("OutputLog/Search").Bottom);
	}

	TEST_CASE("A message of several lines is shown on one")
	{
		FLogTest Test;
		WW_LOG(LogEditor, Log, "One line");
		WW_LOG(LogEditor, Log, "Three\nlines\r\nof text");
		WW_LOG(LogEditor, Log, "One line again");
		Test.DrawFrame();

		REQUIRE(CountLines(Test) == 3);
		CHECK(Test.RequireItem(Line(1)).GetHeight() == Test.RequireItem(Line(0)).GetHeight());
		// What follows it follows as closely as it follows what was before it.
		CHECK(Test.RequireItem(Line(2)).Top - Test.RequireItem(Line(1)).Top ==
			doctest::Approx(Test.RequireItem(Line(1)).Top - Test.RequireItem(Line(0)).Top));
	}

	TEST_CASE("Why a command failed is a line of the log")
	{
		FLogTest Test;
		REQUIRE(Test.Editor.ExecuteCommand("Entity.Create", ParseTestJson(R"({"Name": "Player"})")).HasSucceeded());
		Test.DrawFrame();
		CHECK(CountLines(Test) == 0);

		REQUIRE_FALSE(Test.Editor.ExecuteCommand("Entity.Delete", ParseTestJson(R"({"Entity": 99})")).HasSucceeded());
		Test.DrawFrame();

		// It is a warning: in the colour of one, and among what is left when only what went wrong is wanted.
		REQUIRE(CountLines(Test) == 1);
		CHECK(CountPixels(Test, Line(0), IsYellow) > 20);
		Choose(Test, "Verbosity", "Warnings and errors");
		CHECK(CountLines(Test) == 1);
	}

	TEST_CASE("Warnings are yellow and errors red, and other messages neither")
	{
		FLogTest Test;
		WW_LOG(LogEditor, Log, "Something happened");
		WW_LOG(LogEditor, Warning, "Something is odd");
		WW_LOG(LogEditor, Error, "Something failed");
		Test.DrawFrame();
		REQUIRE(CountLines(Test) == 3);

		CHECK(CountPixels(Test, Line(0), IsRed) == 0);
		CHECK(CountPixels(Test, Line(0), IsYellow) == 0);
		CHECK(CountPixels(Test, Line(1), IsYellow) > 20);
		CHECK(CountPixels(Test, Line(1), IsRed) == 0);
		CHECK(CountPixels(Test, Line(2), IsRed) > 20);
		CHECK(CountPixels(Test, Line(2), IsYellow) == 0);
	}

	TEST_CASE("The first list chooses how important a message has to be")
	{
		FLogTest Test;
		WW_LOG(LogEditor, Log, "Something happened");
		WW_LOG(LogEditor, Display, "Something to see");
		WW_LOG(LogEditor, Warning, "Something is odd");
		WW_LOG(LogEditor, Error, "Something failed");
		Test.DrawFrame();
		REQUIRE(CountLines(Test) == 4);

		Choose(Test, "Verbosity", "Warnings and errors");
		REQUIRE(CountLines(Test) == 2);
		CHECK(CountPixels(Test, Line(0), IsYellow) > 20);
		CHECK(CountPixels(Test, Line(1), IsRed) > 20);

		Choose(Test, "Verbosity", "Errors");
		REQUIRE(CountLines(Test) == 1);
		CHECK(CountPixels(Test, Line(0), IsRed) > 20);

		// What is logged while the list is set so is chosen among in the same way.
		WW_LOG(LogEditor, Warning, "Something else is odd");
		WW_LOG(LogEditor, Error, "Something else failed");
		Test.DrawFrame();
		CHECK(CountLines(Test) == 2);

		Choose(Test, "Verbosity", "All messages");
		CHECK(CountLines(Test) == 6);
	}

	TEST_CASE("The second list offers the categories that have messages, and chooses one of them")
	{
		FLogTest Test;

		// While nothing is logged there is nothing to choose but all of them.
		Test.Click("OutputLog/Category");
		REQUIRE(Test.HasItem("OutputLog/Category/All categories"));
		CHECK_FALSE(Test.HasItem("OutputLog/Category/LogCore"));
		CHECK_FALSE(Test.HasItem("OutputLog/Category/LogEditor"));
		Test.PressKey(EKey::Escape);
		REQUIRE_FALSE(Test.HasItem("OutputLog/Category/All categories"));

		WW_LOG(LogEditor, Log, "From the editor");
		WW_LOG(LogCore, Log, "From the engine");
		WW_LOG(LogEditor, Log, "From the editor again");
		Test.DrawFrames(2);

		Test.Click("OutputLog/Category");
		REQUIRE(Test.HasItem("OutputLog/Category/All categories"));
		REQUIRE(Test.HasItem("OutputLog/Category/LogCore"));
		REQUIRE(Test.HasItem("OutputLog/Category/LogEditor"));
		// In the order of the alphabet, below the entry for all of them.
		CHECK(Test.RequireItem("OutputLog/Category/LogCore").Top >
			Test.RequireItem("OutputLog/Category/All categories").Top);
		CHECK(
			Test.RequireItem("OutputLog/Category/LogEditor").Top > Test.RequireItem("OutputLog/Category/LogCore").Top);
		Test.Click("OutputLog/Category/LogEditor");
		CHECK(CountLines(Test) == 2);

		Choose(Test, "Category", "LogCore");
		CHECK(CountLines(Test) == 1);

		Choose(Test, "Category", "All categories");
		CHECK(CountLines(Test) == 3);

		// A category whose messages the log has forgotten is no longer on offer.
		Test.Click("OutputLog/Clear");
		WW_LOG(LogEditor, Log, "After");
		Test.DrawFrames(2);
		Test.Click("OutputLog/Category");
		REQUIRE(Test.HasItem("OutputLog/Category/LogEditor"));
		CHECK_FALSE(Test.HasItem("OutputLog/Category/LogCore"));
	}

	TEST_CASE("A category is offered by its whole name, whatever is in the name")
	{
		FLogTest Test;
		WW_LOG(LogGame, Log, "From a category with a short name");
		WW_LOG(LogGameNet, Log, "From a category with two number signs in its name");
		WW_LOG(LogNameless, Log, "From a category without a name");
		Test.DrawFrames(2);
		REQUIRE(CountLines(Test) == 3);

		// Laying the open list out fails no check of the library, which a line without a text of its own would.
		Test.Click("OutputLog/Category");
		REQUIRE(Test.HasItem("OutputLog/Category/Game"));
		REQUIRE(Test.HasItem("OutputLog/Category/Game##Net"));
		// The longer name is written with more ink than the shorter one, which it would not be if it ended after
		// "Game".
		CHECK(CountPixels(Test, "OutputLog/Category/Game##Net", IsInk) >
			CountPixels(Test, "OutputLog/Category/Game", IsInk));

		// It chooses the messages of its category, and the field then says the whole name too.
		Test.Click("OutputLog/Category/Game##Net");
		CHECK(CountLines(Test) == 1);
		const int32 LongInk = CountPixels(Test, "OutputLog/Category", IsInk);
		Choose(Test, "Category", "Game");
		CHECK(CountLines(Test) == 1);
		CHECK(LongInk > CountPixels(Test, "OutputLog/Category", IsInk));
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE("The field shows the messages that have what is typed into it")
	{
		FLogTest Test;
		WW_LOG(LogEditor, Log, "The Crate was moved");
		WW_LOG(LogCore, Log, "A file was read");
		WW_LOG(LogCore, Warning, "The crate has no mesh");
		Test.DrawFrame();
		REQUIRE(CountLines(Test) == 3);

		Test.Click("OutputLog/Search");
		Test.Type("crate");
		CHECK(CountLines(Test) == 2);

		// With the lists, it narrows what is shown further.
		Choose(Test, "Verbosity", "Warnings and errors");
		CHECK(CountLines(Test) == 1);

		// The name of a category is looked through as the text is.
		Choose(Test, "Verbosity", "All messages");
		Test.Click("OutputLog/Search");
		Test.PressKey(EKey::A, {EKey::LeftControl});
		Test.Type("logcore");
		CHECK(CountLines(Test) == 2);

		Test.PressKey(EKey::A, {EKey::LeftControl});
		Test.Type("nothing says this");
		CHECK(CountLines(Test) == 0);
	}

	TEST_CASE("Clear forgets the messages, and what is logged afterwards is shown")
	{
		FLogTest Test;
		WW_LOG(LogEditor, Log, "Before");
		WW_LOG(LogEditor, Warning, "Also before");
		Test.DrawFrame();
		REQUIRE(CountLines(Test) == 2);

		Test.Click("OutputLog/Clear");

		CHECK(CountLines(Test) == 0);
		CHECK(Test.Editor.GetLog().GetMessages().empty());

		WW_LOG(LogEditor, Log, "After");
		Test.DrawFrame();
		CHECK(CountLines(Test) == 1);
	}

	TEST_CASE("Of many messages the newest are in sight, and go on being so as more are logged")
	{
		FLogTest Test;
		for (int32 Index = 0; Index < 200; ++Index)
		{
			WW_LOG(LogEditor, Log, "Message {}", Index);
		}
		Test.DrawFrames(3);

		CHECK_FALSE(Test.HasItem(Line(0)));
		// The last of them is whole, as high as the line above it, and nothing is below it in the list. A line
		// that is cut off by the list's edge is found as the part of it that can be seen.
		const float ListBottom = Test.RequireItem("OutputLog/Messages").Bottom;
		const FUiRectangle Last = Test.RequireItem(Line(199));
		CHECK(Last.GetHeight() == doctest::Approx(Test.RequireItem(Line(198)).GetHeight()));
		CHECK(Last.Bottom == doctest::Approx(ListBottom));

		for (int32 Index = 200; Index < 260; ++Index)
		{
			WW_LOG(LogEditor, Log, "Message {}", Index);
		}
		Test.DrawFrames(3);

		CHECK_FALSE(Test.HasItem(Line(199)));
		const FUiRectangle Newest = Test.RequireItem(Line(259));
		CHECK(Newest.GetHeight() == doctest::Approx(Test.RequireItem(Line(258)).GetHeight()));
		CHECK(Newest.Bottom == doctest::Approx(ListBottom));
	}

	TEST_CASE("The wheel goes back through the messages, and the place that was gone to is kept")
	{
		FLogTest Test;
		for (int32 Index = 0; Index < 200; ++Index)
		{
			WW_LOG(LogEditor, Log, "Message {}", Index);
		}
		Test.DrawFrames(3);
		REQUIRE(Test.HasItem(Line(199)));

		Test.MoveMouse(Test.RequireItem(Line(197)).GetCenter());
		Test.Input.HandleScroll(FVector2D(0.0, 5.0));
		Test.DrawFrames(3);
		CHECK_FALSE(Test.HasItem(Line(199)));

		// A message in the middle of what is in sight, and where its line is.
		const int32 Read = FindFirstLineInSight(Test, 200) + 3;
		const float Top = Test.RequireItem(Line(Read)).Top;

		// Whoever reads older messages is not taken away from them by new ones: the line is where it was.
		WW_LOG(LogEditor, Log, "One more");
		Test.DrawFrames(3);
		CHECK_FALSE(Test.HasItem(Line(200)));
		CHECK_FALSE(Test.HasItem(Line(199)));
		CHECK(Test.RequireItem(Line(Read)).Top == doctest::Approx(Top));
	}

	TEST_CASE("Whoever reads older messages goes on reading them when the log is full and forgets its oldest")
	{
		FLogTest Test;
		constexpr int32 Capacity = FMemoryLogSink::DefaultCapacity;
		for (int32 Index = 0; Index < Capacity; ++Index)
		{
			WW_LOG(LogEditor, Log, "Message {}", Index);
		}
		Test.DrawFrames(3);
		REQUIRE(Test.HasItem(Line(Capacity - 1)));

		Test.MoveMouse(Test.RequireItem(Line(Capacity - 3)).GetCenter());
		Test.Input.HandleScroll(FVector2D(0.0, 5.0));
		Test.DrawFrames(3);
		REQUIRE_FALSE(Test.HasItem(Line(Capacity - 1)));

		// A message in the middle of what is in sight, and where its line is.
		const int32 Read = FindFirstLineInSight(Test, Capacity) + 3;
		const float Top = Test.RequireItem(Line(Read)).Top;

		// Each further message pushes the oldest out: what was read is a line earlier in the list, and where it
		// was on the surface, in the frame in which the list is fetched anew and in those that follow.
		for (int32 Pushed = 1; Pushed <= 3; ++Pushed)
		{
			CAPTURE(Pushed);
			WW_LOG(LogEditor, Log, "One more");
			Test.DrawFrame();
			REQUIRE(Test.Editor.GetLog().GetMessages().size() == static_cast<std::size_t>(Capacity));
			CHECK(Test.RequireItem(Line(Read - Pushed)).Top == doctest::Approx(Top));
			Test.DrawFrames(2);
			CHECK(Test.RequireItem(Line(Read - Pushed)).Top == doctest::Approx(Top));
		}
		CHECK_FALSE(Test.HasItem(Line(Capacity - 1)));

		// Another choice among the messages is another list, which begins where lists begin.
		Choose(Test, "Verbosity", "Warnings and errors");
		CHECK(CountLines(Test) == 0);
		CHECK(CountLoggedProblems(Test.Capture) == 0);
	}

	TEST_CASE(
		"A message that is longer than the list is wide gives the list a bar to scroll sideways with, in sight or not")
	{
		FLogTest Test;
		constexpr int32 LineCount = 200;
		constexpr int32 SettlingFrameCount = 6;
		// Far wider than the surface, whatever the letters, and short enough to be laid out as one text.
		const std::string Long(600, 'W');
		const auto LogLines = [&Long](int32 LongIndex)
		{
			for (int32 Index = 0; Index < LineCount; ++Index)
			{
				if (Index == LongIndex)
				{
					WW_LOG(LogEditor, Log, "{}", Long);
				}
				else
				{
					WW_LOG(LogEditor, Log, "Message {}", Index);
				}
			}
		};

		// Only short messages: the last line reaches the bottom of the list.
		constexpr int32 NoLongLine = -1;
		LogLines(NoLongLine);
		Test.DrawFrames(SettlingFrameCount);
		const float BottomWithoutBar = Test.RequireItem(Line(LineCount - 1)).Bottom;

		// The long message last, where it is in sight: the bar lies below the last line.
		Test.Editor.ClearLog();
		LogLines(LineCount - 1);
		Test.DrawFrames(SettlingFrameCount);
		const float BottomWithBar = Test.RequireItem(Line(LineCount - 1)).Bottom;
		CHECK(BottomWithBar < BottomWithoutBar);

		// The same message far above what is in sight: the bar is there still, and stays from frame to frame.
		Test.Editor.ClearLog();
		LogLines(1);
		Test.DrawFrames(SettlingFrameCount);
		for (int32 Frame = 0; Frame < 8; ++Frame)
		{
			CAPTURE(Frame);
			CHECK(Test.RequireItem(Line(LineCount - 1)).Bottom == BottomWithBar);
			Test.DrawFrame();
		}
	}

	TEST_CASE("Output Log in the Window menu closes the panel and opens it again, with what was logged meanwhile")
	{
		FLogTest Test;
		WW_LOG(LogEditor, Log, "While it was open");
		Test.DrawFrame();
		REQUIRE(CountLines(Test) == 1);
		const float ViewportBottom = Test.RequireItem("Viewport").Bottom;

		Test.Click("Menu/Window");
		Test.Click("Menu/Window/Output Log");
		CHECK_FALSE(Test.HasItem("OutputLog/Search"));
		CHECK(CountLines(Test) == 0);
		// The viewport has the room that the panel had.
		CHECK(Test.RequireItem("Viewport").Bottom > ViewportBottom + 50.0f);

		WW_LOG(LogEditor, Log, "While it was closed");
		Test.Click("Menu/Window");
		Test.Click("Menu/Window/Output Log");

		CHECK(Test.HasItem("OutputLog/Search"));
		CHECK(CountLines(Test) == 2);
	}
}
