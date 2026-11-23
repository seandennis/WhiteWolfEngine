#include "Logging/LogFilter.h"

#include "Ui/UiText.h"

bool FLogFilter::Matches(const FLogMessage& Message) const
{
	// The kinds of message are numbered from the most important down.
	const bool bIsImportantEnough = Message.Verbosity <= LeastVerbosity;
	const bool bIsOfCategory = Category.empty() || Message.Category == Category;
	const bool bSaysIt = SearchText.empty() || FUiText::ContainsIgnoringCase(Message.Text, SearchText) ||
		FUiText::ContainsIgnoringCase(Message.Category, SearchText);
	return bIsImportantEnough && bIsOfCategory && bSaysIt;
}
