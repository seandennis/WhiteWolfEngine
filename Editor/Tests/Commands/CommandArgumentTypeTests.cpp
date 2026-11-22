#include "Commands/CommandArgumentType.h"

#include <doctest/doctest.h>

#include <ostream>
#include <string_view>

TEST_SUITE("ECommandArgumentType")
{
	TEST_CASE("Every type has the name the list of commands shows")
	{
		CHECK(GetCommandArgumentTypeName(ECommandArgumentType::Boolean) == "Boolean");
		CHECK(GetCommandArgumentTypeName(ECommandArgumentType::Integer) == "Integer");
		CHECK(GetCommandArgumentTypeName(ECommandArgumentType::Number) == "Number");
		CHECK(GetCommandArgumentTypeName(ECommandArgumentType::String) == "String");
		CHECK(GetCommandArgumentTypeName(ECommandArgumentType::Json) == "Json");
	}

	TEST_CASE("A value that is not a type is named Unknown")
	{
		const ECommandArgumentType NotAType = static_cast<ECommandArgumentType>(200);

		CHECK(GetCommandArgumentTypeName(NotAType) == "Unknown");
	}
}
