#include "Rendering/Material.h"

#include "Object/Class.h"
#include "Object/ObjectGlobals.h"
#include "Object/TestObjects.h"
#include "Reflection/Property.h"

#include <doctest/doctest.h>

#include <string_view>

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

	TEST_CASE("Every property of a material is reflected, so it is saved and can be edited")
	{
		struct FExpectedProperty
		{
			std::string_view Name;
			std::string_view TypeName;
		};

		for (const FExpectedProperty& Expected : {
				 FExpectedProperty{"BaseColor", "FLinearColor"},
				 FExpectedProperty{"Metallic", "float"},
				 FExpectedProperty{"Roughness", "float"},
				 FExpectedProperty{"EmissiveColor", "FLinearColor"},
			 })
		{
			const FProperty* const Property = WMaterial::StaticClass()->FindProperty(Expected.Name);

			CAPTURE(Expected.Name);
			REQUIRE(Property != nullptr);
			CHECK(Property->GetTypeName() == Expected.TypeName);
		}
	}
}
