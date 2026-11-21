#pragma once

#include "Core/CoreTypes.h"

#include <format>
#include <string>

/** Reports failed assertions. Use the WW_CHECK macros instead of calling this directly. */
class FDebug
{
public:
	/**
	 * Logs a Fatal message describing a failed assertion, which terminates the process. Never returns.
	 *
	 * @param Expression The source text of the condition that was false.
	 * @param File The source file containing the assertion.
	 * @param Line The line of the assertion.
	 * @param Message Additional context, or an empty string.
	 */
	[[noreturn]] ENGINE_API static void CheckFailed(
		const char* Expression, const char* File, int32 Line, const std::string& Message);
};

/**
 * Asserts that a condition that must always hold is true. If it is false, a Fatal error is logged and the process is
 * terminated. Checks are active in every build configuration, and Expression is always evaluated exactly once.
 *
 * Use checks for programming errors only. Report conditions that can occur in a correct program (bad input, missing
 * files) through return values and the log.
 */
#define WW_CHECK(Expression) \
	((Expression) ? static_cast<void>(0) : FDebug::CheckFailed(#Expression, __FILE__, __LINE__, std::string()))

/**
 * WW_CHECK with a std::format message that explains the failure:
 *
 *     WW_CHECKF(Index < Count, "Index {} is out of range ({} elements)", Index, Count);
 *
 * The message arguments are evaluated only when the check fails.
 */
#define WW_CHECKF(Expression, Format, ...) \
	((Expression) \
			? static_cast<void>(0) \
			: FDebug::CheckFailed(#Expression, __FILE__, __LINE__, std::format(Format __VA_OPT__(, ) __VA_ARGS__)))
