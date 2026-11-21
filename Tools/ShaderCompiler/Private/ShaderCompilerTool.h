#pragma once

#include "Core/CoreTypes.h"

#include <iosfwd>
#include <span>
#include <string>

/**
 * The shader compiler as a whole: compiles every shader under a directory and writes the source file that embeds
 * them in a module.
 */
class FShaderCompilerTool
{
public:
	/**
	 * Runs the tool.
	 *
	 * @param Arguments The command line, without the program name:
	 *
	 *     --output <file>   The source file to write. Its directory is created if needed.
	 *     --root <dir>      The directory to search for shaders (*.vert, *.frag), with its subdirectories. A shader
	 *                       is named by its path below this directory, and #include looks for files in it.
	 *
	 * @param ErrorStream Receives every problem, one per line, as "file(line): error: message".
	 * @return 0 if the file was written, 1 if a shader has an error, 2 if the arguments are wrong or a file could
	 * not be read or written. Nothing is written unless the result is 0.
	 */
	static int32 Run(std::span<const std::string> Arguments, std::ostream& ErrorStream);
};
