// Entry point of the shader compiler's unit test executable.
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include "Core/ProcessDiagnostics.h"
#include "ShaderSourceCompiler.h"

int main(int ArgumentCount, char** Arguments)
{
	// A test run is unattended: a crash must end it, not open a dialog and wait.
	FProcessDiagnostics::DisableErrorDialogs();
	FProcessDiagnostics::EnableMemoryLeakReport();

	// Starting the compiler library is slow. While this object exists the library stays started, so the compilers
	// the tests create and destroy cost nothing.
	const FShaderSourceCompiler KeepCompilerLibraryStarted;

	doctest::Context Context(ArgumentCount, Arguments);
	return Context.run();
}
