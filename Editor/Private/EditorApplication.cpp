#include "EditorApplication.h"

#include "Agent/AgentServer.h"
#include "Editor.h"
#include "Logging/Log.h"
#include "Logging/StreamLogSink.h"
#include "Object/ObjectGlobals.h"

#include <memory>
#include <ostream>
#include <string_view>

namespace
{
	constexpr int32 SuccessExitCode = 0;
	constexpr int32 UsageErrorExitCode = 2;

	constexpr std::string_view Usage =
		"White Wolf Engine editor\n"
		"\n"
		"  WhiteWolfEditor --mcp     Serve an AI agent: a Model Context Protocol server on standard input and\n"
		"                            output, in which every command of the editor is a tool.\n"
		"  WhiteWolfEditor --help    Print this help.\n"
		"\n"
		"The editor's windows are not built yet.\n";

	/** Serves an agent on the given streams until the input ends. */
	void ServeAgent(std::istream& Input, std::ostream& Output, std::ostream& ErrorOutput)
	{
		// Standard output belongs to the protocol, so what the log has to say goes to the error stream.
		const std::shared_ptr<FStreamLogSink> LogSink =
			std::make_shared<FStreamLogSink>(ErrorOutput, ELogVerbosity::Warning);
		FLog::AddSink(LogSink);
		{
			FEditor Editor;
			FAgentServer Server(Editor);
			Server.Run(Input, Output);
		}
		// The editor's scene is destroyed with the editor; this returns its memory before the process ends.
		CollectGarbage();
		FLog::RemoveSink(LogSink);
	}
}

int32 FEditorApplication::Run(
	const std::vector<std::string>& Arguments, std::istream& Input, std::ostream& Output, std::ostream& ErrorOutput)
{
	if (Arguments.size() == 1 && Arguments[0] == "--mcp")
	{
		ServeAgent(Input, Output, ErrorOutput);
		return SuccessExitCode;
	}
	if (Arguments.size() == 1 && Arguments[0] == "--help")
	{
		Output << Usage;
		return SuccessExitCode;
	}

	if (Arguments.empty())
	{
		ErrorOutput << "The editor's windows are not built yet; start it with --mcp to serve an agent.\n\n";
	}
	else
	{
		ErrorOutput << "The editor does not understand its command line.\n\n";
	}
	ErrorOutput << Usage;
	return UsageErrorExitCode;
}
