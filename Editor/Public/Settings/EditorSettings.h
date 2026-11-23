#pragma once

#include "CoreMinimal.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

/**
 * What the editor remembers from one run to the next for the person who uses it, whichever project is being worked
 * on: the projects that were opened last.
 *
 * The settings are kept as JSON text in a file:
 *
 *     {
 *       "Format": "WhiteWolfEditorSettings",
 *       "Version": 1,
 *       "RecentProjects": ["C:\\Games\\Tetris\\Tetris.wwproject", "C:\\Games\\Maze\\Maze.wwproject"]
 *     }
 */
class FEditorSettings
{
public:
	/** The format version this build writes, and the newest one it reads. */
	static constexpr int32 FormatVersion = 1;

	/** How many projects are remembered. */
	static constexpr int32 MaxRecentProjectCount = 10;

	/**
	 * Returns the file in which the editor keeps the settings of the person who is logged in: in a directory of the
	 * engine's own below the one that the system has for such files (FUserDirectories::GetSettingsDirectory).
	 *
	 * @return The file, whether or not it exists yet, or an empty path if the system does not say where it goes.
	 */
	EDITOR_API static std::filesystem::path GetUserFile();

	/**
	 * Reads settings from a file.
	 *
	 * @param OutError Receives the reason on failure: the file cannot be read, or it is not a settings file of a
	 * version that this build reads.
	 * @return The settings, or nothing on failure.
	 */
	EDITOR_API static std::optional<FEditorSettings> Load(const std::filesystem::path& File, std::string& OutError);

	/**
	 * Writes the settings to a file, creating the file's directory if needed. A file that is there already is
	 * replaced, or left as it was on failure.
	 *
	 * @param OutError Receives the reason on failure.
	 * @return True if the file now holds these settings.
	 */
	EDITOR_API bool Save(const std::filesystem::path& File, std::string& OutError) const;

	/**
	 * Returns the project files that were opened last, the most recent first, each with its whole path. A project
	 * that was moved or deleted since is still among them.
	 */
	const std::vector<std::filesystem::path>& GetRecentProjects() const
	{
		return RecentProjects;
	}

	/**
	 * Makes a project the one that was opened last: its file comes first, wherever it was among the others, and
	 * the project that was opened longest ago is forgotten if that makes more than MaxRecentProjectCount.
	 *
	 * @param ProjectFile The project file. A relative path is taken from the current directory, and remembered
	 * whole. An empty path is a fatal error.
	 */
	EDITOR_API void AddRecentProject(const std::filesystem::path& ProjectFile);

	bool operator==(const FEditorSettings& Other) const = default;

private:
	std::vector<std::filesystem::path> RecentProjects;
};
