#pragma once

#include "Logging/Log.h"
#include "Logging/MemoryLogSink.h"
#include "TestSupport/FatalErrorException.h"

#include <memory>
#include <utility>
#include <vector>

/**
 * Captures the log for the duration of a test.
 *
 * While an instance is alive, every log message is recorded and can be read with GetMessages, and a Fatal message
 * throws FFatalErrorException instead of terminating the process. Destroying the instance restores the previous
 * fatal error handler and stops recording.
 */
class FScopedLogCapture
{
public:
	FScopedLogCapture()
		: Sink(std::make_shared<FMemoryLogSink>())
	{
		FLog::AddSink(Sink);
		PreviousFatalErrorHandler =
			FLog::SetFatalErrorHandler([](const FLogMessage& Message) { throw FFatalErrorException(Message.Text); });
	}

	~FScopedLogCapture()
	{
		FLog::SetFatalErrorHandler(std::move(PreviousFatalErrorHandler));
		FLog::RemoveSink(Sink);
	}

	FScopedLogCapture(const FScopedLogCapture&) = delete;
	FScopedLogCapture& operator=(const FScopedLogCapture&) = delete;

	/** Returns the messages logged since this capture was created, oldest first. */
	std::vector<FLogMessage> GetMessages() const
	{
		return Sink->GetMessages();
	}

private:
	std::shared_ptr<FMemoryLogSink> Sink;
	FLog::FFatalErrorHandler PreviousFatalErrorHandler;
};
