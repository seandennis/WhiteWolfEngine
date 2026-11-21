#include "Json/JsonWriter.h"

#include "Json/JsonValue.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <format>
#include <string_view>
#include <system_error>

namespace
{
	constexpr std::string_view IndentUnit = "  ";

	void AppendNewLine(std::string& Output, int32 IndentLevel)
	{
		Output.push_back('\n');
		for (int32 Level = 0; Level < IndentLevel; ++Level)
		{
			Output.append(IndentUnit);
		}
	}

	void AppendString(std::string& Output, std::string_view String)
	{
		Output.push_back('"');
		for (const char Character : String)
		{
			switch (Character)
			{
				case '"':
					Output.append("\\\"");
					break;
				case '\\':
					Output.append("\\\\");
					break;
				case '\b':
					Output.append("\\b");
					break;
				case '\f':
					Output.append("\\f");
					break;
				case '\n':
					Output.append("\\n");
					break;
				case '\r':
					Output.append("\\r");
					break;
				case '\t':
					Output.append("\\t");
					break;
				default:
					if (static_cast<unsigned char>(Character) < 0x20)
					{
						Output.append(
							std::format("\\u{:04x}", static_cast<uint32>(static_cast<unsigned char>(Character))));
					}
					else
					{
						Output.push_back(Character);
					}
					break;
			}
		}
		Output.push_back('"');
	}

	void AppendDouble(std::string& Output, double Value)
	{
		if (!std::isfinite(Value))
		{
			Output.append("null");
			return;
		}

		std::array<char, 32> Buffer{};
		const std::to_chars_result Result = std::to_chars(Buffer.data(), Buffer.data() + Buffer.size(), Value);
		WW_CHECK(Result.ec == std::errc());

		const std::string_view Digits(Buffer.data(), static_cast<std::size_t>(Result.ptr - Buffer.data()));
		Output.append(Digits);

		// "3" would be read back as an integer; "3.0" stays a double.
		const bool bLooksLikeInteger = Digits.find_first_of(".eE") == std::string_view::npos;
		if (bLooksLikeInteger)
		{
			Output.append(".0");
		}
	}

	bool IsContainer(const FJsonValue& Value)
	{
		return Value.IsArray() || Value.IsObject();
	}

	void AppendValue(std::string& Output, const FJsonValue& Value, EJsonFormat Format, int32 IndentLevel);

	void AppendArray(std::string& Output, const FJsonValue::FArray& Elements, EJsonFormat Format, int32 IndentLevel)
	{
		if (Elements.empty())
		{
			Output.append("[]");
			return;
		}

		const bool bIsPretty = Format == EJsonFormat::Pretty;
		const bool bOneElementPerLine = bIsPretty && std::ranges::any_of(Elements, &IsContainer);

		Output.push_back('[');
		for (std::size_t Index = 0; Index < Elements.size(); ++Index)
		{
			if (Index > 0)
			{
				Output.push_back(',');
				if (bIsPretty && !bOneElementPerLine)
				{
					Output.push_back(' ');
				}
			}
			if (bOneElementPerLine)
			{
				AppendNewLine(Output, IndentLevel + 1);
			}
			AppendValue(Output, Elements[Index], Format, IndentLevel + 1);
		}
		if (bOneElementPerLine)
		{
			AppendNewLine(Output, IndentLevel);
		}
		Output.push_back(']');
	}

	void AppendObject(std::string& Output, const FJsonValue::FObject& Members, EJsonFormat Format, int32 IndentLevel)
	{
		if (Members.empty())
		{
			Output.append("{}");
			return;
		}

		const bool bIsPretty = Format == EJsonFormat::Pretty;

		Output.push_back('{');
		for (std::size_t Index = 0; Index < Members.size(); ++Index)
		{
			if (Index > 0)
			{
				Output.push_back(',');
			}
			if (bIsPretty)
			{
				AppendNewLine(Output, IndentLevel + 1);
			}
			AppendString(Output, Members[Index].first);
			Output.append(bIsPretty ? ": " : ":");
			AppendValue(Output, Members[Index].second, Format, IndentLevel + 1);
		}
		if (bIsPretty)
		{
			AppendNewLine(Output, IndentLevel);
		}
		Output.push_back('}');
	}

	void AppendValue(std::string& Output, const FJsonValue& Value, EJsonFormat Format, int32 IndentLevel)
	{
		if (Value.IsNull())
		{
			Output.append("null");
		}
		else if (Value.IsBoolean())
		{
			Output.append(Value.GetBoolean() ? "true" : "false");
		}
		else if (Value.IsInteger())
		{
			Output.append(std::format("{}", Value.GetInteger()));
		}
		else if (Value.IsNumber())
		{
			AppendDouble(Output, Value.GetNumber());
		}
		else if (Value.IsString())
		{
			AppendString(Output, Value.GetString());
		}
		else if (Value.IsArray())
		{
			AppendArray(Output, Value.GetElements(), Format, IndentLevel);
		}
		else
		{
			AppendObject(Output, Value.GetMembers(), Format, IndentLevel);
		}
	}
}

std::string FJsonWriter::Write(const FJsonValue& Value, EJsonFormat Format)
{
	std::string Output;
	AppendValue(Output, Value, Format, 0);
	return Output;
}
