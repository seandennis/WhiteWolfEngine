#include "Commands/LogCommands.h"

#include "Commands/CommandEffect.h"
#include "Commands/CommandRegistry.h"
#include "Editor.h"
#include "Json/JsonValue.h"
#include "Logging/LogFilter.h"
#include "Logging/LogMessage.h"
#include "Logging/LogVerbosity.h"
#include "Logging/MemoryLogSink.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
	/** How many messages Log.Read returns when it is not told how many. */
	constexpr int64 DefaultMessageCount = 100;

	/** Every kind of message, from the most important down. */
	constexpr std::array<ELogVerbosity, 7> Verbosities = {ELogVerbosity::Fatal,
		ELogVerbosity::Error,
		ELogVerbosity::Warning,
		ELogVerbosity::Display,
		ELogVerbosity::Log,
		ELogVerbosity::Verbose,
		ELogVerbosity::VeryVerbose};

	/** Returns the kind of message that has a name, or nothing if no kind is called that. */
	std::optional<ELogVerbosity> FindVerbosity(std::string_view Name)
	{
		for (const ELogVerbosity Verbosity : Verbosities)
		{
			if (ToString(Verbosity) == Name)
			{
				return Verbosity;
			}
		}
		return std::nullopt;
	}

	/** Returns a message as commands return it. */
	FJsonValue DescribeMessage(const FLogMessage& Message)
	{
		// To the thousandth of a second, in the time of Greenwich, as the lines of a log file have it.
		const std::chrono::sys_time<std::chrono::milliseconds> Time =
			std::chrono::floor<std::chrono::milliseconds>(Message.Time);

		FJsonValue Value = FJsonValue::MakeObject();
		Value.SetMember("Time", std::format("{:%FT%T}Z", Time));
		Value.SetMember("Verbosity", ToString(Message.Verbosity));
		Value.SetMember("Category", Message.Category);
		Value.SetMember("Text", Message.Text);
		return Value;
	}

	FCommandResult ReadLog(FEditor& Editor, const FCommandArguments& Arguments)
	{
		FLogFilter Filter;
		if (Arguments.Has("Verbosity"))
		{
			const std::string Name = Arguments.GetString("Verbosity");
			const std::optional<ELogVerbosity> Verbosity = FindVerbosity(Name);
			if (!Verbosity.has_value())
			{
				return FCommandResult::Failure(std::format(
					"Verbosity must be one of Fatal, Error, Warning, Display, Log, Verbose and VeryVerbose, not '{}'.",
					Name));
			}
			Filter.LeastVerbosity = *Verbosity;
		}
		if (Arguments.Has("Category"))
		{
			Filter.Category = Arguments.GetString("Category");
		}
		if (Arguments.Has("Contains"))
		{
			Filter.SearchText = Arguments.GetString("Contains");
		}

		const int64 Count = Arguments.Has("Count") ? Arguments.GetInteger("Count") : DefaultMessageCount;
		if (Count < 1)
		{
			return FCommandResult::Failure(std::format("Count must be at least 1, not {}.", Count));
		}

		// The messages that are wanted, of which the last ones are returned, in the order they were logged.
		std::vector<FLogMessage> Wanted;
		for (FLogMessage& Message : Editor.GetLog().GetMessages())
		{
			if (Filter.Matches(Message))
			{
				Wanted.push_back(std::move(Message));
			}
		}
		const std::size_t ReturnedCount =
			Wanted.size() > static_cast<std::size_t>(Count) ? static_cast<std::size_t>(Count) : Wanted.size();

		FJsonValue Messages = FJsonValue::MakeArray();
		for (std::size_t Index = Wanted.size() - ReturnedCount; Index < Wanted.size(); ++Index)
		{
			Messages.AddElement(DescribeMessage(Wanted[Index]));
		}

		FJsonValue Value = FJsonValue::MakeObject();
		Value.SetMember("Messages", std::move(Messages));
		Value.SetMember("MatchCount", static_cast<int64>(Wanted.size()));
		return FCommandResult::Success(std::move(Value));
	}

	FCommandResult ClearLog(FEditor& Editor, const FCommandArguments& /*Arguments*/)
	{
		Editor.ClearLog();
		return FCommandResult::Success();
	}
}

void FLogCommands::Register(FCommandRegistry& Registry)
{
	Registry.Register(
		{"Log.Read",
			"Returns what the editor and the engine have logged since the editor started: the messages that the "
			"Output Log shows. It is how to find out why something did not do what was expected: a file that could "
			"not be read, a value that was refused, a command that failed. Each message has its Time, its Verbosity "
			"(how important it is), its Category (what part of the engine it is from) and its Text. Messages are the "
			"last ones that match, oldest first; MatchCount says how many matched in all. The editor keeps the last "
			"few thousand messages.",
			{FCommandParameter::Optional("Verbosity",
				 ECommandArgumentType::String,
				 "The least important kind of message to return: Fatal, Error, Warning, Display, Log, Verbose or "
				 "VeryVerbose, from the most important down. More important kinds are returned with it. Warning "
				 "returns what went wrong. Every kind if left out."),
				FCommandParameter::Optional("Category",
					ECommandArgumentType::String,
					"The category that messages have to be of, such as LogEditor, LogScene or LogAssets. Any "
					"category if left out."),
				FCommandParameter::Optional("Contains",
					ECommandArgumentType::String,
					"What a message has to have somewhere in its text or in the name of its category, in capitals or "
					"small letters."),
				FCommandParameter::Optional("Count",
					ECommandArgumentType::Integer,
					"How many messages to return at most: the most recent ones. At least 1; 100 if left out.")},
			ECommandEffect::LeavesScene},
		ReadLog);

	Registry.Register({"Log.Clear",
						  "Forgets every message that Log.Read would return. What is logged from then on is kept as "
						  "before. Useful before something is tried, to see afterwards what it alone logged.",
						  {},
						  ECommandEffect::LeavesScene},
		ClearLog);
}
