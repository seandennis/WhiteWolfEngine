// Entry point of the header tool's unit test executable.
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include "Core/ProcessDiagnostics.h"

int main(int ArgumentCount, char** Arguments)
{
	// A test run is unattended: a crash must end it, not open a dialog and wait.
	FProcessDiagnostics::DisableErrorDialogs();
	FProcessDiagnostics::EnableMemoryLeakReport();

	doctest::Context Context(ArgumentCount, Arguments);
	return Context.run();
}
