#include "Logging/LogVerbosity.h"

std::string_view ToString(ELogVerbosity Verbosity)
{
	switch (Verbosity)
	{
		case ELogVerbosity::Fatal:
			return "Fatal";
		case ELogVerbosity::Error:
			return "Error";
		case ELogVerbosity::Warning:
			return "Warning";
		case ELogVerbosity::Display:
			return "Display";
		case ELogVerbosity::Log:
			return "Log";
		case ELogVerbosity::Verbose:
			return "Verbose";
		case ELogVerbosity::VeryVerbose:
			return "VeryVerbose";
		default:
			return "Unknown";
	}
}
