#pragma once

#include "CoreMinimal.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

/**
 * A game project: a directory with a project file and, in its "Content" directory, the game's assets and scenes.
 *
 *     MyGame/
 *       MyGame.wwproject     the project file: {"Format": "WhiteWolfProject", "Version": 1, "Name": "My Game"}
 *       Content/             what asset paths are relative to: models, pictures, scenes
 *
 * The editor works on one project at a time; opening one makes its content directory the engine's.
 */
class FProject
{
public:
	/** The end of a project file's name. */
	static constexpr std::string_view FileExtension = ".wwproject";

	/** The name of the directory, inside the project's directory, that holds its content. */
	static constexpr std::string_view ContentDirectoryName = "Content";

	/** The format version this build writes, and the newest one it reads. */
	static constexpr int32 FormatVersion = 1;

	/**
	 * Returns the directory that the editor suggests for a person's first project, to make the project's own
	 * directory in: one of the engine's in the documents of the person who is logged in
	 * (FUserDirectories::GetDocumentsDirectory). Nothing is created.
	 *
	 * @return The directory, whether or not it exists, or an empty path if the system does not say where a
	 * person's documents are.
	 */
	EDITOR_API static std::filesystem::path GetDefaultLocation();

	/**
	 * Creates a project: the directory, if it does not exist, the project file in it and the content directory.
	 *
	 * @param Directory Where the project is to be. It must not hold a project already.
	 * @param InName The project's name, which also names the project file: letters, digits, spaces, "-" and "_", and
	 * at least one that is not a space.
	 * @param OutError Receives the reason on failure.
	 */
	EDITOR_API static std::optional<FProject> Create(
		const std::filesystem::path& Directory, const std::string& InName, std::string& OutError);

	/**
	 * Opens a project.
	 *
	 * @param Path The project file, or the project's directory if it holds exactly one project file.
	 * @param OutError Receives the reason on failure: there is no such project, or its file is not a project file
	 * of a version this build reads.
	 */
	EDITOR_API static std::optional<FProject> Open(const std::filesystem::path& Path, std::string& OutError);

	const std::string& GetName() const
	{
		return Name;
	}

	/** Returns the project file. */
	const std::filesystem::path& GetFile() const
	{
		return File;
	}

	/** Returns the directory the project file is in. */
	EDITOR_API std::filesystem::path GetDirectory() const;

	/** Returns the directory that asset paths of this project are relative to. */
	EDITOR_API std::filesystem::path GetContentDirectory() const;

private:
	FProject() = default;

	std::string Name;
	std::filesystem::path File;
};
