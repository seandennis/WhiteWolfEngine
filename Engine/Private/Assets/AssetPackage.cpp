#include "Assets/AssetPackage.h"

#include "Assets/AssetPath.h"

#include <cstddef>

const std::string& WAssetPackage::GetFile() const
{
	return File;
}

WAsset* WAssetPackage::FindAsset(std::string_view Key) const
{
	for (std::size_t Index = 0; Index < Keys.size(); ++Index)
	{
		if (Keys[Index] == Key)
		{
			return Assets[Index];
		}
	}
	return nullptr;
}

const std::vector<std::string>& WAssetPackage::GetKeys() const
{
	return Keys;
}

const std::vector<TObjectPtr<WAsset>>& WAssetPackage::GetAssets() const
{
	return Assets;
}

const std::vector<std::string>& WAssetPackage::GetWarnings() const
{
	return Warnings;
}

void WAssetPackage::AddAsset(const std::string& Key, WAsset& Asset)
{
	WW_CHECKF(FindAsset(Key) == nullptr, "The package of '{}' has an asset \"{}\" already", File, Key);
	WW_CHECKF(Asset.Package == nullptr, "An asset can belong to one package only");

	Asset.AssetPath = FAssetPath{File, Key}.ToString();
	Asset.Package = this;
	Keys.push_back(Key);
	Assets.emplace_back(&Asset);
}
