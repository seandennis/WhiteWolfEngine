#pragma once

#include "Core/CoreTypes.h"
#include "Logging/LogVerbosity.h"

#include <atomic>
#include <string_view>

/**
 * A named group of log messages with its own verbosity threshold.
 *
 * Define a category once, as a global, and declare it in a header for the code that logs to it:
 *
 *     // MySystem.h
 *     extern ENGINE_API FLogCategory LogMySystem;
 *
 *     // MySystem.cpp
 *     constinit FLogCategory LogMySystem("LogMySystem", ELogVerbosity::Log);
 *
 * A category is constant-initialized and trivially destructible, so it can be used at any time, including during
 * static initialization and shutdown. Its verbosity may be read and changed from any thread.
 */
class FLogCategory
{
public:
	/**
	 * @param InName The category name. Must have static storage duration; normally a string literal.
	 * @param InVerbosity The least severe verbosity this category lets through.
	 */
	constexpr FLogCategory(const char* InName, ELogVerbosity InVerbosity)
		: Name(InName)
		, Verbosity(InVerbosity)
	{
	}

	FLogCategory(const FLogCategory&) = delete;
	FLogCategory& operator=(const FLogCategory&) = delete;

	/** Returns the category name. */
	std::string_view GetName() const
	{
		return Name;
	}

	/** Returns the least severe verbosity this category lets through. */
	ELogVerbosity GetVerbosity() const
	{
		return Verbosity.load(std::memory_order_relaxed);
	}

	/** Sets the least severe verbosity this category lets through. */
	void SetVerbosity(ELogVerbosity InVerbosity)
	{
		Verbosity.store(InVerbosity, std::memory_order_relaxed);
	}

	/** Returns true if a message of the given verbosity is discarded. Fatal messages are never suppressed. */
	bool IsSuppressed(ELogVerbosity MessageVerbosity) const
	{
		return MessageVerbosity != ELogVerbosity::Fatal && MessageVerbosity > GetVerbosity();
	}

private:
	const char* Name;
	std::atomic<ELogVerbosity> Verbosity;
};
