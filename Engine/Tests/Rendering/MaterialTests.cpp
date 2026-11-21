#include "Rendering/Material.h"

#include "Object/Class.h"
#include "Object/ObjectGlobals.h"
#include "Object/TestObjects.h"
#include "Reflection/Property.h"

#include <doctest/doctest.h>

TEST_SUITE("WMaterial")
{
	TEST_CASE("A new material is plain white")
	{
		const FScopedObjectTest ObjectTest;

		const WMaterial* const Material = NewObject<WMaterial>();

		CHECK(Material->BaseColor == FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
	}

	TEST_CASE("The base colour is a reflected property, so it is saved and can be edited")
	{
		const FProperty* const Property = WMaterial::StaticClass()->FindProperty("BaseColor");

		REQUIRE(Property != nullptr);
		CHECK(Property->GetTypeName() == "FLinearColor");
	}
}
