#include "Scene/MissingComponent.h"

const std::string& WMissingComponent::GetMissingClassName() const
{
	return MissingClassName;
}

const FJsonValue& WMissingComponent::GetSavedProperties() const
{
	return SavedProperties;
}
