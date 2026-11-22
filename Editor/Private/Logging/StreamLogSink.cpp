#include "Logging/StreamLogSink.h"

#include "Logging/LogMessage.h"

#include <ostream>

FStreamLogSink::FStreamLogSink(std::ostream& InStream, ELogVerbosity InMaxVerbosity)
	: Stream(InStream)
	, MaxVerbosity(InMaxVerbosity)
{
}

void FStreamLogSink::Write(const FLogMessage& Message)
{
	if (Message.Verbosity > MaxVerbosity)
	{
		return;
	}

	Stream << Message.ToString() << '\n';
	Stream.flush();
}
