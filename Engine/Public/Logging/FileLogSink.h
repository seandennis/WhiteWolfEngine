#pragma once

#include "Core/CoreTypes.h"
#include "Logging/LogSink.h"

#include <filesystem>
#include <fstream>

/** Writes log messages to a text file, one line per message, flushing after every message. */
class ENGINE_API FFileLogSink final : public ILogSink
{
public:
	/**
	 * Creates the file, replacing any existing file, and creates its parent directories if they do not exist.
	 * Check IsOpen afterwards: if the file could not be created, the sink discards every message.
	 */
	explicit FFileLogSink(const std::filesystem::path& Path);

	FFileLogSink(const FFileLogSink&) = delete;
	FFileLogSink& operator=(const FFileLogSink&) = delete;

	/** Returns true if the log file was created and is being written. */
	bool IsOpen() const;

	void Write(const FLogMessage& Message) override;

private:
	std::ofstream Stream;
};
