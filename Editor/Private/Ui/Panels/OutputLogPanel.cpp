#include "Ui/Panels/OutputLogPanel.h"

#include "Editor.h"
#include "Logging/LogMessage.h"
#include "Logging/MemoryLogSink.h"
#include "Math/Color.h"
#include "Ui/ImGuiLibrary.h"
#include "Ui/Panels/UiPanel.h"
#include "Ui/UiFrame.h"
#include "Ui/UiItems.h"
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
	// new to copy, and not in every frame.
	const FMemoryLogSink& Log = Editor.GetLog();
	const int64 Revision = Log.GetRevision();
	if (Revision == FetchedRevision && Filter == FetchedFilter)
	{
		return;
	}

	Lines.clear();
	Categories.clear();
	for (FLogMessage& Message : Log.GetMessages())
	{
		if (std::ranges::find(Categories, Message.Category) == Categories.end())
		{
			Categories.push_back(Message.Category);
		}
		if (Filter.Matches(Message))
		{
			Lines.push_back({Message.Verbosity, std::move(Message.Category), MakeOneLine(std::move(Message.Text))});
		}
	}
	std::ranges::sort(Categories);
	FetchedRevision = Revision;
	FetchedFilter = Filter;
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
	if (ImGui::BeginCombo("##Category", Filter.Category.empty() ? AnyCategoryLabel : Filter.Category.c_str()))
	{
		if (ImGui::Selectable(AnyCategoryLabel, Filter.Category.empty()))
		{
			Filter.Category.clear();
		}
		Frame.Items.NameLastItem(std::format("OutputLog/Category/{}", AnyCategoryLabel));

		// The category that is chosen may be gone from the log, and with it from this list, while the list is
		// gone through: what is chosen is noted, and taken once the list is laid out.
		std::optional<std::string> Chosen;
		for (const std::string& Category : Categories)
		{
			if (ImGui::Selectable(Category.c_str(), Category == Filter.Category))
			{
				Chosen = Category;
			}
			Frame.Items.NameLastItem(std::format("OutputLog/Category/{}", Category));
		}
		if (Chosen.has_value())
		{
			Filter.Category = std::move(*Chosen);
		}
		ImGui::EndCombo();
	}
	Frame.Items.NameLastItem("OutputLog/Category");

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

void FOutputLogPanel::DrawLines(FUiFrame& Frame) const
{
	// The messages have a part of the panel to themselves, which is scrolled through when they are many.
	if (ImGui::BeginChild("##Messages", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
	{
		// Whoever is reading the newest message goes on reading the newest: the end stays in sight while it is.
		const bool bWasAtEnd = ImGui::GetScrollY() >= ImGui::GetScrollMaxY();

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
				ImGui::TextUnformatted(std::format("{}: {}", Line.Category, Line.Text).c_str());
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
}
