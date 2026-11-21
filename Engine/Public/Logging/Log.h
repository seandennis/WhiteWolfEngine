#pragma once

#include "Core/CoreTypes.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMessage.h"
#include "Logging/LogVerbosity.h"

#include <format>
#include <functional>
#include <memory>
#include <string>

class ILogSink;

/**
 * The engine log. Messages are written with WW_LOG and delivered to every registered sink.
 *
 * Every function is thread-safe.
 */
class FLog
{
public:
	/** Called with the message when a Fatal message has been logged. See SetFatalErrorHandler. */
	using FFatalErrorHandler = std::function<void(const FLogMessage&)>;

	/** Registers a sink. Sink must not be null. Adding a sink that is already registered has no effect. */
	ENGINE_API static void AddSink(std::shared_ptr<ILogSink> Sink);

	/** Unregisters a sink. Removing a sink that is not registered has no effect. */
	ENGINE_API static void RemoveSink(const std::shared_ptr<ILogSink>& Sink);

	/**
	 * Delivers a message to every sink. The category's verbosity is not consulted; WW_LOG does that.
	 *
	 * A Fatal message additionally invokes the fatal error handler and then terminates the process, so for Fatal
	 * messages this function does not return.
	 */
	ENGINE_API static void Write(const FLogCategory& Category, ELogVerbosity Verbosity, std::string Text);

	/**
	 * Replaces the fatal error handler.
	 *
	 * The handler runs after a Fatal message has reached every sink. The process is aborted when the handler
	 * returns; a handler can only prevent that by throwing, which is how tests observe fatal errors. Passing an
	 * empty handler restores the default, which prints the message to stderr.
	 *
	 * @return The handler that was installed before, or an empty handler if it was the default.
	 */
	ENGINE_API static FFatalErrorHandler SetFatalErrorHandler(FFatalErrorHandler Handler);
};

/** General-purpose category for the engine's core systems. */
extern ENGINE_API FLogCategory LogCore;

/**
 * Logs a message.
 *
 *     WW_LOG(LogCore, Warning, "Could not open '{}': {}", Path, Reason);
 *
 * Category is an FLogCategory, Verbosity is the name of an ELogVerbosity value, and Format is a std::format string
 * literal, checked at compile time against the arguments that follow it. The arguments are not evaluated when the
 * category suppresses the message.
 */
#define WW_LOG(Category, Verbosity, Format, ...) \
	((Category).IsSuppressed(ELogVerbosity::Verbosity) \
			? static_cast<void>(0) \
			: FLog::Write((Category), ELogVerbosity::Verbosity, std::format(Format __VA_OPT__(, ) __VA_ARGS__)))
