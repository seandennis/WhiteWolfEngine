#include "Core/EngineVersion.h"

// The build system defines these from the version in the top-level CMakeLists.txt.
#if !defined(WW_ENGINE_VERSION_MAJOR) || !defined(WW_ENGINE_VERSION_MINOR) || !defined(WW_ENGINE_VERSION_PATCH)
#error "The engine version macros are not defined. Build EngineVersion.cpp through CMake."
#endif

FVersion FEngineVersion::Current()
{
	return FVersion{WW_ENGINE_VERSION_MAJOR, WW_ENGINE_VERSION_MINOR, WW_ENGINE_VERSION_PATCH};
}
