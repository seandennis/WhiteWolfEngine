#include "Core/ProcessDiagnostics.h"

#include <doctest/doctest.h>

#include <memory>

TEST_SUITE("FProcessDiagnostics")
{
	TEST_CASE("EnableMemoryLeakReport says whether this build can detect leaks")
	{
#if defined(_MSC_VER) && defined(_DEBUG)
		const bool bExpectsLeakReport = true;
#else
		const bool bExpectsLeakReport = false;
#endif

		CHECK(FProcessDiagnostics::EnableMemoryLeakReport() == bExpectsLeakReport);
	}

	TEST_CASE("The diagnostics settings can be applied more than once")
	{
		FProcessDiagnostics::DisableErrorDialogs();
		FProcessDiagnostics::DisableErrorDialogs();

		CHECK(FProcessDiagnostics::EnableMemoryLeakReport() == FProcessDiagnostics::EnableMemoryLeakReport());
	}

	// Skipped in a normal run. The CTest entry EngineTests.DetectsMemoryLeaks runs it alone, with --no-skip, and
	// passes only if the leak is reported, which proves that leak detection really is switched on.
	TEST_CASE("Deliberately leaks memory" * doctest::skip())
	{
		// release() gives up ownership without freeing, which is exactly the mistake leak detection exists to find.
		const int* const LeakedValue = std::make_unique<int>(42).release();

		CHECK(*LeakedValue == 42);
	}
}
