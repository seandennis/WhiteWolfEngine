#pragma once

#include "CoreMinimal.h"

#include "Assets/AssetManager.h"
#include "Assets/EditedAssets.h"
#include "Assets/TestAssets.h"
#include "Core/FileHelper.h"
#include "Editor.h"
#include "Object/Class.h"
#include "Reflection/Property.h"
#include "Rendering/Material.h"
#include "TestEditorObjects.h"

#include <doctest/doctest.h>

#include <string>
#include <string_view>

/** Returns the property of a material with the given name, which a test expects it to have. */
inline const FProperty& FindMaterialProperty(std::string_view Name)
{
	const FProperty* const Property = WMaterial::StaticClass()->FindProperty(Name);
	REQUIRE(Property != nullptr);
	return *Property;
}

/** Loads a material that a test expects to load. */
inline WMaterial& LoadTestMaterial(std::string_view AssetPath)
{
	std::string Error;
	WMaterial* const Material = FAssetManager::Load<WMaterial>(AssetPath, Error);
	INFO("Error: ", Error);
	REQUIRE(Material != nullptr);
	return *Material;
}

/**
 * Gives a property of a material a value through the editor's FEditedAssets, which the test expects to be taken.
 *
 * @param ValueText The value as JSON text.
 */
inline void ChangeMaterial(
	FEditor& Editor, WMaterial& Material, std::string_view PropertyName, std::string_view ValueText)
{
	std::string Error;
	const bool bIsSet = Editor.GetEditedAssets().SetProperty(
		Material, FindMaterialProperty(PropertyName), ParseTestJson(ValueText), Error);
	INFO("Error: ", Error);
	REQUIRE(bIsSet);
}

/** Returns the text of a file of a content directory, which a test expects to be there. */
inline std::string ReadContentFile(const FScopedContentDirectory& Content, std::string_view File)
{
	std::string Text;
	std::string Error;
	const bool bIsRead =
		FFileHelper::LoadFileToString(Content.GetPath() / FFileHelper::PathFromUtf8(File), Text, Error);
	INFO("Error: ", Error);
	REQUIRE(bIsRead);
	return Text;
}
