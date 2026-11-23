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

		/** What the line says: what the message is about and what it says, on one line whatever it was logged as. */
		std::string Text;

		/** The number that the message has among all that were ever logged (FMemoryLogSink::GetMessages). */
		int64 Number = 0;
	};

	/**
	 * Fetches the messages again if the log has changed since they were fetched, or what is wanted of it has, and
	 * measures them. Call it while the panel is being laid out: it asks how wide letters are.
	 */
	void Refresh(const FEditor& Editor);

	/** Lays out what chooses among the messages, and the button that forgets them. */
	void DrawFilter(FUiFrame& Frame);

	/** Lays out the messages that are wanted, of which those in sight are drawn. */
	void DrawLines(FUiFrame& Frame);

	/** Which messages the user wants to see. To begin with, all that the log keeps. */
	FLogFilter Filter;

	/** The messages that the filter wants, oldest first, as they were when they were last fetched. */
	std::vector<FLine> Lines;

	/** The categories of all the messages that the log keeps, in the order of the alphabet. */
	std::vector<std::string> Categories;

	/** What the log's revision was when the messages were fetched, and what was wanted of them then. */
	int64 FetchedRevision = -1;
	FLogFilter FetchedFilter;

	/** How wide the widest of the lines is, in pixels, and the size of the letters that it was measured with. */
	float WidestLine = 0.0f;
	float MeasuredFontSize = 0.0f;

	/**
	 * How many of the lines that were shown have gone from the top of the list since the list was last laid out:
	 * those of messages that a full log has discarded for newer ones.
	 */
	int32 GoneLineCount = 0;
};
