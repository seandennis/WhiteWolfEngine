#include "Editor.h"
#include "Logging/Log.h"
#include "Logging/LogMessage.h"
#include "Logging/MemoryLogSink.h"
#include "TestEditorObjects.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	/** Returns true if a message with a text is among those that an editor has kept. */
	bool HasLogged(const FEditor& Editor, std::string_view Text)
	{
		const std::vector<FLogMessage> Messages = Editor.GetLog().GetMessages();
		return std::ranges::any_of(Messages, [Text](const FLogMessage& Message) { return Message.Text == Text; });
	}
}

TEST_SUITE("FEditor log")
{
	TEST_CASE("An editor keeps what is logged while it exists, by itself and by the engine")
	{
		const FScopedEditorTest EditorTest;
		WW_LOG(LogCore, Display, "Before there was an editor");

		const FEditor Editor;
		WW_LOG(LogEditor, Log, "From the editor");
		WW_LOG(LogCore, Warning, "From the engine");

		CHECK(HasLogged(Editor, "From the editor"));
		CHECK(HasLogged(Editor, "From the engine"));
		CHECK_FALSE(HasLogged(Editor, "Before there was an editor"));

		// With all that the log knows of a message.
		const std::vector<FLogMessage> Messages = Editor.GetLog().GetMessages();
		REQUIRE_FALSE(Messages.empty());
		CHECK(Messages.back().Text == "From the engine");
		CHECK(Messages.back().Category == "LogCore");
		CHECK(Messages.back().Verbosity == ELogVerbosity::Warning);
	}

	TEST_CASE("What a category does not let through is not kept")
	{
		const FScopedEditorTest EditorTest;
		const FEditor Editor;

		// The editor's category lets ordinary messages through, and not the details of debugging.
		WW_LOG(LogEditor, Verbose, "A detail");

		CHECK_FALSE(HasLogged(Editor, "A detail"));
	}

	TEST_CASE("ClearLog forgets what was logged, and what is logged afterwards is kept")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		WW_LOG(LogEditor, Log, "Before");
		const int64 Revision = Editor.GetLog().GetRevision();

		Editor.ClearLog();

		CHECK(Editor.GetLog().GetMessages().empty());
		CHECK(Editor.GetLog().GetRevision() != Revision);

		WW_LOG(LogEditor, Log, "After");
		CHECK(HasLogged(Editor, "After"));
		CHECK_FALSE(HasLogged(Editor, "Before"));
	}

	TEST_CASE("Each editor keeps the log of its own time")
	{
		const FScopedEditorTest EditorTest;
		const std::unique_ptr<FEditor> First = std::make_unique<FEditor>();
		WW_LOG(LogEditor, Log, "While the first was alone");

		{
			const FEditor Second;
			WW_LOG(LogEditor, Log, "While there were two");

			CHECK(HasLogged(*First, "While the first was alone"));
			CHECK(HasLogged(*First, "While there were two"));
			CHECK_FALSE(HasLogged(Second, "While the first was alone"));
			CHECK(HasLogged(Second, "While there were two"));
		}

		// An editor that is gone takes nothing of the log with it.
		WW_LOG(LogEditor, Log, "While the first was alone again");
		CHECK(HasLogged(*First, "While the first was alone again"));
	}

	TEST_CASE("A command that fails is in the log with its reason, and one that works is not")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		Editor.ClearLog();

		REQUIRE(Editor.ExecuteCommand("Entity.Create", ParseTestJson(R"({"Name": "Player"})")).HasSucceeded());
		CHECK(Editor.GetLog().GetMessages().empty());

		REQUIRE_FALSE(Editor.ExecuteCommand("Entity.Delete", ParseTestJson(R"({"Entity": 99})")).HasSucceeded());
		CHECK(HasLogged(Editor, "Command Entity.Delete failed: There is no entity with the identifier 99."));
	}
}
