#include "Logging/LogCategory.h"

#include <doctest/doctest.h>

// doctest prints the std::string_view operands of a comparison with operator<<, which needs std::ostream.
#include <ostream>

namespace
{
	// Proves that a category can be constant-initialized, which is what makes it safe to use during static
	// initialization. This line does not compile otherwise.
	constinit FLogCategory LogConstantInitialized("LogConstantInitialized", ELogVerbosity::Warning);
}

TEST_SUITE("FLogCategory")
{
	TEST_CASE("A category reports its name and initial verbosity")
	{
		const FLogCategory Category("LogExample", ELogVerbosity::Verbose);

		CHECK(Category.GetName() == "LogExample");
		CHECK(Category.GetVerbosity() == ELogVerbosity::Verbose);
	}

	TEST_CASE("A constant-initialized category behaves like any other")
	{
		CHECK(LogConstantInitialized.GetName() == "LogConstantInitialized");
		CHECK(LogConstantInitialized.GetVerbosity() == ELogVerbosity::Warning);
	}

	TEST_CASE("A category suppresses messages less severe than its verbosity")
	{
		const FLogCategory Category("LogExample", ELogVerbosity::Display);

		CHECK_FALSE(Category.IsSuppressed(ELogVerbosity::Error));
		CHECK_FALSE(Category.IsSuppressed(ELogVerbosity::Warning));
		CHECK_FALSE(Category.IsSuppressed(ELogVerbosity::Display));
		CHECK(Category.IsSuppressed(ELogVerbosity::Log));
		CHECK(Category.IsSuppressed(ELogVerbosity::Verbose));
		CHECK(Category.IsSuppressed(ELogVerbosity::VeryVerbose));
	}

	TEST_CASE("A category at VeryVerbose suppresses nothing")
	{
		const FLogCategory Category("LogExample", ELogVerbosity::VeryVerbose);

		CHECK_FALSE(Category.IsSuppressed(ELogVerbosity::Fatal));
		CHECK_FALSE(Category.IsSuppressed(ELogVerbosity::Log));
		CHECK_FALSE(Category.IsSuppressed(ELogVerbosity::VeryVerbose));
	}

	TEST_CASE("Fatal messages are never suppressed")
	{
		// Fatal is the most severe verbosity, so no threshold can exclude it.
		const FLogCategory Category("LogExample", ELogVerbosity::Fatal);

		CHECK_FALSE(Category.IsSuppressed(ELogVerbosity::Fatal));
		CHECK(Category.IsSuppressed(ELogVerbosity::Error));
	}

	TEST_CASE("SetVerbosity changes which messages are suppressed")
	{
		FLogCategory Category("LogExample", ELogVerbosity::Log);
		REQUIRE(Category.IsSuppressed(ELogVerbosity::Verbose));

		Category.SetVerbosity(ELogVerbosity::Verbose);

		CHECK(Category.GetVerbosity() == ELogVerbosity::Verbose);
		CHECK_FALSE(Category.IsSuppressed(ELogVerbosity::Verbose));
		CHECK(Category.IsSuppressed(ELogVerbosity::VeryVerbose));

		Category.SetVerbosity(ELogVerbosity::Error);

		CHECK_FALSE(Category.IsSuppressed(ELogVerbosity::Error));
		CHECK(Category.IsSuppressed(ELogVerbosity::Warning));
	}
}
