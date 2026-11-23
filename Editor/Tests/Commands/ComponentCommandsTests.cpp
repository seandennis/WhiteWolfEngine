#include "Editor.h"
#include "Object/Class.h"
#include "Object/ObjectGlobals.h"
#include "Scene/Component.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "TestEditorObjects.h"

#include <doctest/doctest.h>

#include <algorithm>
#include <format>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

TEST_SUITE("Component commands")
{
	TEST_CASE("Component.Add creates a component of the named class on an entity and returns its identifier")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		const WEntity* const Entity = Editor.GetScene().CreateEntity("Player");
		REQUIRE(Entity != nullptr);
		std::string ClassName;

		SUBCASE("A component class")
		{
			ClassName = "WEditorTestComponent";
		}
		SUBCASE("A class derived from another component class")
		{
			ClassName = "WEditorTestDerivedComponent";
		}

		const FCommandResult Result = Editor.ExecuteCommand("Component.Add",
			ParseTestJson(std::format(R"({{"Entity": {}, "Class": "{}"}})", Entity->GetId(), ClassName)));

		REQUIRE(Result.HasSucceeded());
		REQUIRE(Entity->GetComponents().size() == 1);
		const WComponent* const Component = Entity->GetComponents()[0];
		CHECK(Component->GetClass()->GetName() == ClassName);
		CHECK(Component->GetOwner() == Entity);
		CHECK(WriteTestJson(Result.GetValue()) == std::format(R"({{"Id":{}}})", Component->GetId()));
	}

	TEST_CASE("Component.Add fails, and adds nothing, when it cannot create the component")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		const WEntity* const Entity = Editor.GetScene().CreateEntity("Player");
		REQUIRE(Entity != nullptr);
		std::string ArgumentText;
		std::string ExpectedError;

		SUBCASE("An entity that does not exist")
		{
			ArgumentText = R"({"Entity": 99, "Class": "WEditorTestComponent"})";
			ExpectedError = "There is no entity with the identifier 99.";
		}
		SUBCASE("A class that does not exist")
		{
			ArgumentText = std::format(R"({{"Entity": {}, "Class": "WNoSuchComponent"}})", Entity->GetId());
			ExpectedError = "There is no class \"WNoSuchComponent\".";
		}
		SUBCASE("A class that is not a component class")
		{
			ArgumentText = std::format(R"({{"Entity": {}, "Class": "WEntity"}})", Entity->GetId());
			ExpectedError = "The class WEntity is not a component class.";
		}
		SUBCASE("An abstract class")
		{
			ArgumentText = std::format(R"({{"Entity": {}, "Class": "WEditorTestAbstractComponent"}})", Entity->GetId());
			ExpectedError = "The class WEditorTestAbstractComponent is abstract: there can be no component of that "
							"class itself.";
		}
		SUBCASE("The class that all components derive from")
		{
			ArgumentText = std::format(R"({{"Entity": {}, "Class": "WComponent"}})", Entity->GetId());
			ExpectedError = "The class WComponent is what all components have in common and holds nothing by itself. "
							"Add a component of one of the classes that Component.ListClasses returns.";
		}
		SUBCASE("The class that stands in for classes that are missing")
		{
			ArgumentText = std::format(R"({{"Entity": {}, "Class": "WMissingComponent"}})", Entity->GetId());
			ExpectedError = "The class WMissingComponent stands in for the components of a scene file whose class is "
							"not in this build. Loading a scene makes them; they cannot be added.";
		}

		const FCommandResult Result = Editor.ExecuteCommand("Component.Add", ParseTestJson(ArgumentText));

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError() == ExpectedError);
		CHECK(Entity->GetComponents().empty());
	}

	TEST_CASE("Component.Remove takes a component off its entity and destroys it")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		WEntity* const Entity = Editor.GetScene().CreateEntity("Player");
		REQUIRE(Entity != nullptr);
		const WComponent* const Removed = Entity->AddComponent<WEditorTestComponent>();
		REQUIRE(Removed != nullptr);
		const WComponent* const Kept = Entity->AddComponent<WEditorTestDerivedComponent>();
		REQUIRE(Kept != nullptr);

		const FCommandResult Result = Editor.ExecuteCommand(
			"Component.Remove", ParseTestJson(std::format(R"({{"Component": {}}})", Removed->GetId())));

		REQUIRE(Result.HasSucceeded());
		CHECK(Result.GetValue().IsNull());
		CHECK_FALSE(IsValid(Removed));
		REQUIRE(Entity->GetComponents().size() == 1);
		CHECK(Entity->GetComponents()[0].Get() == Kept);
		CHECK(IsValid(Entity));
	}

	TEST_CASE("Component.Remove of a component that does not exist fails")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		WEntity* const Entity = Editor.GetScene().CreateEntity("Player");
		REQUIRE(Entity != nullptr);
		REQUIRE(Entity->AddComponent<WEditorTestComponent>() != nullptr);
		int64 MissingId = 99;

		SUBCASE("An identifier that nothing has")
		{
		}
		SUBCASE("The identifier of an entity")
		{
			MissingId = Entity->GetId();
		}

		const FCommandResult Result =
			Editor.ExecuteCommand("Component.Remove", ParseTestJson(std::format(R"({{"Component": {}}})", MissingId)));

		CHECK_FALSE(Result.HasSucceeded());
		CHECK(Result.GetError() == std::format("There is no component with the identifier {}.", MissingId));
		CHECK(IsValid(Entity));
		CHECK(Entity->GetComponents().size() == 1);
	}

	TEST_CASE("Component.ListClasses names the classes that Component.Add accepts, in the order of their names")
	{
		const FScopedEditorTest EditorTest;
		FEditor Editor;
		const WEntity* const Entity = Editor.GetScene().CreateEntity("Player");
		REQUIRE(Entity != nullptr);

		const FCommandResult Result = Editor.ExecuteCommand("Component.ListClasses", FJsonValue());

		REQUIRE(Result.HasSucceeded());
		const FJsonValue* const Classes = Result.GetValue().FindMember("Classes");
		REQUIRE(Classes != nullptr);
		REQUIRE(Classes->IsArray());
		std::vector<std::string> Names;
		for (const FJsonValue& Class : Classes->GetElements())
		{
			Names.push_back(Class.GetString());
		}

		const auto IsListed = [&Names](std::string_view Name)
		{
			return std::ranges::find(Names, Name) != Names.end();
		};
		// The engine's components and those that another module, here the tests, has registered.
		CHECK(IsListed("WStaticMeshComponent"));
		CHECK(IsListed("WDirectionalLightComponent"));
		CHECK(IsListed("WPointLightComponent"));
		CHECK(IsListed("WSkyLightComponent"));
		CHECK(IsListed("WEditorTestComponent"));
		CHECK(IsListed("WEditorTestDerivedComponent"));
		// Nothing that an entity cannot be given.
		CHECK_FALSE(IsListed("WComponent"));
		CHECK_FALSE(IsListed("WMissingComponent"));
		CHECK_FALSE(IsListed("WEditorTestAbstractComponent"));
		CHECK_FALSE(IsListed("WEntity"));
		CHECK(std::ranges::is_sorted(Names));

		// Listing changes nothing, and every class that is listed can be added.
		CHECK(Entity->GetComponents().empty());
		for (const std::string& Name : Names)
		{
			CAPTURE(Name);
			CHECK(Editor
					  .ExecuteCommand("Component.Add",
						  ParseTestJson(std::format(R"({{"Entity": {}, "Class": "{}"}})", Entity->GetId(), Name)))
					  .HasSucceeded());
		}
		CHECK(Entity->GetComponents().size() == Names.size());
	}
}
