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

	/**
	 * Returns a number that is another one after every change to what is stored: a message that was written, and
	 * the messages being discarded. Whoever shows the messages keeps the number that went with its copy of them,
	 * and fetches them again only when the number is no longer that.
	 */
	int64 GetRevision() const;

	/** Discards every stored message. */
	void Clear();

private:
	mutable std::mutex Mutex;
	std::deque<FLogMessage> Messages;
	int32 Capacity;

	/** How many times what is stored has changed. */
	int64 Revision = 0;
};
