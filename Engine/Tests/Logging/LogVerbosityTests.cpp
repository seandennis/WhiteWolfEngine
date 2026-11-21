#include "Logging/LogVerbosity.h"

#include <doctest/doctest.h>

TEST_SUITE("ELogVerbosity")
{
	TEST_CASE("ToString names every verbosity")
	{
		CHECK(ToString(ELogVerbosity::Fatal) == "Fatal");
		CHECK(ToString(ELogVerbosity::Error) == "Error");
		CHECK(ToString(ELogVerbosity::Warning) == "Warning");
		CHECK(ToString(ELogVerbosity::Display) == "Display");
		CHECK(ToString(ELogVerbosity::Log) == "Log");
		CHECK(ToString(ELogVerbosity::Verbose) == "Verbose");
		CHECK(ToString(ELogVerbosity::VeryVerbose) == "VeryVerbose");
	}

	TEST_CASE("ToString reports a value outside the enumeration as Unknown")
	{
		CHECK(ToString(static_cast<ELogVerbosity>(200)) == "Unknown");
	}

	TEST_CASE("Verbosities order from most to least severe")
	{
		CHECK(ELogVerbosity::Fatal < ELogVerbosity::Error);
		CHECK(ELogVerbosity::Error < ELogVerbosity::Warning);
		CHECK(ELogVerbosity::Warning < ELogVerbosity::Display);
		CHECK(ELogVerbosity::Display < ELogVerbosity::Log);
		CHECK(ELogVerbosity::Log < ELogVerbosity::Verbose);
		CHECK(ELogVerbosity::Verbose < ELogVerbosity::VeryVerbose);
	}
}
