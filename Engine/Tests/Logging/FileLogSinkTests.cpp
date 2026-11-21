#include "Logging/FileLogSink.h"

#include "Logging/LogMessage.h"
#include "TestSupport/ScopedTempDirectory.h"

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace
{
	FLogMessage MakeMessage(const std::string& Text)
	{
		return FLogMessage{
			.Time = {},
			.Verbosity = ELogVerbosity::Warning,
			.Category = "LogExample",
			.Text = Text,
		};
	}

	/** Returns the exact bytes of a file. */
	std::string ReadFileBytes(const std::filesystem::path& Path)
	{
		std::ifstream Stream(Path, std::ios::in | std::ios::binary);
		std::ostringstream Contents;
		Contents << Stream.rdbuf();
		return Contents.str();
	}
}

TEST_SUITE("FFileLogSink")
{
	TEST_CASE("Messages are written to the file one per line and are readable immediately")
	{
		const FScopedTempDirectory Directory;
		const std::filesystem::path Path = Directory.GetPath() / "Engine.log";
		const FLogMessage First = MakeMessage("first");
		const FLogMessage Second = MakeMessage("second");

		FFileLogSink Sink(Path);
		REQUIRE(Sink.IsOpen());
		Sink.Write(First);
		Sink.Write(Second);

		// The sink is still open: each message must already be on disk, because a fatal error may follow.
		CHECK(ReadFileBytes(Path) == First.ToString() + "\n" + Second.ToString() + "\n");
	}

	TEST_CASE("Lines end with a line feed only, on every platform")
	{
		const FScopedTempDirectory Directory;
		const std::filesystem::path Path = Directory.GetPath() / "Engine.log";

		{
			FFileLogSink Sink(Path);
			Sink.Write(MakeMessage("text"));
		}

		CHECK(ReadFileBytes(Path).find('\r') == std::string::npos);
	}

	TEST_CASE("Missing parent directories are created")
	{
		const FScopedTempDirectory Directory;
		const std::filesystem::path Path = Directory.GetPath() / "Saved" / "Logs" / "Engine.log";

		FFileLogSink Sink(Path);

		CHECK(Sink.IsOpen());
		CHECK(std::filesystem::is_regular_file(Path));
	}

	TEST_CASE("An existing file is replaced")
	{
		const FScopedTempDirectory Directory;
		const std::filesystem::path Path = Directory.GetPath() / "Engine.log";
		{
			std::ofstream Existing(Path, std::ios::out | std::ios::binary);
			Existing << "old contents\n";
		}
		const FLogMessage Message = MakeMessage("new");

		{
			FFileLogSink Sink(Path);
			Sink.Write(Message);
		}

		CHECK(ReadFileBytes(Path) == Message.ToString() + "\n");
	}

	TEST_CASE("A sink whose file cannot be created reports that and discards messages")
	{
		const FScopedTempDirectory Directory;

		// A directory already exists at this path, so no file can be created there.
		FFileLogSink Sink(Directory.GetPath());

		CHECK_FALSE(Sink.IsOpen());
		Sink.Write(MakeMessage("discarded"));
		CHECK(std::filesystem::is_directory(Directory.GetPath()));
	}
}
