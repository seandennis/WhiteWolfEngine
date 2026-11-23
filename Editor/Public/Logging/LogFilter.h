#pragma once

#include "CoreMinimal.h"

#include "Logging/LogMessage.h"
#include "Logging/LogVerbosity.h"

#include <string>

/**
 * Which messages of the log are wanted: by how important they are, by what they are about and by what they say.
 * A new filter wants them all. The Output Log shows the messages that its filter wants, and the command Log.Read
 * returns them.
 */
struct FLogFilter
{
	/** The least important kind of message that is wanted. Every more important kind is wanted with it. */
	ELogVerbosity LeastVerbosity = ELogVerbosity::VeryVerbose;

	/** The category that a message has to be of, such as "LogScene", written exactly so. Empty for any. */
	std::string Category;

	/**
	 * What a message has to have somewhere in its text or in the name of its category, in capitals or in small
	 * letters. Empty for anything.
	 */
	std::string SearchText;

	/** Returns true if a message is one of those that are wanted. */
	EDITOR_API bool Matches(const FLogMessage& Message) const;

	bool operator==(const FLogFilter& Other) const = default;
};
