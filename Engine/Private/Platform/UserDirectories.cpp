#include "Platform/UserDirectories.h"

#if defined(_WIN32)
#include <objbase.h>
#include <shlobj.h>
#include <windows.h>
#else
#include <cstdlib>
#endif

namespace
{
#if defined(_WIN32)
	/** Returns one of the folders that Windows knows by an identifier, or an empty path if it has none. */
	std::filesystem::path GetKnownFolder(const KNOWNFOLDERID& FolderId)
	{
		// The system allocates the text, also when it fails, and it is ours to free.
		PWSTR Text = nullptr;
		const HRESULT Result = SHGetKnownFolderPath(FolderId, KF_FLAG_DEFAULT, nullptr, &Text);
		std::filesystem::path Folder;
		if (SUCCEEDED(Result) && Text != nullptr)
		{
			Folder = Text;
		}
		CoTaskMemFree(Text);
		return Folder;
	}
#else
	/** Returns what an environment variable names, as a path; empty if it is not set or set to nothing. */
	std::filesystem::path GetEnvironmentPath(const char* Name)
	{
		const char* const Value = std::getenv(Name);
		return Value != nullptr ? std::filesystem::path(Value) : std::filesystem::path();
	}
#endif
}

std::filesystem::path FUserDirectories::GetSettingsDirectory()
{
#if defined(_WIN32)
	return GetKnownFolder(FOLDERID_LocalAppData);
#else
	// The XDG Base Directory Specification: the variable if it names a directory, and otherwise ~/.config.
	const std::filesystem::path Configured = GetEnvironmentPath("XDG_CONFIG_HOME");
	if (Configured.is_absolute())
	{
		return Configured;
	}
	const std::filesystem::path Home = GetEnvironmentPath("HOME");
	return Home.empty() ? std::filesystem::path() : Home / ".config";
#endif
}

std::filesystem::path FUserDirectories::GetDocumentsDirectory()
{
#if defined(_WIN32)
	return GetKnownFolder(FOLDERID_Documents);
#else
	const std::filesystem::path Home = GetEnvironmentPath("HOME");
	return Home.empty() ? std::filesystem::path() : Home / "Documents";
#endif
}
