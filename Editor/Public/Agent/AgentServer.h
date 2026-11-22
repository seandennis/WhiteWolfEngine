#pragma once

#include "CoreMinimal.h"

#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>

class FEditor;

/**
 * Lets an AI agent give the editor commands: a server of the Model Context Protocol (MCP), in which every command
 * of the editor is a tool.
 *
 * The server speaks MCP's stdio transport: it reads JSON-RPC messages, one to a line, and writes its answers, one to
 * a line. An agent starts the editor as a child process and talks to it through its standard input and output; it
 * needs no window and no network. The tools are the commands of the editor's FCommandRegistry at the time they are
 * asked for, under their own names, with a JSON schema made from their parameters, so a command that is registered
 * later is a tool without further code.
 *
 * Both generations of the protocol are served. A client that opens with "initialize" gets the handshake of the
 * revisions up to 2025-11-25. A client whose requests carry their protocol version in "_meta" gets revision
 * 2026-07-28, which has no handshake. See docs/DECISIONS.md, D-042.
 */
class FAgentServer
{
public:
	/** The name the server gives itself to its clients. */
	static constexpr std::string_view ServerName = "WhiteWolfEditor";

	/** @param InEditor The editor whose commands are the tools. It must outlive the server. */
	EDITOR_API explicit FAgentServer(FEditor& InEditor);

	/**
	 * Serves a client until its input ends: reads a message from each line of Input and writes the answer, if the
	 * message has one, as a line to Output. Empty lines are skipped. Between messages the garbage collector gets
	 * its turn, as it does between the frames of a game.
	 */
	EDITOR_API void Run(std::istream& Input, std::ostream& Output);

	/**
	 * Handles one message.
	 *
	 * @param Message A JSON-RPC message as UTF-8 text.
	 * @return The answer as one line of JSON text without a line break, or nothing if the message was a
	 * notification or an answer, neither of which is answered. A message that cannot be understood is answered
	 * with a JSON-RPC error; nothing a client sends is a fatal error.
	 */
	EDITOR_API std::optional<std::string> HandleMessage(std::string_view Message);

private:
	FEditor& Editor;
};
