#include "Assets/AssetReferenceContext.h"

#include "Assets/Asset.h"
#include "Assets/AssetManager.h"
#include "Object/Class.h"
#include "Object/ObjectGlobals.h"

#include <format>

FJsonValue FAssetReferenceContext::ExportObjectReference(const WObject& Object)
{
	const WAsset* const Asset = Cast<WAsset>(&Object);
	if (Asset != nullptr && !Asset->GetAssetPath().empty())
	{
		return FJsonValue(Asset->GetAssetPath());
	}

	Unwritten.push_back(Object.GetClass()->GetName());
	return FJsonValue();
}

bool FAssetReferenceContext::ImportObjectReference(const FJsonValue& Json, WObject*& OutObject, std::string& OutError)
{
	if (!Json.IsString())
	{
		OutError = std::format("expected the path of an asset, but found {}", Json.GetTypeDescription());
		return false;
	}

	WAsset* const Asset = FAssetManager::Load(Json.GetString(), OutError);
	if (Asset == nullptr)
	{
		return false;
	}
	OutObject = Asset;
	return true;
}
