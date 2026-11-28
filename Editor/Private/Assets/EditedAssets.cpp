#include "Assets/EditedAssets.h"

#include "Assets/Asset.h"
#include "Assets/AssetManager.h"
#include "Assets/AssetPath.h"
#include "Assets/AssetReferenceContext.h"
#include "Core/FileHelper.h"
#include "Editor.h"
#include "Json/JsonFormat.h"
#include "Json/JsonReader.h"
#include "Json/JsonValue.h"
#include "Json/JsonWriter.h"
#include "Object/Class.h"
#include "Object/ObjectGlobals.h"
#include "Reflection/ObjectProperties.h"
#include "Reflection/Property.h"
#include "Rendering/Material.h"
#include "Rendering/MaterialSerializer.h"

#include <algorithm>
#include <filesystem>
#include <optional>
#include <system_error>
#include <utility>

namespace
{
	/** What the properties of an asset are saved with, for the sentence about a property that is not saved. */
	constexpr std::string_view SavedWith = "an asset";

	/** Returns the saved properties of an asset as JSON, with references as asset paths. */
	FJsonValue ExportProperties(const WAsset& Asset)
	{
		FAssetReferenceContext Context;
		return FObjectProperties::Export(Asset, Context);
	}

	/** Returns the saved properties of an asset as one line of compact JSON: what is compared and written down. */
	std::string WriteProperties(const WAsset& Asset)
	{
		return FJsonWriter::Write(ExportProperties(Asset), EJsonFormat::Compact);
	}

	/** Returns the JSON object of text that the editor wrote itself, which only a mistake in the code can spoil. */
	FJsonValue ParseWritten(std::string_view Text)
	{
		FJsonValue Value;
		std::string Error;
		const bool bIsParsed = FJsonReader::Parse(Text, Value, Error);
		WW_CHECKF(bIsParsed && Value.IsObject(), "What the editor wrote down of its assets cannot be read: {}", Error);
		return Value;
	}

	/** Gives an asset the values of a JSON object of its properties, and logs what could not be put back. */
	void PutBack(WAsset& Asset, const FJsonValue& Properties)
	{
		FAssetReferenceContext Context;
		std::vector<std::string> Problems;
		FObjectProperties::Import(Asset, Properties, Context, SavedWith, Problems);
		for (const std::string& Problem : Problems)
		{
			WW_LOG(LogEditor, Warning, "Putting \"{}\" back: {}", Asset.GetAssetPath(), Problem);
		}
	}
}

FEditedAssets::FEditedAssets() = default;

FEditedAssets::~FEditedAssets()
{
	ReleaseAll();
}

bool FEditedAssets::CanChange(const FAssetPath& Path)
{
	return Path.Key.empty() && FMaterialSerializer::IsMaterialFile(FFileHelper::PathFromUtf8(Path.File));
}

bool FEditedAssets::CanChange(const WAsset& Asset)
{
	std::string Error;
	const std::optional<FAssetPath> Path = FAssetPath::Parse(Asset.GetAssetPath(), Error);
	return Path.has_value() && CanChange(*Path) && Asset.IsA(WMaterial::StaticClass());
}

bool FEditedAssets::SetProperty(
	WAsset& Asset, const FProperty& Property, const FJsonValue& Value, std::string& OutError)
{
	const std::string& AssetPath = Asset.GetAssetPath();
	WW_CHECKF(CanChange(Asset), "The editor was asked to change the asset '{}', which it cannot save", AssetPath);
	const std::vector<const FProperty*> SavedProperties = FObjectProperties::GetSaved(*Asset.GetClass());
	WW_CHECKF(std::ranges::find(SavedProperties, &Property) != SavedProperties.end(),
		"The editor was asked to change the property {} of the asset '{}', which is not saved with it",
		Property.GetName(),
		AssetPath);

	auto Entry = KeptAssets.find(AssetPath);
	const bool bWasKept = Entry != KeptAssets.end();
	if (bWasKept)
	{
		WW_CHECKF(&GetAsset(Entry->first, Entry->second) == &Asset,
			"The editor was asked to change an object of the asset '{}' other than the one it keeps",
			AssetPath);
	}
	else
	{
		// Kept before it is changed: from now on it may differ from its file, and the history may change it again.
		Asset.AddToRoot();
		const std::string Now = WriteProperties(Asset);
		Entry = KeptAssets.emplace(AssetPath, FKeptAsset{TWeakObjectPtr<WAsset>(&Asset), Now, Now}).first;
	}

	FAssetReferenceContext Context;
	const bool bIsSet = Property.ImportJson(Property.GetValuePtr(&Asset), Value, Context, OutError);

	// Taken into keeping for a value that was refused, or that it had already: it is what its file says.
	if (!bWasKept && WriteProperties(Asset) == Entry->second.Baseline)
	{
		Asset.RemoveFromRoot();
		KeptAssets.erase(Entry);
	}
	return bIsSet;
}

bool FEditedAssets::Save(WAsset& Asset, bool& bOutWritten, std::string& OutError)
{
	const std::string& AssetPath = Asset.GetAssetPath();
	WW_CHECKF(CanChange(Asset), "The editor was asked to save the asset '{}', which it cannot save", AssetPath);
	bOutWritten = false;

	const auto Entry = KeptAssets.find(AssetPath);
	const bool bIsKept = Entry != KeptAssets.end();
	if (bIsKept)
	{
		WW_CHECKF(&GetAsset(Entry->first, Entry->second) == &Asset,
			"The editor was asked to save an object of the asset '{}' other than the one it keeps",
			AssetPath);
	}
	if (FAssetManager::GetContentDirectory().empty())
	{
		OutError = "there is no content directory to write it to";
		return false;
	}

	// An asset path that can be changed is the name of a file without a key.
	const std::filesystem::path File = FAssetManager::GetPathOfAssetFile(AssetPath);
	std::error_code ErrorCode;
	const bool bHasFile = std::filesystem::is_regular_file(File, ErrorCode);
	const std::string Now = WriteProperties(Asset);
	const bool bHasUnsavedChanges = bIsKept && Now != Entry->second.Saved;
	if (bHasFile && !bHasUnsavedChanges)
	{
		return true;
	}

	if (!FMaterialSerializer::SaveToFile(*Cast<WMaterial>(&Asset), File, OutError))
	{
		return false;
	}
	bOutWritten = true;
	if (bIsKept)
	{
		Entry->second.Saved = Now;
	}
	return true;
}

bool FEditedAssets::IsKept(std::string_view AssetPath) const
{
	return KeptAssets.contains(AssetPath);
}

bool FEditedAssets::HasUnsavedChanges(std::string_view AssetPath) const
{
	const auto Entry = KeptAssets.find(AssetPath);
	return Entry != KeptAssets.end() && WriteProperties(GetAsset(Entry->first, Entry->second)) != Entry->second.Saved;
}

std::vector<std::string> FEditedAssets::GetUnsaved() const
{
	std::vector<std::string> Unsaved;
	for (const auto& [AssetPath, Kept] : KeptAssets)
	{
		if (WriteProperties(GetAsset(AssetPath, Kept)) != Kept.Saved)
		{
			Unsaved.push_back(AssetPath);
		}
	}
	return Unsaved;
}

std::string FEditedAssets::WriteState() const
{
	FJsonValue Changed = FJsonValue::MakeObject();
	bool bIsAnyChanged = false;
	for (const auto& [AssetPath, Kept] : KeptAssets)
	{
		FJsonValue Properties = ExportProperties(GetAsset(AssetPath, Kept));
		if (FJsonWriter::Write(Properties, EJsonFormat::Compact) != Kept.Baseline)
		{
			Changed.SetMember(AssetPath, std::move(Properties));
			bIsAnyChanged = true;
		}
	}
	return bIsAnyChanged ? FJsonWriter::Write(Changed, EJsonFormat::Compact) : std::string();
}

void FEditedAssets::RestoreState(std::string_view State)
{
	const FJsonValue Changed = State.empty() ? FJsonValue::MakeObject() : ParseWritten(State);
	for (const FJsonValue::FMember& Member : Changed.GetMembers())
	{
		WW_CHECKF(KeptAssets.contains(Member.first),
			"A state of the history names the asset '{}', which the editor does not keep",
			Member.first);
	}

	for (const auto& [AssetPath, Kept] : KeptAssets)
	{
		WAsset& Asset = GetAsset(AssetPath, Kept);
		const FJsonValue* const Named = Changed.FindMember(AssetPath);
		const std::string Target = Named != nullptr ? FJsonWriter::Write(*Named, EJsonFormat::Compact) : Kept.Baseline;
		if (Target == WriteProperties(Asset))
		{
			continue;
		}
		if (Named != nullptr)
		{
			PutBack(Asset, *Named);
		}
		else
		{
			PutBack(Asset, ParseWritten(Kept.Baseline));
		}
	}
}

void FEditedAssets::BeginHistory()
{
	for (auto Entry = KeptAssets.begin(); Entry != KeptAssets.end();)
	{
		WAsset& Asset = GetAsset(Entry->first, Entry->second);
		std::string Now = WriteProperties(Asset);
		if (Now == Entry->second.Saved)
		{
			Asset.RemoveFromRoot();
			Entry = KeptAssets.erase(Entry);
		}
		else
		{
			Entry->second.Baseline = std::move(Now);
			++Entry;
		}
	}
}

void FEditedAssets::ReleaseAll()
{
	// An asset that something else destroyed is gone already, with its changes; this is also called at the end.
	std::vector<std::pair<WAsset*, const FKeptAsset*>> GivenUp;
	std::string Names;
	for (const auto& [AssetPath, Kept] : KeptAssets)
	{
		WAsset* const Asset = Kept.Asset.Get();
		if (Asset != nullptr && WriteProperties(*Asset) != Kept.Saved)
		{
			GivenUp.emplace_back(Asset, &Kept);
			Names += Names.empty() ? AssetPath : ", " + AssetPath;
		}
	}
	if (!GivenUp.empty())
	{
		WW_LOG(LogEditor, Warning, "Changes to these assets were not saved and are given up: {}", Names);
	}

	// No longer kept, an asset has to be what its file says: the asset manager gives it to whoever asks for it.
	for (const auto& [Asset, Kept] : GivenUp)
	{
		PutBack(*Asset, ParseWritten(Kept->Saved));
	}
	for (const std::pair<const std::string, FKeptAsset>& Entry : KeptAssets)
	{
		if (WAsset* const Asset = Entry.second.Asset.Get())
		{
			Asset->RemoveFromRoot();
		}
	}
	KeptAssets.clear();
}

WAsset& FEditedAssets::GetAsset(const std::string& AssetPath, const FKeptAsset& Kept)
{
	WAsset* const Asset = Kept.Asset.Get();
	WW_CHECKF(Asset != nullptr,
		"The asset '{}', which the editor keeps, was destroyed by something other than the editor",
		AssetPath);
	return *Asset;
}
