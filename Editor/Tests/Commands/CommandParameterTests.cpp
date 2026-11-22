#include "Commands/CommandParameter.h"

#include <doctest/doctest.h>

#include <ostream>
#include <string>

TEST_SUITE("FCommandParameter")
{
	TEST_CASE("A required parameter has to be given an argument")
	{
		const FCommandParameter Parameter =
			FCommandParameter::Required("Entity", ECommandArgumentType::Integer, "The identifier of the entity.");

		CHECK(Parameter.Name == "Entity");
		CHECK(Parameter.Type == ECommandArgumentType::Integer);
		CHECK(Parameter.Description == "The identifier of the entity.");
		CHECK_FALSE(Parameter.bOptional);
	}

	TEST_CASE("An optional parameter may be left out")
	{
		const FCommandParameter Parameter =
			FCommandParameter::Optional("Name", ECommandArgumentType::String, "The name of the new scene.");

		CHECK(Parameter.Name == "Name");
		CHECK(Parameter.Type == ECommandArgumentType::String);
		CHECK(Parameter.Description == "The name of the new scene.");
		CHECK(Parameter.bOptional);
	}
}
