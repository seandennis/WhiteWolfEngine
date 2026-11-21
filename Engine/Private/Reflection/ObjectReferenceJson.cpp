#include "Reflection/ObjectReferenceJson.h"

#include "Json/JsonValue.h"
#include "Object/Class.h"
#include "Object/Object.h"
#include "Object/ObjectGlobals.h"
#include "Reflection/PropertyJsonContext.h"

#include <format>

FJsonValue FObjectReferenceJson::Export(const WObject* Object, IPropertyJsonContext& Context)
{
	return IsValid(Object) ? Context.ExportObjectReference(*Object) : FJsonValue();
}

bool FObjectReferenceJson::Import(const FJsonValue& Json,
	const WClass& RequiredClass,
	IPropertyJsonContext& Context,
	WObject*& OutObject,
	std::string& OutError)
{
	if (Json.IsNull())
	{
		OutObject = nullptr;
		return true;
	}

	WObject* Object = nullptr;
	if (!Context.ImportObjectReference(Json, Object, OutError))
	{
		return false;
	}
	WW_CHECKF(Object != nullptr, "ImportObjectReference reported success without an object");

	if (!Object->IsA(&RequiredClass))
	{
		OutError = std::format(
			"the reference is to a {}, which is not a {}", Object->GetClass()->GetName(), RequiredClass.GetName());
		return false;
	}

	OutObject = Object;
	return true;
}
