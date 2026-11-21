#include "Logging/ConsoleLogSink.h"

#include "Logging/LogMessage.h"

#include <iostream>

FConsoleLogSink::FConsoleLogSink(ELogVerbosity InMaxVerbosity)
	: MaxVerbosity(InMaxVerbosity)
{
}

void FConsoleLogSink::Write(const FLogMessage& Message)
{
	if (Message.Verbosity > MaxVerbosity)
	{
		return;
	}

	std::cout << Message.ToString() << '\n';
	std::cout.flush();
}
