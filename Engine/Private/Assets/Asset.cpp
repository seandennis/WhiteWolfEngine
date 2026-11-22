#include "Assets/Asset.h"

#include "Assets/AssetPackage.h"

const std::string& WAsset::GetAssetPath() const
{
	return AssetPath;
}

WAssetPackage* WAsset::GetPackage() const
{
	return Package;
}
