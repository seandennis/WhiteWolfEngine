#pragma once

#include "Core/CoreTypes.h"

/**
 * Process-wide settings that make failures visible in unattended and debug runs. An executable calls these first
 * thing in main, before it creates any engine object.
 */
class FProcessDiagnostics
{
public:
	/**
	 * Makes a crash end the process at once instead of waiting for someone to dismiss a dialog box.
	 *
	 * On Windows, abort(), a failed C runtime assertion and an unhandled exception each open a dialog by default,
	 * which blocks a test run or a headless build forever. After this call they write their message to stderr and
	 * the process exits with a failure code. Other platforms have no such dialogs, and the call does nothing there.
	 */
	ENGINE_API static void DisableErrorDialogs();

	/**
	 * Makes the process report every heap allocation that is still allocated when it exits.
	 *
	 * The report is written to stderr after every module has destroyed its static objects. It starts with the line
	 * "Detected memory leaks!" and lists each leaked block. Nothing is written when nothing was leaked.
	 *
	 * This uses the debug heap of the Microsoft C runtime, so it is available in debug builds made with MSVC only.
	 * Development builds on Linux find leaks with LeakSanitizer instead (the linux-sanitize preset).
	 *
	 * @return True if leaks will be reported, false if this build cannot detect them.
	 */
	ENGINE_API static bool EnableMemoryLeakReport();
};
