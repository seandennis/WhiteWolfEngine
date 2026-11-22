#include "Agent/AgentServer.h"

#include "Commands/CommandArgumentType.h"
#include "Commands/CommandDescription.h"
#include "Commands/CommandParameter.h"
#include "Commands/CommandRegistry.h"
#include "Commands/CommandResult.h"
#include "Core/EngineVersion.h"
#include "Core/Version.h"
#include "Editor.h"
#include "Json/JsonFormat.h"
#include "Json/JsonReader.h"
#include "Json/JsonValue.h"
#include "Json/JsonWriter.h"
#include "Object/GarbageCollector.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <format>
#include <istream>
#include <ostream>
#include <utility>

namespace
{
	// The error codes of JSON-RPC 2.0, and the one MCP adds for a protocol version the server does not speak.
	constexpr int32 ParseErrorCode = -32700;
	constexpr int32 InvalidRequestCode = -32600;
	constexpr int32 MethodNotFoundCode = -32601;
	constexpr int32 InvalidParametersCode = -32602;
	constexpr int32 UnsupportedProtocolVersionCode = -32022;

	/** The revision in which every request carries its protocol version and there is no handshake. */
	constexpr std::string_view ModernVersion = "2026-07-28";

	/** The revisions that open with "initialize", newest first. A tools-only server behaves the same in all. */
	constexpr std::array<std::string_view, 4> HandshakeVersions = {
		"2025-11-25", "2025-06-18", "2025-03-26", "2024-11-05"};

	constexpr std::string_view ProtocolVersionKey = "io.modelcontextprotocol/protocolVersion";
	constexpr std::string_view ClientCapabilitiesKey = "io.modelcontextprotocol/clientCapabilities";
	constexpr std::string_view ServerInfoKey = "io.modelcontextprotocol/serverInfo";

	constexpr std::string_view Instructions =
		"The editor of White Wolf Engine. Every tool is a command of the editor; Editor.ListCommands describes them "
		"all. Entities and components are referred to by integer identifiers, which Scene.GetHierarchy lists. A "
		"tool that fails says why and leaves the scene as it was. Edit.Undo undoes the last change to the scene.";

	/** What a request is answered with: a result, or an error with a code, a message and perhaps data. */
	struct FReply
	{
		bool bIsError = false;
		FJsonValue Result;
		int32 ErrorCode = 0;
		std::string ErrorMessage;
		FJsonValue ErrorData;

		static FReply Success(FJsonValue InResult)
		{
			FReply Reply;
			Reply.Result = std::move(InResult);
			return Reply;
		}

		static FReply Error(int32 InErrorCode, std::string InErrorMessage, FJsonValue InErrorData = FJsonValue())
		{
			FReply Reply;
			Reply.bIsError = true;
			Reply.ErrorCode = InErrorCode;
			Reply.ErrorMessage = std::move(InErrorMessage);
			Reply.ErrorData = std::move(InErrorData);
			return Reply;
		}
	};

	/** Returns the answer to the request with the given identifier as a line of JSON text. */
	std::string WriteResponse(const FJsonValue& RequestId, const FReply& Reply)
	{
		FJsonValue Response = FJsonValue::MakeObject();
		Response.SetMember("jsonrpc", "2.0");
		Response.SetMember("id", RequestId);
		if (Reply.bIsError)
		{
			FJsonValue Error = FJsonValue::MakeObject();
			Error.SetMember("code", Reply.ErrorCode);
			Error.SetMember("message", Reply.ErrorMessage);
			if (!Reply.ErrorData.IsNull())
			{
				Error.SetMember("data", Reply.ErrorData);
			}
			Response.SetMember("error", std::move(Error));
		}
		else
		{
			Response.SetMember("result", Reply.Result);
		}
		// The compact form has no line break: one that is part of a string is written as an escape.
		return FJsonWriter::Write(Response, EJsonFormat::Compact);
	}

	/** Returns the server's name and version, as both generations of the protocol want them. */
	FJsonValue MakeServerInfo()
	{
		FJsonValue ServerInfo = FJsonValue::MakeObject();
		ServerInfo.SetMember("name", FAgentServer::ServerName);
		ServerInfo.SetMember("version", FEngineVersion::Current().ToString());
		return ServerInfo;
	}

	/** Returns what the server offers: tools, and nothing else. */
	FJsonValue MakeCapabilities()
	{
		FJsonValue Capabilities = FJsonValue::MakeObject();
		Capabilities.SetMember("tools", FJsonValue::MakeObject());
		return Capabilities;
	}

	/** Returns every protocol version the server speaks, newest first. */
	FJsonValue MakeSupportedVersions()
	{
		FJsonValue Versions = FJsonValue::MakeArray();
		Versions.AddElement(ModernVersion);
		for (const std::string_view Version : HandshakeVersions)
		{
			Versions.AddElement(Version);
		}
		return Versions;
	}

	/** Returns the JSON schema of the arguments of a command: an object with one property per parameter. */
	FJsonValue MakeInputSchema(const FCommandDescription& Description)
	{
		FJsonValue Properties = FJsonValue::MakeObject();
		FJsonValue Required = FJsonValue::MakeArray();
		for (const FCommandParameter& Parameter : Description.Parameters)
		{
			FJsonValue Property = FJsonValue::MakeObject();
			switch (Parameter.Type)
			{
				case ECommandArgumentType::Boolean:
					Property.SetMember("type", "boolean");
					break;
				case ECommandArgumentType::Integer:
					Property.SetMember("type", "integer");
					break;
				case ECommandArgumentType::Number:
					Property.SetMember("type", "number");
					break;
				case ECommandArgumentType::String:
					Property.SetMember("type", "string");
					break;
				case ECommandArgumentType::Json:
				default:
					// Any JSON value: a schema without a type allows every one.
					break;
			}
			Property.SetMember("description", Parameter.Description);
			Properties.SetMember(Parameter.Name, std::move(Property));

			if (!Parameter.bOptional)
			{
				Required.AddElement(Parameter.Name);
			}
		}

		FJsonValue Schema = FJsonValue::MakeObject();
		Schema.SetMember("type", "object");
		Schema.SetMember("properties", std::move(Properties));
		if (!Required.GetElements().empty())
		{
			Schema.SetMember("required", std::move(Required));
		}
		Schema.SetMember("additionalProperties", false);
		return Schema;
	}

	FReply Initialize(const FJsonValue& Parameters)
	{
		const FJsonValue* const Requested = Parameters.IsObject() ? Parameters.FindMember("protocolVersion") : nullptr;
		if (Requested == nullptr || !Requested->IsString())
		{
			return FReply::Error(InvalidParametersCode, "initialize needs the string \"protocolVersion\".");
		}

		// The version the client asked for if the server speaks it, and otherwise the newest it does speak, which
		// the client then accepts or declines.
		const std::string_view RequestedVersion = Requested->GetString();
		const bool bIsSupported = std::ranges::find(HandshakeVersions, RequestedVersion) != HandshakeVersions.end();
		const std::string_view Version = bIsSupported ? RequestedVersion : HandshakeVersions[0];

		FJsonValue Result = FJsonValue::MakeObject();
		Result.SetMember("protocolVersion", Version);
		Result.SetMember("capabilities", MakeCapabilities());
		Result.SetMember("serverInfo", MakeServerInfo());
		Result.SetMember("instructions", Instructions);
		return FReply::Success(std::move(Result));
	}

	FReply Discover()
	{
		FJsonValue SupportedVersions = FJsonValue::MakeArray();
		SupportedVersions.AddElement(ModernVersion);

		FJsonValue Meta = FJsonValue::MakeObject();
		Meta.SetMember(std::string(ServerInfoKey), MakeServerInfo());

		FJsonValue Result = FJsonValue::MakeObject();
		Result.SetMember("supportedVersions", std::move(SupportedVersions));
		Result.SetMember("capabilities", MakeCapabilities());
		Result.SetMember("_meta", std::move(Meta));
		Result.SetMember("instructions", Instructions);
		// What the server is does not change while it runs.
		Result.SetMember("ttlMs", 3600000);
		Result.SetMember("cacheScope", "public");
		return FReply::Success(std::move(Result));
	}

	FReply ListTools(const FEditor& Editor, bool bIsModern)
	{
		FJsonValue Tools = FJsonValue::MakeArray();
		for (const FCommandDescription* const Description : Editor.GetCommands().GetCommands())
		{
			FJsonValue Tool = FJsonValue::MakeObject();
			Tool.SetMember("name", Description->Name);
			Tool.SetMember("description", Description->Summary);
			Tool.SetMember("inputSchema", MakeInputSchema(*Description));
			Tools.AddElement(std::move(Tool));
		}

		FJsonValue Result = FJsonValue::MakeObject();
		Result.SetMember("tools", std::move(Tools));
		if (bIsModern)
		{
			// Commands can be registered at any time, so a client should not keep the list.
			Result.SetMember("ttlMs", 0);
			Result.SetMember("cacheScope", "public");
		}
		return FReply::Success(std::move(Result));
	}

	FReply CallTool(FEditor& Editor, const FJsonValue& Parameters)
	{
		const FJsonValue* const Name = Parameters.IsObject() ? Parameters.FindMember("name") : nullptr;
		if (Name == nullptr || !Name->IsString())
		{
			return FReply::Error(InvalidParametersCode, "tools/call needs the string \"name\".");
		}
		// A tool that does not exist is a mistake in the request. Everything a command itself refuses, wrong
		// arguments included, is a result, so that the agent reads the reason and can try again.
		if (Editor.GetCommands().Find(Name->GetString()) == nullptr)
		{
			return FReply::Error(InvalidParametersCode, std::format("Unknown tool: {}", Name->GetString()));
		}

		const FJsonValue* const Arguments = Parameters.FindMember("arguments");
		if (Arguments != nullptr && !Arguments->IsObject() && !Arguments->IsNull())
		{
			return FReply::Error(InvalidParametersCode, "The \"arguments\" of tools/call must be an object.");
		}

		const FCommandResult CommandResult =
			Editor.ExecuteCommand(Name->GetString(), Arguments != nullptr ? *Arguments : FJsonValue());

		FJsonValue Text = FJsonValue::MakeObject();
		Text.SetMember("type", "text");
		Text.SetMember("text",
			CommandResult.HasSucceeded() ? FJsonWriter::Write(CommandResult.GetValue(), EJsonFormat::Pretty)
										 : CommandResult.GetError());
		FJsonValue Content = FJsonValue::MakeArray();
		Content.AddElement(std::move(Text));

		FJsonValue Result = FJsonValue::MakeObject();
		Result.SetMember("content", std::move(Content));
		Result.SetMember("isError", !CommandResult.HasSucceeded());
		return FReply::Success(std::move(Result));
	}

	/**
	 * Checks the "_meta" of a request of the modern revision, which says which version the request is in.
	 *
	 * @param OutIsModern Receives whether the request carries a protocol version at all.
	 * @return An error to answer with, or nothing if the request can be served.
	 */
	std::optional<FReply> CheckRequestMeta(const FJsonValue& Parameters, bool& OutIsModern)
	{
		const FJsonValue* const Meta = Parameters.IsObject() ? Parameters.FindMember("_meta") : nullptr;
		const FJsonValue* const Version =
			Meta != nullptr && Meta->IsObject() ? Meta->FindMember(ProtocolVersionKey) : nullptr;
		OutIsModern = Version != nullptr;
		if (!OutIsModern)
		{
			return std::nullopt;
		}

		if (!Version->IsString())
		{
			return FReply::Error(InvalidParametersCode, std::format("\"{}\" must be a string.", ProtocolVersionKey));
		}
		if (Version->GetString() != ModernVersion)
		{
			FJsonValue Data = FJsonValue::MakeObject();
			Data.SetMember("supported", MakeSupportedVersions());
			Data.SetMember("requested", Version->GetString());
			return FReply::Error(UnsupportedProtocolVersionCode, "Unsupported protocol version", std::move(Data));
		}

		const FJsonValue* const Capabilities = Meta->FindMember(ClientCapabilitiesKey);
		if (Capabilities == nullptr || !Capabilities->IsObject())
		{
			return FReply::Error(InvalidParametersCode,
				std::format("The request needs the object \"{}\" in \"_meta\".", ClientCapabilitiesKey));
		}
		return std::nullopt;
	}

	FReply HandleRequest(FEditor& Editor, const std::string& Method, const FJsonValue& Parameters)
	{
		bool bIsModern = false;
		if (std::optional<FReply> MetaError = CheckRequestMeta(Parameters, bIsModern))
		{
			return std::move(*MetaError);
		}

		FReply Reply;
		if (Method == "initialize")
		{
			Reply = Initialize(Parameters);
		}
		else if (Method == "ping")
		{
			Reply = FReply::Success(FJsonValue::MakeObject());
		}
		else if (Method == "server/discover")
		{
			if (!bIsModern)
			{
				return FReply::Error(InvalidParametersCode,
					std::format("server/discover needs \"{}\" in \"_meta\".", ProtocolVersionKey));
			}
			Reply = Discover();
		}
		else if (Method == "tools/list")
		{
			Reply = ListTools(Editor, bIsModern);
		}
		else if (Method == "tools/call")
		{
			Reply = CallTool(Editor, Parameters);
		}
		else
		{
			return FReply::Error(MethodNotFoundCode, std::format("Method not found: {}", Method));
		}

		// The modern revision wants every result to say that it is complete.
		if (bIsModern && !Reply.bIsError)
		{
			Reply.Result.SetMember("resultType", "complete");
		}
		return Reply;
	}
}

FAgentServer::FAgentServer(FEditor& InEditor)
	: Editor(InEditor)
{
}

void FAgentServer::Run(std::istream& Input, std::ostream& Output)
{
	using FClock = std::chrono::steady_clock;
	FClock::time_point LastMessageTime = FClock::now();

	std::string Line;
	while (std::getline(Input, Line))
	{
		// A client on Windows may end its lines with a carriage return and a line feed.
		if (!Line.empty() && Line.back() == '\r')
		{
			Line.pop_back();
		}
		if (Line.empty())
		{
			continue;
		}

		if (const std::optional<std::string> Response = HandleMessage(Line))
		{
			Output << *Response << '\n';
			Output.flush();
		}

		const FClock::time_point Now = FClock::now();
		FGarbageCollector::Tick(std::chrono::duration<double>(Now - LastMessageTime).count());
		LastMessageTime = Now;
	}
}

std::optional<std::string> FAgentServer::HandleMessage(std::string_view Message)
{
	// Until the identifier of a request is known, an error is answered with the identifier null.
	const FJsonValue NoId;

	FJsonValue Json;
	std::string ParseError;
	if (!FJsonReader::Parse(Message, Json, ParseError))
	{
		return WriteResponse(NoId, FReply::Error(ParseErrorCode, std::format("Parse error: {}", ParseError)));
	}
	if (!Json.IsObject())
	{
		// This includes an array of messages, a batch, which the protocol no longer has.
		return WriteResponse(NoId,
			FReply::Error(InvalidRequestCode,
				std::format("Invalid request: a message must be a JSON object, not {}.", Json.GetTypeDescription())));
	}

	const FJsonValue* const Method = Json.FindMember("method");
	const FJsonValue* const RequestId = Json.FindMember("id");
	if (Method == nullptr && (Json.FindMember("result") != nullptr || Json.FindMember("error") != nullptr))
	{
		// An answer to a request. The server makes no requests, so there is nothing it could belong to.
		return std::nullopt;
	}

	const bool bHasValidId = RequestId != nullptr && (RequestId->IsString() || RequestId->IsInteger());
	const FJsonValue* const Version = Json.FindMember("jsonrpc");
	if (Version == nullptr || *Version != FJsonValue("2.0") || Method == nullptr || !Method->IsString())
	{
		return WriteResponse(bHasValidId ? *RequestId : NoId,
			FReply::Error(InvalidRequestCode,
				"Invalid request: a message needs \"jsonrpc\": \"2.0\" and a \"method\" that is a string."));
	}
	if (RequestId == nullptr)
	{
		// A notification, such as the one that ends the handshake. None changes what the server does, and none
		// is answered.
		return std::nullopt;
	}
	if (!bHasValidId)
	{
		return WriteResponse(NoId,
			FReply::Error(
				InvalidRequestCode, "Invalid request: the \"id\" of a request must be a string or an integer."));
	}

	const FJsonValue* const Parameters = Json.FindMember("params");
	if (Parameters != nullptr && !Parameters->IsObject())
	{
		return WriteResponse(
			*RequestId, FReply::Error(InvalidParametersCode, "The \"params\" of a request must be an object."));
	}

	return WriteResponse(*RequestId,
		HandleRequest(Editor, Method->GetString(), Parameters != nullptr ? *Parameters : FJsonValue::MakeObject()));
}
