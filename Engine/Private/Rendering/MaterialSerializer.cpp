#include "Rendering/MaterialSerializer.h"

#include "Assets/AssetManager.h"
#include "Assets/AssetReferenceContext.h"
#include "Core/FileHelper.h"
#include "Json/JsonReader.h"
#include "Json/JsonValue.h"
#include "Json/JsonWriter.h"
#include "Object/Class.h"
#include "Reflection/ObjectProperties.h"
#include "Reflection/Property.h"
#include "Rendering/Material.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <format>
#include <utility>

namespace
{
	constexpr std::string_view FormatName = "WhiteWolfMaterial";

	/** What a material's properties are saved with, as the sentence about a property that is not saved says it. */
	constexpr std::string_view SavedWith = "a material";
}

bool FMaterialSerializer::IsMaterialFile(const std::filesystem::path& Path)
{
	std::string Extension = FFileHelper::PathToUtf8(Path.extension());
	std::ranges::transform(Extension,
		Extension.begin(),
		[](unsigned char Character) { return static_cast<char>(std::tolower(Character)); });
	return Extension == FileExtension;
}

FJsonValue FMaterialSerializer::SaveToJson(const WMaterial& Material)
{
	FAssetReferenceContext Context;
	FJsonValue Properties = FJsonValue::MakeObject();
	for (const FProperty* const Property : FObjectProperties::GetSaved(*Material.GetClass()))
	{
		const std::size_t UnwrittenBefore = Context.GetUnwritten().size();
		Properties.SetMember(Property->GetName(), Property->ExportJson(Property->GetValuePtr(&Material), Context));

		// A reference that cannot be written is null in the file, and the material loses it when the file is read.
		for (std::size_t Index = UnwrittenBefore; Index < Context.GetUnwritten().size(); ++Index)
		{
			const std::string& Path = Material.GetAssetPath();
			WW_LOG(LogAssets,
				Warning,
				"Saving {}: property {} refers to a {} that has no asset path; the reference is saved as null",
				Path.empty() ? std::string("a material") : std::format("the material '{}'", Path),
				Property->GetName(),
				Context.GetUnwritten()[Index]);
		}
	}

	FJsonValue Document = FJsonValue::MakeObject();
	Document.SetMember("Format", FormatName);
	Document.SetMember("Version", FormatVersion);
	Document.SetMember("Properties", std::move(Properties));
	return Document;
}

std::string FMaterialSerializer::SaveToText(const WMaterial& Material)
{
	return FJsonWriter::Write(SaveToJson(Material), EJsonFormat::Pretty) + "\n";
}

bool FMaterialSerializer::SaveToFile(
	const WMaterial& Material, const std::filesystem::path& Path, std::string& OutError)
{
	return FFileHelper::SaveStringToFile(SaveToText(Material), Path, OutError);
}

bool FMaterialSerializer::LoadFromJson(
	const FJsonValue& Json, WMaterial& Material, std::string& OutError, std::vector<std::string>& OutWarnings)
{
	if (!Json.IsObject())
	{
		OutError = std::format("a material must be a JSON object, not {}", Json.GetTypeDescription());
		return false;
	}

	const FJsonValue* const FormatValue = Json.FindMember("Format");
	if (FormatValue == nullptr || !FormatValue->IsString() || FormatValue->GetString() != FormatName)
	{
		OutError = std::format("this is not a material: \"Format\" must be \"{}\"", FormatName);
		return false;
	}

	const FJsonValue* const VersionValue = Json.FindMember("Version");
	if (VersionValue == nullptr || !VersionValue->IsInteger() || VersionValue->GetInteger() < 1)
	{
		OutError = "\"Version\" must be a positive whole number";
		return false;
	}
	if (VersionValue->GetInteger() > FormatVersion)
	{
		OutError =
			std::format("the material was saved in format version {}, but this build reads only up to version {}",
				VersionValue->GetInteger(),
				FormatVersion);
		return false;
	}

	const FJsonValue* const Properties = Json.FindMember("Properties");
	if (Properties != nullptr && !Properties->IsObject())
	{
		OutError =
			std::format("\"Properties\" of the material must be an object, not {}", Properties->GetTypeDescription());
		return false;
	}

	// The document is a material. What follows changes the material, and warns of what it cannot use.
	for (const FJsonValue::FMember& Member : Json.GetMembers())
	{
		const bool bIsKnown = Member.first == "Format" || Member.first == "Version" || Member.first == "Properties";
		if (!bIsKnown)
		{
			OutWarnings.push_back(
				std::format("the material has the unknown key \"{}\", which was ignored", Member.first));
		}
	}
	if (Properties != nullptr)
	{
		FAssetReferenceContext Context;
		FObjectProperties::Import(Material, *Properties, Context, SavedWith, OutWarnings);
	}
	return true;
}

bool FMaterialSerializer::LoadFromText(
	std::string_view Text, WMaterial& Material, std::string& OutError, std::vector<std::string>& OutWarnings)
{
	FJsonValue Json;
	std::string ParseError;
	if (!FJsonReader::Parse(Text, Json, ParseError))
	{
		OutError = std::format("the material is not valid JSON: {}", ParseError);
		return false;
	}
	return LoadFromJson(Json, Material, OutError, OutWarnings);
}

bool FMaterialSerializer::LoadFromFile(const std::filesystem::path& Path,
	WMaterial& Material,
	std::string& OutError,
	std::vector<std::string>& OutWarnings)
{
	std::string Text;
	if (!FFileHelper::LoadFileToString(Path, Text, OutError))
	{
		return false;
	}

	std::string LoadError;
	if (!LoadFromText(Text, Material, LoadError, OutWarnings))
	{
		OutError = std::format("{}: {}", FFileHelper::PathToUtf8(Path), LoadError);
		return false;
	}
	return true;
}
