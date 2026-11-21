#include "Logging/Log.h"

#include "Core/Assert.h"
#include "Logging/LogSink.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <utility>
#include <vector>

constinit FLogCategory LogCore("LogCore", ELogVerbosity::Log);

namespace
{
	/** Everything the log shares between threads. */
	struct FLogState
	{
		std::mutex Mutex;
		std::vector<std::shared_ptr<ILogSink>> Sinks;
		FLog::FFatalErrorHandler FatalErrorHandler;
	};

	FLogState& GetLogState()
	{
		static FLogState State;
		return State;
	}
}

void FLog::AddSink(std::shared_ptr<ILogSink> Sink)
{
	WW_CHECK(Sink != nullptr);

	FLogState& State = GetLogState();
	const std::scoped_lock Lock(State.Mutex);

	const bool bAlreadyRegistered = std::ranges::find(State.Sinks, Sink) != State.Sinks.end();
	if (!bAlreadyRegistered)
	{
		State.Sinks.push_back(std::move(Sink));
	}
}

void FLog::RemoveSink(const std::shared_ptr<ILogSink>& Sink)
{
	FLogState& State = GetLogState();
	const std::scoped_lock Lock(State.Mutex);

	std::erase(State.Sinks, Sink);
}

void FLog::Write(const FLogCategory& Category, ELogVerbosity Verbosity, std::string Text)
{
	const FLogMessage Message{
		.Time = std::chrono::system_clock::now(),
		.Verbosity = Verbosity,
		.Category = std::string(Category.GetName()),
		.Text = std::move(Text),
	};

	FLogState& State = GetLogState();
	FFatalErrorHandler FatalErrorHandler;
	{
		const std::scoped_lock Lock(State.Mutex);

		for (const std::shared_ptr<ILogSink>& Sink : State.Sinks)
		{
			Sink->Write(Message);
		}

		FatalErrorHandler = State.FatalErrorHandler;
	}

	if (Verbosity != ELogVerbosity::Fatal)
	{
		return;
	}

	// The handler runs outside the lock so that it may log, or throw, without leaving the log locked.
	if (FatalErrorHandler)
	{
		FatalErrorHandler(Message);
	}
	else
	{
		const std::string Line = Message.ToString() + "\n";
		std::fputs(Line.c_str(), stderr);
		std::fflush(stderr);
	}

	std::abort();
}

FLog::FFatalErrorHandler FLog::SetFatalErrorHandler(FFatalErrorHandler Handler)
{
	FLogState& State = GetLogState();
	const std::scoped_lock Lock(State.Mutex);

	return std::exchange(State.FatalErrorHandler, std::move(Handler));
}
