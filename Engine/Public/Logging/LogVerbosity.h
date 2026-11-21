#pragma once

#include "Core/CoreTypes.h"

#include <string_view>

/** How important a log message is. Lower values are more severe. */
enum class ELogVerbosity : uint8
{
	/** An unrecoverable error. The message is written to every sink and the process is terminated. */
	Fatal,

	/** Something failed. The engine continues, but a result is wrong or missing. */
	Error,

	/** Something unexpected happened that the engine handled. */
	Warning,

	/** Information the user should see on the console as well as in the log. */
	Display,

	/** Ordinary information for the log. */
	Log,

	/** Detailed information for debugging. Suppressed by default. */
	Verbose,

	/** High-volume information for debugging. Suppressed by default. */
	VeryVerbose,
};

/** Returns the name of a verbosity level, for example "Warning". */
ENGINE_API std::string_view ToString(ELogVerbosity Verbosity);
