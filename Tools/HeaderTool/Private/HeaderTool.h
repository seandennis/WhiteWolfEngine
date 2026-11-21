#pragma once

#include "Core/CoreTypes.h"

#include <iosfwd>
#include <span>
#include <string>

/**
 * The header tool as a whole: reads every header under a module's source directories and writes the source file
 * that registers the module's reflected classes.
 */
class FHeaderTool
{
public:
	/**
	 * Runs the tool.
	 *
	 * @param Arguments The command line, without the program name:
	 *
	 *     --output <file>   The source file to write. Its directory is created if needed.
	 *     --root <dir>      A directory to search for headers (*.h), with its subdirectories. Give one for each
	 *                       include directory of the module; a header is included by its path below its root.
	 *
	 * @param ErrorStream Receives every problem, one per line, as "file(line): error: message".
	 * @return 0 if the file was written, 1 if a header has an error, 2 if the arguments are wrong or a file could
	 * not be read or written. Nothing is written unless the result is 0.
	 */
	static int32 Run(std::span<const std::string> Arguments, std::ostream& ErrorStream);
};
