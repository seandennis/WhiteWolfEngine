#include "Logging/FileLogSink.h"

#include "Logging/LogMessage.h"

#include <system_error>

FFileLogSink::FFileLogSink(const std::filesystem::path& Path)
{
	// A failure to create the directories shows up as a failure to open the file, which IsOpen reports.
	std::error_code IgnoredError;
	std::filesystem::create_directories(Path.parent_path(), IgnoredError);

	// Binary mode keeps line endings as "\n" on every platform.
	Stream.open(Path, std::ios::out | std::ios::trunc | std::ios::binary);
}

bool FFileLogSink::IsOpen() const
{
	return Stream.is_open();
}

void FFileLogSink::Write(const FLogMessage& Message)
{
	if (!Stream.is_open())
	{
		return;
	}

	Stream << Message.ToString() << '\n';
	Stream.flush();
}
