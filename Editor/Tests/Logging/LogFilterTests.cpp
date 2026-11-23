#include "Logging/LogFilter.h"

#include <doctest/doctest.h>

#include <initializer_list>
#include <string>

namespace
{
	FLogMessage MakeMessage(ELogVerbosity Verbosity, const std::string& Category, const std::string& Text)
	{
		FLogMessage Message;
		Message.Verbosity = Verbosity;
		Message.Category = Category;
		Message.Text = Text;
		return Message;
	}
}

TEST_SUITE("FLogFilter")
{
	TEST_CASE("A new filter wants every message")
	{
		const FLogFilter Filter;

		for (const ELogVerbosity Verbosity : {ELogVerbosity::Fatal,
				 ELogVerbosity::Error,
				 ELogVerbosity::Warning,
				 ELogVerbosity::Display,
				 ELogVerbosity::Log,
				 ELogVerbosity::Verbose,
				 ELogVerbosity::VeryVerbose})
		{
			CHECK(Filter.Matches(MakeMessage(Verbosity, "LogScene", "An entity was created")));
		}
		CHECK(Filter.Matches(MakeMessage(ELogVerbosity::Log, "", "")));
	}

	TEST_CASE("A filter wants the kind of message that it names and every more important kind")
	{
		FLogFilter Filter;
		Filter.LeastVerbosity = ELogVerbosity::Warning;

		CHECK(Filter.Matches(MakeMessage(ELogVerbosity::Fatal, "LogCore", "Gone")));
		CHECK(Filter.Matches(MakeMessage(ELogVerbosity::Error, "LogCore", "Failed")));
		CHECK(Filter.Matches(MakeMessage(ELogVerbosity::Warning, "LogCore", "Odd")));
		CHECK_FALSE(Filter.Matches(MakeMessage(ELogVerbosity::Display, "LogCore", "Shown")));
		CHECK_FALSE(Filter.Matches(MakeMessage(ELogVerbosity::Log, "LogCore", "Noted")));
		CHECK_FALSE(Filter.Matches(MakeMessage(ELogVerbosity::VeryVerbose, "LogCore", "Detail")));

		Filter.LeastVerbosity = ELogVerbosity::Fatal;
		CHECK(Filter.Matches(MakeMessage(ELogVerbosity::Fatal, "LogCore", "Gone")));
		CHECK_FALSE(Filter.Matches(MakeMessage(ELogVerbosity::Error, "LogCore", "Failed")));
	}

	TEST_CASE("A filter with a category wants the messages of that category, written exactly so")
	{
		FLogFilter Filter;
		Filter.Category = "LogScene";

		CHECK(Filter.Matches(MakeMessage(ELogVerbosity::Log, "LogScene", "An entity was created")));
		CHECK_FALSE(Filter.Matches(MakeMessage(ELogVerbosity::Log, "LogSceneRenderer", "A scene was drawn")));
		CHECK_FALSE(Filter.Matches(MakeMessage(ELogVerbosity::Log, "logscene", "An entity was created")));
		CHECK_FALSE(Filter.Matches(MakeMessage(ELogVerbosity::Log, "LogCore", "About LogScene")));
		CHECK_FALSE(Filter.Matches(MakeMessage(ELogVerbosity::Log, "", "An entity was created")));
	}

	TEST_CASE("A filter with a text wants the messages that have it in their text or their category, in any case")
	{
		FLogFilter Filter;
		Filter.SearchText = "crate";

		CHECK(Filter.Matches(MakeMessage(ELogVerbosity::Log, "LogScene", "The Crate was moved")));
		CHECK(Filter.Matches(MakeMessage(ELogVerbosity::Log, "LogScene", "crates")));
		CHECK(Filter.Matches(MakeMessage(ELogVerbosity::Log, "LogCrate", "Something else")));
		CHECK_FALSE(Filter.Matches(MakeMessage(ELogVerbosity::Log, "LogScene", "The barrel was moved")));
		CHECK_FALSE(Filter.Matches(MakeMessage(ELogVerbosity::Log, "LogScene", "")));

		Filter.SearchText = "LOGSCENE";
		CHECK(Filter.Matches(MakeMessage(ELogVerbosity::Log, "LogScene", "The barrel was moved")));
	}

	TEST_CASE("A message has to be wanted for its kind, its category and its text at once")
	{
		FLogFilter Filter;
		Filter.LeastVerbosity = ELogVerbosity::Warning;
		Filter.Category = "LogAssets";
		Filter.SearchText = "wood";

		CHECK(Filter.Matches(MakeMessage(ELogVerbosity::Warning, "LogAssets", "Textures/Wood.png was not read")));
		CHECK_FALSE(Filter.Matches(MakeMessage(ELogVerbosity::Log, "LogAssets", "Textures/Wood.png was read")));
		CHECK_FALSE(Filter.Matches(MakeMessage(ELogVerbosity::Warning, "LogScene", "The wood is missing")));
		CHECK_FALSE(
			Filter.Matches(MakeMessage(ELogVerbosity::Warning, "LogAssets", "Textures/Stone.png was not read")));
	}

	TEST_CASE("Filters are equal when they want the same")
	{
		FLogFilter Filter;
		Filter.LeastVerbosity = ELogVerbosity::Error;
		Filter.Category = "LogCore";
		Filter.SearchText = "file";
		FLogFilter Other = Filter;
		CHECK(Filter == Other);

		Other.SearchText = "File";
		CHECK_FALSE(Filter == Other);
		Other = Filter;
		Other.Category.clear();
		CHECK_FALSE(Filter == Other);
		Other = Filter;
		Other.LeastVerbosity = ELogVerbosity::Warning;
		CHECK_FALSE(Filter == Other);
	}
}
