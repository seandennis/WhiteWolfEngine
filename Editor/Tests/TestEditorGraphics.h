#pragma once

#include "CoreMinimal.h"

#include "Editor.h"
#include "EditorGraphics.h"
#include "Rendering/RenderDevice.h"
#include "Rendering/RenderDeviceDescription.h"

#include <doctest/doctest.h>

#include <string>

/**
 * Opens the render device of an editor with validation, as a test that draws through the editor needs it, and
 * returns it: the editor's own, so that the test draws beside the editor with the one render device there can be.
 */
inline FRenderDevice& OpenValidatedGraphics(FEditor& Editor)
{
	std::string Error;
	FRenderDevice* const Device = Editor.GetGraphics().Open(FRenderDeviceDescription{.bEnableValidation = true}, Error);
	INFO("Error: ", Error);
	REQUIRE(Device != nullptr);
	return *Device;
}
