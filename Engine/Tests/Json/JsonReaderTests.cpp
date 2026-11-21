#include "Json/JsonReader.h"

#include "Json/JsonValue.h"

#include <doctest/doctest.h>

#include <limits>
#include <ostream>
#include <string>
#include <string_view>

namespace
{
	/** Parses text that the test expects to be valid. */
	FJsonValue ParseValid(std::string_view Text)
	{
		FJsonValue Value;
		std::string Error;
		const bool bParsed = FJsonReader::Parse(Text, Value, Error);
		INFO("Text: ", std::string(Text));
		INFO("Error: ", Error);
		REQUIRE(bParsed);
		return Value;
	}

	/** Parses text that the test expects to be rejected, and returns the error message. */
	std::string ParseInvalid(std::string_view Text)
	{
		FJsonValue Value("unchanged");
		std::string Error;
		const bool bParsed = FJsonReader::Parse(Text, Value, Error);
		INFO("Text: ", std::string(Text));
		REQUIRE_FALSE(bParsed);
		CHECK(Value == FJsonValue("unchanged"));
		CHECK_FALSE(Error.empty());
		return Error;
	}
}

TEST_SUITE("FJsonReader")
{
	TEST_CASE("The literals parse to null and booleans")
	{
		CHECK(ParseValid("null").IsNull());
		CHECK(ParseValid("true") == FJsonValue(true));
		CHECK(ParseValid("false") == FJsonValue(false));
	}

	TEST_CASE("A number without fraction or exponent parses as an integer")
	{
		CHECK(ParseValid("0") == FJsonValue(0));
		CHECK(ParseValid("-0") == FJsonValue(0));
		CHECK(ParseValid("42") == FJsonValue(42));
		CHECK(ParseValid("-17") == FJsonValue(-17));
		CHECK(ParseValid("9223372036854775807") == FJsonValue(std::numeric_limits<int64>::max()));
		CHECK(ParseValid("-9223372036854775808") == FJsonValue(std::numeric_limits<int64>::min()));
	}

	TEST_CASE("A number with a fraction or an exponent parses as a double")
	{
		CHECK(ParseValid("1.5") == FJsonValue(1.5));
		CHECK(ParseValid("-0.25") == FJsonValue(-0.25));
		CHECK(ParseValid("1e3") == FJsonValue(1000.0));
		CHECK(ParseValid("1E+3") == FJsonValue(1000.0));
		CHECK(ParseValid("25e-1") == FJsonValue(2.5));
		CHECK(ParseValid("3.0") == FJsonValue(3.0));
		CHECK(ParseValid("1.7976931348623157e308") == FJsonValue(std::numeric_limits<double>::max()));
	}

	TEST_CASE("An integer above the signed 64-bit range parses as an unsigned integer")
	{
		CHECK(ParseValid("9223372036854775808") == FJsonValue(uint64{9223372036854775808ull}));
		CHECK(ParseValid("18446744073709551615") == FJsonValue(std::numeric_limits<uint64>::max()));
	}

	TEST_CASE("An integer too large for 64 bits parses as the nearest double")
	{
		const FJsonValue TooLarge = ParseValid("18446744073709551616");
		const FJsonValue TooSmall = ParseValid("-9223372036854775809");

		CHECK_FALSE(TooLarge.IsInteger());
		CHECK_FALSE(TooLarge.IsUnsignedInteger());
		CHECK(TooLarge.GetNumber() == 18446744073709551616.0);
		CHECK_FALSE(TooSmall.IsInteger());
		CHECK(TooSmall.GetNumber() == -9223372036854775808.0);
	}

	TEST_CASE("A double keeps every digit needed to tell it apart")
	{
		CHECK(ParseValid("149597870700.123") == FJsonValue(149597870700.123));
		CHECK(ParseValid("0.1") == FJsonValue(0.1));
	}

	TEST_CASE("A string parses with its escapes resolved")
	{
		CHECK(ParseValid(R"("")") == FJsonValue(""));
		CHECK(ParseValid(R"("plain text")") == FJsonValue("plain text"));
		CHECK(ParseValid(R"("quote \" backslash \\ slash \/")") == FJsonValue("quote \" backslash \\ slash /"));
		CHECK(ParseValid(R"("\b\f\n\r\t")") == FJsonValue("\b\f\n\r\t"));
	}

	TEST_CASE("Unicode escapes become UTF-8")
	{
		CHECK(ParseValid(R"("A")") == FJsonValue("A"));
		CHECK(ParseValid(R"("é")") == FJsonValue("\xC3\xA9"));
		CHECK(ParseValid(R"("€")") == FJsonValue("\xE2\x82\xAC"));
		CHECK(ParseValid(R"("😀")") == FJsonValue("\xF0\x9F\x98\x80"));
	}

	TEST_CASE("UTF-8 text inside a string is kept as it is")
	{
		CHECK(ParseValid("\"caf\xC3\xA9 \xF0\x9F\x98\x80\"") == FJsonValue("caf\xC3\xA9 \xF0\x9F\x98\x80"));
	}

	TEST_CASE("Arrays parse with their elements in order")
	{
		CHECK(ParseValid("[]") == FJsonValue::MakeArray());
		CHECK(ParseValid("[ ]") == FJsonValue::MakeArray());

		const FJsonValue Array = ParseValid(R"([1, "two", [3], null])");
		REQUIRE(Array.GetElements().size() == 4);
		CHECK(Array.GetElements()[0] == FJsonValue(1));
		CHECK(Array.GetElements()[1] == FJsonValue("two"));
		CHECK(Array.GetElements()[2] == FJsonValue(FJsonValue::FArray{FJsonValue(3)}));
		CHECK(Array.GetElements()[3].IsNull());
	}

	TEST_CASE("Objects parse with their members in document order")
	{
		CHECK(ParseValid("{}") == FJsonValue::MakeObject());

		const FJsonValue Object = ParseValid(R"({"Zebra": 1, "Apple": {"Nested": true}, "": []})");
		REQUIRE(Object.GetMembers().size() == 3);
		CHECK(Object.GetMembers()[0].first == "Zebra");
		CHECK(Object.GetMembers()[1].first == "Apple");
		CHECK(Object.GetMembers()[2].first == "");
		REQUIRE(Object.FindMember("Apple") != nullptr);
		CHECK(Object.FindMember("Apple")->FindMember("Nested") != nullptr);
	}

	TEST_CASE("Whitespace around values is ignored")
	{
		CHECK(ParseValid(" \t\r\n{ \"A\" :\n[ 1 ,\t2 ] }\r\n") == ParseValid(R"({"A":[1,2]})"));
	}

	TEST_CASE("A leading byte order mark is accepted")
	{
		CHECK(ParseValid("\xEF\xBB\xBFtrue") == FJsonValue(true));
	}

	TEST_CASE("Text that is not a complete document is rejected")
	{
		ParseInvalid("");
		ParseInvalid("   ");
		ParseInvalid("nul");
		ParseInvalid("True");
		ParseInvalid("[1, 2");
		ParseInvalid("{\"A\": 1");
		ParseInvalid("{\"A\"}");
		ParseInvalid("{\"A\": }");
		ParseInvalid("{A: 1}");
		ParseInvalid("\"unterminated");
		ParseInvalid("1 2");
		ParseInvalid("{} extra");
	}

	TEST_CASE("Trailing commas and comments are rejected")
	{
		ParseInvalid("[1, 2,]");
		ParseInvalid("{\"A\": 1,}");
		ParseInvalid("[1] // comment");
		ParseInvalid("/* comment */ 1");
	}

	TEST_CASE("Malformed numbers are rejected")
	{
		ParseInvalid("01");
		ParseInvalid("-");
		ParseInvalid("+1");
		ParseInvalid("1.");
		ParseInvalid(".5");
		ParseInvalid("1e");
		ParseInvalid("1e+");
		ParseInvalid("0x10");
		ParseInvalid("NaN");
		ParseInvalid("Infinity");
		ParseInvalid("1e999");
	}

	TEST_CASE("Malformed strings are rejected")
	{
		ParseInvalid("\"line\nbreak\"");
		ParseInvalid("\"tab\tcharacter\"");
		ParseInvalid(R"("bad \x escape")");
		ParseInvalid(R"("\u12")");
		ParseInvalid(R"("\u12G4")");
		ParseInvalid(R"("\ud83d")");
		ParseInvalid(R"("\ud83dA")");
		ParseInvalid(R"("\ude00")");
		ParseInvalid("\"ends with backslash\\");
	}

	TEST_CASE("An object with the same key twice is rejected")
	{
		const std::string Error = ParseInvalid(R"({"Name": 1, "Name": 2})");

		CHECK(Error.find("Name") != std::string::npos);
	}

	TEST_CASE("The error message gives the line and column of the problem")
	{
		const std::string Error = ParseInvalid("{\n  \"A\": 1,\n  \"B\": oops\n}");

		CHECK(Error.starts_with("line 3, column 8: "));
	}

	TEST_CASE("Nesting up to the limit parses and deeper nesting is rejected")
	{
		const std::string AtLimit = std::string(128, '[') + std::string(128, ']');
		const std::string OverLimit = std::string(129, '[') + std::string(129, ']');

		ParseValid(AtLimit);
		const std::string Error = ParseInvalid(OverLimit);
		CHECK(Error.find("nested") != std::string::npos);
	}
}
