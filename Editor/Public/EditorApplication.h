#pragma once

#include "CoreMinimal.h"

#include <iosfwd>
#include <string>
#include <vector>

/**
 * What the editor's executable does, as a function that tests can call with streams of their own.
 *
 * WhiteWolfEditor --mcp     serves an AI agent on standard input and output (FAgentServer) until the input ends
 * WhiteWolfEditor --help    prints how to start the editor
 *
 * The editor's windows are not built yet, so starting it without an option prints the same help as an error.
 */
class FEditorApplication
{
public:
	/**
	 * Runs the editor.
	 *
	 * @param Arguments The command line without the name of the program.
	 * @param Input Where an agent's messages are read from.
	 * @param Output Where the answers to an agent, or the help, are written. With --mcp nothing else is written
	 * here: the protocol allows only its own messages on this stream.
	 * @param ErrorOutput Where errors are written, and with --mcp the warnings and errors of the log.
	 * @return The exit code of the process: 0 if the editor did what was asked, 2 for a command line it does not
	 * understand.
	 */
	EDITOR_API static int32 Run(const std::vector<std::string>& Arguments,
		std::istream& Input,
		std::ostream& Output,
		std::ostream& ErrorOutput);
};
