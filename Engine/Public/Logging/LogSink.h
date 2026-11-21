#pragma once

#include "Core/CoreTypes.h"

struct FLogMessage;

/**
 * A destination for log messages, registered with FLog::AddSink.
 *
 * FLog calls Write for one message at a time, so an implementation needs no locking to protect itself from
 * concurrent Write calls. An implementation must make each message durable before returning (for example by
 * flushing), because a Fatal message is followed by process termination. It must not call FLog or WW_LOG.
 */
class ENGINE_API ILogSink
{
public:
	virtual ~ILogSink() = default;

	/** Receives one message. */
	virtual void Write(const FLogMessage& Message) = 0;
};
