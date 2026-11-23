#pragma once

#include "CoreMinimal.h"

#include <string>
#include <string_view>

/**
 * How the user interface words what it shows: the names of the code as people read them, and the search for text
 * in text that its search fields do.
 */
class FUiText
{
public:
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
};
