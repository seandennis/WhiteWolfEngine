#pragma once

#include "CoreMinimal.h"

#include <functional>
#include <string>
#include <string_view>

/**
 * How the user interface words what it shows: the names of the code as people read them, the search for text in
 * text that its search fields do, and what to show of a text that is too long for its place.
 */
class FUiText
{
public:
	/** What FitEnd puts in the place of the beginning that it leaves out. */
	static constexpr std::string_view Ellipsis = "...";

	/**
	 * Returns a name of the code with its words apart: "CastShadows" is "Cast Shadows", "HDRImage" is "HDR Image"
	 * and "Level2Boss" is "Level 2 Boss". An underscore becomes a space. A name that already has spaces, or that
	 * has no capitals inside it, comes back as it is.
	 */
	EDITOR_API static std::string SplitWords(std::string_view Name);

	/**
	 * Returns what a property is called beside its value: its name as words, without the b that the name of a
	 * boolean begins with. "bCastShadows" is "Cast Shadows".
	 */
	EDITOR_API static std::string MakePropertyLabel(std::string_view PropertyName);

	/**
	 * Returns what a class is called: its name as words, without the letter that says what kind of type it is.
	 * "WPointLightComponent" is "Point Light Component".
	 */
	EDITOR_API static std::string MakeClassLabel(std::string_view ClassName);

	/**
	 * Returns true if Text contains Part, taking the letters A to Z as the same in capitals and small. Every text
	 * contains the empty one.
	 */
	EDITOR_API static bool ContainsIgnoringCase(std::string_view Text, std::string_view Part);

	/**
	 * Returns a text as it is shown where it has only so much room: whole if it fits, and otherwise as much of
	 * its end as fits after Ellipsis. That is how to shorten a path, whose end says which file or directory it is
	 * and whose beginning only where the disk begins. What is left out is whole characters, never part of one.
	 *
	 * @param Text The text, in UTF-8.
	 * @param Width How much room there is, in the unit that Measure answers in.
	 * @param Measure Returns how wide a text is as it will be drawn.
	 * @return What to show: Text, or the end of it after Ellipsis, or Ellipsis alone if not even one character
	 * fits beside it.
	 */
	EDITOR_API static std::string FitEnd(
		std::string_view Text, float Width, const std::function<float(std::string_view)>& Measure);
};
