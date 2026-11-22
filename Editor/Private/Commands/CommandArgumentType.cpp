#include "Commands/CommandArgumentType.h"

std::string_view GetCommandArgumentTypeName(ECommandArgumentType Type)
{
	switch (Type)
	{
		case ECommandArgumentType::Boolean:
			return "Boolean";
		case ECommandArgumentType::Integer:
			return "Integer";
		case ECommandArgumentType::Number:
			return "Number";
		case ECommandArgumentType::String:
			return "String";
		case ECommandArgumentType::Json:
			return "Json";
		default:
			return "Unknown";
	}
}
