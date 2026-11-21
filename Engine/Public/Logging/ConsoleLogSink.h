#pragma once

#include "Core/CoreTypes.h"
#include "Logging/LogSink.h"
#include "Logging/LogVerbosity.h"

/** Writes log messages to standard output, one line per message. */
class ENGINE_API FConsoleLogSink final : public ILogSink
{
public:
	/**
	 * @param InMaxVerbosity The least severe verbosity to print. The default shows Display and everything more
	 * severe, so ordinary Log messages stay out of the console.
	 */
	explicit FConsoleLogSink(ELogVerbosity InMaxVerbosity = ELogVerbosity::Display);

	void Write(const FLogMessage& Message) override;

private:
	ELogVerbosity MaxVerbosity;
};
