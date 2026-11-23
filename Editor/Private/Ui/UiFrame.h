#pragma once

#include "CoreMinimal.h"

class FEditor;
class FUiItems;
class FUiTextures;
struct FUiRequests;
struct FUiWorkspace;

/** What the parts of the user interface work with while a frame is laid out. */
struct FUiFrame
{
	/** The editor whose scene is shown, and which is given the commands that the widgets stand for. */
	FEditor& Editor;

	/** Where the items that have names are recorded. */
	FUiItems& Items;

	/** Where the pictures that widgets show are made known. */
	FUiTextures& Textures;

	FUiWorkspace& Workspace;

	/** What the parts of the user interface ask of one another. */
	FUiRequests& Requests;
};
