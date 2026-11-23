#pragma once

#include "CoreMinimal.h"

#include <filesystem>

/**
 * The directories that the operating system keeps for the person who is logged in. A program that remembers
 * something from one run to the next, or that suggests where to put what its user makes, starts from these.
 *
 * Nothing is created: a directory that is returned is where the system says it is, whether or not it exists.
 */
class FUserDirectories
{
public:
	/**
	 * Returns the directory in which programs keep what they remember for the user on this machine: the local
	 * application data on Windows, and $XDG_CONFIG_HOME, or .config in the home directory, elsewhere. A program
	 * keeps its files in a directory of its own below it.
	 *
	 * @return The directory, or an empty path if the system does not say where it is.
	 */
	ENGINE_API static std::filesystem::path GetSettingsDirectory();

	/**
	 * Returns the directory in which the user keeps documents: the Documents folder on Windows, wherever the user
	 * has moved it to, and Documents in the home directory elsewhere.
	 *
	 * @return The directory, or an empty path if the system does not say where it is.
	 */
	ENGINE_API static std::filesystem::path GetDocumentsDirectory();
};
