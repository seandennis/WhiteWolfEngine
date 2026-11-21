#include "Core/EngineVersion.h"

#include <doctest/doctest.h>

// The build system defines these from the version in the top-level CMakeLists.txt.
#if !defined(WW_EXPECTED_VERSION_MAJOR) || !defined(WW_EXPECTED_VERSION_MINOR) || !defined(WW_EXPECTED_VERSION_PATCH)
#error "The expected engine version macros are not defined. Build EngineVersionTests.cpp through CMake."
#endif

TEST_SUITE("FEngineVersion")
{
	TEST_CASE("Current matches the version declared by the build")
	{
		const FVersion Expected{WW_EXPECTED_VERSION_MAJOR, WW_EXPECTED_VERSION_MINOR, WW_EXPECTED_VERSION_PATCH};

		CHECK(FEngineVersion::Current().ToString() == Expected.ToString());
	}

	TEST_CASE("Current is the same on every call")
	{
		CHECK(FEngineVersion::Current() == FEngineVersion::Current());
	}
}
