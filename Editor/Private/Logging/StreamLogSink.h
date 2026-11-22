#pragma once

#include "CoreMinimal.h"

#include "Logging/LogSink.h"
#include "Logging/LogVerbosity.h"

#include <iosfwd>

struct FLogMessage;

/**
 * Writes log messages to a stream of the caller's, one line per message. The editor uses it to send the log to
 * standard error while standard output carries a protocol.
 */
class FStreamLogSink final : public ILogSink
{
public:
	/**
	 * @param InStream The stream to write to. It must outlive the sink's registration with the log.
	 * @param InMaxVerbosity The least severe verbosity to write.
	 */
	FStreamLogSink(std::ostream& InStream, ELogVerbosity InMaxVerbosity);

	void Write(const FLogMessage& Message) override;

private:
	std::ostream& Stream;
	ELogVerbosity MaxVerbosity;
};
