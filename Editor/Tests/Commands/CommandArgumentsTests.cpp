#include "Commands/CommandArguments.h"

#include "TestEditorObjects.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>

#include <ostream>
#include <string>

TEST_SUITE("FCommandArguments")
{
	TEST_CASE("Arguments are read by name, each as its type")
	{
		const FCommandArguments Arguments(
			ParseTestJson(R"({"Visible": true, "Entity": 7, "Speed": 2.5, "Name": "Player", "Value": [1, 2, 3]})"));

		CHECK(Arguments.GetBoolean("Visible"));
		CHECK(Arguments.GetInteger("Entity") == 7);
		CHECK(Arguments.GetNumber("Speed") == 2.5);
		CHECK(Arguments.GetString("Name") == "Player");
		CHECK(WriteTestJson(Arguments.GetJson("Value")) == "[1,2,3]");
	}

	TEST_CASE("Has says whether an argument was given")
	{
		const FCommandArguments Arguments(ParseTestJson(R"({"Parent": 3, "Value": null})"));

		CHECK(Arguments.Has("Parent"));
		// An argument that is JSON null was given: null is a value a property can be set to.
		CHECK(Arguments.Has("Value"));
		CHECK_FALSE(Arguments.Has("Name"));
		CHECK_FALSE(Arguments.Has("parent"));
	}

	TEST_CASE("A whole number can be read as a number, and any argument as JSON")
	{
		const FCommandArguments Arguments(ParseTestJson(R"({"Speed": 3, "Name": "Player"})"));

		CHECK(Arguments.GetNumber("Speed") == 3.0);
		CHECK(Arguments.GetJson("Name") == FJsonValue("Player"));
	}

	TEST_CASE("Reading an argument that was not given is a fatal error")
	{
		const FScopedLogCapture Capture;
		const FCommandArguments Arguments(FJsonValue::MakeObject());

		CHECK_THROWS_AS(Arguments.GetBoolean("Visible"), FFatalErrorException);
		CHECK_THROWS_AS(Arguments.GetInteger("Entity"), FFatalErrorException);
		CHECK_THROWS_AS(Arguments.GetNumber("Speed"), FFatalErrorException);
		CHECK_THROWS_AS(Arguments.GetString("Name"), FFatalErrorException);
		CHECK_THROWS_AS(Arguments.GetJson("Value"), FFatalErrorException);
	}

	TEST_CASE("Reading an argument as another type than it has is a fatal error")
	{
		const FScopedLogCapture Capture;
		const FCommandArguments Arguments(ParseTestJson(R"({"Name": "Player", "Speed": 2.5})"));

		CHECK_THROWS_AS(Arguments.GetBoolean("Name"), FFatalErrorException);
		CHECK_THROWS_AS(Arguments.GetInteger("Name"), FFatalErrorException);
		CHECK_THROWS_AS(Arguments.GetNumber("Name"), FFatalErrorException);
		CHECK_THROWS_AS(Arguments.GetString("Speed"), FFatalErrorException);
		// A number with a fraction is not an integer.
		CHECK_THROWS_AS(Arguments.GetInteger("Speed"), FFatalErrorException);
	}

	TEST_CASE("Arguments that are not a JSON object are a fatal error")
	{
		const FScopedLogCapture Capture;

		CHECK_THROWS_AS(FCommandArguments(FJsonValue("Player")), FFatalErrorException);
		CHECK_THROWS_AS(FCommandArguments(FJsonValue(nullptr)), FFatalErrorException);
	}
}
