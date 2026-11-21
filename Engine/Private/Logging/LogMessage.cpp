#include "Logging/LogMessage.h"

#include <format>

std::string FLogMessage::ToString() const
{
	const std::chrono::sys_time<std::chrono::milliseconds> TimeInMilliseconds =
		std::chrono::floor<std::chrono::milliseconds>(Time);

	const bool bShowVerbosity = Verbosity != ELogVerbosity::Display && Verbosity != ELogVerbosity::Log;
	if (bShowVerbosity)
	{
		return std::format("[{:%FT%T}Z] {}: {}: {}", TimeInMilliseconds, Category, ::ToString(Verbosity), Text);
	}

	return std::format("[{:%FT%T}Z] {}: {}", TimeInMilliseconds, Category, Text);
}
