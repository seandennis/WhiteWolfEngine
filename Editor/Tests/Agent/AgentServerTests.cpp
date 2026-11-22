#include "Agent/AgentServer.h"

#include "Core/EngineVersion.h"
#include "Core/Version.h"
#include "Editor.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "TestEditorObjects.h"

#include <doctest/doctest.h>

#include <cstddef>
#include <format>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	/** The "_meta" that every request of the protocol revision without a handshake carries. */
	constexpr std::string_view ModernMeta =
		R"("_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28","io.modelcontextprotocol/clientCapabilities":{}})";

	/** An editor with an agent server, and a way to send it messages and read the answers. */
	struct FAgentServerTest
	{
		/** Sends a message that a test expects to be answered, and returns the answer. */
		FJsonValue Send(std::string_view Message)
		{
			const std::optional<std::string> Response = Server.HandleMessage(Message);
			REQUIRE(Response.has_value());
			// An answer is one line: the transport ends a message at a line break.
			CHECK(Response->find('\n') == std::string::npos);
			CHECK(Response->find('\r') == std::string::npos);
			return ParseTestJson(*Response);
		}

		/** Sends a request and returns its result as text, which a test expects not to be an error. */
		std::string SendForResult(std::string_view Message)
		{
			const FJsonValue Response = Send(Message);
			const FJsonValue* const Result = Response.FindMember("result");
			INFO("Response: ", WriteTestJson(Response));
			REQUIRE(Result != nullptr);
			return WriteTestJson(*Result);
		}

		/** Sends a message and returns the error it is answered with as "code: message". */
		std::string SendForError(std::string_view Message)
		{
			const FJsonValue Response = Send(Message);
			const FJsonValue* const Error = Response.FindMember("error");
			INFO("Response: ", WriteTestJson(Response));
			REQUIRE(Error != nullptr);
			CHECK(Response.FindMember("result") == nullptr);
			const FJsonValue* const Code = Error->FindMember("code");
			const FJsonValue* const Text = Error->FindMember("message");
			REQUIRE(Code != nullptr);
			REQUIRE(Text != nullptr);
			return std::format("{}: {}", Code->GetInteger(), Text->GetString());
		}

		/** Calls a tool with arguments written as JSON text and returns the result of tools/call as text. */
		std::string CallTool(std::string_view Name, std::string_view Arguments = "{}")
		{
			return SendForResult(std::format(
				R"({{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{{"name":"{}","arguments":{}}}}})",
				Name,
				Arguments));
		}

		FScopedEditorTest EditorTest;
		FEditor Editor;
		FAgentServer Server{Editor};
	};
}

TEST_SUITE("FAgentServer")
{
	TEST_CASE("initialize answers with the version the client asked for, the server's name and that it has tools")
	{
		FAgentServerTest Test;
		std::string Version;

		SUBCASE("The newest revision with a handshake")
		{
			Version = "2025-11-25";
		}
		SUBCASE("The oldest revision")
		{
			Version = "2024-11-05";
		}

		const FJsonValue Response = Test.Send(std::format(
			R"({{"jsonrpc":"2.0","id":1,"method":"initialize","params":{{"protocolVersion":"{}","capabilities":{{}},)"
			R"("clientInfo":{{"name":"TestClient","version":"1.0.0"}}}}}})",
			Version));

		CHECK(*Response.FindMember("jsonrpc") == FJsonValue("2.0"));
		CHECK(*Response.FindMember("id") == FJsonValue(1));
		const FJsonValue* const Result = Response.FindMember("result");
		REQUIRE(Result != nullptr);
		CHECK(*Result->FindMember("protocolVersion") == FJsonValue(Version));
		CHECK(WriteTestJson(*Result->FindMember("capabilities")) == R"({"tools":{}})");
		CHECK(WriteTestJson(*Result->FindMember("serverInfo")) ==
			std::format(R"({{"name":"WhiteWolfEditor","version":"{}"}})", FEngineVersion::Current().ToString()));
		REQUIRE(Result->FindMember("instructions") != nullptr);
		CHECK_FALSE(Result->FindMember("instructions")->GetString().empty());
		// The revisions with a handshake know nothing of a result type.
		CHECK(Result->FindMember("resultType") == nullptr);
	}

	TEST_CASE("initialize with a version the server does not speak answers with the newest it does")
	{
		FAgentServerTest Test;

		const std::string Result = Test.SendForResult(
			R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"1999-01-01","capabilities":{}}})");

		CHECK(Result.starts_with(R"({"protocolVersion":"2025-11-25",)"));
	}

	TEST_CASE("initialize without a version is refused")
	{
		FAgentServerTest Test;

		CHECK(Test.SendForError(R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"capabilities":{}}})") ==
			"-32602: initialize needs the string \"protocolVersion\".");
		CHECK(Test.SendForError(R"({"jsonrpc":"2.0","id":1,"method":"initialize"})") ==
			"-32602: initialize needs the string \"protocolVersion\".");
	}

	TEST_CASE("Notifications and answers are not answered")
	{
		FAgentServerTest Test;

		CHECK_FALSE(Test.Server.HandleMessage(R"({"jsonrpc":"2.0","method":"notifications/initialized"})").has_value());
		CHECK_FALSE(Test.Server.HandleMessage(R"({"jsonrpc":"2.0","method":"notifications/cancelled","params":{}})")
						.has_value());
		CHECK_FALSE(Test.Server.HandleMessage(R"({"jsonrpc":"2.0","method":"notifications/unheard-of"})").has_value());
		// A notification is not answered even if it names a method that a request would be answered for.
		CHECK_FALSE(Test.Server.HandleMessage(R"({"jsonrpc":"2.0","method":"tools/list"})").has_value());
		CHECK_FALSE(Test.Server.HandleMessage(R"({"jsonrpc":"2.0","id":7,"result":{}})").has_value());
		CHECK_FALSE(Test.Server.HandleMessage(R"({"jsonrpc":"2.0","id":7,"error":{"code":-32601,"message":"No"}})")
						.has_value());
	}

	TEST_CASE("ping is answered with an empty result, also before the handshake")
	{
		FAgentServerTest Test;

		CHECK(WriteTestJson(Test.Send(R"({"jsonrpc":"2.0","id":"123","method":"ping"})")) ==
			R"({"jsonrpc":"2.0","id":"123","result":{}})");
	}

	TEST_CASE("The identifier of a request comes back as it was sent")
	{
		FAgentServerTest Test;

		CHECK(*Test.Send(R"({"jsonrpc":"2.0","id":42,"method":"ping"})").FindMember("id") == FJsonValue(42));
		CHECK(*Test.Send(R"({"jsonrpc":"2.0","id":"request-7","method":"ping"})").FindMember("id") ==
			FJsonValue("request-7"));
	}

	TEST_CASE("tools/list offers every command of the editor as a tool")
	{
		FAgentServerTest Test;

		const FJsonValue Response = Test.Send(R"({"jsonrpc":"2.0","id":2,"method":"tools/list"})");

		const FJsonValue* const Result = Response.FindMember("result");
		REQUIRE(Result != nullptr);
		const FJsonValue* const Tools = Result->FindMember("tools");
		REQUIRE(Tools != nullptr);
		REQUIRE(Tools->IsArray());
		const std::vector<const FCommandDescription*> Commands = Test.Editor.GetCommands().GetCommands();
		REQUIRE(Tools->GetElements().size() == Commands.size());
		for (std::size_t Index = 0; Index < Commands.size(); ++Index)
		{
			const FJsonValue& Tool = Tools->GetElements()[Index];
			CHECK(*Tool.FindMember("name") == FJsonValue(Commands[Index]->Name));
			CHECK(*Tool.FindMember("description") == FJsonValue(Commands[Index]->Summary));
			REQUIRE(Tool.FindMember("inputSchema") != nullptr);
			CHECK(*Tool.FindMember("inputSchema")->FindMember("type") == FJsonValue("object"));
		}
		CHECK(Result->FindMember("resultType") == nullptr);
		CHECK(Result->FindMember("ttlMs") == nullptr);
	}

	TEST_CASE("A tool's schema is made from the parameters of its command")
	{
		FAgentServerTest Test;
		Test.Editor.GetCommands().Register(
			{"Test.Everything",
				"Takes one argument of every type.",
				{FCommandParameter::Required("Flag", ECommandArgumentType::Boolean, "A flag."),
					FCommandParameter::Required("Count", ECommandArgumentType::Integer, "A count."),
					FCommandParameter::Optional("Amount", ECommandArgumentType::Number, "An amount."),
					FCommandParameter::Optional("Label", ECommandArgumentType::String, "A label."),
					FCommandParameter::Required("Value", ECommandArgumentType::Json, "Anything.")}},
			[](FEditor& /*CommandEditor*/, const FCommandArguments& /*Arguments*/)
			{ return FCommandResult::Success(); });

		const FJsonValue Response = Test.Send(R"({"jsonrpc":"2.0","id":2,"method":"tools/list"})");

		const FJsonValue* Schema = nullptr;
		const FJsonValue* NoParameterSchema = nullptr;
		for (const FJsonValue& Tool : Response.FindMember("result")->FindMember("tools")->GetElements())
		{
			if (*Tool.FindMember("name") == FJsonValue("Test.Everything"))
			{
				Schema = Tool.FindMember("inputSchema");
			}
			if (*Tool.FindMember("name") == FJsonValue("Scene.GetHierarchy"))
			{
				NoParameterSchema = Tool.FindMember("inputSchema");
			}
		}
		REQUIRE(Schema != nullptr);
		const FJsonValue Expected = ParseTestJson(R"({
			"type": "object",
			"properties": {
				"Flag": { "type": "boolean", "description": "A flag." },
				"Count": { "type": "integer", "description": "A count." },
				"Amount": { "type": "number", "description": "An amount." },
				"Label": { "type": "string", "description": "A label." },
				"Value": { "description": "Anything." }
			},
			"required": ["Flag", "Count", "Value"],
			"additionalProperties": false
		})");
		CHECK(WriteTestJson(*Schema) == WriteTestJson(Expected));

		REQUIRE(NoParameterSchema != nullptr);
		CHECK(WriteTestJson(*NoParameterSchema) == R"({"type":"object","properties":{},"additionalProperties":false})");
	}

	TEST_CASE("tools/call carries out the command and returns what it returned as JSON text")
	{
		FAgentServerTest Test;

		const std::string Result = Test.CallTool("Entity.Create", R"({"Name": "Player"})");

		CHECK(Result == R"({"content":[{"type":"text","text":"{\n  \"Id\": 1\n}"}],"isError":false})");
		const WEntity* const Player = Test.Editor.GetScene().FindEntityById(1);
		REQUIRE(Player != nullptr);
		CHECK(Player->GetName() == "Player");
	}

	TEST_CASE("tools/call without arguments calls a command that needs none")
	{
		FAgentServerTest Test;

		const std::string Result =
			Test.SendForResult(R"({"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"Edit.GetHistory"}})");

		CHECK(Result.ends_with(R"("isError":false})"));
		CHECK(Result.find("Undo") != std::string::npos);
	}

	TEST_CASE("A command that returns nothing is answered with the text null")
	{
		FAgentServerTest Test;
		Test.CallTool("Entity.Create", R"({"Name": "Player"})");

		CHECK(Test.CallTool("Entity.Rename", R"({"Entity": 1, "Name": "Hero"})") ==
			R"({"content":[{"type":"text","text":"null"}],"isError":false})");
	}

	TEST_CASE("A command that fails is a result that says why, so that the agent can read the reason")
	{
		FAgentServerTest Test;

		CHECK(Test.CallTool("Entity.Delete", R"({"Entity": 99})") ==
			R"({"content":[{"type":"text","text":"There is no entity with the identifier 99."}],"isError":true})");
		// Arguments that do not fit the command are the command's failure too, not a mistake in the protocol.
		CHECK(Test.CallTool("Entity.Create", R"({"Nmae": "Player"})") ==
			R"({"content":[{"type":"text","text":"The command \"Entity.Create\" has no parameter \"Nmae\"."}],"isError":true})");
	}

	TEST_CASE("What an agent does through the server can be undone through the server")
	{
		FAgentServerTest Test;
		Test.CallTool("Entity.Create", R"({"Name": "Player"})");

		const std::string Result = Test.CallTool("Edit.Undo");

		CHECK(Result.find("Entity.Create") != std::string::npos);
		CHECK(Test.Editor.GetScene().GetAllEntities().empty());
	}

	TEST_CASE("tools/call that does not say which tool, or names one that does not exist, is an error")
	{
		FAgentServerTest Test;

		CHECK(Test.SendForError(R"({"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"No.SuchTool"}})") ==
			"-32602: Unknown tool: No.SuchTool");
		CHECK(Test.SendForError(R"({"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"arguments":{}}})") ==
			"-32602: tools/call needs the string \"name\".");
		CHECK(Test.SendForError(R"({"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":7}})") ==
			"-32602: tools/call needs the string \"name\".");
		CHECK(Test.SendForError(R"({"jsonrpc":"2.0","id":1,"method":"tools/call"})") ==
			"-32602: tools/call needs the string \"name\".");
		CHECK(
			Test.SendForError(
				R"({"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"Entity.Create","arguments":[1]}})") ==
			"-32602: The \"arguments\" of tools/call must be an object.");
	}

	TEST_CASE("A request that carries its protocol version is answered in the revision without a handshake")
	{
		FAgentServerTest Test;

		const std::string Tools = Test.SendForResult(
			std::format(R"({{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{{{}}}}})", ModernMeta));
		CHECK(Tools.starts_with(R"({"tools":[)"));
		CHECK(Tools.ends_with(R"(,"ttlMs":0,"cacheScope":"public","resultType":"complete"})"));

		const std::string Called = Test.SendForResult(std::format(
			R"({{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{{"name":"Entity.Create","arguments":{{"Name":"Player"}},{}}}}})",
			ModernMeta));
		CHECK(Called ==
			R"({"content":[{"type":"text","text":"{\n  \"Id\": 1\n}"}],"isError":false,"resultType":"complete"})");
	}

	TEST_CASE("server/discover says which versions the server speaks and what it offers")
	{
		FAgentServerTest Test;

		const std::string Result = Test.SendForResult(std::format(
			R"({{"jsonrpc":"2.0","id":"discover-1","method":"server/discover","params":{{{}}}}})", ModernMeta));

		const FJsonValue Json = ParseTestJson(Result);
		CHECK(WriteTestJson(*Json.FindMember("supportedVersions")) == R"(["2026-07-28"])");
		CHECK(WriteTestJson(*Json.FindMember("capabilities")) == R"({"tools":{}})");
		CHECK(WriteTestJson(*Json.FindMember("_meta")) ==
			std::format(R"({{"io.modelcontextprotocol/serverInfo":{{"name":"WhiteWolfEditor","version":"{}"}}}})",
				FEngineVersion::Current().ToString()));
		CHECK(*Json.FindMember("ttlMs") == FJsonValue(3600000));
		CHECK(*Json.FindMember("cacheScope") == FJsonValue("public"));
		CHECK(*Json.FindMember("resultType") == FJsonValue("complete"));
		CHECK_FALSE(Json.FindMember("instructions")->GetString().empty());
	}

	TEST_CASE("server/discover without a protocol version is refused, which sends an older client to initialize")
	{
		FAgentServerTest Test;

		CHECK(Test.SendForError(R"({"jsonrpc":"2.0","id":1,"method":"server/discover"})") ==
			"-32602: server/discover needs \"io.modelcontextprotocol/protocolVersion\" in \"_meta\".");
	}

	TEST_CASE("A protocol version the server does not speak is refused with the versions it does speak")
	{
		FAgentServerTest Test;

		const FJsonValue Response = Test.Send(
			R"({"jsonrpc":"2.0","id":1,"method":"tools/list","params":{"_meta":{"io.modelcontextprotocol/protocolVersion":"1900-01-01","io.modelcontextprotocol/clientCapabilities":{}}}})");

		const FJsonValue* const Error = Response.FindMember("error");
		REQUIRE(Error != nullptr);
		CHECK(WriteTestJson(*Error) ==
			R"({"code":-32022,"message":"Unsupported protocol version","data":{"supported":["2026-07-28","2025-11-25","2025-06-18","2025-03-26","2024-11-05"],"requested":"1900-01-01"}})");
	}

	TEST_CASE("A request of the revision without a handshake needs the client's capabilities")
	{
		FAgentServerTest Test;

		CHECK(
			Test.SendForError(
				R"({"jsonrpc":"2.0","id":1,"method":"tools/list","params":{"_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28"}}})") ==
			"-32602: The request needs the object \"io.modelcontextprotocol/clientCapabilities\" in \"_meta\".");
		CHECK(
			Test.SendForError(
				R"({"jsonrpc":"2.0","id":1,"method":"tools/list","params":{"_meta":{"io.modelcontextprotocol/protocolVersion":7}}})") ==
			"-32602: \"io.modelcontextprotocol/protocolVersion\" must be a string.");
	}

	TEST_CASE("A message that is not a request is answered with an error, never with a crash")
	{
		FAgentServerTest Test;

		SUBCASE("Text that is not JSON")
		{
			const FJsonValue Response = Test.Send("{ this is not JSON");
			CHECK(Response.FindMember("id")->IsNull());
			CHECK(*Response.FindMember("error")->FindMember("code") == FJsonValue(-32700));
			CHECK(Response.FindMember("error")->FindMember("message")->GetString().starts_with("Parse error: "));
		}
		SUBCASE("A value that is not an object")
		{
			CHECK(Test.SendForError("42") == "-32600: Invalid request: a message must be a JSON object, not a number.");
		}
		SUBCASE("A batch of messages")
		{
			CHECK(Test.SendForError(R"([{"jsonrpc":"2.0","id":1,"method":"ping"}])") ==
				"-32600: Invalid request: a message must be a JSON object, not an array.");
		}
		SUBCASE("A message without a method")
		{
			CHECK(Test.SendForError(R"({"jsonrpc":"2.0","id":1})") ==
				"-32600: Invalid request: a message needs \"jsonrpc\": \"2.0\" and a \"method\" that is a string.");
		}
		SUBCASE("A method that is not a string")
		{
			CHECK(Test.SendForError(R"({"jsonrpc":"2.0","id":1,"method":5})") ==
				"-32600: Invalid request: a message needs \"jsonrpc\": \"2.0\" and a \"method\" that is a string.");
		}
		SUBCASE("A message of another JSON-RPC version")
		{
			CHECK(Test.SendForError(R"({"jsonrpc":"1.0","id":1,"method":"ping"})") ==
				"-32600: Invalid request: a message needs \"jsonrpc\": \"2.0\" and a \"method\" that is a string.");
		}
		SUBCASE("An identifier that is neither a string nor an integer")
		{
			const FJsonValue Response = Test.Send(R"({"jsonrpc":"2.0","id":[1],"method":"ping"})");
			CHECK(Response.FindMember("id")->IsNull());
			CHECK(*Response.FindMember("error")->FindMember("code") == FJsonValue(-32600));
		}
		SUBCASE("Parameters that are not an object")
		{
			CHECK(Test.SendForError(R"({"jsonrpc":"2.0","id":1,"method":"tools/list","params":[1,2]})") ==
				"-32602: The \"params\" of a request must be an object.");
		}
		SUBCASE("A method the server does not have")
		{
			CHECK(Test.SendForError(R"({"jsonrpc":"2.0","id":1,"method":"resources/list"})") ==
				"-32601: Method not found: resources/list");
		}
	}

	TEST_CASE("An error keeps the identifier of its request")
	{
		FAgentServerTest Test;

		const FJsonValue Response = Test.Send(R"({"jsonrpc":"2.0","id":"abc","method":"resources/list"})");

		CHECK(*Response.FindMember("id") == FJsonValue("abc"));
	}

	TEST_CASE("Run answers each line of its input with a line of output until the input ends")
	{
		FAgentServerTest Test;
		// A notification, an empty line and a line that ends as lines do on Windows are among them.
		std::istringstream Input(
			"{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"ping\"}\n"
			"{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}\n"
			"\n"
			"{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/call\",\"params\":{\"name\":\"Entity.Create\","
			"\"arguments\":{\"Name\":\"Player\"}}}\r\n"
			"not JSON\n"
			"{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"ping\"}");
		std::ostringstream Output;

		Test.Server.Run(Input, Output);

		std::istringstream Lines(Output.str());
		std::string Line;
		std::vector<std::string> Answers;
		while (std::getline(Lines, Line))
		{
			Answers.push_back(Line);
		}
		REQUIRE(Answers.size() == 4);
		CHECK(Answers[0] == R"({"jsonrpc":"2.0","id":1,"result":{}})");
		CHECK(Answers[1].starts_with(R"({"jsonrpc":"2.0","id":2,"result":{"content":)"));
		CHECK(Answers[2].starts_with(R"({"jsonrpc":"2.0","id":null,"error":{"code":-32700,)"));
		CHECK(Answers[3] == R"({"jsonrpc":"2.0","id":3,"result":{}})");
		CHECK(Output.str().ends_with("\n"));
		CHECK(Test.Editor.GetScene().FindEntityByName("Player") != nullptr);
	}

	TEST_CASE("Run with no input writes nothing")
	{
		FAgentServerTest Test;
		std::istringstream Input("");
		std::ostringstream Output;

		Test.Server.Run(Input, Output);

		CHECK(Output.str().empty());
	}
}
