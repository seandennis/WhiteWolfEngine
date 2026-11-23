#include "Editor.h"
#include "Logging/Log.h"
#include "Logging/LogMessage.h"
#include "Scene/Scene.h"
#include "TestEditorObjects.h"

#include <doctest/doctest.h>

#include <cstddef>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

// What these tests log is logged to the editor's own category and to the engine's, through the log itself: the
// editor keeps whatever is logged while it exists, whoever logs it.

namespace
{
	/** An editor whose log holds nothing yet, whatever it logged while it was made. */
	struct FQuietEditor
	{
		FQuietEditor()
		{
			Editor.ClearLog();
		}

		FScopedEditorTest EditorTest;
		FEditor Editor;
	};

	/** Gives the command Log.Read and returns what it returned; the test fails if the command does. */
	FJsonValue ReadLog(FEditor& Editor, std::string_view Arguments = "{}")
	{
		const FCommandResult Result = Editor.ExecuteCommand("Log.Read", ParseTestJson(Arguments));
		INFO("Error: ", Result.GetError());
		REQUIRE(Result.HasSucceeded());
		return Result.GetValue();
	}

	/** Returns the texts of the messages that Log.Read returned, in their order. */
	std::vector<std::string> GetTexts(const FJsonValue& Value)
	{
		std::vector<std::string> Texts;
		for (const FJsonValue& Message : Value.FindMember("Messages")->GetElements())
		{
			Texts.push_back(Message.FindMember("Text")->GetString());
		}
		return Texts;
	}
}

TEST_SUITE("Log commands")
{
	TEST_CASE("Log.Read returns what was logged, oldest first, with all that a message has")
	{
		FQuietEditor Test;
		WW_LOG(LogEditor, Log, "The first thing");
		WW_LOG(LogCore, Warning, "The second thing, which went {}", "wrong");
		WW_LOG(LogEditor, Error, "The third thing");

		const FJsonValue Value = ReadLog(Test.Editor);

		const std::vector<std::string> Expected = {
			"The first thing", "The second thing, which went wrong", "The third thing"};
		CHECK(GetTexts(Value) == Expected);
		CHECK(Value.FindMember("MatchCount")->GetInteger() == 3);

		const FJsonValue& Second = Value.FindMember("Messages")->GetElements()[1];
		CHECK(Second.FindMember("Verbosity")->GetString() == "Warning");
		CHECK(Second.FindMember("Category")->GetString() == "LogCore");
		// The time, to the thousandth of a second, as in 2026-11-21T02:23:45.123Z.
		const std::string Time = Second.FindMember("Time")->GetString();
		REQUIRE(Time.size() == 24);
		CHECK(Time[4] == '-');
		CHECK(Time[10] == 'T');
		CHECK(Time[19] == '.');
		CHECK(Time.back() == 'Z');
	}

	TEST_CASE("Log.Read returns nothing while nothing has been logged")
	{
		FQuietEditor Test;

		const FJsonValue Value = ReadLog(Test.Editor);

		CHECK(WriteTestJson(Value) == R"({"Messages":[],"MatchCount":0})");
	}

	TEST_CASE("Log.Read returns the kind of message that it is asked for and every more important kind")
	{
		FQuietEditor Test;
		WW_LOG(LogEditor, Log, "Noted");
		WW_LOG(LogEditor, Display, "Shown");
		WW_LOG(LogEditor, Warning, "Odd");
		WW_LOG(LogEditor, Error, "Failed");

		const std::vector<std::string> Problems = {"Odd", "Failed"};
		CHECK(GetTexts(ReadLog(Test.Editor, R"({"Verbosity": "Warning"})")) == Problems);
		const std::vector<std::string> Errors = {"Failed"};
		CHECK(GetTexts(ReadLog(Test.Editor, R"({"Verbosity": "Error"})")) == Errors);
		const std::vector<std::string> Everything = {"Noted", "Shown", "Odd", "Failed"};
		CHECK(GetTexts(ReadLog(Test.Editor, R"({"Verbosity": "VeryVerbose"})")) == Everything);
		CHECK(GetTexts(ReadLog(Test.Editor, R"({"Verbosity": "Fatal"})")).empty());
	}

	TEST_CASE("Log.Read returns the messages of a category, and those that have a text in them")
	{
		FQuietEditor Test;
		WW_LOG(LogEditor, Log, "The Crate was moved");
		WW_LOG(LogCore, Log, "A file was read");
		WW_LOG(LogCore, Warning, "The crate has no mesh");

		const std::vector<std::string> OfCore = {"A file was read", "The crate has no mesh"};
		CHECK(GetTexts(ReadLog(Test.Editor, R"({"Category": "LogCore"})")) == OfCore);
		CHECK(GetTexts(ReadLog(Test.Editor, R"({"Category": "LogNothing"})")).empty());

		const std::vector<std::string> AboutCrates = {"The Crate was moved", "The crate has no mesh"};
		CHECK(GetTexts(ReadLog(Test.Editor, R"({"Contains": "crate"})")) == AboutCrates);
		const std::vector<std::string> Both = {"The crate has no mesh"};
		CHECK(GetTexts(ReadLog(
				  Test.Editor, R"({"Contains": "CRATE", "Category": "LogCore", "Verbosity": "Warning"})")) == Both);
	}

	TEST_CASE("Log.Read returns the most recent messages when there are more than it is asked for")
	{
		FQuietEditor Test;
		for (int32 Index = 1; Index <= 5; ++Index)
		{
			WW_LOG(LogEditor, Log, "Message {}", Index);
		}

		const FJsonValue Value = ReadLog(Test.Editor, R"({"Count": 2})");

		const std::vector<std::string> LastTwo = {"Message 4", "Message 5"};
		CHECK(GetTexts(Value) == LastTwo);
		CHECK(Value.FindMember("MatchCount")->GetInteger() == 5);
		CHECK(GetTexts(ReadLog(Test.Editor, R"({"Count": 50})")).size() == 5);
	}

	TEST_CASE("Log.Read returns a hundred messages unless it is asked for another number")
	{
		FQuietEditor Test;
		for (int32 Index = 1; Index <= 130; ++Index)
		{
			WW_LOG(LogEditor, Log, "Message {}", Index);
		}

		const FJsonValue Value = ReadLog(Test.Editor);

		const std::vector<std::string> Texts = GetTexts(Value);
		REQUIRE(Texts.size() == 100);
		CHECK(Texts.front() == "Message 31");
		CHECK(Texts.back() == "Message 130");
		CHECK(Value.FindMember("MatchCount")->GetInteger() == 130);
	}

	TEST_CASE("Log.Read refuses a kind of message that there is not, and a count that is none")
	{
		FQuietEditor Test;
		WW_LOG(LogEditor, Log, "Noted");
		std::string Arguments;
		std::string ExpectedError;

		SUBCASE("A kind that is not one")
		{
			Arguments = R"({"Verbosity": "Loud"})";
			ExpectedError =
				"Verbosity must be one of Fatal, Error, Warning, Display, Log, Verbose and VeryVerbose, not 'Loud'.";
		}
		SUBCASE("A kind in small letters")
		{
			Arguments = R"({"Verbosity": "warning"})";
			ExpectedError =
				"Verbosity must be one of Fatal, Error, Warning, Display, Log, Verbose and VeryVerbose, not 'warning'.";
		}
		SUBCASE("No messages at all")
		{
			Arguments = R"({"Count": 0})";
			ExpectedError = "Count must be at least 1, not 0.";
		}
		SUBCASE("Fewer than none")
		{
			Arguments = R"({"Count": -3})";
			ExpectedError = "Count must be at least 1, not -3.";
		}

		const FCommandResult Result = Test.Editor.ExecuteCommand("Log.Read", ParseTestJson(Arguments));

		REQUIRE_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError() == ExpectedError);
	}

	TEST_CASE("Log.Read refuses arguments of the wrong type")
	{
		FQuietEditor Test;

		CHECK_FALSE(Test.Editor.ExecuteCommand("Log.Read", ParseTestJson(R"({"Count": "many"})")).HasSucceeded());
		CHECK_FALSE(Test.Editor.ExecuteCommand("Log.Read", ParseTestJson(R"({"Verbosity": 2})")).HasSucceeded());
		CHECK_FALSE(Test.Editor.ExecuteCommand("Log.Read", ParseTestJson(R"({"Category": []})")).HasSucceeded());
		CHECK_FALSE(Test.Editor.ExecuteCommand("Log.Read", ParseTestJson(R"({"Since": 5})")).HasSucceeded());
	}

	TEST_CASE("Why a command failed can be read in the log")
	{
		FQuietEditor Test;
		REQUIRE_FALSE(Test.Editor.ExecuteCommand("Entity.Delete", ParseTestJson(R"({"Entity": 99})")).HasSucceeded());

		const FJsonValue Value = ReadLog(Test.Editor, R"({"Contains": "Entity.Delete"})");

		const std::vector<std::string> Expected = {
			"Command Entity.Delete failed: There is no entity with the identifier 99."};
		CHECK(GetTexts(Value) == Expected);
		CHECK(Value.FindMember("Messages")->GetElements()[0].FindMember("Category")->GetString() == "LogEditor");
	}

	TEST_CASE("Log.Clear forgets what was logged, and what is logged afterwards is kept")
	{
		FQuietEditor Test;
		WW_LOG(LogEditor, Warning, "Before");

		const FCommandResult Result = Test.Editor.ExecuteCommand("Log.Clear", FJsonValue());

		REQUIRE(Result.HasSucceeded());
		CHECK(Result.GetValue().IsNull());
		CHECK(GetTexts(ReadLog(Test.Editor)).empty());

		WW_LOG(LogEditor, Warning, "After");
		const std::vector<std::string> Expected = {"After"};
		CHECK(GetTexts(ReadLog(Test.Editor)) == Expected);
	}

	TEST_CASE("Reading and clearing the log are no steps of the history")
	{
		FQuietEditor Test;
		REQUIRE(Test.Editor.ExecuteCommand("Entity.Create", ParseTestJson(R"({"Name": "Player"})")).HasSucceeded());
		const std::size_t StepCount = Test.Editor.GetHistory().GetUndoStepNames().size();

		ReadLog(Test.Editor);
		REQUIRE(Test.Editor.ExecuteCommand("Log.Clear", FJsonValue()).HasSucceeded());

		CHECK(Test.Editor.GetHistory().GetUndoStepNames().size() == StepCount);
		CHECK(Test.Editor.GetScene().GetAllEntities().size() == 1);
	}
}
