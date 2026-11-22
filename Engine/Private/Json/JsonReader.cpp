#include "Json/JsonReader.h"

#include "Core/Utf8.h"
#include "Json/JsonValue.h"

#include <charconv>
#include <cstddef>
#include <format>
#include <system_error>
#include <utility>

namespace
{
	/** Deeper nesting than this is rejected, so that hostile input cannot overflow the stack. */
	constexpr int32 MaxNestingDepth = 128;

	bool IsDigit(char Character)
	{
		return Character >= '0' && Character <= '9';
	}

	/** A recursive-descent parser over one document. */
	class FJsonParser
	{
	public:
		explicit FJsonParser(std::string_view InText)
			: Text(InText)
		{
		}

		bool ParseDocument(FJsonValue& OutValue)
		{
			constexpr std::string_view ByteOrderMark = "\xEF\xBB\xBF";
			if (Text.starts_with(ByteOrderMark))
			{
				Position = ByteOrderMark.size();
			}

			FJsonValue Value;
			if (!ParseValue(Value, 0))
			{
				return false;
			}

			SkipWhitespace();
			if (!IsAtEnd())
			{
				return Fail("unexpected text after the end of the document");
			}

			OutValue = std::move(Value);
			return true;
		}

		/** Returns the failure message, prefixed with the line and column where parsing stopped. */
		std::string GetError() const
		{
			int32 Line = 1;
			int32 Column = 1;
			for (std::size_t Index = 0; Index < Position && Index < Text.size(); ++Index)
			{
				if (Text[Index] == '\n')
				{
					++Line;
					Column = 1;
				}
				else
				{
					++Column;
				}
			}

			return std::format("line {}, column {}: {}", Line, Column, Error);
		}

	private:
		bool IsAtEnd() const
		{
			return Position >= Text.size();
		}

		char Peek() const
		{
			return IsAtEnd() ? '\0' : Text[Position];
		}

		bool Fail(std::string Message)
		{
			Error = std::move(Message);
			return false;
		}

		void SkipWhitespace()
		{
			while (!IsAtEnd())
			{
				const char Character = Text[Position];
				const bool bIsWhitespace =
					Character == ' ' || Character == '\t' || Character == '\n' || Character == '\r';
				if (!bIsWhitespace)
				{
					return;
				}
				++Position;
			}
		}

		/** Consumes Literal if the text continues with it. */
		bool TryConsume(std::string_view Literal)
		{
			if (!Text.substr(Position).starts_with(Literal))
			{
				return false;
			}

			Position += Literal.size();
			return true;
		}

		bool ParseValue(FJsonValue& OutValue, int32 Depth)
		{
			SkipWhitespace();
			if (IsAtEnd())
			{
				return Fail("expected a value but the text ends here");
			}

			const char Character = Peek();
			if (Character == '{')
			{
				return ParseObject(OutValue, Depth);
			}
			if (Character == '[')
			{
				return ParseArray(OutValue, Depth);
			}
			if (Character == '"')
			{
				std::string String;
				if (!ParseString(String))
				{
					return false;
				}
				OutValue = FJsonValue(std::move(String));
				return true;
			}
			if (Character == '-' || IsDigit(Character))
			{
				return ParseNumber(OutValue);
			}
			if (TryConsume("true"))
			{
				OutValue = FJsonValue(true);
				return true;
			}
			if (TryConsume("false"))
			{
				OutValue = FJsonValue(false);
				return true;
			}
			if (TryConsume("null"))
			{
				OutValue = FJsonValue();
				return true;
			}

			return Fail("expected a value");
		}

		bool ParseObject(FJsonValue& OutValue, int32 Depth)
		{
			if (Depth >= MaxNestingDepth)
			{
				return Fail(std::format("the document is nested more than {} levels deep", MaxNestingDepth));
			}

			++Position;
			FJsonValue Object = FJsonValue::MakeObject();

			SkipWhitespace();
			if (Peek() == '}')
			{
				++Position;
				OutValue = std::move(Object);
				return true;
			}

			while (true)
			{
				SkipWhitespace();
				if (Peek() != '"')
				{
					return Fail("expected a string for the member's key");
				}

				const std::size_t KeyPosition = Position;
				std::string Key;
				if (!ParseString(Key))
				{
					return false;
				}
				if (Object.FindMember(Key) != nullptr)
				{
					Position = KeyPosition;
					return Fail(std::format("the key \"{}\" appears twice in the same object", Key));
				}

				SkipWhitespace();
				if (Peek() != ':')
				{
					return Fail("expected ':' after the member's key");
				}
				++Position;

				FJsonValue Value;
				if (!ParseValue(Value, Depth + 1))
				{
					return false;
				}
				Object.SetMember(std::move(Key), std::move(Value));

				SkipWhitespace();
				if (Peek() == ',')
				{
					++Position;
					continue;
				}
				if (Peek() == '}')
				{
					++Position;
					OutValue = std::move(Object);
					return true;
				}

				return Fail("expected ',' or '}' after the member");
			}
		}

		bool ParseArray(FJsonValue& OutValue, int32 Depth)
		{
			if (Depth >= MaxNestingDepth)
			{
				return Fail(std::format("the document is nested more than {} levels deep", MaxNestingDepth));
			}

			++Position;
			FJsonValue Array = FJsonValue::MakeArray();

			SkipWhitespace();
			if (Peek() == ']')
			{
				++Position;
				OutValue = std::move(Array);
				return true;
			}

			while (true)
			{
				FJsonValue Element;
				if (!ParseValue(Element, Depth + 1))
				{
					return false;
				}
				Array.AddElement(std::move(Element));

				SkipWhitespace();
				if (Peek() == ',')
				{
					++Position;
					continue;
				}
				if (Peek() == ']')
				{
					++Position;
					OutValue = std::move(Array);
					return true;
				}

				return Fail("expected ',' or ']' after the element");
			}
		}

		/** Reads the four hexadecimal digits of a \u escape. */
		bool ParseHexQuad(uint32& OutValue)
		{
			if (Text.size() - Position < 4)
			{
				return Fail("a \\u escape needs four hexadecimal digits");
			}

			uint32 Value = 0;
			const char* const First = Text.data() + Position;
			const std::from_chars_result Result = std::from_chars(First, First + 4, Value, 16);
			if (Result.ec != std::errc() || Result.ptr != First + 4)
			{
				return Fail("a \\u escape needs four hexadecimal digits");
			}

			Position += 4;
			OutValue = Value;
			return true;
		}

		bool ParseUnicodeEscape(std::string& Output)
		{
			uint32 CodePoint = 0;
			if (!ParseHexQuad(CodePoint))
			{
				return false;
			}

			const bool bIsLowSurrogate = CodePoint >= 0xDC00 && CodePoint <= 0xDFFF;
			if (bIsLowSurrogate)
			{
				return Fail("a \\u escape holds the second half of a surrogate pair without the first");
			}

			const bool bIsHighSurrogate = CodePoint >= 0xD800 && CodePoint <= 0xDBFF;
			if (bIsHighSurrogate)
			{
				uint32 LowSurrogate = 0;
				if (!TryConsume("\\u") || !ParseHexQuad(LowSurrogate) || LowSurrogate < 0xDC00 || LowSurrogate > 0xDFFF)
				{
					return Fail("a \\u escape holds the first half of a surrogate pair without the second");
				}
				CodePoint = 0x10000 + ((CodePoint - 0xD800) << 10) + (LowSurrogate - 0xDC00);
			}

			// What is left is a character: a lone half of a pair was refused, and a pair was put together.
			FUtf8::AppendCharacter(Output, static_cast<char32_t>(CodePoint));
			return true;
		}

		bool ParseString(std::string& OutString)
		{
			++Position;
			std::string Result;

			while (true)
			{
				if (IsAtEnd())
				{
					return Fail("the string is not closed with '\"'");
				}

				const char Character = Text[Position];
				if (Character == '"')
				{
					++Position;
					OutString = std::move(Result);
					return true;
				}
				if (static_cast<unsigned char>(Character) < 0x20)
				{
					return Fail("a control character inside a string must be written as an escape");
				}
				if (Character != '\\')
				{
					Result.push_back(Character);
					++Position;
					continue;
				}

				++Position;
				if (IsAtEnd())
				{
					return Fail("the string is not closed with '\"'");
				}

				const char Escape = Text[Position];
				++Position;
				switch (Escape)
				{
					case '"':
						Result.push_back('"');
						break;
					case '\\':
						Result.push_back('\\');
						break;
					case '/':
						Result.push_back('/');
						break;
					case 'b':
						Result.push_back('\b');
						break;
					case 'f':
						Result.push_back('\f');
						break;
					case 'n':
						Result.push_back('\n');
						break;
					case 'r':
						Result.push_back('\r');
						break;
					case 't':
						Result.push_back('\t');
						break;
					case 'u':
						if (!ParseUnicodeEscape(Result))
						{
							return false;
						}
						break;
					default:
						--Position;
						return Fail(std::format("'\\{}' is not a valid escape", Escape));
				}
			}
		}

		bool ParseNumber(FJsonValue& OutValue)
		{
			const std::size_t Start = Position;
			if (Peek() == '-')
			{
				++Position;
			}

			// The integer part: a single zero, or digits that do not start with zero.
			if (Peek() == '0')
			{
				++Position;
				if (IsDigit(Peek()))
				{
					return Fail("a number must not have leading zeros");
				}
			}
			else if (IsDigit(Peek()))
			{
				ConsumeDigits();
			}
			else
			{
				return Fail("expected a digit");
			}

			bool bIsInteger = true;
			if (Peek() == '.')
			{
				bIsInteger = false;
				++Position;
				if (!IsDigit(Peek()))
				{
					return Fail("expected a digit after the decimal point");
				}
				ConsumeDigits();
			}
			if (Peek() == 'e' || Peek() == 'E')
			{
				bIsInteger = false;
				++Position;
				if (Peek() == '+' || Peek() == '-')
				{
					++Position;
				}
				if (!IsDigit(Peek()))
				{
					return Fail("expected a digit in the exponent");
				}
				ConsumeDigits();
			}

			const char* const First = Text.data() + Start;
			const char* const Last = Text.data() + Position;

			if (bIsInteger)
			{
				int64 IntegerValue = 0;
				const std::from_chars_result Result = std::from_chars(First, Last, IntegerValue);
				if (Result.ec == std::errc())
				{
					OutValue = FJsonValue(IntegerValue);
					return true;
				}

				uint64 UnsignedValue = 0;
				const std::from_chars_result UnsignedResult = std::from_chars(First, Last, UnsignedValue);
				if (UnsignedResult.ec == std::errc())
				{
					OutValue = FJsonValue(UnsignedValue);
					return true;
				}
				// An integer too large for 64 bits is kept as the nearest double below.
			}

			double DoubleValue = 0.0;
			const std::from_chars_result Result = std::from_chars(First, Last, DoubleValue);
			if (Result.ec != std::errc())
			{
				Position = Start;
				return Fail("the number is too large or too small to represent");
			}

			OutValue = FJsonValue(DoubleValue);
			return true;
		}

		void ConsumeDigits()
		{
			while (IsDigit(Peek()))
			{
				++Position;
			}
		}

		std::string_view Text;
		std::size_t Position = 0;
		std::string Error;
	};
}

bool FJsonReader::Parse(std::string_view Text, FJsonValue& OutValue, std::string& OutError)
{
	FJsonParser Parser(Text);
	if (!Parser.ParseDocument(OutValue))
	{
		OutError = Parser.GetError();
		return false;
	}

	return true;
}
