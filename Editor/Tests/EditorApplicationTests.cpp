#include "EditorApplication.h"

#include "Core/FileHelper.h"
#include "Editor.h"
#include "TestEditorObjects.h"
#include "TestSupport/ScopedTempDirectory.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <format>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	/** Runs the editor with a command line and an input, and keeps what it wrote and returned. */
	struct FEditorApplicationRun
	{
		FEditorApplicationRun(const std::vector<std::string>& Arguments, const std::string& InputText)
		{
			std::istringstream Input(InputText);
			ExitCode = FEditorApplication::Run(Arguments, Input, Output, ErrorOutput);
		}

		FScopedEditorTest EditorTest;
		std::ostringstream Output;
		std::ostringstream ErrorOutput;
		int32 ExitCode = -1;
	};
}

TEST_SUITE("FEditorApplication")
{
	TEST_CASE("--mcp serves an agent until its input ends, and writes nothing but answers to the output")
	{
		const FEditorApplicationRun Run({"--mcp"},
			"{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"ping\"}\n"
			"{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\",\"params\":{\"name\":\"Entity.Create\","
			"\"arguments\":{\"Name\":\"Player\"}}}\n");

		CHECK(Run.ExitCode == 0);
		CHECK(Run.Output.str() ==
			"{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":{}}\n"
			"{\"jsonrpc\":\"2.0\",\"id\":2,\"result\":{\"content\":[{\"type\":\"text\",\"text\":\"{\\n  \\\"Id\\\": "
			"1\\n}\"}],"
			"\"isError\":false}}\n");
		CHECK(Run.ErrorOutput.str().empty());
	}

	TEST_CASE("--mcp writes the warnings of the log to the error output, where they do not disturb the protocol")
	{
		// A scene file with a property that entities do not have: opening it is a warning in the log.
		const FScopedTempDirectory Directory;
		const std::filesystem::path Path = Directory.GetPath() / "Level.json";
		std::string Error;
		REQUIRE(FFileHelper::SaveStringToFile(
			R"({
				"Format": "WhiteWolfScene",
				"Version": 1,
				"NextId": 2,
				"Properties": { "Name": "Level 1" },
				"Entities": [ { "Id": 1, "Properties": { "Name": "Player", "Mood": "Happy" }, "Components": [] } ]
			})",
			Path,
			Error));
		FJsonValue Arguments = FJsonValue::MakeObject();
		Arguments.SetMember("Path", FFileHelper::PathToUtf8(Path));
		const std::string Message = std::format(
			R"({{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{{"name":"Scene.Open","arguments":{}}}}})",
			WriteTestJson(Arguments));

		const FEditorApplicationRun Run({"--mcp"}, Message + "\n");

		CHECK(Run.ExitCode == 0);
		// The output is the answer and nothing else: one line.
		const std::string Output = Run.Output.str();
		CHECK(std::ranges::count(Output, '\n') == 1);
		CHECK(Output.starts_with(R"({"jsonrpc":"2.0","id":1,"result":)"));
		const std::string Errors = Run.ErrorOutput.str();
		CHECK(Errors.find("Warning") != std::string::npos);
		CHECK(Errors.find("Mood") != std::string::npos);

		// The editor stopped listening to the log when it ended.
		WW_LOG(LogEditor, Warning, "Logged after the editor has ended");
		CHECK(Run.ErrorOutput.str() == Errors);
	}

	TEST_CASE("--help prints how to start the editor")
	{
		const FEditorApplicationRun Run({"--help"}, "");

		CHECK(Run.ExitCode == 0);
		CHECK(Run.Output.str().find("--mcp") != std::string::npos);
		CHECK(Run.Output.str().find("--help") != std::string::npos);
		CHECK(Run.ErrorOutput.str().empty());
	}

	TEST_CASE("Without an option the editor says that it has no windows yet, as an error")
	{
		const FEditorApplicationRun Run({}, "");

		CHECK(Run.ExitCode == 2);
		CHECK(Run.Output.str().empty());
		CHECK(Run.ErrorOutput.str().starts_with("The editor's windows are not built yet"));
		CHECK(Run.ErrorOutput.str().find("--mcp") != std::string::npos);
	}

	TEST_CASE("A command line the editor does not understand is an error, and nothing is served")
	{
		std::vector<std::string> Arguments;

		SUBCASE("An option it does not have")
		{
			Arguments = {"--fullscreen"};
		}
		SUBCASE("An option too many")
		{
			Arguments = {"--mcp", "--verbose"};
		}
		SUBCASE("An option in other letters")
		{
			Arguments = {"--MCP"};
		}

		const FEditorApplicationRun Run(Arguments, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"ping\"}\n");

		CHECK(Run.ExitCode == 2);
		CHECK(Run.Output.str().empty());
		CHECK(Run.ErrorOutput.str().starts_with("The editor does not understand its command line."));
	}
}
