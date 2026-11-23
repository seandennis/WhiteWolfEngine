#include "Settings/EditorSettings.h"

#include "Core/FileHelper.h"
#include "Json/JsonReader.h"
#include "Json/JsonValue.h"
#include "Json/JsonWriter.h"
#include "Platform/UserDirectories.h"

#include <algorithm>
#include <cstddef>
#include <format>
#include <string_view>
#include <system_error>
#include <utility>

namespace
{
	constexpr std::string_view FormatName = "WhiteWolfEditorSettings";

	/** The engine's own directory below the one that the system has for settings, and the editor's file in it. */
	constexpr const char* UserDirectoryName = "WhiteWolfEngine";
	constexpr const char* UserFileName = "Editor.json";

	/** Returns a file with its whole path and without steps back and forth in it: as a file is remembered. */
	std::filesystem::path MakeWholePath(const std::filesystem::path& File)
	{
		std::error_code ErrorCode;
		const std::filesystem::path Whole = std::filesystem::absolute(File, ErrorCode);
		// Without a current directory to start from there is no whole path to be had, and the path stays as it is.
		return (ErrorCode ? File : Whole).lexically_normal();
	}

	/** Returns true if two paths lead to one file, however each of them is written. */
	bool IsSameFile(const std::filesystem::path& First, const std::filesystem::path& Second)
	{
		// Files that are both there are compared as the file system knows them, which sees through links, and
		// through capital letters where it does not tell them from small ones. An error, as for a file that is
		// gone, means that the two are not known to be one.
		std::error_code ErrorCode;
		return First == Second || std::filesystem::equivalent(First, Second, ErrorCode);
	}

	/** Returns true if a value of a settings file is a list of files: an array of strings that are not empty. */
	bool IsListOfFiles(const FJsonValue& Value)
	{
		return Value.IsArray() &&
			std::ranges::all_of(Value.GetElements(),
				[](const FJsonValue& Element) { return Element.IsString() && !Element.GetString().empty(); });
	}
}

std::filesystem::path FEditorSettings::GetUserFile()
{
	const std::filesystem::path Directory = FUserDirectories::GetSettingsDirectory();
	return Directory.empty() ? std::filesystem::path() : Directory / UserDirectoryName / UserFileName;
}

std::optional<FEditorSettings> FEditorSettings::Load(const std::filesystem::path& File, std::string& OutError)
{
	std::string Text;
	if (!FFileHelper::LoadFileToString(File, Text, OutError))
	{
		return std::nullopt;
	}

	const std::string FileText = FFileHelper::PathToUtf8(File);
	FJsonValue Document;
	std::string ParseError;
	if (!FJsonReader::Parse(Text, Document, ParseError))
	{
		OutError = std::format("'{}' is not a settings file: {}", FileText, ParseError);
		return std::nullopt;
	}

	const FJsonValue* const Format = Document.IsObject() ? Document.FindMember("Format") : nullptr;
	if (Format == nullptr || *Format != FJsonValue(FormatName))
	{
		OutError = std::format("'{}' is not a settings file: it lacks \"Format\": \"{}\"", FileText, FormatName);
		return std::nullopt;
	}
	const FJsonValue* const Version = Document.FindMember("Version");
	if (Version == nullptr || !Version->IsInteger() || Version->GetInteger() < 1)
	{
		OutError = std::format("'{}' does not say which version of the settings format it is", FileText);
		return std::nullopt;
	}
	if (Version->GetInteger() > FormatVersion)
	{
		OutError = std::format("'{}' is of version {} of the settings format, and this editor reads up to version {}",
			FileText,
			Version->GetInteger(),
			FormatVersion);
		return std::nullopt;
	}

	FEditorSettings Settings;
	// Settings that say nothing about projects are those of an editor that has opened none.
	if (const FJsonValue* const Projects = Document.FindMember("RecentProjects"))
	{
		if (!IsListOfFiles(*Projects))
		{
			OutError = std::format("'{}' does not give the recent projects as a list of their files", FileText);
			return std::nullopt;
		}

		// The files are taken as they are written. More of them than this build remembers were written by
		// another, and the ones that were opened longest ago are left out.
		for (const FJsonValue& Project : Projects->GetElements())
		{
			if (Settings.RecentProjects.size() < static_cast<std::size_t>(MaxRecentProjectCount))
			{
				Settings.RecentProjects.push_back(FFileHelper::PathFromUtf8(Project.GetString()));
			}
		}
	}
	return Settings;
}

bool FEditorSettings::Save(const std::filesystem::path& File, std::string& OutError) const
{
	FJsonValue Projects = FJsonValue::MakeArray();
	for (const std::filesystem::path& Project : RecentProjects)
	{
		Projects.AddElement(FFileHelper::PathToUtf8(Project));
	}

	FJsonValue Document = FJsonValue::MakeObject();
	Document.SetMember("Format", FormatName);
	Document.SetMember("Version", FormatVersion);
	Document.SetMember("RecentProjects", std::move(Projects));
	return FFileHelper::SaveStringToFile(FJsonWriter::Write(Document) + "\n", File, OutError);
}

void FEditorSettings::AddRecentProject(const std::filesystem::path& ProjectFile)
{
	WW_CHECKF(!ProjectFile.empty(), "A project without a file cannot be remembered");

	std::filesystem::path File = MakeWholePath(ProjectFile);
	std::erase_if(RecentProjects, [&File](const std::filesystem::path& Other) { return IsSameFile(Other, File); });
	RecentProjects.insert(RecentProjects.begin(), std::move(File));
	if (RecentProjects.size() > static_cast<std::size_t>(MaxRecentProjectCount))
	{
		RecentProjects.resize(static_cast<std::size_t>(MaxRecentProjectCount));
	}
}
