#pragma once

#include "Core/CoreTypes.h"
#include "Logging/LogVerbosity.h"

#include <chrono>
#include <string>

/** One message written to the log. */
struct FLogMessage
{
	/** When the message was logged. */
	std::chrono::system_clock::time_point Time;

	/** How important the message is. */
	ELogVerbosity Verbosity = ELogVerbosity::Log;

	/** Name of the category the message was logged to. Stored by value so a message can outlive its category. */
	std::string Category;

	/** The message text. */
	std::string Text;

	/**
	 * Formats the message as a single line with a UTC timestamp, for example
	 * "[2026-11-21T02:23:45.123Z] LogCore: Warning: Text".
	 *
	 * The verbosity is omitted for Display and Log messages.
	 */
	ENGINE_API std::string ToString() const;
};
