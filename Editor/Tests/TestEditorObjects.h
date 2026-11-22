#pragma once

#include "CoreMinimal.h"

#include "Json/JsonReader.h"
#include "Json/JsonValue.h"
#include "Json/JsonWriter.h"
#include "Object/ObjectGlobals.h"
#include "Object/ObjectMacros.h"
#include "Object/ObjectPtr.h"
#include "Scene/Component.h"
#include "Scene/Entity.h"

#include <doctest/doctest.h>

#include <string>
#include <string_view>
#include <vector>

/** A component with properties of several kinds, for the tests of the editor's commands. */
WCLASS()
class WEditorTestComponent : public WComponent
{
	WW_DECLARE_CLASS(WEditorTestComponent, WComponent)

public:
	WPROPERTY()
	int32 Health = 100;

	WPROPERTY(Category = "Text")
	std::string Label = "Unnamed";

	WPROPERTY()
	TObjectPtr<WEntity> TargetEntity;

	WPROPERTY()
	std::vector<int32> Scores;

	/** Not saved, so commands neither read nor change it. */
	WPROPERTY(Transient)
	float CachedDistance = 0.0f;
};

/** A component class derived from another one. */
WCLASS()
class WEditorTestDerivedComponent : public WEditorTestComponent
{
	WW_DECLARE_CLASS(WEditorTestDerivedComponent, WEditorTestComponent)

public:
	WPROPERTY()
	double Range = 1.5;
};

/** A component class that cannot be created. */
WCLASS()
class WEditorTestAbstractComponent : public WComponent
{
	WW_DECLARE_CLASS(WEditorTestAbstractComponent, WComponent)

public:
	virtual void DoSomething() = 0;
};

/** Isolates a test that creates an editor: collects garbage before and after it. Declare it before the editor. */
class FScopedEditorTest
{
public:
	FScopedEditorTest()
	{
		CollectGarbage();
	}

	~FScopedEditorTest()
	{
		CollectGarbage();
	}

	FScopedEditorTest(const FScopedEditorTest&) = delete;
	FScopedEditorTest& operator=(const FScopedEditorTest&) = delete;
};

/** Returns the JSON value that Text is, which a test expects to be valid JSON. */
inline FJsonValue ParseTestJson(std::string_view Text)
{
	FJsonValue Value;
	std::string Error;
	const bool bParsed = FJsonReader::Parse(Text, Value, Error);
	INFO("JSON error: ", Error);
	REQUIRE(bParsed);
	return Value;
}

/** Returns a JSON value as text without whitespace, which a failed comparison can print. */
inline std::string WriteTestJson(const FJsonValue& Value)
{
	return FJsonWriter::Write(Value, EJsonFormat::Compact);
}
