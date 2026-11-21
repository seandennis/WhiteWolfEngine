#include "HeaderTool.h"

#include "CodeGenerator.h"
#include "HeaderParser.h"
#include "ReflectedHeader.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <map>
#include <ostream>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
	constexpr int32 ExitSuccess = 0;
	constexpr int32 ExitHeaderError = 1;
	constexpr int32 ExitUsageError = 2;

	constexpr std::string_view Usage = "usage: WhiteWolfHeaderTool --output <file> --root <dir> [--root <dir>...]";

	/** Returns a path as UTF-8 text with forward slashes, which is how paths are printed and written to #includes. */
	std::string ToUtf8(const std::filesystem::path& Path)
	{
		const std::u8string Text = Path.generic_u8string();
		return std::string(Text.begin(), Text.end());
	}

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

	/** Collects every header below Root, sorted so that the output does not depend on the file system's order. */
	bool FindHeaders(
		const std::filesystem::path& Root, std::vector<std::filesystem::path>& OutHeaders, std::ostream& ErrorStream)
	{
		std::error_code Error;
		std::filesystem::recursive_directory_iterator Iterator(Root, Error);
		if (Error)
		{
			ErrorStream << std::format("error: cannot read the directory '{}': {}\n", ToUtf8(Root), Error.message());
			return false;
		}

		const std::filesystem::recursive_directory_iterator End;
		while (Iterator != End)
		{
			const std::filesystem::directory_entry& Entry = *Iterator;
			std::error_code EntryError;
			if (Entry.is_regular_file(EntryError) && Entry.path().extension() == ".h")
			{
				OutHeaders.push_back(Entry.path());
			}

			Iterator.increment(Error);
			if (Error)
			{
				ErrorStream << std::format(
					"error: cannot read the directory '{}': {}\n", ToUtf8(Root), Error.message());
				return false;
			}
		}

		std::ranges::sort(OutHeaders,
			[](const std::filesystem::path& Left, const std::filesystem::path& Right)
			{ return Left.generic_u8string() < Right.generic_u8string(); });
		return true;
	}

	bool ReadFile(const std::filesystem::path& Path, std::string& OutText)
	{
		std::ifstream Stream(Path, std::ios::binary);
		if (!Stream)
		{
			return false;
		}

		OutText.assign(std::istreambuf_iterator<char>(Stream), std::istreambuf_iterator<char>());
		return !Stream.bad();
	}

	/**
	 * Writes Text to the file at Path, unless the file already holds exactly that text. Leaving an unchanged file
	 * alone keeps its modification time, so the build does not compile it again.
	 */
	bool WriteFileIfChanged(const std::filesystem::path& Path, std::string_view Text)
	{
		std::string ExistingText;
		if (ReadFile(Path, ExistingText) && ExistingText == Text)
		{
			return true;
		}

		std::error_code Error;
		std::filesystem::create_directories(Path.parent_path(), Error);
		if (Error)
		{
			return false;
		}

		std::ofstream Stream(Path, std::ios::binary | std::ios::trunc);
		Stream.write(Text.data(), static_cast<std::streamsize>(Text.size()));
		Stream.flush();
		return Stream.good();
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
		if (!FindHeaders(Root, HeaderPaths, ErrorStream))
		{
			return ExitUsageError;
		}

		for (const std::filesystem::path& HeaderPath : HeaderPaths)
		{
			std::string Source;
			if (!ReadFile(HeaderPath, Source))
			{
				ErrorStream << std::format("error: cannot read '{}'\n", ToUtf8(HeaderPath));
				return ExitUsageError;
			}

			FReflectedHeader Header;
			std::vector<FParseError> Errors;
			if (!FHeaderParser::Parse(Source, Header.Classes, Errors))
			{
				for (const FParseError& Error : Errors)
				{
					ErrorStream << std::format("{}({}): error: {}\n", ToUtf8(HeaderPath), Error.Line, Error.Message);
				}
				bHasHeaderErrors = true;
				continue;
			}

			for (const FReflectedClass& Class : Header.Classes)
			{
				const std::string Location = std::format("{}({})", ToUtf8(HeaderPath), Class.Line);
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

			Header.IncludePath = ToUtf8(HeaderPath.lexically_relative(Root));
			Headers.push_back(std::move(Header));
		}
	}

	if (bHasHeaderErrors)
	{
		return ExitHeaderError;
	}

	if (!WriteFileIfChanged(Options.OutputFile, FCodeGenerator::Generate(Headers)))
	{
		ErrorStream << std::format("error: cannot write '{}'\n", ToUtf8(Options.OutputFile));
		return ExitUsageError;
	}

	return ExitSuccess;
}
