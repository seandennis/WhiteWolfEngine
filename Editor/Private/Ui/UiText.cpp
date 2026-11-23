#include "Ui/UiText.h"

#include <algorithm>
#include <cstddef>

namespace
{
	// Names are ASCII as far as their structure goes; anything else is a letter like any other and is left alone.

	bool IsUpper(char Character)
	{
		return Character >= 'A' && Character <= 'Z';
	}

	bool IsLower(char Character)
	{
		return Character >= 'a' && Character <= 'z';
	}

	bool IsDigit(char Character)
	{
		return Character >= '0' && Character <= '9';
	}

	char ToLower(char Character)
	{
		return IsUpper(Character) ? static_cast<char>(Character - 'A' + 'a') : Character;
	}

	/** The letters that names of types begin with: object classes, other types, enums, interfaces and templates. */
	constexpr std::string_view TypePrefixes = "WFEIT";
}

std::string FUiText::SplitWords(std::string_view Name)
{
	std::string Words;
	Words.reserve(Name.size() + 8);
	for (std::size_t Index = 0; Index < Name.size(); ++Index)
	{
		const char Character = Name[Index];
		if (Character == '_')
		{
			Words.push_back(' ');
			continue;
		}

		if (Index > 0)
		{
			const char Previous = Name[Index - 1];
			const bool bIsFollowedByLower = Index + 1 < Name.size() && IsLower(Name[Index + 1]);

			// A capital begins a word after a small letter, and where a run of capitals or digits ends in a word
			// that goes on in small letters: the I of "HDRImage", the B of "Level2Boss".
			const bool bCapitalBeginsWord = IsUpper(Character) &&
				(IsLower(Previous) || ((IsUpper(Previous) || IsDigit(Previous)) && bIsFollowedByLower));
			// A number begins a word after a letter, and takes the capitals after it along: "Vector 3D".
			const bool bDigitBeginsWord = IsDigit(Character) && (IsLower(Previous) || IsUpper(Previous));
			if (bCapitalBeginsWord || bDigitBeginsWord)
			{
				Words.push_back(' ');
			}
		}
		Words.push_back(Character);
	}
	return Words;
}

std::string FUiText::MakePropertyLabel(std::string_view PropertyName)
{
	const bool bHasBooleanPrefix = PropertyName.size() >= 2 && PropertyName[0] == 'b' && IsUpper(PropertyName[1]);
	return SplitWords(bHasBooleanPrefix ? PropertyName.substr(1) : PropertyName);
}

std::string FUiText::MakeClassLabel(std::string_view ClassName)
{
	const bool bHasTypePrefix =
		ClassName.size() >= 2 && TypePrefixes.find(ClassName[0]) != std::string_view::npos && IsUpper(ClassName[1]);
	return SplitWords(bHasTypePrefix ? ClassName.substr(1) : ClassName);
}

bool FUiText::ContainsIgnoringCase(std::string_view Text, std::string_view Part)
{
	const auto Found = std::search(Text.begin(),
		Text.end(),
		Part.begin(),
		Part.end(),
		[](char Left, char Right) { return ToLower(Left) == ToLower(Right); });
	return Part.empty() || Found != Text.end();
}
