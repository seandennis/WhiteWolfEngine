#include "Core/ProcessDiagnostics.h"

#if defined(_WIN32)
#include <crtdbg.h>
#include <cstdlib>
#include <initializer_list>
#include <windows.h>
#endif

void FProcessDiagnostics::DisableErrorDialogs()
{
#if defined(_WIN32)
	// The system: no "has stopped working" and no "there is no disk in the drive" dialogs.
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);

	// abort(): no message box and no Windows Error Reporting. The caller has already logged why it aborts.
	_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);

#if defined(_DEBUG)
	// The debug C runtime: assertions and errors go to stderr (and to an attached debugger) instead of a dialog.
	for (const int ReportType : {_CRT_WARN, _CRT_ERROR, _CRT_ASSERT})
	{
		_CrtSetReportMode(ReportType, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
		_CrtSetReportFile(ReportType, _CRTDBG_FILE_STDERR);
	}
#endif
#endif
}

bool FProcessDiagnostics::EnableMemoryLeakReport()
{
#if defined(_MSC_VER) && defined(_DEBUG)
	// The leak report is a C runtime warning; send it where a test run can see it.
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
	_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);

	// The runtime makes the report when it shuts down, which is after every module has run its static destructors,
	// so memory owned by a static object is not mistaken for a leak.
	const int CurrentFlags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
	_CrtSetDbgFlag(CurrentFlags | _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	return true;
#else
	return false;
#endif
}
