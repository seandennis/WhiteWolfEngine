#pragma once

#include "ParseError.h"
#include "ReflectedClass.h"

#include <string_view>
#include <vector>

/**
 * Finds the reflected classes in a C++ header.
 *
 * A reflected class looks like this:
 *
 *     WCLASS()
 *     class ENGINE_API WDoor : public WObject
 *     {
 *         WW_DECLARE_CLASS(WDoor, WObject)
 *
 *     public:
 *         WPROPERTY(Category = "Door")
 *         float OpenAngle = 90.0f;
 *
 *         WFUNCTION(BlueprintCallable)
 *         void Open(float Speed);
 *     };
 *
 * The parser understands only as much C++ as that takes. It reads the names of the class, its base class, the
 * marked members and their parameters; the types are left to the compiler, which sees them through the generated
 * registration code.
 */
class FHeaderParser
{
public:
	/**
	 * Parses the text of one header.
	 *
	 * @param OutClasses Receives the reflected classes, in the order they appear. Empty if parsing fails.
	 * @param OutErrors Receives every problem found. Empty if parsing succeeds.
	 * @return True if the header is acceptable; that includes a header with no reflected class at all.
	 */
	static bool Parse(
		std::string_view Source, std::vector<FReflectedClass>& OutClasses, std::vector<FParseError>& OutErrors);
};
