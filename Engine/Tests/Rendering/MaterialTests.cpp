#include "Rendering/Material.h"

#include "Object/Class.h"
#include "Object/ObjectGlobals.h"
#include "Object/TestObjects.h"
#include "Reflection/ObjectProperties.h"
#include "Reflection/Property.h"
#include "Reflection/PropertyFlags.h"

#include <doctest/doctest.h>

#include <cstddef>
#include <ostream>
#include <string_view>
#include <vector>

TEST_SUITE("WMaterial")
{
	TEST_CASE("A new material is white, matte, not a metal and does not glow")
	{
		const FScopedObjectTest ObjectTest;

		const WMaterial* const Material = NewObject<WMaterial>();

		CHECK(Material->BaseColor == FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
		CHECK(Material->Metallic == 0.0f);
		CHECK(Material->Roughness == 0.5f);
		CHECK(Material->EmissiveColor == FLinearColor(0.0f, 0.0f, 0.0f, 1.0f));
	}

	TEST_CASE("Every property of a material is reflected and saved, in the order that a material file writes them")
	{
		struct FExpectedProperty
		{
			std::string_view Name;
			std::string_view TypeName;
		};
		const std::vector<FExpectedProperty> Expected = {
			{"BaseColor", "FLinearColor"},
			{"Metallic", "float"},
			{"Roughness", "float"},
			{"EmissiveColor", "FLinearColor"},
			{"BaseColorTexture", "TObjectPtr<WTexture>"},
			{"MetallicRoughnessTexture", "TObjectPtr<WTexture>"},
			{"NormalTexture", "TObjectPtr<WTexture>"},
			{"NormalScale", "float"},
			{"EmissiveTexture", "TObjectPtr<WTexture>"},
			{"OcclusionTexture", "TObjectPtr<WTexture>"},
			{"OcclusionStrength", "float"},
		};

		// What a material saves is these and nothing else: what the asset itself has is not saved.
		const std::vector<const FProperty*> Saved = FObjectProperties::GetSaved(*WMaterial::StaticClass());
		REQUIRE(Saved.size() == Expected.size());
		for (std::size_t Index = 0; Index < Expected.size(); ++Index)
		{
			CAPTURE(Index);
			CHECK(Saved[Index]->GetName() == Expected[Index].Name);
			CHECK(Saved[Index]->GetTypeName() == Expected[Index].TypeName);
			CHECK_FALSE(Saved[Index]->HasAnyFlags(EPropertyFlags::Transient));
		}
	}
}
