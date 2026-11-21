#include "Tokenizer.h"

#include <doctest/doctest.h>

#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	/** Returns the text of every token, which is what most tests compare. */
	std::vector<std::string> TokenTexts(std::string_view Source)
	{
		std::vector<std::string> Texts;
		for (const FToken& Token : FTokenizer::Tokenize(Source))
		{
			Texts.push_back(Token.Text);
		}
		return Texts;
	}
}

TEST_SUITE("FTokenizer")
{
	TEST_CASE("Empty or blank text has no tokens")
	{
		CHECK(FTokenizer::Tokenize("").empty());
		CHECK(FTokenizer::Tokenize(" \t\r\n\n").empty());
	}

	TEST_CASE("Identifiers, numbers and punctuation are separate tokens")
	{
		const std::vector<FToken> Tokens = FTokenizer::Tokenize("int32 Count_2 = 10;");

		REQUIRE(Tokens.size() == 5);
		CHECK(Tokens[0] == FToken{ETokenType::Identifier, "int32", 1});
		CHECK(Tokens[1] == FToken{ETokenType::Identifier, "Count_2", 1});
		CHECK(Tokens[2] == FToken{ETokenType::Punctuation, "=", 1});
		CHECK(Tokens[3] == FToken{ETokenType::Number, "10", 1});
		CHECK(Tokens[4] == FToken{ETokenType::Punctuation, ";", 1});
	}

	TEST_CASE("The scope operator is one token and every other mark is a single character")
	{
		CHECK(TokenTexts("std::vector<TObjectPtr<WDoor>>") ==
			std::vector<std::string>{"std", "::", "vector", "<", "TObjectPtr", "<", "WDoor", ">", ">"});
		CHECK(TokenTexts("A->B") == std::vector<std::string>{"A", "-", ">", "B"});
		CHECK(TokenTexts("Base : public") == std::vector<std::string>{"Base", ":", "public"});
	}

	TEST_CASE("Tokens know the line they start on")
	{
		const std::vector<FToken> Tokens = FTokenizer::Tokenize("A\n\nB\r\n  C");

		REQUIRE(Tokens.size() == 3);
		CHECK(Tokens[0].Line == 1);
		CHECK(Tokens[1].Line == 3);
		CHECK(Tokens[2].Line == 4);
	}

	TEST_CASE("Comments produce no tokens")
	{
		CHECK(TokenTexts("A // WPROPERTY() ignored\nB") == std::vector<std::string>{"A", "B"});
		CHECK(TokenTexts("A /* WCLASS()\n class X */ B") == std::vector<std::string>{"A", "B"});
		CHECK(TokenTexts("/** Documentation */").empty());
		CHECK(TokenTexts("A /* never closed").size() == 1);
	}

	TEST_CASE("Lines are still counted inside comments")
	{
		const std::vector<FToken> Tokens = FTokenizer::Tokenize("/* one\n two\n three */ A // four\nB");

		REQUIRE(Tokens.size() == 2);
		CHECK(Tokens[0].Line == 3);
		CHECK(Tokens[1].Line == 4);
	}

	TEST_CASE("Preprocessor directives produce no tokens")
	{
		CHECK(TokenTexts("#pragma once\n#include \"CoreMinimal.h\"\nA") == std::vector<std::string>{"A"});
		CHECK(TokenTexts("  #  define WCLASS(...)\nA") == std::vector<std::string>{"A"});
	}

	TEST_CASE("A directive continued with a backslash is skipped to its real end")
	{
		const std::vector<FToken> Tokens = FTokenizer::Tokenize("#define MACRO(Name) \\\n\tclass Name \\\n\t{}\nAfter");

		REQUIRE(Tokens.size() == 1);
		CHECK(Tokens[0].Text == "After");
		CHECK(Tokens[0].Line == 4);
	}

	TEST_CASE("A hash that does not start a line is ordinary punctuation")
	{
		CHECK(TokenTexts("A # B") == std::vector<std::string>{"A", "#", "B"});
	}

	TEST_CASE("String and character literals are single tokens")
	{
		const std::vector<FToken> Tokens = FTokenizer::Tokenize(R"(Name = "WCLASS() { ; }" + 'x' + '\'' + "a\"b";)");

		REQUIRE(Tokens.size() == 10);
		CHECK(Tokens[2] == FToken{ETokenType::String, R"("WCLASS() { ; }")", 1});
		CHECK(Tokens[4] == FToken{ETokenType::Character, "'x'", 1});
		CHECK(Tokens[6] == FToken{ETokenType::Character, R"('\'')", 1});
		CHECK(Tokens[8] == FToken{ETokenType::String, R"("a\"b")", 1});
	}

	TEST_CASE("A literal keeps its prefix")
	{
		const std::vector<FToken> Tokens = FTokenizer::Tokenize("u8\"text\" L'c'");

		REQUIRE(Tokens.size() == 2);
		CHECK(Tokens[0] == FToken{ETokenType::String, "u8\"text\"", 1});
		CHECK(Tokens[1] == FToken{ETokenType::Character, "L'c'", 1});
	}

	TEST_CASE("A raw string literal ends only at its own delimiter")
	{
		const std::vector<FToken> Tokens = FTokenizer::Tokenize("R\"x(first \" )\" \n second)x\" After");

		REQUIRE(Tokens.size() == 2);
		CHECK(Tokens[0].Type == ETokenType::String);
		CHECK(Tokens[0].Text == "R\"x(first \" )\" \n second)x\"");
		CHECK(Tokens[1] == FToken{ETokenType::Identifier, "After", 2});
	}

	TEST_CASE("Numbers include separators, suffixes, hexadecimal digits and exponents")
	{
		CHECK(TokenTexts("1'000'000 0xFFu 1.5e-3f .25 90.0f") ==
			std::vector<std::string>{"1'000'000", "0xFFu", "1.5e-3f", ".25", "90.0f"});
		CHECK(TokenTexts("A-1") == std::vector<std::string>{"A", "-", "1"});
	}
}
