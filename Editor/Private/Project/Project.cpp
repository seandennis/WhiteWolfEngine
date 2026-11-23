#include "Project/Project.h"

#include "Core/FileHelper.h"
#include "Json/JsonReader.h"
#include "Json/JsonValue.h"
#include "Json/JsonWriter.h"
#include "Platform/UserDirectories.h"

#include <algorithm>
#include <format>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
	constexpr std::string_view FormatName = "WhiteWolfProject";

	/** The directory, in a person's documents, that the editor suggests to keep projects in. */
	constexpr const char* DefaultLocationName = "White Wolf Projects";

	/** Returns true if a project may be called this: what is allowed in a file name everywhere, and not blank. */
	bool IsValidName(std::string_view Name)
	{
		const auto IsAllowed = [](char Character)
		{
			const bool bIsLetter = (Character >= 'a' && Character <= 'z') || (Character >= 'A' && Character <= 'Z');
			const bool bIsDigit = Character >= '0' && Character <= '9';
			return bIsLetter || bIsDigit || Character == ' ' || Character == '-' || Character == '_';
		};
		const bool bHasVisibleCharacter = std::ranges::any_of(Name, [](char Character) { return Character != ' '; });
		return bHasVisibleCharacter && std::ranges::all_of(Name, IsAllowed);
	}

	/** Returns the project files in a directory, sorted. */
	std::vector<std::filesystem::path> FindProjectFiles(const std::filesystem::path& Directory)
	{
		std::vector<std::filesystem::path> Files;
		std::error_code ErrorCode;
		for (std::filesystem::directory_iterator Entry(Directory, ErrorCode);
			 !ErrorCode && Entry != std::filesystem::directory_iterator();
			 Entry.increment(ErrorCode))
		{
			if (Entry->is_regular_file(ErrorCode) && Entry->path().extension() == FProject::FileExtension)
			{
				Files.push_back(Entry->path());
			}
		}
		std::ranges::sort(Files);
		return Files;
	}
}

std::filesystem::path FProject::GetDefaultLocation()
{
	const std::filesystem::path Documents = FUserDirectories::GetDocumentsDirectory();
	return Documents.empty() ? std::filesystem::path() : Documents / DefaultLocationName;
}

std::optional<FProject> FProject::Create(
	const std::filesystem::path& Directory, const std::string& InName, std::string& OutError)
{
	if (!IsValidName(InName))
	{
		OutError = std::format("\"{}\" cannot be the name of a project: a name is made of letters, digits, spaces, "
							   "\"-\" and \"_\", and is not blank",
			InName);
		return std::nullopt;
	}
	if (Directory.empty())
	{
		OutError = "a project needs a directory";
		return std::nullopt;
	}

	const std::string DirectoryText = FFileHelper::PathToUtf8(Directory);
	std::error_code ErrorCode;
	if (std::filesystem::exists(Directory, ErrorCode) && !std::filesystem::is_directory(Directory, ErrorCode))
	{
		OutError = std::format("'{}' is a file, not a directory", DirectoryText);
		return std::nullopt;
	}
	if (!FindProjectFiles(Directory).empty())
	{
		OutError = std::format("the directory '{}' holds a project already", DirectoryText);
		return std::nullopt;
	}

	FProject Project;
	Project.Name = InName;
	Project.File = Directory / FFileHelper::PathFromUtf8(InName + std::string(FileExtension));

	FJsonValue Document = FJsonValue::MakeObject();
	Document.SetMember("Format", FormatName);
	Document.SetMember("Version", FormatVersion);
	Document.SetMember("Name", InName);
	// Writing the file creates the directory if it is not there.
	if (!FFileHelper::SaveStringToFile(FJsonWriter::Write(Document) + "\n", Project.File, OutError))
	{
		return std::nullopt;
	}

	const std::filesystem::path ContentDirectory = Project.GetContentDirectory();
	std::filesystem::create_directories(ContentDirectory, ErrorCode);
	if (ErrorCode)
	{
		OutError = std::format("cannot create the directory '{}': {}",
			FFileHelper::PathToUtf8(ContentDirectory),
			FFileHelper::DescribeError(ErrorCode));
		return std::nullopt;
	}
	return Project;
}

std::optional<FProject> FProject::Open(const std::filesystem::path& Path, std::string& OutError)
{
	const std::string PathText = FFileHelper::PathToUtf8(Path);

	std::filesystem::path ProjectFile = Path;
	std::error_code ErrorCode;
	if (std::filesystem::is_directory(Path, ErrorCode))
	{
		const std::vector<std::filesystem::path> Files = FindProjectFiles(Path);
		if (Files.empty())
		{
			OutError = std::format("the directory '{}' holds no project file ({})", PathText, FileExtension);
			return std::nullopt;
		}
		if (Files.size() > 1)
		{
			OutError = std::format("the directory '{}' holds {} project files; say which one", PathText, Files.size());
			return std::nullopt;
		}
		ProjectFile = Files.front();
	}

	std::string Text;
	if (!FFileHelper::LoadFileToString(ProjectFile, Text, OutError))
	{
		return std::nullopt;
	}

	const std::string FileText = FFileHelper::PathToUtf8(ProjectFile);
	FJsonValue Document;
	std::string ParseError;
	if (!FJsonReader::Parse(Text, Document, ParseError))
	{
		OutError = std::format("'{}' is not a project file: {}", FileText, ParseError);
		return std::nullopt;
	}

	const FJsonValue* const Format = Document.IsObject() ? Document.FindMember("Format") : nullptr;
	if (Format == nullptr || *Format != FJsonValue(FormatName))
	{
		OutError = std::format("'{}' is not a project file: it lacks \"Format\": \"{}\"", FileText, FormatName);
		return std::nullopt;
	}
	const FJsonValue* const Version = Document.FindMember("Version");
	if (Version == nullptr || !Version->IsInteger() || Version->GetInteger() < 1)
	{
		OutError = std::format("'{}' does not say which version of the project format it is", FileText);
		return std::nullopt;
	}
	if (Version->GetInteger() > FormatVersion)
	{
		OutError = std::format("'{}' is of version {} of the project format, and this editor reads up to version {}",
			FileText,
			Version->GetInteger(),
			FormatVersion);
		return std::nullopt;
	}
	const FJsonValue* const NameValue = Document.FindMember("Name");
	if (NameValue == nullptr || !NameValue->IsString() || NameValue->GetString().empty())
	{
		OutError = std::format("'{}' does not give the project a name", FileText);
		return std::nullopt;
	}

	FProject Project;
	Project.Name = NameValue->GetString();
	Project.File = std::move(ProjectFile);
	return Project;
}

std::filesystem::path FProject::GetDirectory() const
{
	return File.parent_path();
}

std::filesystem::path FProject::GetContentDirectory() const
{
	return GetDirectory() / FFileHelper::PathFromUtf8(ContentDirectoryName);
}
