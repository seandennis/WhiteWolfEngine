#pragma once

#include "CoreMinimal.h"

#include "Logging/LogFilter.h"
#include "Logging/LogVerbosity.h"

#include <string>
#include <vector>

class FEditor;
struct FUiFrame;

/**
 * The panel that shows what the editor and the engine have logged: the messages that the editor keeps
 * (FEditor::GetLog), one to a line, the newest last. It is where the user reads why something did not do what was
 * expected, as an agent reads it with Log.Read.
 *
 * Above the messages are what chooses among them: how important a message has to be, what it has to be about,
 * and a text that it has to have in it. Warnings and errors are in colours of their own.
 */
class FOutputLogPanel
{
public:
	/** What the panel is called on its tab. Dear ImGui also knows the panel by this. */
	static constexpr const char* Title = "Output Log";

	/** Lays the panel out. */
	void Draw(FUiFrame& Frame);

private:
	/** A message as the panel shows it. */
	struct FLine
	{
		ELogVerbosity Verbosity = ELogVerbosity::Log;
		std::string Category;

		/** What the message says, on one line whatever it was logged as. */
		std::string Text;
	};

	/** Fetches the messages again if the log has changed since they were fetched, or what is wanted of it has. */
	void Refresh(const FEditor& Editor);

	/** Lays out what chooses among the messages, and the button that forgets them. */
	void DrawFilter(FUiFrame& Frame);

	/** Lays out the messages that are wanted, of which those in sight are drawn. */
	void DrawLines(FUiFrame& Frame) const;

	/** Which messages the user wants to see. To begin with, all that the log keeps. */
	FLogFilter Filter;

	/** The messages that the filter wants, oldest first, as they were when they were last fetched. */
	std::vector<FLine> Lines;

	/** The categories of all the messages that the log keeps, in the order of the alphabet. */
	std::vector<std::string> Categories;

	/** What the log's revision was when the messages were fetched, and what was wanted of them then. */
	int64 FetchedRevision = -1;
	FLogFilter FetchedFilter;
};
