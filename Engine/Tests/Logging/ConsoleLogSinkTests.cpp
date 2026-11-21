#include "Logging/ConsoleLogSink.h"

#include "Logging/LogMessage.h"

#include <doctest/doctest.h>

#include <iostream>
#include <sstream>
#include <streambuf>
#include <string>

namespace
{
	/** Redirects std::cout into a string for the lifetime of the object. */
	class FScopedStandardOutputCapture
	{
	public:
		FScopedStandardOutputCapture()
			: OriginalBuffer(std::cout.rdbuf(Captured.rdbuf()))
		{
		}

		~FScopedStandardOutputCapture()
		{
			std::cout.rdbuf(OriginalBuffer);
		}

		FScopedStandardOutputCapture(const FScopedStandardOutputCapture&) = delete;
		FScopedStandardOutputCapture& operator=(const FScopedStandardOutputCapture&) = delete;

		std::string GetOutput() const
		{
			return Captured.str();
		}

	private:
		std::ostringstream Captured;
		std::streambuf* OriginalBuffer;
	};

	FLogMessage MakeMessage(ELogVerbosity Verbosity, const std::string& Text)
	{
		return FLogMessage{
			.Time = {},
			.Verbosity = Verbosity,
			.Category = "LogExample",
			.Text = Text,
		};
	}
}

TEST_SUITE("FConsoleLogSink")
{
	TEST_CASE("A message is printed as its one-line form followed by a newline")
	{
		const FLogMessage Message = MakeMessage(ELogVerbosity::Warning, "Low memory");
		std::string Output;
		{
			const FScopedStandardOutputCapture Capture;
			FConsoleLogSink Sink;

			Sink.Write(Message);

			Output = Capture.GetOutput();
		}

		CHECK(Output == Message.ToString() + "\n");
	}

	TEST_CASE("By default Display and more severe messages are printed and the rest are not")
	{
		std::string Output;
		{
			const FScopedStandardOutputCapture Capture;
			FConsoleLogSink Sink;

			Sink.Write(MakeMessage(ELogVerbosity::Fatal, "fatal-text"));
			Sink.Write(MakeMessage(ELogVerbosity::Error, "error-text"));
			Sink.Write(MakeMessage(ELogVerbosity::Warning, "warning-text"));
			Sink.Write(MakeMessage(ELogVerbosity::Display, "display-text"));
			Sink.Write(MakeMessage(ELogVerbosity::Log, "log-text"));
			Sink.Write(MakeMessage(ELogVerbosity::Verbose, "verbose-text"));
			Sink.Write(MakeMessage(ELogVerbosity::VeryVerbose, "very-verbose-text"));

			Output = Capture.GetOutput();
		}

		CHECK(Output.find("fatal-text") != std::string::npos);
		CHECK(Output.find("error-text") != std::string::npos);
		CHECK(Output.find("warning-text") != std::string::npos);
		CHECK(Output.find("display-text") != std::string::npos);
		CHECK(Output.find("log-text") == std::string::npos);
		CHECK(Output.find("verbose-text") == std::string::npos);
	}

	TEST_CASE("The maximum verbosity can be changed at construction")
	{
		std::string Output;
		{
			const FScopedStandardOutputCapture Capture;
			FConsoleLogSink Sink(ELogVerbosity::Verbose);

			Sink.Write(MakeMessage(ELogVerbosity::Log, "log-text"));
			Sink.Write(MakeMessage(ELogVerbosity::Verbose, "verbose-text"));
			Sink.Write(MakeMessage(ELogVerbosity::VeryVerbose, "very-verbose-text"));

			Output = Capture.GetOutput();
		}

		CHECK(Output.find("log-text") != std::string::npos);
		CHECK(Output.find("verbose-text") != std::string::npos);
		CHECK(Output.find("very-verbose-text") == std::string::npos);
	}

	TEST_CASE("Messages are printed in the order they were written, one per line")
	{
		std::string Output;
		{
			const FScopedStandardOutputCapture Capture;
			FConsoleLogSink Sink;

			Sink.Write(MakeMessage(ELogVerbosity::Display, "first"));
			Sink.Write(MakeMessage(ELogVerbosity::Display, "second"));

			Output = Capture.GetOutput();
		}

		const FLogMessage First = MakeMessage(ELogVerbosity::Display, "first");
		const FLogMessage Second = MakeMessage(ELogVerbosity::Display, "second");
		CHECK(Output == First.ToString() + "\n" + Second.ToString() + "\n");
	}
}
