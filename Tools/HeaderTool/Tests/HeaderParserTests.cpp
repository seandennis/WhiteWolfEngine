#include "HeaderParser.h"

#include <doctest/doctest.h>

#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	/** Parses a header that the test expects to be accepted. */
	std::vector<FReflectedClass> ParseValid(std::string_view Source)
	{
		std::vector<FReflectedClass> Classes;
		std::vector<FParseError> Errors;
		const bool bParsed = FHeaderParser::Parse(Source, Classes, Errors);
		for (const FParseError& Error : Errors)
		{
			INFO("Line ", Error.Line, ": ", Error.Message);
			CHECK(false);
		}
		REQUIRE(bParsed);
		return Classes;
	}

	/** Parses a header that the test expects to be rejected, and returns the errors. */
	std::vector<FParseError> ParseInvalid(std::string_view Source)
	{
		std::vector<FReflectedClass> Classes;
		std::vector<FParseError> Errors;
		const bool bParsed = FHeaderParser::Parse(Source, Classes, Errors);
		INFO("Source: ", std::string(Source));
		REQUIRE_FALSE(bParsed);
		REQUIRE_FALSE(Errors.empty());
		CHECK(Classes.empty());
		return Errors;
	}

	/** Wraps member declarations in a valid reflected class, so a test can focus on the members. */
	std::string InDoorClass(std::string_view Members)
	{
		return "WCLASS()\nclass WDoor : public WObject\n{\n\tWW_DECLARE_CLASS(WDoor, WObject)\npublic:\n" +
			std::string(Members) + "\n};\n";
	}

	/** Returns the message of the only error that parsing Source produces. */
	std::string SingleError(std::string_view Source)
	{
		const std::vector<FParseError> Errors = ParseInvalid(Source);
		REQUIRE(Errors.size() == 1);
		return Errors[0].Message;
	}

	bool Mentions(const std::string& Message, std::string_view Text)
	{
		return Message.find(Text) != std::string::npos;
	}
}

TEST_SUITE("FHeaderParser")
{
	TEST_CASE("A header without reflected classes is accepted and yields nothing")
	{
		CHECK(ParseValid("").empty());
		CHECK(ParseValid("#pragma once\nclass FPlain\n{\npublic:\n\tint32 Value = 0;\n};\n").empty());
	}

	TEST_CASE("A class registered by hand, without WCLASS, is ignored")
	{
		CHECK(ParseValid("class WManual : public WObject\n{\n\tWW_DECLARE_CLASS(WManual, WObject)\n};\n").empty());
	}

	TEST_CASE("A reflected class is found with its name, base class and line")
	{
		const std::vector<FReflectedClass> Classes = ParseValid("#pragma once\n"
																"\n"
																"/** A door. */\n"
																"WCLASS()\n"
																"class ENGINE_API WDoor final : public WObject\n"
																"{\n"
																"\tWW_DECLARE_CLASS(WDoor, WObject)\n"
																"};\n");

		REQUIRE(Classes.size() == 1);
		CHECK(Classes[0].Name == "WDoor");
		CHECK(Classes[0].SuperName == "WObject");
		CHECK(Classes[0].Line == 4);
		CHECK(Classes[0].Properties.empty());
		CHECK(Classes[0].Functions.empty());
	}

	TEST_CASE("Several classes in one header are found in order, and further base classes are ignored")
	{
		const std::vector<FReflectedClass> Classes = ParseValid(
			"WCLASS()\nclass WFirst : public WObject\n{\n\tWW_DECLARE_CLASS(WFirst, WObject)\n};\n"
			"class FUnrelated {};\n"
			"WCLASS()\nclass WSecond : public WFirst, public IInterface\n{\n\tWW_DECLARE_CLASS(WSecond, WFirst)\n};\n");

		REQUIRE(Classes.size() == 2);
		CHECK(Classes[0].Name == "WFirst");
		CHECK(Classes[1].Name == "WSecond");
		CHECK(Classes[1].SuperName == "WFirst");
	}

	TEST_CASE("Properties are found with their names, whatever their types and initializers")
	{
		const std::vector<FReflectedClass> Classes =
			ParseValid(InDoorClass("\tWPROPERTY()\n\tfloat OpenAngle = 90.0f;\n"
								   "\tWPROPERTY()\n\tTObjectPtr<WHinge> Hinge;\n"
								   "\tWPROPERTY()\n\tstd::vector<TObjectPtr<WPanel>> Panels;\n"
								   "\tWPROPERTY()\n\tFVector Location{0.0, 1.0, 2.0};\n"
								   "\tWPROPERTY()\n\tFQuat Rotation = FQuat(1.0f, 0.0f, 0.0f, 0.0f);\n"
								   "\tWPROPERTY()\n\tstd::string Label = \"a;b\";\n"
								   "\tWPROPERTY()\n\tconst WDoor* Raw = nullptr;\n"));

		REQUIRE(Classes.size() == 1);
		const std::vector<FReflectedProperty>& Properties = Classes[0].Properties;
		REQUIRE(Properties.size() == 7);
		CHECK(Properties[0].Name == "OpenAngle");
		CHECK(Properties[0].Line == 6);
		CHECK(Properties[1].Name == "Hinge");
		CHECK(Properties[2].Name == "Panels");
		CHECK(Properties[3].Name == "Location");
		CHECK(Properties[4].Name == "Rotation");
		CHECK(Properties[5].Name == "Label");
		CHECK(Properties[6].Name == "Raw");
	}

	TEST_CASE("Members without a macro are not reflected")
	{
		const std::vector<FReflectedClass> Classes =
			ParseValid(InDoorClass("\tint32 Hidden = 0;\n\tWPROPERTY()\n\tint32 Shown = 0;\n\tvoid Helper();\n"));

		REQUIRE(Classes.size() == 1);
		REQUIRE(Classes[0].Properties.size() == 1);
		CHECK(Classes[0].Properties[0].Name == "Shown");
		CHECK(Classes[0].Functions.empty());
	}

	TEST_CASE("Property specifiers become flags and a category")
	{
		const std::vector<FReflectedClass> Classes = ParseValid(
			InDoorClass("\tWPROPERTY(Transient, BlueprintReadOnly, Category = \"Door Settings\")\n\tfloat OpenAngle;\n"
						"\tWPROPERTY(BlueprintReadWrite)\n\tfloat Speed;\n"));

		REQUIRE(Classes.size() == 1);
		REQUIRE(Classes[0].Properties.size() == 2);
		CHECK(Classes[0].Properties[0].Flags == std::vector<std::string>{"Transient", "BlueprintReadOnly"});
		CHECK(Classes[0].Properties[0].Category == "Door Settings");
		CHECK(Classes[0].Properties[1].Flags == std::vector<std::string>{"BlueprintReadWrite"});
		CHECK(Classes[0].Properties[1].Category.empty());
	}

	TEST_CASE("Functions are found with their names and parameter names")
	{
		const std::vector<FReflectedClass> Classes =
			ParseValid(InDoorClass("\tWFUNCTION()\n\tvoid Open(float Speed, bool bInstant = false);\n"
								   "\tWFUNCTION()\n\tbool IsOpen() const;\n"
								   "\tWFUNCTION()\n\tvirtual std::vector<int32> GetCodes(void) const override;\n"
								   "\tWFUNCTION()\n\tvoid Attach(WHinge* NewHinge, const FVector& Offset, const "
								   "std::string& Label = std::string(\"a,b\"));\n"
								   "\tWFUNCTION()\n\t[[nodiscard]] TObjectPtr<WHinge> FindHinge(const "
								   "std::vector<TObjectPtr<WHinge>>& Candidates, long long Seed);\n"));

		REQUIRE(Classes.size() == 1);
		const std::vector<FReflectedFunction>& Functions = Classes[0].Functions;
		REQUIRE(Functions.size() == 5);
		CHECK(Functions[0].Name == "Open");
		CHECK(Functions[0].ParameterNames == std::vector<std::string>{"Speed", "bInstant"});
		CHECK(Functions[0].Line == 6);
		CHECK(Functions[1].Name == "IsOpen");
		CHECK(Functions[1].ParameterNames.empty());
		CHECK(Functions[2].Name == "GetCodes");
		CHECK(Functions[2].ParameterNames.empty());
		CHECK(Functions[3].Name == "Attach");
		CHECK(Functions[3].ParameterNames == std::vector<std::string>{"NewHinge", "Offset", "Label"});
		CHECK(Functions[4].Name == "FindHinge");
		CHECK(Functions[4].ParameterNames == std::vector<std::string>{"Candidates", "Seed"});
	}

	TEST_CASE("A function defined in the class is reflected and its body is skipped")
	{
		const std::vector<FReflectedClass> Classes = ParseValid(InDoorClass(
			"\tWFUNCTION()\n\tint32 GetCount() const\n\t{\n\t\tif (bOpen) { return 1; }\n\t\treturn 0;\n\t}\n"
			"\tWPROPERTY()\n\tbool bOpen = false;\n"));

		REQUIRE(Classes.size() == 1);
		REQUIRE(Classes[0].Functions.size() == 1);
		CHECK(Classes[0].Functions[0].Name == "GetCount");
		REQUIRE(Classes[0].Properties.size() == 1);
		CHECK(Classes[0].Properties[0].Name == "bOpen");
	}

	TEST_CASE("Function specifiers become flags and a category")
	{
		const std::vector<FReflectedClass> Classes =
			ParseValid(InDoorClass("\tWFUNCTION(BlueprintCallable, Category = \"Door\")\n\tvoid Open();\n"
								   "\tWFUNCTION(BlueprintPure)\n\tbool IsOpen() const;\n"));

		REQUIRE(Classes.size() == 1);
		REQUIRE(Classes[0].Functions.size() == 2);
		CHECK(Classes[0].Functions[0].Flags == std::vector<std::string>{"BlueprintCallable"});
		CHECK(Classes[0].Functions[0].Category == "Door");
		CHECK(Classes[0].Functions[1].Flags == std::vector<std::string>{"BlueprintPure"});
	}

	TEST_CASE("Macros in comments, strings and preprocessor directives are not declarations")
	{
		const std::vector<FReflectedClass> Classes =
			ParseValid("#define WCLASS(...)\n"
					   "// WCLASS() class WCommented : public WObject {};\n"
					   "const char* const Text = \"WCLASS() class WQuoted : public WObject {};\";\n" +
				InDoorClass("\t// WPROPERTY()\n\tint32 NotReflected = 0;\n"));

		REQUIRE(Classes.size() == 1);
		CHECK(Classes[0].Name == "WDoor");
		CHECK(Classes[0].Properties.empty());
	}

	TEST_CASE("An error reports the line of the declaration it is about")
	{
		const std::vector<FParseError> Errors = ParseInvalid(
			InDoorClass("\tWPROPERTY()\n\tint32 Fine = 0;\n\n\tWPROPERTY(Unknown)\n\tint32 Broken = 0;\n"));

		REQUIRE(Errors.size() == 1);
		CHECK(Errors[0].Line == 9);
		CHECK(Mentions(Errors[0].Message, "Unknown"));
	}

	TEST_CASE("Every problem in a header is reported, not only the first")
	{
		const std::vector<FParseError> Errors = ParseInvalid(
			InDoorClass("\tWPROPERTY(Unknown)\n\tint32 First = 0;\n\tWFUNCTION()\n\tstatic void Second();\n"));

		CHECK(Errors.size() == 2);
	}

	TEST_CASE("WCLASS must be followed by the definition of a class that derives publicly from a plain base")
	{
		CHECK(Mentions(SingleError("WCLASS()\nstruct FDoor {};"), "class declaration"));
		CHECK(Mentions(SingleError("WCLASS()\nclass WDoor;"), "forward declaration"));
		CHECK(Mentions(SingleError("WCLASS()\nclass WDoor\n{\n};"), "must derive from WObject"));
		CHECK(Mentions(SingleError("WCLASS()\nclass WDoor : WObject\n{\n\tWW_DECLARE_CLASS(WDoor, WObject)\n};"),
			"derive publicly"));
		CHECK(
			Mentions(SingleError("WCLASS()\nclass WDoor : private WObject\n{\n\tWW_DECLARE_CLASS(WDoor, WObject)\n};"),
				"derive publicly"));
		CHECK(Mentions(
			SingleError("WCLASS()\nclass WDoor : public Engine::WObject\n{\n\tWW_DECLARE_CLASS(WDoor, WObject)\n};"),
			"named plainly"));
		CHECK(Mentions(
			SingleError("WCLASS()\nclass WDoor : public TBase<WObject>\n{\n\tWW_DECLARE_CLASS(WDoor, TBase)\n};"),
			"named plainly"));
		CHECK(Mentions(SingleError("WCLASS(\nclass WDoor : public WObject {};"), "not closed"));
	}

	TEST_CASE("The name of a reflected class must start with W")
	{
		const std::string Error =
			SingleError("WCLASS()\nclass FDoor : public WObject\n{\n\tWW_DECLARE_CLASS(FDoor, WObject)\n};");

		CHECK(Mentions(Error, "must start with W"));
	}

	TEST_CASE("WCLASS takes no specifiers")
	{
		const std::string Error =
			SingleError("WCLASS(Abstract)\nclass WDoor : public WObject\n{\n\tWW_DECLARE_CLASS(WDoor, WObject)\n};");

		CHECK(Mentions(Error, "Abstract"));
	}

	TEST_CASE("A reflected class needs a matching WW_DECLARE_CLASS")
	{
		CHECK(Mentions(
			SingleError("WCLASS()\nclass WDoor : public WObject\n{\n};"), "needs WW_DECLARE_CLASS(WDoor, WObject)"));
		CHECK(Mentions(SingleError("WCLASS()\nclass WDoor : public WObject\n{\n\tWW_DECLARE_CLASS(WDoor, WGate)\n};"),
			"WW_DECLARE_CLASS(WDoor, WObject)"));
		CHECK(Mentions(SingleError("WCLASS()\nclass WDoor : public WObject\n{\n\tWW_DECLARE_CLASS(WGate, WObject)\n};"),
			"WW_DECLARE_CLASS(WDoor, WObject)"));
	}

	TEST_CASE("A reflected class must be at global scope and its body must be closed")
	{
		CHECK(Mentions(SingleError("namespace Game\n{\nWCLASS()\nclass WDoor : public "
								   "WObject\n{\n\tWW_DECLARE_CLASS(WDoor, WObject)\n};\n}"),
			"global scope"));
		CHECK(Mentions(SingleError("WCLASS()\nclass WDoor : public WObject\n{\n\tWW_DECLARE_CLASS(WDoor, WObject)\n"),
			"not closed"));
	}

	TEST_CASE("The member macros are only allowed directly inside a reflected class")
	{
		CHECK(Mentions(SingleError("WPROPERTY()\nint32 Global = 0;"), "inside a class that is marked with WCLASS()"));
		CHECK(Mentions(SingleError("class WManual : public WObject\n{\n\tWFUNCTION()\n\tvoid Open();\n};"),
			"inside a class that is marked with WCLASS()"));
		CHECK(Mentions(SingleError(InDoorClass("\tstruct FInner\n\t{\n\t\tWPROPERTY()\n\t\tint32 Value = 0;\n\t};\n")),
			"nested scope"));
		CHECK(Mentions(
			SingleError(InDoorClass("\tWCLASS()\n\tclass WInner : public WObject\n\t{\n\t};\n")), "nested scope"));
	}

	TEST_CASE("Malformed specifier lists are rejected")
	{
		CHECK(Mentions(
			SingleError(InDoorClass("\tWPROPERTY(Transient BlueprintReadOnly)\n\tint32 A;\n")), "expected ','"));
		CHECK(
			Mentions(SingleError(InDoorClass("\tWPROPERTY(Transient,)\n\tint32 A;\n")), "nothing after the last ','"));
		CHECK(Mentions(
			SingleError(InDoorClass("\tWPROPERTY(\"Transient\")\n\tint32 A;\n")), "expected a specifier name"));
		CHECK(Mentions(SingleError(InDoorClass("\tWPROPERTY(Category = Door)\n\tint32 A;\n")), "double quotes"));
		CHECK(Mentions(SingleError(InDoorClass("\tWPROPERTY(Category = \"Do\\\\or\")\n\tint32 A;\n")), "backslash"));
	}

	TEST_CASE("Unknown, repeated, conflicting and misused specifiers are rejected")
	{
		CHECK(Mentions(SingleError(InDoorClass("\tWPROPERTY(EditAnywhere)\n\tint32 A;\n")),
			"does not have a specifier named EditAnywhere"));
		CHECK(Mentions(SingleError(InDoorClass("\tWPROPERTY(BlueprintCallable)\n\tint32 A;\n")),
			"does not have a specifier named BlueprintCallable"));
		CHECK(Mentions(SingleError(InDoorClass("\tWFUNCTION(Transient)\n\tvoid A();\n")),
			"does not have a specifier named Transient"));
		CHECK(Mentions(SingleError(InDoorClass("\tWPROPERTY(Transient, Transient)\n\tint32 A;\n")), "given twice"));
		CHECK(Mentions(
			SingleError(InDoorClass("\tWPROPERTY(Category = \"A\", Category = \"B\")\n\tint32 A;\n")), "given twice"));
		CHECK(Mentions(SingleError(InDoorClass("\tWPROPERTY(BlueprintReadOnly, BlueprintReadWrite)\n\tint32 A;\n")),
			"cannot be used together"));
		CHECK(Mentions(SingleError(InDoorClass("\tWFUNCTION(BlueprintCallable, BlueprintPure)\n\tvoid A();\n")),
			"cannot be used together"));
		CHECK(Mentions(SingleError(InDoorClass("\tWPROPERTY(Category)\n\tint32 A;\n")), "Category needs a value"));
		CHECK(Mentions(
			SingleError(InDoorClass("\tWPROPERTY(Transient = \"Yes\")\n\tint32 A;\n")), "does not take a value"));
	}

	TEST_CASE("WPROPERTY must be followed by a plain member variable")
	{
		CHECK(
			Mentions(SingleError(InDoorClass("\tWPROPERTY()\n\tvoid Open();\n")), "mark a function with WFUNCTION()"));
		CHECK(Mentions(SingleError(InDoorClass("\tWPROPERTY()\n\tstatic int32 Count;\n")), "static"));
		CHECK(Mentions(SingleError(InDoorClass("\tWPROPERTY()\n\tint32 Values[4];\n")), "C array"));
		CHECK(Mentions(SingleError(InDoorClass("\tWPROPERTY()\n\tuint32 bFlag : 1;\n")), "bit-field"));
		CHECK(Mentions(SingleError(InDoorClass("\tWPROPERTY()\n\tWPROPERTY()\n\tint32 A;\n")),
			"must be followed by a member variable"));
		CHECK(Mentions(SingleError(InDoorClass("\tWPROPERTY()\n")), "must be followed by a member variable"));
	}

	TEST_CASE("Two properties of a class cannot have the same name")
	{
		const std::string Error = SingleError(InDoorClass("\tWPROPERTY()\n\tint32 A;\n\tWPROPERTY()\n\tfloat A;\n"));

		CHECK(Mentions(Error, "already has a property named A"));
	}

	TEST_CASE("WFUNCTION must be followed by an ordinary member function")
	{
		CHECK(Mentions(
			SingleError(InDoorClass("\tWFUNCTION()\n\tint32 Count = 0;\n")), "mark a variable with WPROPERTY()"));
		CHECK(Mentions(SingleError(InDoorClass("\tWFUNCTION()\n\tstatic void Open();\n")), "static"));
		CHECK(Mentions(
			SingleError(InDoorClass("\tWFUNCTION()\n\ttemplate <typename T> void Open(T Value);\n")), "template"));
		CHECK(Mentions(SingleError(InDoorClass("\tWFUNCTION()\n\tfriend void Open(WDoor& Door);\n")), "friend"));
		CHECK(Mentions(
			SingleError(InDoorClass("\tWFUNCTION()\n\tbool operator==(const WDoor& Other) const;\n")), "operator"));
		CHECK(Mentions(SingleError(InDoorClass("\tWFUNCTION()\n\tWDoor();\n")), "constructor"));
		CHECK(Mentions(SingleError(InDoorClass("\tWFUNCTION()\n")), "must be followed by a member function"));
	}

	TEST_CASE("Every parameter of a reflected function needs a name")
	{
		CHECK(Mentions(
			SingleError(InDoorClass("\tWFUNCTION()\n\tvoid Open(float);\n")), "parameter 1 of Open needs a name"));
		CHECK(Mentions(SingleError(InDoorClass("\tWFUNCTION()\n\tvoid Open(float Speed, const FVector&);\n")),
			"parameter 2 of Open needs a name"));
		CHECK(Mentions(SingleError(InDoorClass("\tWFUNCTION()\n\tvoid Open(const FVector);\n")),
			"parameter 1 of Open needs a name"));
		CHECK(Mentions(SingleError(InDoorClass("\tWFUNCTION()\n\tvoid Open(std::string);\n")),
			"parameter 1 of Open needs a name"));
		CHECK(Mentions(SingleError(InDoorClass("\tWFUNCTION()\n\tvoid Open(unsigned int);\n")),
			"parameter 1 of Open needs a name"));
		CHECK(Mentions(
			SingleError(InDoorClass("\tWFUNCTION()\n\tvoid Open(WHinge*);\n")), "parameter 1 of Open needs a name"));
		CHECK(Mentions(SingleError(InDoorClass("\tWFUNCTION()\n\tvoid Open(int32 Values[4]);\n")), "C array"));
	}

	TEST_CASE("Overloaded functions cannot be reflected")
	{
		const std::string Error =
			SingleError(InDoorClass("\tWFUNCTION()\n\tvoid Open();\n\tWFUNCTION()\n\tvoid Open(float Speed);\n"));

		CHECK(Mentions(Error, "overloaded"));
	}
}
