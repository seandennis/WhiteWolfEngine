#include "Core/Assert.h"

#include "Logging/Log.h"

#include <cstdlib>

void FDebug::CheckFailed(const char* Expression, const char* File, int32 Line, const std::string& Message)
{
	const std::string Text = Message.empty()
		? std::format("Assertion failed: {} [{}:{}]", Expression, File, Line)
		: std::format("Assertion failed: {} [{}:{}] {}", Expression, File, Line, Message);

	FLog::Write(LogCore, ELogVerbosity::Fatal, Text);

	// FLog::Write does not return for Fatal messages; this satisfies [[noreturn]] for the compiler.
	std::abort();
}
