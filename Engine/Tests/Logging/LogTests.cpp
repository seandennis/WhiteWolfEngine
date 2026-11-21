#include "Logging/Log.h"

#include "Logging/MemoryLogSink.h"
#include "TestSupport/FatalErrorException.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>

#include <chrono>
#include <cstddef>
#include <format>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace
{
	/** Registers a memory sink for the lifetime of the object, without changing the fatal error handler. */
	class FScopedMemorySink
	{
	public:
		FScopedMemorySink()
			: Sink(std::make_shared<FMemoryLogSink>())
		{
			FLog::AddSink(Sink);
		}

		~FScopedMemorySink()
		{
			FLog::RemoveSink(Sink);
		}

		FScopedMemorySink(const FScopedMemorySink&) = delete;
		FScopedMemorySink& operator=(const FScopedMemorySink&) = delete;

		const std::shared_ptr<FMemoryLogSink>& GetSink() const
		{
			return Sink;
		}

		std::vector<FLogMessage> GetMessages() const
		{
			return Sink->GetMessages();
		}

	private:
		std::shared_ptr<FMemoryLogSink> Sink;
	};
}

TEST_SUITE("FLog")
{
	TEST_CASE("WW_LOG delivers a formatted message to a registered sink")
	{
		const FScopedLogCapture Capture;
		FLogCategory Category("LogExample", ELogVerbosity::Log);

		const std::chrono::system_clock::time_point Before = std::chrono::system_clock::now();
		WW_LOG(Category, Warning, "Loaded {} of {} assets from '{}'", 3, 4, "Content");
		const std::chrono::system_clock::time_point After = std::chrono::system_clock::now();

		const std::vector<FLogMessage> Messages = Capture.GetMessages();
		REQUIRE(Messages.size() == 1);
		CHECK(Messages[0].Category == "LogExample");
		CHECK(Messages[0].Verbosity == ELogVerbosity::Warning);
		CHECK(Messages[0].Text == "Loaded 3 of 4 assets from 'Content'");
		CHECK(Messages[0].Time >= Before);
		CHECK(Messages[0].Time <= After);
	}

	TEST_CASE("WW_LOG accepts a format string with no arguments")
	{
		const FScopedLogCapture Capture;
		FLogCategory Category("LogExample", ELogVerbosity::Log);

		WW_LOG(Category, Log, "Engine started");

		const std::vector<FLogMessage> Messages = Capture.GetMessages();
		REQUIRE(Messages.size() == 1);
		CHECK(Messages[0].Text == "Engine started");
	}

	TEST_CASE("WW_LOG discards messages that the category suppresses")
	{
		const FScopedLogCapture Capture;
		FLogCategory Category("LogExample", ELogVerbosity::Warning);

		WW_LOG(Category, Log, "Suppressed");
		WW_LOG(Category, Verbose, "Suppressed");
		WW_LOG(Category, Warning, "Delivered");
		WW_LOG(Category, Error, "Delivered");

		const std::vector<FLogMessage> Messages = Capture.GetMessages();
		REQUIRE(Messages.size() == 2);
		CHECK(Messages[0].Verbosity == ELogVerbosity::Warning);
		CHECK(Messages[1].Verbosity == ELogVerbosity::Error);
	}

	TEST_CASE("WW_LOG does not evaluate its arguments when the message is suppressed")
	{
		const FScopedLogCapture Capture;
		FLogCategory Category("LogExample", ELogVerbosity::Log);
		int32 EvaluationCount = 0;
		const auto CountEvaluation = [&EvaluationCount]()
		{
			++EvaluationCount;
			return EvaluationCount;
		};

		WW_LOG(Category, Verbose, "Value {}", CountEvaluation());
		CHECK(EvaluationCount == 0);

		WW_LOG(Category, Log, "Value {}", CountEvaluation());
		CHECK(EvaluationCount == 1);
	}

	TEST_CASE("WW_LOG follows changes to the category's verbosity")
	{
		const FScopedLogCapture Capture;
		FLogCategory Category("LogExample", ELogVerbosity::Log);

		WW_LOG(Category, Verbose, "Before");
		Category.SetVerbosity(ELogVerbosity::Verbose);
		WW_LOG(Category, Verbose, "After");

		const std::vector<FLogMessage> Messages = Capture.GetMessages();
		REQUIRE(Messages.size() == 1);
		CHECK(Messages[0].Text == "After");
	}

	TEST_CASE("WW_LOG is an expression that can be used wherever a statement can")
	{
		const FScopedLogCapture Capture;
		FLogCategory Category("LogExample", ELogVerbosity::Log);

		const bool bLogged = (WW_LOG(Category, Log, "Inside an expression"), true);

		CHECK(bLogged);
		CHECK(Capture.GetMessages().size() == 1);
	}

	TEST_CASE("Write delivers a message even when the category would suppress it")
	{
		const FScopedLogCapture Capture;
		const FLogCategory Category("LogExample", ELogVerbosity::Error);

		FLog::Write(Category, ELogVerbosity::Verbose, "Unfiltered");

		const std::vector<FLogMessage> Messages = Capture.GetMessages();
		REQUIRE(Messages.size() == 1);
		CHECK(Messages[0].Verbosity == ELogVerbosity::Verbose);
		CHECK(Messages[0].Text == "Unfiltered");
	}

	TEST_CASE("Every registered sink receives each message once")
	{
		const FScopedMemorySink First;
		const FScopedMemorySink Second;
		FLogCategory Category("LogExample", ELogVerbosity::Log);

		WW_LOG(Category, Log, "Hello");

		CHECK(First.GetMessages().size() == 1);
		CHECK(Second.GetMessages().size() == 1);
	}

	TEST_CASE("Adding a sink twice does not deliver messages twice")
	{
		const FScopedMemorySink Scoped;
		FLog::AddSink(Scoped.GetSink());
		FLogCategory Category("LogExample", ELogVerbosity::Log);

		WW_LOG(Category, Log, "Hello");

		CHECK(Scoped.GetMessages().size() == 1);
	}

	TEST_CASE("A removed sink receives no further messages")
	{
		const std::shared_ptr<FMemoryLogSink> Sink = std::make_shared<FMemoryLogSink>();
		FLogCategory Category("LogExample", ELogVerbosity::Log);

		FLog::AddSink(Sink);
		WW_LOG(Category, Log, "First");
		FLog::RemoveSink(Sink);
		WW_LOG(Category, Log, "Second");

		const std::vector<FLogMessage> Messages = Sink->GetMessages();
		REQUIRE(Messages.size() == 1);
		CHECK(Messages[0].Text == "First");
	}

	TEST_CASE("Removing a sink that was never added has no effect")
	{
		const FScopedMemorySink Registered;
		const std::shared_ptr<FMemoryLogSink> NeverAdded = std::make_shared<FMemoryLogSink>();
		FLogCategory Category("LogExample", ELogVerbosity::Log);

		FLog::RemoveSink(NeverAdded);
		WW_LOG(Category, Log, "Hello");

		CHECK(Registered.GetMessages().size() == 1);
		CHECK(NeverAdded->GetMessages().empty());
	}

	TEST_CASE("Adding a null sink is a fatal error")
	{
		const FScopedLogCapture Capture;

		CHECK_THROWS_AS(FLog::AddSink(nullptr), FFatalErrorException);
	}

	TEST_CASE("A Fatal message reaches the sinks and then the fatal error handler")
	{
		const FScopedLogCapture Capture;
		FLogCategory Category("LogExample", ELogVerbosity::Log);

		CHECK_THROWS_AS(WW_LOG(Category, Fatal, "Device lost after {} frames", 12), FFatalErrorException);

		const std::vector<FLogMessage> Messages = Capture.GetMessages();
		REQUIRE(Messages.size() == 1);
		CHECK(Messages[0].Verbosity == ELogVerbosity::Fatal);
		CHECK(Messages[0].Text == "Device lost after 12 frames");
	}

	TEST_CASE("A Fatal message is logged even when the category is at its most restrictive")
	{
		const FScopedLogCapture Capture;
		FLogCategory Category("LogExample", ELogVerbosity::Fatal);

		CHECK_THROWS_AS(WW_LOG(Category, Fatal, "Unrecoverable"), FFatalErrorException);

		CHECK(Capture.GetMessages().size() == 1);
	}

	TEST_CASE("The fatal error handler receives the message that was logged")
	{
		const FScopedMemorySink Scoped;
		FLogCategory Category("LogExample", ELogVerbosity::Log);
		FLogMessage Received;
		const FLog::FFatalErrorHandler Previous = FLog::SetFatalErrorHandler(
			[&Received](const FLogMessage& Message)
			{
				Received = Message;
				throw FFatalErrorException(Message.Text);
			});

		CHECK_THROWS_AS(WW_LOG(Category, Fatal, "Out of memory"), FFatalErrorException);
		FLog::SetFatalErrorHandler(Previous);

		CHECK(Received.Category == "LogExample");
		CHECK(Received.Verbosity == ELogVerbosity::Fatal);
		CHECK(Received.Text == "Out of memory");
	}

	TEST_CASE("SetFatalErrorHandler returns the handler it replaces")
	{
		int32 FirstCallCount = 0;
		const FLog::FFatalErrorHandler Original =
			FLog::SetFatalErrorHandler([&FirstCallCount](const FLogMessage&) { ++FirstCallCount; });

		const FLog::FFatalErrorHandler Replaced = FLog::SetFatalErrorHandler(Original);

		REQUIRE(static_cast<bool>(Replaced));
		Replaced(FLogMessage{});
		CHECK(FirstCallCount == 1);
	}

	TEST_CASE("Messages logged from several threads all arrive intact and in order per thread")
	{
		constexpr int32 ThreadCount = 8;
		constexpr int32 MessagesPerThread = 500;

		const std::shared_ptr<FMemoryLogSink> Sink = std::make_shared<FMemoryLogSink>(ThreadCount * MessagesPerThread);
		FLog::AddSink(Sink);
		FLogCategory Category("LogExample", ELogVerbosity::Log);

		{
			std::vector<std::jthread> Threads;
			for (int32 ThreadIndex = 0; ThreadIndex < ThreadCount; ++ThreadIndex)
			{
				Threads.emplace_back(
					[&Category, ThreadIndex]()
					{
						for (int32 MessageIndex = 0; MessageIndex < MessagesPerThread; ++MessageIndex)
						{
							WW_LOG(Category, Log, "{}:{}", ThreadIndex, MessageIndex);
						}
					});
			}
		}
		FLog::RemoveSink(Sink);

		const std::vector<FLogMessage> Messages = Sink->GetMessages();
		REQUIRE(Messages.size() == static_cast<std::size_t>(ThreadCount * MessagesPerThread));

		// Each thread's messages must appear complete and in the order that thread logged them.
		std::vector<int32> NextIndexPerThread(static_cast<std::size_t>(ThreadCount), 0);
		for (const FLogMessage& Message : Messages)
		{
			const std::size_t Separator = Message.Text.find(':');
			REQUIRE(Separator != std::string::npos);
			const int32 ThreadIndex = std::stoi(Message.Text.substr(0, Separator));
			const int32 MessageIndex = std::stoi(Message.Text.substr(Separator + 1));

			REQUIRE(ThreadIndex >= 0);
			REQUIRE(ThreadIndex < ThreadCount);
			int32& NextIndex = NextIndexPerThread[static_cast<std::size_t>(ThreadIndex)];
			REQUIRE(MessageIndex == NextIndex);
			++NextIndex;
		}

		for (const int32 NextIndex : NextIndexPerThread)
		{
			CHECK(NextIndex == MessagesPerThread);
		}
	}

	TEST_CASE("LogCore is available and logs at Log verbosity by default")
	{
		CHECK(LogCore.GetName() == "LogCore");
		CHECK(LogCore.GetVerbosity() == ELogVerbosity::Log);
	}
}
