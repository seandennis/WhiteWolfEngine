#include "Commands/CommandRegistry.h"

#include "Editor.h"
#include "TestEditorObjects.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>

#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
	/** A command that does nothing, for tests that are about the registry and not about what a command does. */
	FCommandResult DoNothing(FEditor& /*Editor*/, const FCommandArguments& /*Arguments*/)
	{
		return FCommandResult::Success();
	}

	/**
	 * A registry with the command "Test.Move", which has a required parameter of every type and an optional one,
	 * counts how often it runs and returns its arguments.
	 */
	struct FMoveCommandTest
	{
		FMoveCommandTest()
		{
			Registry.Register(
				{"Test.Move",
					"Moves something.",
					{FCommandParameter::Required("Entity", ECommandArgumentType::Integer, "What to move."),
						FCommandParameter::Required("Distance", ECommandArgumentType::Number, "How far."),
						FCommandParameter::Required("Direction", ECommandArgumentType::String, "Which way."),
						FCommandParameter::Required("Relative", ECommandArgumentType::Boolean, "From where it is."),
						FCommandParameter::Required("Payload", ECommandArgumentType::Json, "Anything."),
						FCommandParameter::Optional("Steps", ECommandArgumentType::Integer, "In how many steps.")}},
				[this](FEditor& /*Editor*/, const FCommandArguments& Arguments)
				{
					++RunCount;
					FJsonValue Value = FJsonValue::MakeObject();
					Value.SetMember("Entity", Arguments.GetInteger("Entity"));
					Value.SetMember("Distance", Arguments.GetNumber("Distance"));
					Value.SetMember("Direction", Arguments.GetString("Direction"));
					Value.SetMember("Relative", Arguments.GetBoolean("Relative"));
					Value.SetMember("Payload", Arguments.GetJson("Payload"));
					Value.SetMember("HasSteps", Arguments.Has("Steps"));
					return FCommandResult::Success(std::move(Value));
				});
		}

		/** Gives Test.Move the arguments written as JSON text. */
		FCommandResult Execute(std::string_view ArgumentText)
		{
			return Registry.Execute(Editor, "Test.Move", ParseTestJson(ArgumentText));
		}

		FScopedEditorTest EditorTest;
		FEditor Editor;
		FCommandRegistry Registry;
		int32 RunCount = 0;
	};

	/** Arguments that fit Test.Move, to change one of in a test. */
	constexpr std::string_view ValidArguments =
		R"({"Entity": 7, "Distance": 2.5, "Direction": "Up", "Relative": true, "Payload": null})";
}

TEST_SUITE("FCommandRegistry")
{
	TEST_CASE("A registered command is found by its name")
	{
		FCommandRegistry Registry;
		Registry.Register({"Test.DoNothing", "Does nothing.", {}}, DoNothing);

		const FCommandDescription* const Description = Registry.Find("Test.DoNothing");

		REQUIRE(Description != nullptr);
		CHECK(Description->Name == "Test.DoNothing");
		CHECK(Description->Summary == "Does nothing.");
		CHECK(Description->Parameters.empty());
		CHECK(Registry.Find("Test.DoSomething") == nullptr);
		CHECK(Registry.Find("test.donothing") == nullptr);
		CHECK(Registry.Find("") == nullptr);
	}

	TEST_CASE("The commands are listed in the order of their names")
	{
		FCommandRegistry Registry;
		CHECK(Registry.GetCommands().empty());

		Registry.Register({"Scene.Save", "Saves.", {}}, DoNothing);
		Registry.Register({"Entity.Create", "Creates.", {}}, DoNothing);
		Registry.Register({"Scene.New", "Starts anew.", {}}, DoNothing);

		const std::vector<const FCommandDescription*> Descriptions = Registry.GetCommands();

		REQUIRE(Descriptions.size() == 3);
		CHECK(Descriptions[0]->Name == "Entity.Create");
		CHECK(Descriptions[1]->Name == "Scene.New");
		CHECK(Descriptions[2]->Name == "Scene.Save");
	}

	TEST_CASE("A command is given its editor and its arguments, and what it returns is returned")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		FCommandRegistry Registry;
		const FEditor* GivenEditor = nullptr;
		Registry.Register({"Test.Echo",
							  "Returns its argument.",
							  {FCommandParameter::Required("Text", ECommandArgumentType::String, "")}},
			[&GivenEditor](FEditor& CommandEditor, const FCommandArguments& Arguments)
			{
				GivenEditor = &CommandEditor;
				return FCommandResult::Success(Arguments.GetString("Text"));
			});

		const FCommandResult Result = Registry.Execute(Editor, "Test.Echo", ParseTestJson(R"({"Text": "Hello"})"));

		CHECK(Result.HasSucceeded());
		CHECK(Result.GetValue() == FJsonValue("Hello"));
		CHECK(GivenEditor == &Editor);
	}

	TEST_CASE("A failure of the command is returned as it is")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		FCommandRegistry Registry;
		Registry.Register({"Test.Fail", "Fails.", {}},
			[](FEditor& /*Editor*/, const FCommandArguments& /*Arguments*/)
			{ return FCommandResult::Failure("It did not work."); });

		const FCommandResult Result = Registry.Execute(Editor, "Test.Fail", FJsonValue::MakeObject());

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError() == "It did not work.");
	}

	TEST_CASE("A command gets the arguments it was given")
	{
		FMoveCommandTest Test;

		const FCommandResult Result = Test.Execute(
			R"({"Entity": 7, "Distance": 2.5, "Direction": "Up", "Relative": true, "Payload": [1, "two"], "Steps": 4})");

		REQUIRE(Result.HasSucceeded());
		CHECK(Test.RunCount == 1);
		CHECK(WriteTestJson(Result.GetValue()) ==
			R"({"Entity":7,"Distance":2.5,"Direction":"Up","Relative":true,"Payload":[1,"two"],"HasSteps":true})");
	}

	TEST_CASE("An optional argument can be left out")
	{
		FMoveCommandTest Test;

		const FCommandResult Result = Test.Execute(ValidArguments);

		REQUIRE(Result.HasSucceeded());
		const FJsonValue* const HasSteps = Result.GetValue().FindMember("HasSteps");
		REQUIRE(HasSteps != nullptr);
		CHECK(*HasSteps == FJsonValue(false));
	}

	TEST_CASE("A whole number is a number, and anything is a JSON value")
	{
		FMoveCommandTest Test;

		CHECK(Test.Execute(R"({"Entity": 7, "Distance": 3, "Direction": "Up", "Relative": true, "Payload": {"A": 1}})")
				  .HasSucceeded());
		CHECK(Test.Execute(R"({"Entity": 7, "Distance": 3, "Direction": "Up", "Relative": true, "Payload": "Text"})")
				  .HasSucceeded());
		CHECK(Test.RunCount == 2);
	}

	TEST_CASE("JSON null stands for no arguments")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		FCommandRegistry Registry;
		Registry.Register({"Test.DoNothing", "Does nothing.", {}}, DoNothing);

		CHECK(Registry.Execute(Editor, "Test.DoNothing", FJsonValue()).HasSucceeded());
		CHECK(Registry.Execute(Editor, "Test.DoNothing", FJsonValue::MakeObject()).HasSucceeded());
	}

	TEST_CASE("A command that does not exist fails")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		FCommandRegistry Registry;

		const FCommandResult Result = Registry.Execute(Editor, "Test.Missing", FJsonValue::MakeObject());

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError() == "There is no command \"Test.Missing\".");
	}

	TEST_CASE("Arguments that are not a JSON object are refused")
	{
		FMoveCommandTest Test;

		const FCommandResult Result = Test.Execute(R"([7, 2.5, "Up"])");

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError() ==
			"The arguments of the command \"Test.Move\" must be a JSON object, but they are an array.");
		CHECK(Test.RunCount == 0);
	}

	TEST_CASE("A missing argument is refused")
	{
		FMoveCommandTest Test;

		const FCommandResult Result =
			Test.Execute(R"({"Entity": 7, "Distance": 2.5, "Relative": true, "Payload": null})");

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError() == "The command \"Test.Move\" needs the argument \"Direction\".");
		CHECK(Test.RunCount == 0);
	}

	TEST_CASE("An argument without a parameter is refused")
	{
		FMoveCommandTest Test;

		const FCommandResult Result = Test.Execute(
			R"({"Entity": 7, "Distance": 2.5, "Direction": "Up", "Relative": true, "Payload": null, "Speed": 1})");

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError() == "The command \"Test.Move\" has no parameter \"Speed\".");
		CHECK(Test.RunCount == 0);
	}

	TEST_CASE("Parameter names are case-sensitive")
	{
		FMoveCommandTest Test;

		const FCommandResult Result =
			Test.Execute(R"({"entity": 7, "Distance": 2.5, "Direction": "Up", "Relative": true, "Payload": null})");

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError() == "The command \"Test.Move\" has no parameter \"entity\".");
	}

	TEST_CASE("An argument of the wrong type is refused, and the command does not run")
	{
		FMoveCommandTest Test;
		std::string ArgumentText;
		std::string ExpectedError;

		SUBCASE("A string for an integer")
		{
			ArgumentText = R"({"Entity": "7", "Distance": 2.5, "Direction": "Up", "Relative": true, "Payload": null})";
			ExpectedError =
				"The argument \"Entity\" of the command \"Test.Move\" must be an integer, but it is a string.";
		}
		SUBCASE("A number with a fraction for an integer")
		{
			ArgumentText = R"({"Entity": 7.5, "Distance": 2.5, "Direction": "Up", "Relative": true, "Payload": null})";
			ExpectedError =
				"The argument \"Entity\" of the command \"Test.Move\" must be an integer, but it is a number "
				"that is not written as an integer.";
		}
		SUBCASE("A number beyond the range of an integer")
		{
			ArgumentText =
				R"({"Entity": 18446744073709551615, "Distance": 2.5, "Direction": "Up", "Relative": true, "Payload": null})";
			ExpectedError =
				"The argument \"Entity\" of the command \"Test.Move\" must be an integer, but it is a number "
				"too large for an integer.";
		}
		SUBCASE("A string for a number")
		{
			ArgumentText = R"({"Entity": 7, "Distance": "far", "Direction": "Up", "Relative": true, "Payload": null})";
			ExpectedError =
				"The argument \"Distance\" of the command \"Test.Move\" must be a number, but it is a string.";
		}
		SUBCASE("A number for a string")
		{
			ArgumentText = R"({"Entity": 7, "Distance": 2.5, "Direction": 1, "Relative": true, "Payload": null})";
			ExpectedError =
				"The argument \"Direction\" of the command \"Test.Move\" must be a string, but it is a number.";
		}
		SUBCASE("A number for a boolean")
		{
			ArgumentText = R"({"Entity": 7, "Distance": 2.5, "Direction": "Up", "Relative": 1, "Payload": null})";
			ExpectedError =
				"The argument \"Relative\" of the command \"Test.Move\" must be a boolean, but it is a number.";
		}
		SUBCASE("Null for a string")
		{
			ArgumentText = R"({"Entity": 7, "Distance": 2.5, "Direction": null, "Relative": true, "Payload": null})";
			ExpectedError = "The argument \"Direction\" of the command \"Test.Move\" must be a string, but it is null.";
		}
		SUBCASE("An optional argument of the wrong type")
		{
			ArgumentText =
				R"({"Entity": 7, "Distance": 2.5, "Direction": "Up", "Relative": true, "Payload": null, "Steps": "4"})";
			ExpectedError =
				"The argument \"Steps\" of the command \"Test.Move\" must be an integer, but it is a string.";
		}

		const FCommandResult Result = Test.Execute(ArgumentText);

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError() == ExpectedError);
		CHECK(Test.RunCount == 0);
	}

	TEST_CASE("Registering a command that cannot be given is a fatal error")
	{
		const FScopedLogCapture Capture;
		FCommandRegistry Registry;
		Registry.Register({"Test.DoNothing", "Does nothing.", {}}, DoNothing);

		SUBCASE("A command without a name")
		{
			CHECK_THROWS_AS(Registry.Register({"", "Has no name.", {}}, DoNothing), FFatalErrorException);
		}
		SUBCASE("A name that is taken")
		{
			CHECK_THROWS_AS(Registry.Register({"Test.DoNothing", "Again.", {}}, DoNothing), FFatalErrorException);
		}
		SUBCASE("A command without a function")
		{
			CHECK_THROWS_AS(
				Registry.Register({"Test.Empty", "Has no function.", {}}, FCommandFunction()), FFatalErrorException);
		}
		SUBCASE("A parameter without a name")
		{
			CHECK_THROWS_AS(Registry.Register({"Test.Unnamed",
												  "Has a parameter.",
												  {FCommandParameter::Required("", ECommandArgumentType::String, "")}},
								DoNothing),
				FFatalErrorException);
		}
		SUBCASE("Two parameters of the same name")
		{
			CHECK_THROWS_AS(
				Registry.Register({"Test.Twice",
									  "Has two parameters.",
									  {FCommandParameter::Required("Name", ECommandArgumentType::String, ""),
										  FCommandParameter::Optional("Name", ECommandArgumentType::Integer, "")}},
					DoNothing),
				FFatalErrorException);
		}

		// Nothing was added by the calls that failed.
		CHECK(Registry.GetCommands().size() == 1);
	}
}
