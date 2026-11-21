#include "Tokenizer.h"

#include <cstddef>
#include <string>
#include <utility>

namespace
{
	bool IsIdentifierStart(char Character)
	{
		return (Character >= 'a' && Character <= 'z') || (Character >= 'A' && Character <= 'Z') || Character == '_';
	}

	bool IsDigit(char Character)
	{
		return Character >= '0' && Character <= '9';
	}

	bool IsIdentifierPart(char Character)
	{
		return IsIdentifierStart(Character) || IsDigit(Character);
	}

	/** Walks over the source one construct at a time. */
	class FScanner
	{
	public:
		explicit FScanner(std::string_view InSource)
			: Source(InSource)
		{
		}

		std::vector<FToken> Scan()
		{
			while (Position < Source.size())
			{
				const char Character = Source[Position];
				if (Character == '\n')
				{
					++Line;
					++Position;
					bIsAtLineStart = true;
				}
				else if (Character == ' ' || Character == '\t' || Character == '\r' || Character == '\f' ||
					Character == '\v')
				{
					++Position;
				}
				else if (StartsWith("//"))
				{
					SkipLineComment();
				}
				else if (StartsWith("/*"))
				{
					SkipBlockComment();
				}
				else if (Character == '#' && bIsAtLineStart)
				{
					SkipDirective();
				}
				else
				{
					bIsAtLineStart = false;
					ScanToken();
				}
			}

			return std::move(Tokens);
		}

	private:
		bool StartsWith(std::string_view Text) const
		{
			return Source.substr(Position).starts_with(Text);
		}

		char PeekAt(std::size_t Offset) const
		{
			return Position + Offset < Source.size() ? Source[Position + Offset] : '\0';
		}

		void SkipLineComment()
		{
			while (Position < Source.size() && Source[Position] != '\n')
			{
				// A backslash at the end of the line continues the comment on the next line.
				if (Source[Position] == '\\' && PeekAt(1) == '\n')
				{
					++Line;
					++Position;
				}
				++Position;
			}
		}

		void SkipBlockComment()
		{
			Position += 2;
			while (Position < Source.size() && !StartsWith("*/"))
			{
				if (Source[Position] == '\n')
				{
					++Line;
				}
				++Position;
			}
			Position = Position < Source.size() ? Position + 2 : Source.size();
		}

		/** Skips a preprocessor directive, including the lines it continues onto with a backslash. */
		void SkipDirective()
		{
			while (Position < Source.size() && Source[Position] != '\n')
			{
				if (Source[Position] == '\\' && PeekAt(1) == '\n')
				{
					++Line;
					Position += 2;
				}
				else if (Source[Position] == '\\' && PeekAt(1) == '\r' && PeekAt(2) == '\n')
				{
					++Line;
					Position += 3;
				}
				else if (StartsWith("/*"))
				{
					SkipBlockComment();
				}
				else if (StartsWith("//"))
				{
					SkipLineComment();
				}
				else
				{
					++Position;
				}
			}
		}

		void AddToken(ETokenType Type, std::size_t Start, int32 StartLine)
		{
			Tokens.push_back(
				FToken{.Type = Type, .Text = std::string(Source.substr(Start, Position - Start)), .Line = StartLine});
		}

		void ScanToken()
		{
			const std::size_t Start = Position;
			const int32 StartLine = Line;
			const char Character = Source[Position];

			if (IsIdentifierStart(Character))
			{
				while (Position < Source.size() && IsIdentifierPart(Source[Position]))
				{
					++Position;
				}

				// An identifier directly followed by a quote is the prefix of a literal: u8"text", L'c', R"(raw)".
				const bool bIsRawPrefix = Source[Position - 1] == 'R';
				if (Position < Source.size() && Source[Position] == '"')
				{
					if (bIsRawPrefix)
					{
						ScanRawString();
					}
					else
					{
						ScanQuoted('"');
					}
					AddToken(ETokenType::String, Start, StartLine);
				}
				else if (Position < Source.size() && Source[Position] == '\'')
				{
					ScanQuoted('\'');
					AddToken(ETokenType::Character, Start, StartLine);
				}
				else
				{
					AddToken(ETokenType::Identifier, Start, StartLine);
				}
			}
			else if (IsDigit(Character) || (Character == '.' && IsDigit(PeekAt(1))))
			{
				ScanNumber();
				AddToken(ETokenType::Number, Start, StartLine);
			}
			else if (Character == '"')
			{
				ScanQuoted('"');
				AddToken(ETokenType::String, Start, StartLine);
			}
			else if (Character == '\'')
			{
				ScanQuoted('\'');
				AddToken(ETokenType::Character, Start, StartLine);
			}
			else
			{
				Position += StartsWith("::") ? 2 : 1;
				AddToken(ETokenType::Punctuation, Start, StartLine);
			}
		}

		/** Scans a number, including digit separators, suffixes, hexadecimal digits and exponents. */
		void ScanNumber()
		{
			while (Position < Source.size())
			{
				const char Character = Source[Position];
				const bool bIsExponentSign = (Character == '+' || Character == '-') && Position > 0 &&
					(Source[Position - 1] == 'e' || Source[Position - 1] == 'E' || Source[Position - 1] == 'p' ||
						Source[Position - 1] == 'P');
				const bool bIsSeparator = Character == '\'' && IsIdentifierPart(PeekAt(1));
				if (!IsIdentifierPart(Character) && Character != '.' && !bIsExponentSign && !bIsSeparator)
				{
					return;
				}
				++Position;
			}
		}

		/** Scans a string or character literal that starts at the current position with Quote. */
		void ScanQuoted(char Quote)
		{
			++Position;
			while (Position < Source.size() && Source[Position] != Quote)
			{
				if (Source[Position] == '\\' && Position + 1 < Source.size())
				{
					++Position;
				}
				if (Source[Position] == '\n')
				{
					++Line;
				}
				++Position;
			}
			Position = Position < Source.size() ? Position + 1 : Source.size();
		}

		/** Scans a raw string literal, whose opening quote is at the current position: "delimiter(text)delimiter". */
		void ScanRawString()
		{
			++Position;
			const std::size_t DelimiterStart = Position;
			while (Position < Source.size() && Source[Position] != '(' && Source[Position] != '\n')
			{
				++Position;
			}

			const std::string Terminator =
				")" + std::string(Source.substr(DelimiterStart, Position - DelimiterStart)) + "\"";
			while (Position < Source.size() && !StartsWith(Terminator))
			{
				if (Source[Position] == '\n')
				{
					++Line;
				}
				++Position;
			}
			Position = Position < Source.size() ? Position + Terminator.size() : Source.size();
		}

		std::string_view Source;
		std::size_t Position = 0;
		int32 Line = 1;

		/** True until the first token of a line; a '#' is a directive only there. */
		bool bIsAtLineStart = true;

		std::vector<FToken> Tokens;
	};
}

std::vector<FToken> FTokenizer::Tokenize(std::string_view Source)
{
	FScanner Scanner(Source);
	return Scanner.Scan();
}
