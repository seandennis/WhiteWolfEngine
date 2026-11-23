#include "Logging/MemoryLogSink.h"

#include "TestSupport/FatalErrorException.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>

#include <chrono>
#include <string>
#include <vector>

namespace
{
	FLogMessage MakeMessage(const std::string& Text)
	{
		return FLogMessage{
			.Time = {},
			.Verbosity = ELogVerbosity::Log,
			.Category = "LogExample",
			.Text = Text,
		};
	}
}

TEST_SUITE("FMemoryLogSink")
{
	TEST_CASE("A new sink is empty")
	{
		const FMemoryLogSink Sink;

		CHECK(Sink.GetMessages().empty());
	}

	TEST_CASE("Messages are stored in the order they were written")
	{
		FMemoryLogSink Sink;

		Sink.Write(MakeMessage("First"));
		Sink.Write(MakeMessage("Second"));
		Sink.Write(MakeMessage("Third"));

		const std::vector<FLogMessage> Messages = Sink.GetMessages();
		REQUIRE(Messages.size() == 3);
		CHECK(Messages[0].Text == "First");
		CHECK(Messages[1].Text == "Second");
		CHECK(Messages[2].Text == "Third");
	}

	TEST_CASE("Every field of a message is stored")
	{
		FMemoryLogSink Sink;
		FLogMessage Message = MakeMessage("Text");
		Message.Verbosity = ELogVerbosity::Error;
		Message.Category = "LogOther";
		Message.Time = std::chrono::system_clock::time_point(std::chrono::seconds(1234));

		Sink.Write(Message);

		const std::vector<FLogMessage> Messages = Sink.GetMessages();
		REQUIRE(Messages.size() == 1);
		CHECK(Messages[0].Verbosity == ELogVerbosity::Error);
		CHECK(Messages[0].Category == "LogOther");
		CHECK(Messages[0].Text == "Text");
		CHECK(Messages[0].Time == Message.Time);
	}

	TEST_CASE("The oldest messages are discarded once the capacity is reached")
	{
		FMemoryLogSink Sink(3);

		for (int32 Index = 1; Index <= 5; ++Index)
		{
			Sink.Write(MakeMessage(std::to_string(Index)));
		}

		const std::vector<FLogMessage> Messages = Sink.GetMessages();
		REQUIRE(Messages.size() == 3);
		CHECK(Messages[0].Text == "3");
		CHECK(Messages[1].Text == "4");
		CHECK(Messages[2].Text == "5");
	}

	TEST_CASE("A sink with a capacity of one keeps only the latest message")
	{
		FMemoryLogSink Sink(1);

		Sink.Write(MakeMessage("First"));
		Sink.Write(MakeMessage("Second"));

		const std::vector<FLogMessage> Messages = Sink.GetMessages();
		REQUIRE(Messages.size() == 1);
		CHECK(Messages[0].Text == "Second");
	}

	TEST_CASE("A capacity below one is a fatal error")
	{
		const FScopedLogCapture Capture;

		CHECK_THROWS_AS(FMemoryLogSink(0), FFatalErrorException);
		CHECK_THROWS_AS(FMemoryLogSink(-5), FFatalErrorException);
	}

	TEST_CASE("Clear discards every message and the sink keeps working")
	{
		FMemoryLogSink Sink;
		Sink.Write(MakeMessage("First"));

		Sink.Clear();
		CHECK(Sink.GetMessages().empty());

		Sink.Write(MakeMessage("Second"));
		const std::vector<FLogMessage> Messages = Sink.GetMessages();
		REQUIRE(Messages.size() == 1);
		CHECK(Messages[0].Text == "Second");
	}

	TEST_CASE("The revision is another number after every change to what is stored, and only then")
	{
		FMemoryLogSink Sink(2);
		const int64 Empty = Sink.GetRevision();
		CHECK(Sink.GetRevision() == Empty);

		Sink.Write(MakeMessage("First"));
		const int64 AfterFirst = Sink.GetRevision();
		CHECK(AfterFirst != Empty);

		// Reading changes nothing.
		CHECK(Sink.GetMessages().size() == 1);
		CHECK(Sink.GetRevision() == AfterFirst);

		// A message that pushes the oldest one out is a change like any other.
		Sink.Write(MakeMessage("Second"));
		const int64 AfterSecond = Sink.GetRevision();
		Sink.Write(MakeMessage("Third"));
		const int64 AfterThird = Sink.GetRevision();
		CHECK(AfterSecond != AfterFirst);
		CHECK(AfterThird != AfterSecond);

		// So is discarding them, also when the same number of messages is stored afterwards as some time before.
		Sink.Clear();
		const int64 AfterClear = Sink.GetRevision();
		CHECK(AfterClear != AfterThird);
		CHECK(AfterClear != Empty);
	}

	TEST_CASE("GetMessages returns a snapshot that later writes do not change")
	{
		FMemoryLogSink Sink;
		Sink.Write(MakeMessage("First"));

		const std::vector<FLogMessage> Snapshot = Sink.GetMessages();
		Sink.Write(MakeMessage("Second"));
		Sink.Clear();

		REQUIRE(Snapshot.size() == 1);
		CHECK(Snapshot[0].Text == "First");
	}
}
