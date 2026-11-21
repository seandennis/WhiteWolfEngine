#pragma once

#include "Core/CoreTypes.h"
#include "Logging/LogMessage.h"
#include "Logging/LogSink.h"

#include <deque>
#include <mutex>
#include <vector>

/**
 * Keeps the most recent log messages in memory.
 *
 * Used wherever the log has to be read back: the editor's Output Log, the agent interface and tests. Messages may be
 * read from any thread while other threads log.
 */
class ENGINE_API FMemoryLogSink final : public ILogSink
{
public:
	/** The number of messages kept when no capacity is given. */
	static constexpr int32 DefaultCapacity = 4096;

	/** @param InCapacity The number of messages to keep; older messages are discarded. Must be at least 1. */
	explicit FMemoryLogSink(int32 InCapacity = DefaultCapacity);

	FMemoryLogSink(const FMemoryLogSink&) = delete;
	FMemoryLogSink& operator=(const FMemoryLogSink&) = delete;

	void Write(const FLogMessage& Message) override;

	/** Returns a copy of the stored messages, oldest first. */
	std::vector<FLogMessage> GetMessages() const;

	/** Discards every stored message. */
	void Clear();

private:
	mutable std::mutex Mutex;
	std::deque<FLogMessage> Messages;
	int32 Capacity;
};
