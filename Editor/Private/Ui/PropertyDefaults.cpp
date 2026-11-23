#include "Ui/PropertyDefaults.h"

#include "Commands/SavedProperties.h"
#include "Commands/ScenePropertyContext.h"
#include "Object/Class.h"
#include "Object/ObjectGlobals.h"
#include "Reflection/Property.h"

#include <utility>

const FJsonValue* FPropertyDefaults::Find(WClass& Class, const FProperty& Property, const WScene& Scene)
{
	auto Found = ValuesByClass.find(&Class);
	if (Found == ValuesByClass.end())
	{
		FValues Values;
		if (!Class.IsAbstract())
		{
			// The object belongs to nothing: no entity, no scene. It is read here and then is garbage.
			const WObject* const NewlyCreated = NewObject(&Class);
			FScenePropertyContext Context(Scene);
			for (const FProperty* const Saved : FSavedProperties::GetAll(Class))
			{
				Values.emplace(Saved, Saved->ExportJson(Saved->GetValuePtr(NewlyCreated), Context));
			}
		}
		Found = ValuesByClass.emplace(&Class, std::move(Values)).first;
	}

	const auto Value = Found->second.find(&Property);
	return Value != Found->second.end() ? &Value->second : nullptr;
}
