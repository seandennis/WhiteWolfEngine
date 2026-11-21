#include "Core/Assert.h"

#include "TestSupport/FatalErrorException.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>

#include <string>
#include <vector>

namespace
{
	bool Contains(const std::string& Text, const std::string& Substring)
	{
		return Text.find(Substring) != std::string::npos;
	}

	/**
	 * Returns its argument. Going through a function call keeps the conditions in these tests from being constant
	 * expressions, so they exercise the checks the way real, run-time conditions do.
	 */
	template <typename ValueType>
	ValueType RuntimeValue(ValueType Value)
	{
		return Value;
	}
}

TEST_SUITE("WW_CHECK")
{
	TEST_CASE("A check whose condition holds does nothing")
	{
		const FScopedLogCapture Capture;
		const int32 Value = RuntimeValue(2);

		WW_CHECK(Value == 2);
		WW_CHECKF(Value == 2, "Value was {}", Value);

		CHECK(Capture.GetMessages().empty());
	}

	TEST_CASE("A failed check logs a Fatal message with the expression and its location")
	{
		const FScopedLogCapture Capture;
		const int32 Value = RuntimeValue(3);

		CHECK_THROWS_AS(WW_CHECK(Value == 2), FFatalErrorException);

		const std::vector<FLogMessage> Messages = Capture.GetMessages();
		REQUIRE(Messages.size() == 1);
		CHECK(Messages[0].Verbosity == ELogVerbosity::Fatal);
		CHECK(Messages[0].Category == "LogCore");
		CHECK(Contains(Messages[0].Text, "Assertion failed: Value == 2"));
		CHECK(Contains(Messages[0].Text, "AssertTests.cpp:"));
	}

	TEST_CASE("A failed WW_CHECKF adds its formatted message")
	{
		const FScopedLogCapture Capture;
		const int32 Index = RuntimeValue(7);
		const int32 Count = RuntimeValue(4);

		CHECK_THROWS_AS(
			WW_CHECKF(Index < Count, "Index {} is out of range ({} elements)", Index, Count), FFatalErrorException);

		const std::vector<FLogMessage> Messages = Capture.GetMessages();
		REQUIRE(Messages.size() == 1);
		CHECK(Contains(Messages[0].Text, "Assertion failed: Index < Count"));
		CHECK(Contains(Messages[0].Text, "Index 7 is out of range (4 elements)"));
	}

	TEST_CASE("A failed WW_CHECKF accepts a message with no arguments")
	{
		const FScopedLogCapture Capture;
		const bool bInitialized = RuntimeValue(false);

		CHECK_THROWS_AS(WW_CHECKF(bInitialized, "The renderer was not initialized"), FFatalErrorException);

		const std::vector<FLogMessage> Messages = Capture.GetMessages();
		REQUIRE(Messages.size() == 1);
		CHECK(Contains(Messages[0].Text, "The renderer was not initialized"));
	}

	TEST_CASE("The condition is evaluated exactly once")
	{
		const FScopedLogCapture Capture;
		int32 EvaluationCount = 0;
		const auto EvaluateTo = [&EvaluationCount](bool bResult)
		{
			++EvaluationCount;
			return bResult;
		};

		WW_CHECK(EvaluateTo(true));
		CHECK(EvaluationCount == 1);

		CHECK_THROWS_AS(WW_CHECK(EvaluateTo(false)), FFatalErrorException);
		CHECK(EvaluationCount == 2);

		WW_CHECKF(EvaluateTo(true), "Unused");
		CHECK(EvaluationCount == 3);
	}

	TEST_CASE("The message arguments are evaluated only when the check fails")
	{
		const FScopedLogCapture Capture;
		int32 EvaluationCount = 0;
		const auto CountEvaluation = [&EvaluationCount]()
		{
			++EvaluationCount;
			return EvaluationCount;
		};

		WW_CHECKF(RuntimeValue(true), "Value {}", CountEvaluation());
		CHECK(EvaluationCount == 0);

		CHECK_THROWS_AS(WW_CHECKF(RuntimeValue(false), "Value {}", CountEvaluation()), FFatalErrorException);
		CHECK(EvaluationCount == 1);
	}

	TEST_CASE("A check is an expression that can be used wherever a statement can")
	{
		const FScopedLogCapture Capture;
		const int32 Value = RuntimeValue(1);

		const bool bChecked = (WW_CHECK(Value == 1), true);

		CHECK(bChecked);
	}
}
