#include "HeaderTool.h"

#include "CodeGenerator.h"
#include "HeaderParser.h"
#include "ReflectedHeader.h"
#include "ToolFiles.h"

#include <cstddef>
#include <filesystem>
#include <format>
#include <map>
#include <ostream>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
	constexpr int32 ExitSuccess = 0;
	constexpr int32 ExitHeaderError = 1;
	constexpr int32 ExitUsageError = 2;

	/** The files the tool reads. */
	constexpr std::string_view HeaderExtensions[] = {".h"};

	constexpr std::string_view Usage = "usage: WhiteWolfHeaderTool --output <file> --root <dir> [--root <dir>...]";

	struct FArguments
	{
		std::filesystem::path OutputFile;
		std::vector<std::filesystem::path> Roots;
	};

	bool ParseArguments(std::span<const std::string> Arguments, FArguments& OutArguments, std::ostream& ErrorStream)
	{
		for (std::size_t Index = 0; Index < Arguments.size(); ++Index)
		{
			const std::string& Option = Arguments[Index];
			const bool bIsKnownOption = Option == "--output" || Option == "--root";
			if (!bIsKnownOption)
			{
				ErrorStream << std::format("error: unknown argument '{}'\n{}\n", Option, Usage);
				return false;
			}
			if (Index + 1 == Arguments.size())
			{
				ErrorStream << std::format("error: {} needs a value\n{}\n", Option, Usage);
				return false;
			}

			++Index;
			if (Option == "--output")
			{
				OutArguments.OutputFile = Arguments[Index];
			}
			else
			{
				OutArguments.Roots.emplace_back(Arguments[Index]);
			}
		}

		if (OutArguments.OutputFile.empty() || OutArguments.Roots.empty())
		{
			ErrorStream << std::format("error: --output and at least one --root are required\n{}\n", Usage);
			return false;
		}

		return true;
	}
}

int32 FHeaderTool::Run(std::span<const std::string> Arguments, std::ostream& ErrorStream)
{
	FArguments Options;
	if (!ParseArguments(Arguments, Options, ErrorStream))
	{
		return ExitUsageError;
	}

	std::vector<FReflectedHeader> Headers;
	// Where each class was declared, to report a name that is used twice.
	std::map<std::string, std::string> ClassLocations;
	bool bHasHeaderErrors = false;

	for (const std::filesystem::path& Root : Options.Roots)
	{
		std::vector<std::filesystem::path> HeaderPaths;
		std::string FindError;
		if (!FToolFiles::FindFiles(Root, HeaderExtensions, HeaderPaths, FindError))
		{
			ErrorStream << std::format("error: {}\n", FindError);
			return ExitUsageError;
		}

		for (const std::filesystem::path& HeaderPath : HeaderPaths)
		{
			std::string Source;
			if (!FToolFiles::ReadTextFile(HeaderPath, Source))
			{
				ErrorStream << std::format("error: cannot read '{}'\n", FToolFiles::ToUtf8(HeaderPath));
				return ExitUsageError;
			}

			FReflectedHeader Header;
			std::vector<FParseError> Errors;
			if (!FHeaderParser::Parse(Source, Header.Classes, Errors))
			{
				for (const FParseError& Error : Errors)
				{
					ErrorStream << std::format(
						"{}({}): error: {}\n", FToolFiles::ToUtf8(HeaderPath), Error.Line, Error.Message);
				}
				bHasHeaderErrors = true;
				continue;
			}

			for (const FReflectedClass& Class : Header.Classes)
			{
				const std::string Location = std::format("{}({})", FToolFiles::ToUtf8(HeaderPath), Class.Line);
				const auto [Existing, bIsNew] = ClassLocations.emplace(Class.Name, Location);
				if (!bIsNew)
				{
					ErrorStream << std::format("{}: error: class {} is already declared at {}; class names must be "
											   "unique\n",
						Location,
						Class.Name,
						Existing->second);
					bHasHeaderErrors = true;
				}
			}

			Header.IncludePath = FToolFiles::ToUtf8(HeaderPath.lexically_relative(Root));
			Headers.push_back(std::move(Header));
		}
	}

	if (bHasHeaderErrors)
	{
		return ExitHeaderError;
	}

	if (!FToolFiles::WriteTextFileIfChanged(Options.OutputFile, FCodeGenerator::Generate(Headers)))
	{
		ErrorStream << std::format("error: cannot write '{}'\n", FToolFiles::ToUtf8(Options.OutputFile));
		return ExitUsageError;
	}

	return ExitSuccess;
}
