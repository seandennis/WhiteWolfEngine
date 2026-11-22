// Entry point of the editor's unit test executable.
//
// Runs every doctest test case and then fails the run if any object was leaked, so a test that leaves an object in
// the root set cannot pass unnoticed. Leaked memory allocations are reported when the process exits; CTest fails the
// test on that report.
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include "Core/ProcessDiagnostics.h"
#include "Logging/ConsoleLogSink.h"
#include "Logging/Log.h"
#include "Object/GarbageCollector.h"

#include <cstdio>
#include <cstdlib>
#include <memory>

int main(int ArgumentCount, char** Arguments)
{
	// A test run is unattended: a crash must end it, not open a dialog and wait.
	FProcessDiagnostics::DisableErrorDialogs();
	FProcessDiagnostics::EnableMemoryLeakReport();

	doctest::Context Context(ArgumentCount, Arguments);
	const int TestResult = Context.run();

	// Options such as --help and --list-test-cases only query the executable; no test ran, so nothing leaked.
	if (Context.shouldExit())
	{
		return TestResult;
	}

	// The leak report is written to the log, which has no sink of its own in the test executable.
	const std::shared_ptr<FConsoleLogSink> ConsoleSink = std::make_shared<FConsoleLogSink>();
	FLog::AddSink(ConsoleSink);
	const int32 LeakedObjectCount = FGarbageCollector::ReportLeakedObjects();
	FLog::RemoveSink(ConsoleSink);

	if (LeakedObjectCount > 0)
	{
		std::fprintf(stderr, "FAILED: %d object(s) were leaked by the tests.\n", LeakedObjectCount);
		return EXIT_FAILURE;
	}

	return TestResult;
}
