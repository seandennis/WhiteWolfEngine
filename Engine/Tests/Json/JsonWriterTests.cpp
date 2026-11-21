#include "Json/JsonWriter.h"

#include "Json/JsonReader.h"
#include "Json/JsonValue.h"

#include <doctest/doctest.h>

#include <limits>
#include <ostream>
#include <string>

namespace
{
	/** A value with every kind of JSON value inside it. */
	FJsonValue MakeSampleDocument()
	{
		FJsonValue Transform = FJsonValue::MakeObject();
		Transform.SetMember(
			"Location", FJsonValue(FJsonValue::FArray{FJsonValue(1.5), FJsonValue(-2.0), FJsonValue(0.0)}));

		FJsonValue Entity = FJsonValue::MakeObject();
		Entity.SetMember("Id", 7);
		Entity.SetMember("Name", "Door \"A\"");
		Entity.SetMember("Visible", true);
		Entity.SetMember("Parent", FJsonValue());
		Entity.SetMember("Transform", Transform);
		Entity.SetMember("Tags", FJsonValue::MakeArray());
		Entity.SetMember("Extra", FJsonValue::MakeObject());

		FJsonValue Document = FJsonValue::MakeObject();
		Document.SetMember("Version", 1);
		Document.SetMember("Entities", FJsonValue(FJsonValue::FArray{Entity}));
		return Document;
	}
}

TEST_SUITE("FJsonWriter")
{
	TEST_CASE("Scalars are written the same way in both formats")
	{
		for (const EJsonFormat Format : {EJsonFormat::Pretty, EJsonFormat::Compact})
		{
			CHECK(FJsonWriter::Write(FJsonValue(), Format) == "null");
			CHECK(FJsonWriter::Write(FJsonValue(true), Format) == "true");
			CHECK(FJsonWriter::Write(FJsonValue(false), Format) == "false");
			CHECK(FJsonWriter::Write(FJsonValue(-42), Format) == "-42");
			CHECK(FJsonWriter::Write(FJsonValue("text"), Format) == "\"text\"");
		}
	}

	TEST_CASE("Integers are written exactly")
	{
		CHECK(FJsonWriter::Write(FJsonValue(std::numeric_limits<int64>::max())) == "9223372036854775807");
		CHECK(FJsonWriter::Write(FJsonValue(std::numeric_limits<int64>::min())) == "-9223372036854775808");
		CHECK(FJsonWriter::Write(FJsonValue(std::numeric_limits<uint64>::max())) == "18446744073709551615");
	}

	TEST_CASE("Doubles are written with the fewest digits and always look like doubles")
	{
		CHECK(FJsonWriter::Write(FJsonValue(0.1)) == "0.1");
		CHECK(FJsonWriter::Write(FJsonValue(3.0)) == "3.0");
		CHECK(FJsonWriter::Write(FJsonValue(-2.5)) == "-2.5");
		CHECK(FJsonWriter::Write(FJsonValue(0.0)) == "0.0");
		CHECK(FJsonWriter::Write(FJsonValue(149597870700.123)) == "149597870700.123");
	}

	TEST_CASE("A float is written without the noise of its double conversion")
	{
		CHECK(FJsonWriter::Write(FJsonValue(0.1f)) == "0.1");
		CHECK(FJsonWriter::Write(FJsonValue(1.0f)) == "1.0");
	}

	TEST_CASE("Infinity and NaN are written as null")
	{
		CHECK(FJsonWriter::Write(FJsonValue(std::numeric_limits<double>::infinity())) == "null");
		CHECK(FJsonWriter::Write(FJsonValue(-std::numeric_limits<double>::infinity())) == "null");
		CHECK(FJsonWriter::Write(FJsonValue(std::numeric_limits<double>::quiet_NaN())) == "null");
	}

	TEST_CASE("Strings are escaped")
	{
		CHECK(FJsonWriter::Write(FJsonValue("quote \" backslash \\")) == R"("quote \" backslash \\")");
		CHECK(FJsonWriter::Write(FJsonValue("\b\f\n\r\t")) == R"("\b\f\n\r\t")");
		CHECK(FJsonWriter::Write(FJsonValue(std::string("\x01\x1f", 2))) == R"("\u0001\u001f")");
		CHECK(FJsonWriter::Write(FJsonValue("slash / stays")) == "\"slash / stays\"");
	}

	TEST_CASE("UTF-8 text is written as it is")
	{
		CHECK(FJsonWriter::Write(FJsonValue("caf\xC3\xA9")) == "\"caf\xC3\xA9\"");
	}

	TEST_CASE("Empty containers are written without whitespace")
	{
		CHECK(FJsonWriter::Write(FJsonValue::MakeArray()) == "[]");
		CHECK(FJsonWriter::Write(FJsonValue::MakeObject()) == "{}");
	}

	TEST_CASE("The pretty format puts members on their own lines and keeps arrays of scalars on one")
	{
		const std::string Expected = "{\n"
									 "  \"Version\": 1,\n"
									 "  \"Entities\": [\n"
									 "    {\n"
									 "      \"Id\": 7,\n"
									 "      \"Name\": \"Door \\\"A\\\"\",\n"
									 "      \"Visible\": true,\n"
									 "      \"Parent\": null,\n"
									 "      \"Transform\": {\n"
									 "        \"Location\": [1.5, -2.0, 0.0]\n"
									 "      },\n"
									 "      \"Tags\": [],\n"
									 "      \"Extra\": {}\n"
									 "    }\n"
									 "  ]\n"
									 "}";

		CHECK(FJsonWriter::Write(MakeSampleDocument(), EJsonFormat::Pretty) == Expected);
	}

	TEST_CASE("The compact format has no whitespace")
	{
		const std::string Expected =
			"{\"Version\":1,\"Entities\":[{\"Id\":7,\"Name\":\"Door \\\"A\\\"\",\"Visible\":true,"
			"\"Parent\":null,\"Transform\":{\"Location\":[1.5,-2.0,0.0]},\"Tags\":[],\"Extra\":{}}]}";

		CHECK(FJsonWriter::Write(MakeSampleDocument(), EJsonFormat::Compact) == Expected);
	}

	TEST_CASE("Written text parses back to an equal value in both formats")
	{
		const FJsonValue Original = MakeSampleDocument();

		for (const EJsonFormat Format : {EJsonFormat::Pretty, EJsonFormat::Compact})
		{
			FJsonValue Parsed;
			std::string Error;
			REQUIRE(FJsonReader::Parse(FJsonWriter::Write(Original, Format), Parsed, Error));

			CHECK(Parsed == Original);
		}
	}

	TEST_CASE("Extreme doubles survive a round trip exactly")
	{
		for (const double Original : {std::numeric_limits<double>::max(),
				 std::numeric_limits<double>::min(),
				 std::numeric_limits<double>::denorm_min(),
				 -0.0,
				 1.0e22,
				 1.0e-7,
				 123456789012345680.0,
				 1.0e13})
		{
			FJsonValue Parsed;
			std::string Error;
			REQUIRE(FJsonReader::Parse(FJsonWriter::Write(FJsonValue(Original)), Parsed, Error));

			CHECK_FALSE(Parsed.IsInteger());
			CHECK(Parsed.GetNumber() == Original);
		}
	}
}
