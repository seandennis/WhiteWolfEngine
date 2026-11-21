#include "Logging/LogMessage.h"

#include <doctest/doctest.h>

#include <chrono>

namespace
{
	/** Returns 2026-11-21 02:23:45.123 UTC. */
	std::chrono::system_clock::time_point MakeTestTime()
	{
		using namespace std::chrono;

		return sys_days{year{2026} / month{11} / day{21}} + hours{2} + minutes{23} + seconds{45} + milliseconds{123};
	}

	FLogMessage MakeTestMessage(ELogVerbosity Verbosity)
	{
		return FLogMessage{
			.Time = MakeTestTime(),
			.Verbosity = Verbosity,
			.Category = "LogExample",
			.Text = "Something happened",
		};
	}
}

TEST_SUITE("FLogMessage")
{
	TEST_CASE("ToString formats the UTC time, category, verbosity and text on one line")
	{
		CHECK(MakeTestMessage(ELogVerbosity::Warning).ToString() ==
			"[2026-11-21T02:23:45.123Z] LogExample: Warning: Something happened");
	}

	TEST_CASE("ToString names the verbosity of messages that need attention or are for debugging")
	{
		CHECK(MakeTestMessage(ELogVerbosity::Fatal).ToString() ==
			"[2026-11-21T02:23:45.123Z] LogExample: Fatal: Something happened");
		CHECK(MakeTestMessage(ELogVerbosity::Error).ToString() ==
			"[2026-11-21T02:23:45.123Z] LogExample: Error: Something happened");
		CHECK(MakeTestMessage(ELogVerbosity::Verbose).ToString() ==
			"[2026-11-21T02:23:45.123Z] LogExample: Verbose: Something happened");
		CHECK(MakeTestMessage(ELogVerbosity::VeryVerbose).ToString() ==
			"[2026-11-21T02:23:45.123Z] LogExample: VeryVerbose: Something happened");
	}

	TEST_CASE("ToString omits the verbosity of Display and Log messages")
	{
		CHECK(MakeTestMessage(ELogVerbosity::Display).ToString() ==
			"[2026-11-21T02:23:45.123Z] LogExample: Something happened");
		CHECK(MakeTestMessage(ELogVerbosity::Log).ToString() ==
			"[2026-11-21T02:23:45.123Z] LogExample: Something happened");
	}

	TEST_CASE("ToString always prints three millisecond digits")
	{
		FLogMessage Message = MakeTestMessage(ELogVerbosity::Log);
		Message.Time -= std::chrono::milliseconds{123};

		CHECK(Message.ToString() == "[2026-11-21T02:23:45.000Z] LogExample: Something happened");
	}

	TEST_CASE("ToString truncates the time to whole milliseconds")
	{
		FLogMessage Message = MakeTestMessage(ELogVerbosity::Log);
		Message.Time += std::chrono::microseconds{999};

		CHECK(Message.ToString() == "[2026-11-21T02:23:45.123Z] LogExample: Something happened");
	}

	TEST_CASE("ToString keeps text and category exactly as given")
	{
		FLogMessage Message = MakeTestMessage(ELogVerbosity::Log);
		Message.Category = "";
		Message.Text = "braces {} and: colons";

		CHECK(Message.ToString() == "[2026-11-21T02:23:45.123Z] : braces {} and: colons");
	}
}
