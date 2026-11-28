#include "Commands/CommandResult.h"

#include "TestEditorObjects.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>

#include <cstddef>
#include <ostream>
#include <string>
#include <vector>

TEST_SUITE("FCommandResult")
{
	TEST_CASE("A success carries the value the command returned")
	{
		const FCommandResult Result = FCommandResult::Success(FJsonValue(42));

		CHECK(Result.HasSucceeded());
		CHECK(Result.GetValue() == FJsonValue(42));
		CHECK(Result.GetError().empty());
		CHECK(WriteTestJson(Result.ToJson()) == R"({"Succeeded":true,"Value":42})");
	}

	TEST_CASE("A success without a value carries null")
	{
		const FCommandResult Result = FCommandResult::Success();

		CHECK(Result.HasSucceeded());
		CHECK(Result.GetValue().IsNull());
		CHECK(WriteTestJson(Result.ToJson()) == R"({"Succeeded":true,"Value":null})");
	}

	TEST_CASE("A failure carries the reason and no value")
	{
		const FCommandResult Result = FCommandResult::Failure("There is no entity with the identifier 7.");

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetValue().IsNull());
		CHECK(Result.GetError() == "There is no entity with the identifier 7.");
		CHECK(WriteTestJson(Result.ToJson()) ==
			R"({"Succeeded":false,"Error":"There is no entity with the identifier 7."})");
	}

	TEST_CASE("A failure without a reason is a fatal error")
	{
		const FScopedLogCapture Capture;

		CHECK_THROWS_AS(FCommandResult::Failure(""), FFatalErrorException);
	}

	TEST_CASE("A success with a picture carries the value and the picture, and its JSON is the value's alone")
	{
		const std::vector<std::byte> Picture = {std::byte{0x89}, std::byte{'P'}, std::byte{'N'}, std::byte{'G'}};

		const FCommandResult Result = FCommandResult::SuccessWithPicture(FJsonValue(42), Picture);

		CHECK(Result.HasSucceeded());
		CHECK(Result.GetValue() == FJsonValue(42));
		CHECK(Result.GetPicture() == Picture);
		CHECK(WriteTestJson(Result.ToJson()) == R"({"Succeeded":true,"Value":42})");
	}

	TEST_CASE("Other results carry no picture")
	{
		CHECK(FCommandResult::Success(FJsonValue(42)).GetPicture().empty());
		CHECK(FCommandResult::Failure("There is nothing to undo.").GetPicture().empty());
	}

	TEST_CASE("A picture without a byte is a fatal error")
	{
		const FScopedLogCapture Capture;

		CHECK_THROWS_AS(FCommandResult::SuccessWithPicture(FJsonValue(), {}), FFatalErrorException);
	}
}
