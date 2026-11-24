#include "Ui/Panels/OutputLogPanel.h"

#include "Editor.h"
#include "Logging/LogMessage.h"
#include "Logging/MemoryLogSink.h"
#include "Math/Color.h"
#include "Ui/ImGuiLibrary.h"
#include "Ui/Panels/UiPanel.h"
#include "Ui/UiFrame.h"
#include "Ui/UiItems.h"
#include "Ui/UiNames.h"
#include "Ui/UiTheme.h"
#include "Ui/UiWorkspace.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{
	/** A choice of how important a message has to be to be shown. */
	struct FVerbosityChoice
	{
		ELogVerbosity LeastVerbosity;
		const char* Label;
	};

	constexpr std::array<FVerbosityChoice, 3> VerbosityChoices = {{
		{ELogVerbosity::VeryVerbose, "All messages"},
		{ELogVerbosity::Warning, "Warnings and errors"},
		{ELogVerbosity::Error, "Errors"},
	}};

	/** What the list of categories shows while messages of every category are wanted. */
	constexpr const char* AnyCategoryLabel = "All categories";

	/** How wide the field for a text to look for is, in heights of a letter. */
	constexpr float SearchWidth = 14.0f;

	/** Returns how wide a list has to be to show the widest of some texts, with the arrow that opens it. */
	float GetListWidth(float WidestText)
	{
		return WidestText + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetFrameHeight();
	}

	/** Returns a text on one line: where it went on in a new line, it goes on after a space. */
	std::string MakeOneLine(std::string Text)
	{
		std::erase(Text, '\r');
		std::ranges::replace(Text, '\n', ' ');
		return Text;
	}

	/** Returns the colour that a kind of message is written in, or nothing for the colour of any text. */
	std::optional<FColor> GetVerbosityColor(ELogVerbosity Verbosity)
	{
		switch (Verbosity)
		{
			case ELogVerbosity::Fatal:
			case ELogVerbosity::Error:
				return FUiTheme::GetErrorColor();
			case ELogVerbosity::Warning:
				return FUiTheme::GetWarningColor();
			case ELogVerbosity::Display:
			case ELogVerbosity::Log:
			case ELogVerbosity::Verbose:
			case ELogVerbosity::VeryVerbose:
			default:
				return std::nullopt;
		}
	}
}

void FOutputLogPanel::Draw(FUiFrame& Frame)
{
	if (!Frame.Workspace.bShowOutputLog)
	{
		return;
	}

	if (FUiPanel::Begin(Title, Frame.Workspace.bShowOutputLog))
	{
		DrawFilter(Frame);
		Refresh(Frame.Editor);
		DrawLines(Frame);
	}
	ImGui::End();
}

void FOutputLogPanel::Refresh(const FEditor& Editor)
{
	// The messages are copied out of the log, which other threads write to. That is done when there is something
	// new to copy, and not in every frame. How wide they are is measured then, and again when the letters have
	// another size.
	const FMemoryLogSink& Log = Editor.GetLog();
	const int64 Revision = Log.GetRevision();
	const float FontSize = ImGui::GetFontSize();
	if (Revision == FetchedRevision && Filter == FetchedFilter && FontSize == MeasuredFontSize)
	{
		return;
	}

	int64 FirstNumber = 0;
	std::vector<FLogMessage> Messages = Log.GetMessages(FirstNumber);

	// Lines whose messages the log has discarded since they were shown have gone from the top of the list. With
	// another filter the lines are other lines, and none of them was shown before.
	if (Filter == FetchedFilter)
	{
		GoneLineCount += static_cast<int32>(
			std::ranges::count_if(Lines, [FirstNumber](const FLine& Line) { return Line.Number < FirstNumber; }));
	}
	else
	{
		GoneLineCount = 0;
	}

	Lines.clear();
	Categories.clear();
	WidestLine = 0.0f;
	int64 Number = FirstNumber;
	for (FLogMessage& Message : Messages)
	{
		if (std::ranges::find(Categories, Message.Category) == Categories.end())
		{
			Categories.push_back(Message.Category);
		}
		if (Filter.Matches(Message))
		{
			std::string Text = std::format("{}: {}", Message.Category, MakeOneLine(std::move(Message.Text)));
			WidestLine = std::max(WidestLine, ImGui::CalcTextSize(Text.c_str()).x);
			Lines.push_back({Message.Verbosity, std::move(Text), Number});
		}
		++Number;
	}
	std::ranges::sort(Categories);
	FetchedRevision = Revision;
	FetchedFilter = Filter;
	MeasuredFontSize = FontSize;
}

void FOutputLogPanel::DrawFilter(FUiFrame& Frame)
{
	// How important a message has to be.
	const char* VerbosityLabel = VerbosityChoices.front().Label;
	float WidestVerbosity = 0.0f;
	for (const FVerbosityChoice& Choice : VerbosityChoices)
	{
		WidestVerbosity = std::max(WidestVerbosity, ImGui::CalcTextSize(Choice.Label).x);
		if (Choice.LeastVerbosity == Filter.LeastVerbosity)
		{
			VerbosityLabel = Choice.Label;
		}
	}
	ImGui::SetNextItemWidth(GetListWidth(WidestVerbosity));
	if (ImGui::BeginCombo("##Verbosity", VerbosityLabel))
	{
		for (const FVerbosityChoice& Choice : VerbosityChoices)
		{
			if (ImGui::Selectable(Choice.Label, Choice.LeastVerbosity == Filter.LeastVerbosity))
			{
				Filter.LeastVerbosity = Choice.LeastVerbosity;
			}
			Frame.Items.NameLastItem(std::format("OutputLog/Verbosity/{}", Choice.Label));
		}
		ImGui::EndCombo();
	}
	Frame.Items.NameLastItem("OutputLog/Verbosity");

	// What it has to be about: one of the categories that the log has messages of, or any.
	float WidestCategory = ImGui::CalcTextSize(AnyCategoryLabel).x;
	for (const std::string& Category : Categories)
	{
		WidestCategory = std::max(WidestCategory, ImGui::CalcTextSize(Category.c_str()).x);
	}
	ImGui::SameLine();
	ImGui::SetNextItemWidth(GetListWidth(WidestCategory));
	// The name of a category is a module's to give, and is neither the field's own text nor a line's: one with
	// "##" in it would be shown cut off, and one of no letters fails a check of the library.
	const std::string ShownCategory = Filter.Category.empty() ? std::string(AnyCategoryLabel) : Filter.Category;
	if (ImGui::BeginCombo("##Category", nullptr))
	{
		if (ImGui::Selectable(AnyCategoryLabel, Filter.Category.empty()))
		{
			Filter.Category.clear();
		}
		Frame.Items.NameLastItem(std::format("OutputLog/Category/{}", AnyCategoryLabel));

		for (std::size_t Index = 0; Index < Categories.size(); ++Index)
		{
			const std::string& Category = Categories[Index];
			ImGui::PushID(static_cast<int>(Index));
			const std::string LineName = std::format("OutputLog/Category/{}", Category);
			if (FUiNames::Selectable(Frame, LineName, Category, Category == Filter.Category))
			{
				Filter.Category = Category;
			}
			ImGui::PopID();
		}
		ImGui::EndCombo();
	}
	Frame.Items.NameLastItem("OutputLog/Category");
	FUiNames::WriteInField(ShownCategory);

	// What it has to say.
	ImGui::SameLine();
	ImGui::SetNextItemWidth(ImGui::GetFontSize() * SearchWidth);
	ImGui::InputTextWithHint("##Search", "Search", &Filter.SearchText);
	Frame.Items.NameLastItem("OutputLog/Search");

	ImGui::SameLine();
	if (ImGui::Button("Clear"))
	{
		Frame.Editor.ClearLog();
	}
	ImGui::SetItemTooltip("%s", "Forget the messages that have been logged so far");
	Frame.Items.NameLastItem("OutputLog/Clear");
}

void FOutputLogPanel::DrawLines(FUiFrame& Frame)
{
	// The messages have a part of the panel to themselves, which is scrolled through when they are many, and
	// sideways when one of them is longer than the panel is wide. How wide the widest of them is, is said here and
	// not left to be found out: only the lines in sight are laid out, and a bar that came and went with them would
	// move them up and down.
	ImGui::SetNextWindowContentSize(ImVec2(WidestLine, 0.0f));
	if (ImGui::BeginChild("##Messages", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
	{
		// Whoever is reading the newest message goes on reading the newest: the end stays in sight while it is.
		const bool bWasAtEnd = ImGui::GetScrollY() >= ImGui::GetScrollMaxY();

		// Whoever is reading older messages goes on reading those. Lines that have gone from the top of the list
		// have taken every line below them up with them: from the next frame on the list is scrolled back by as
		// much, and in this one it begins that much further down. The lines are there to begin it with: a place
		// that nothing is laid out at fails a check of the library.
		if (GoneLineCount > 0 && !bWasAtEnd && !Lines.empty())
		{
			const float GoneHeight = static_cast<float>(GoneLineCount) * ImGui::GetTextLineHeightWithSpacing();
			const float KeptHeight = std::min(GoneHeight, ImGui::GetScrollY());
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + KeptHeight);
			ImGui::SetScrollY(ImGui::GetScrollY() - KeptHeight);
		}
		GoneLineCount = 0;

		// Only the lines that are in sight are laid out, however many there are.
		ImGuiListClipper Clipper;
		Clipper.Begin(static_cast<int>(Lines.size()));
		while (Clipper.Step())
		{
			for (int Index = Clipper.DisplayStart; Index < Clipper.DisplayEnd; ++Index)
			{
				const FLine& Line = Lines[static_cast<std::size_t>(Index)];
				const std::optional<FColor> Color = GetVerbosityColor(Line.Verbosity);
				if (Color.has_value())
				{
					ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(Color->R, Color->G, Color->B, Color->A));
				}
				ImGui::TextUnformatted(Line.Text.c_str());
				if (Color.has_value())
				{
					ImGui::PopStyleColor();
				}
				Frame.Items.NameLastItem(std::format("OutputLog/Line/{}", Index));
			}
		}
		Clipper.End();

		if (bWasAtEnd)
		{
			constexpr float AtBottom = 1.0f;
			ImGui::SetScrollHereY(AtBottom);
		}
	}
	ImGui::EndChild();
	Frame.Items.NameLastItem("OutputLog/Messages");
}
