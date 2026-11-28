#include "Scene/SceneSerializer.h"

#include "Assets/Asset.h"
#include "Assets/AssetManager.h"
#include "Core/FileHelper.h"
#include "Json/JsonReader.h"
#include "Json/JsonValue.h"
#include "Json/JsonWriter.h"
#include "Object/Class.h"
#include "Object/ObjectGlobals.h"
#include "Reflection/ObjectProperties.h"
#include "Reflection/Property.h"
#include "Reflection/PropertyJsonContext.h"
#include "Scene/Component.h"
#include "Scene/Entity.h"
#include "Scene/MissingComponent.h"
#include "Scene/Scene.h"

#include <algorithm>
#include <cstddef>
#include <format>
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace
{
	constexpr std::string_view FormatName = "WhiteWolfScene";

	/** What a scene's objects are saved with, as the sentence about a property that is not saved says it. */
	constexpr std::string_view SavedWith = "a scene";
}

/** Writes one scene. It is also the context through which the scene's properties write their object references. */
class FSceneSerializer::FSaver final : private IPropertyJsonContext
{
public:
	explicit FSaver(const WScene& InScene)
		: Scene(InScene)
	{
	}

	FJsonValue Save()
	{
		FJsonValue EntityList = FJsonValue::MakeArray();
		for (const WEntity* const Entity : Scene.GetAllEntities())
		{
			EntityList.AddElement(SaveEntity(*Entity));
		}

		FJsonValue Document = FJsonValue::MakeObject();
		Document.SetMember("Format", FormatName);
		Document.SetMember("Version", FormatVersion);
		Document.SetMember("NextId", Scene.NextId);
		Document.SetMember("Properties", SaveProperties(Scene, "the scene"));
		Document.SetMember("Entities", std::move(EntityList));
		return Document;
	}

private:
	/** Writes every saved property of Object. OwnerDescription names the object in warnings. */
	FJsonValue SaveProperties(const WObject& Object, std::string OwnerDescription)
	{
		CurrentOwner = std::move(OwnerDescription);

		FJsonValue Json = FJsonValue::MakeObject();
		for (const FProperty* const Property : FObjectProperties::GetSaved(*Object.GetClass()))
		{
			CurrentProperty = Property->GetName();
			Json.SetMember(Property->GetName(), Property->ExportJson(Property->GetValuePtr(&Object), *this));
		}
		return Json;
	}

	FJsonValue SaveEntity(const WEntity& Entity)
	{
		FJsonValue Json = FJsonValue::MakeObject();
		Json.SetMember("Id", Entity.GetId());

		const WEntity* const ParentEntity = Entity.GetParent();
		if (ParentEntity != nullptr)
		{
			Json.SetMember("Parent", ParentEntity->GetId());
		}

		Json.SetMember(
			"Properties", SaveProperties(Entity, std::format("entity {} '{}'", Entity.GetId(), Entity.GetName())));

		FJsonValue ComponentList = FJsonValue::MakeArray();
		for (const TObjectPtr<WComponent>& Component : Entity.GetComponents())
		{
			if (Component != nullptr)
			{
				ComponentList.AddElement(SaveComponent(*Component, Entity));
			}
		}
		Json.SetMember("Components", std::move(ComponentList));
		return Json;
	}

	FJsonValue SaveComponent(const WComponent& Component, const WEntity& Entity)
	{
		FJsonValue Json = FJsonValue::MakeObject();
		Json.SetMember("Id", Component.GetId());

		// A component whose class was not available when the scene was loaded goes back exactly as it came.
		const WMissingComponent* const Missing = Cast<WMissingComponent>(&Component);
		if (Missing != nullptr && !Missing->MissingClassName.empty())
		{
			Json.SetMember("Class", Missing->MissingClassName);
			Json.SetMember("Properties", Missing->SavedProperties);
			return Json;
		}

		const std::string& ClassName = Component.GetClass()->GetName();
		Json.SetMember("Class", ClassName);
		Json.SetMember("Properties",
			SaveProperties(Component,
				std::format("component {} ({}) of entity {} '{}'",
					Component.GetId(),
					ClassName,
					Entity.GetId(),
					Entity.GetName())));
		return Json;
	}

	// IPropertyJsonContext
	FJsonValue ExportObjectReference(const WObject& Object) override
	{
		const WEntity* const Entity = Cast<WEntity>(&Object);
		if (Entity != nullptr && Entity->GetScene() == &Scene)
		{
			return FJsonValue(Entity->GetId());
		}

		const WComponent* const Component = Cast<WComponent>(&Object);
		if (Component != nullptr && Component->GetScene() == &Scene)
		{
			return FJsonValue(Component->GetId());
		}

		// An asset that was loaded from the content directory is written as the path it can be loaded by again.
		const WAsset* const Asset = Cast<WAsset>(&Object);
		if (Asset != nullptr && !Asset->GetAssetPath().empty())
		{
			return FJsonValue(Asset->GetAssetPath());
		}

		WW_LOG(LogScene,
			Warning,
			"Saving scene '{}': property {} of {} refers to a {} that is not part of the scene and has no asset "
			"path; the reference is saved as null",
			Scene.GetName(),
			CurrentProperty,
			CurrentOwner,
			Object.GetClass()->GetName());
		return FJsonValue();
	}

	bool ImportObjectReference(const FJsonValue& /*Json*/, WObject*& /*OutObject*/, std::string& OutError) override
	{
		OutError = "a scene that is being saved does not resolve references";
		return false;
	}

	const WScene& Scene;

	/** The object and property being written, for the warning about a reference that cannot be saved. */
	std::string CurrentOwner;
	std::string CurrentProperty;
};

/**
 * Reads one scene. The document is checked completely before anything is created, so a file with a structural
 * error leaves nothing behind. It is also the context through which properties resolve their object references.
 */
class FSceneSerializer::FLoader final : private IPropertyJsonContext
{
public:
	FLoader(const FJsonValue& InDocument, std::vector<std::string>* InWarnings)
		: Document(InDocument)
		, Warnings(InWarnings)
	{
	}

	WScene* Load(std::string& OutError)
	{
		if (!ReadDocument(OutError))
		{
			return nullptr;
		}

		Scene = NewObject<WScene>();
		if (!CreateEntities(OutError))
		{
			Scene->Destroy();
			return nullptr;
		}

		// Every object exists before any property is read, so that a property can refer to an object that comes
		// later in the file. Components hear that they are attached last, when their data is complete.
		CreateComponents();
		LoadAllProperties();
		NotifyComponentsAttached();

		Scene->NextId = std::max(Scene->NextId, SavedNextId);
		return Scene;
	}

private:
	/** What the document says about one component. */
	struct FComponentRecord
	{
		int64 Id = 0;
		std::string ClassName;

		/** The component's "Properties" object, or null if it has none. */
		const FJsonValue* Properties = nullptr;

		WComponent* Component = nullptr;

		/** True if the class is not available and the component was created as a WMissingComponent. */
		bool bIsMissing = false;
	};

	/** What the document says about one entity. */
	struct FEntityRecord
	{
		int64 Id = 0;
		bool bHasParent = false;
		int64 ParentId = 0;

		/** The entity's "Properties" object, or null if it has none. */
		const FJsonValue* Properties = nullptr;

		std::vector<FComponentRecord> Components;
		WEntity* Entity = nullptr;
	};

	void AddWarning(std::string Message)
	{
		WW_LOG(LogScene, Warning, "Loading scene: {}", Message);
		if (Warnings != nullptr)
		{
			Warnings->push_back(std::move(Message));
		}
	}

	/** Warns about every member of Json whose key is not one of KnownKeys; such a key is usually a typing mistake. */
	void WarnAboutUnknownKeys(
		const FJsonValue& Json, std::initializer_list<std::string_view> KnownKeys, std::string_view Description)
	{
		for (const FJsonValue::FMember& Member : Json.GetMembers())
		{
			if (std::ranges::find(KnownKeys, std::string_view(Member.first)) == KnownKeys.end())
			{
				AddWarning(std::format("{} has the unknown key \"{}\", which was ignored", Description, Member.first));
			}
		}
	}

	/** Reads the "Id" of an entity or component and makes sure no other object has it. */
	bool ReadId(const FJsonValue& Json, std::string_view Description, int64& OutId, std::string& OutError)
	{
		const FJsonValue* const IdValue = Json.FindMember("Id");
		if (IdValue == nullptr || !IdValue->IsInteger() || IdValue->GetInteger() < 1)
		{
			OutError = std::format("{} needs an \"Id\" that is a positive whole number", Description);
			return false;
		}

		OutId = IdValue->GetInteger();
		if (!UsedIds.insert(OutId).second)
		{
			OutError = std::format("the identifier {} is used by more than one entity or component", OutId);
			return false;
		}

		return true;
	}

	/** Reads the optional "Properties" member of Json, which must be an object if it is there. */
	bool ReadProperties(
		const FJsonValue& Json, std::string_view Description, const FJsonValue*& OutProperties, std::string& OutError)
	{
		const FJsonValue* const PropertiesValue = Json.FindMember("Properties");
		if (PropertiesValue != nullptr && !PropertiesValue->IsObject())
		{
			OutError = std::format(
				"\"Properties\" of {} must be an object, not {}", Description, PropertiesValue->GetTypeDescription());
			return false;
		}

		OutProperties = PropertiesValue;
		return true;
	}

	/** Checks the whole document and fills in the records. Creates no objects. */
	bool ReadDocument(std::string& OutError)
	{
		if (!Document.IsObject())
		{
			OutError = std::format("a scene must be a JSON object, not {}", Document.GetTypeDescription());
			return false;
		}

		const FJsonValue* const FormatValue = Document.FindMember("Format");
		if (FormatValue == nullptr || !FormatValue->IsString() || FormatValue->GetString() != FormatName)
		{
			OutError = std::format("this is not a scene: \"Format\" must be \"{}\"", FormatName);
			return false;
		}

		const FJsonValue* const VersionValue = Document.FindMember("Version");
		if (VersionValue == nullptr || !VersionValue->IsInteger() || VersionValue->GetInteger() < 1)
		{
			OutError = "\"Version\" must be a positive whole number";
			return false;
		}
		if (VersionValue->GetInteger() > FormatVersion)
		{
			OutError =
				std::format("the scene was saved in format version {}, but this build reads only up to version {}",
					VersionValue->GetInteger(),
					FormatVersion);
			return false;
		}

		const FJsonValue* const NextIdValue = Document.FindMember("NextId");
		if (NextIdValue != nullptr)
		{
			if (!NextIdValue->IsInteger() || NextIdValue->GetInteger() < 1)
			{
				OutError = "\"NextId\" must be a positive whole number";
				return false;
			}
			SavedNextId = NextIdValue->GetInteger();
		}

		if (!ReadProperties(Document, "the scene", SceneProperties, OutError))
		{
			return false;
		}

		const FJsonValue* const EntityList = Document.FindMember("Entities");
		if (EntityList != nullptr)
		{
			if (!EntityList->IsArray())
			{
				OutError = std::format("\"Entities\" must be an array, not {}", EntityList->GetTypeDescription());
				return false;
			}

			const FJsonValue::FArray& Elements = EntityList->GetElements();
			Entities.reserve(Elements.size());
			for (std::size_t Index = 0; Index < Elements.size(); ++Index)
			{
				FEntityRecord Record;
				if (!ReadEntity(Elements[Index], Index, Record, OutError))
				{
					return false;
				}
				Entities.push_back(std::move(Record));
			}
		}

		std::unordered_set<int64> EntityIds;
		for (const FEntityRecord& Record : Entities)
		{
			EntityIds.insert(Record.Id);
		}
		for (const FEntityRecord& Record : Entities)
		{
			if (Record.bHasParent && !EntityIds.contains(Record.ParentId))
			{
				OutError = std::format(
					"entity {} has the parent {}, which is not an entity of this scene", Record.Id, Record.ParentId);
				return false;
			}
		}

		WarnAboutUnknownKeys(Document, {"Format", "Version", "NextId", "Properties", "Entities"}, "the scene");
		return true;
	}

	bool ReadEntity(const FJsonValue& Json, std::size_t Position, FEntityRecord& OutRecord, std::string& OutError)
	{
		if (!Json.IsObject())
		{
			OutError = std::format(
				"element {} of \"Entities\" must be an object, not {}", Position, Json.GetTypeDescription());
			return false;
		}
		if (!ReadId(Json, std::format("element {} of \"Entities\"", Position), OutRecord.Id, OutError))
		{
			return false;
		}

		const std::string Description = std::format("entity {}", OutRecord.Id);

		const FJsonValue* const ParentValue = Json.FindMember("Parent");
		if (ParentValue != nullptr && !ParentValue->IsNull())
		{
			if (!ParentValue->IsInteger())
			{
				OutError = std::format("\"Parent\" of {} must be the identifier of an entity or null, not {}",
					Description,
					ParentValue->GetTypeDescription());
				return false;
			}
			OutRecord.bHasParent = true;
			OutRecord.ParentId = ParentValue->GetInteger();
		}

		if (!ReadProperties(Json, Description, OutRecord.Properties, OutError))
		{
			return false;
		}

		const FJsonValue* const ComponentList = Json.FindMember("Components");
		if (ComponentList != nullptr)
		{
			if (!ComponentList->IsArray())
			{
				OutError = std::format(
					"\"Components\" of {} must be an array, not {}", Description, ComponentList->GetTypeDescription());
				return false;
			}

			for (const FJsonValue& Element : ComponentList->GetElements())
			{
				FComponentRecord ComponentRecord;
				if (!ReadComponent(Element, Description, ComponentRecord, OutError))
				{
					return false;
				}
				OutRecord.Components.push_back(std::move(ComponentRecord));
			}
		}

		WarnAboutUnknownKeys(Json, {"Id", "Parent", "Properties", "Components"}, Description);
		return true;
	}

	bool ReadComponent(
		const FJsonValue& Json, std::string_view EntityDescription, FComponentRecord& OutRecord, std::string& OutError)
	{
		if (!Json.IsObject())
		{
			OutError = std::format("every element of \"Components\" of {} must be an object, not {}",
				EntityDescription,
				Json.GetTypeDescription());
			return false;
		}
		if (!ReadId(Json, std::format("a component of {}", EntityDescription), OutRecord.Id, OutError))
		{
			return false;
		}

		const std::string Description = std::format("component {} of {}", OutRecord.Id, EntityDescription);

		const FJsonValue* const ClassValue = Json.FindMember("Class");
		if (ClassValue == nullptr || !ClassValue->IsString() || ClassValue->GetString().empty())
		{
			OutError = std::format("{} needs a \"Class\" that is the name of a component class", Description);
			return false;
		}
		OutRecord.ClassName = ClassValue->GetString();

		if (!ReadProperties(Json, Description, OutRecord.Properties, OutError))
		{
			return false;
		}

		WarnAboutUnknownKeys(Json, {"Id", "Class", "Properties"}, Description);
		return true;
	}

	/** Creates every entity, in file order, and then puts each one under its parent. */
	bool CreateEntities(std::string& OutError)
	{
		std::unordered_map<int64, WEntity*> EntitiesById;
		for (FEntityRecord& Record : Entities)
		{
			Record.Entity = Scene->CreateEntityWithId(std::string(), nullptr, Record.Id);
			EntitiesById.emplace(Record.Id, Record.Entity);
		}

		for (const FEntityRecord& Record : Entities)
		{
			if (!Record.bHasParent)
			{
				continue;
			}

			// The transforms have not been loaded yet, so there is no world position to preserve.
			WEntity* const ParentEntity = EntitiesById.find(Record.ParentId)->second;
			if (!Record.Entity->SetParent(ParentEntity, ETransformRule::KeepRelative))
			{
				OutError = std::format("entity {} cannot have the parent {}: an entity cannot be its own ancestor",
					Record.Id,
					Record.ParentId);
				return false;
			}
		}

		return true;
	}

	void CreateComponents()
	{
		for (FEntityRecord& Record : Entities)
		{
			for (FComponentRecord& ComponentRecord : Record.Components)
			{
				WClass* const Class = WClass::FindByName(ComponentRecord.ClassName);
				std::string_view Problem;
				if (Class == nullptr)
				{
					Problem = "is not available in this build";
				}
				else if (!Class->IsChildOf(WComponent::StaticClass()))
				{
					Problem = "is not a component class";
				}
				else if (Class->IsAbstract())
				{
					Problem = "is abstract";
				}

				if (Problem.empty())
				{
					ComponentRecord.Component = Record.Entity->CreateComponentWithId(Class, ComponentRecord.Id);
					continue;
				}

				WMissingComponent* const Missing = static_cast<WMissingComponent*>(
					Record.Entity->CreateComponentWithId(WMissingComponent::StaticClass(), ComponentRecord.Id));
				Missing->MissingClassName = ComponentRecord.ClassName;
				if (ComponentRecord.Properties != nullptr)
				{
					Missing->SavedProperties = *ComponentRecord.Properties;
				}
				ComponentRecord.Component = Missing;
				ComponentRecord.bIsMissing = true;

				AddWarning(std::format("component {} of entity {}: the class {} {}; its data is kept and will be "
									   "saved again unchanged",
					ComponentRecord.Id,
					Record.Id,
					ComponentRecord.ClassName,
					Problem));
			}
		}
	}

	void LoadAllProperties()
	{
		LoadProperties(*Scene, SceneProperties, "the scene");

		for (const FEntityRecord& Record : Entities)
		{
			LoadProperties(*Record.Entity, Record.Properties, std::format("entity {}", Record.Id));

			for (const FComponentRecord& ComponentRecord : Record.Components)
			{
				if (!ComponentRecord.bIsMissing)
				{
					LoadProperties(*ComponentRecord.Component,
						ComponentRecord.Properties,
						std::format("component {} ({}) of entity {}",
							ComponentRecord.Id,
							ComponentRecord.ClassName,
							Record.Id));
				}
			}
		}
	}

	/** Sets the properties of Object from a "Properties" object, which may be null. Problems become warnings. */
	void LoadProperties(WObject& Object, const FJsonValue* Properties, std::string_view Description)
	{
		if (Properties == nullptr)
		{
			return;
		}

		std::vector<std::string> Problems;
		FObjectProperties::Import(Object, *Properties, *this, SavedWith, Problems);
		for (const std::string& Problem : Problems)
		{
			AddWarning(std::format("{}: {}", Description, Problem));
		}
	}

	void NotifyComponentsAttached()
	{
		for (const FEntityRecord& Record : Entities)
		{
			for (const FComponentRecord& ComponentRecord : Record.Components)
			{
				Record.Entity->NotifyComponentAttached(*ComponentRecord.Component);
			}
		}
	}

	// IPropertyJsonContext
	FJsonValue ExportObjectReference(const WObject& /*Object*/) override
	{
		return FJsonValue();
	}

	bool ImportObjectReference(const FJsonValue& Json, WObject*& OutObject, std::string& OutError) override
	{
		if (Json.IsString())
		{
			// The path of an asset, which is loaded if it is not loaded yet.
			WAsset* const Asset = FAssetManager::Load(Json.GetString(), OutError);
			if (Asset == nullptr)
			{
				return false;
			}
			OutObject = Asset;
			return true;
		}
		if (!Json.IsInteger())
		{
			OutError =
				std::format("expected the identifier of an entity or component, or the path of an asset, but found {}",
					Json.GetTypeDescription());
			return false;
		}

		WObject* const Object = Scene->FindObjectById(Json.GetInteger());
		if (Object == nullptr)
		{
			OutError = std::format("there is no entity or component with the identifier {}", Json.GetInteger());
			return false;
		}

		OutObject = Object;
		return true;
	}

	const FJsonValue& Document;
	std::vector<std::string>* Warnings;

	WScene* Scene = nullptr;
	const FJsonValue* SceneProperties = nullptr;
	int64 SavedNextId = 1;
	std::vector<FEntityRecord> Entities;
	std::unordered_set<int64> UsedIds;
};

/**
 * Copies one entity with everything below it, within its scene. It is also the context through which the
 * properties of the originals write their object references and those of the copies read them: a reference is
 * written as a place in a list of the objects referred to, which lasts no longer than the copying of one property.
 */
class FSceneSerializer::FDuplicator final : private IPropertyJsonContext
{
public:
	explicit FDuplicator(WScene& InScene)
		: Scene(InScene)
	{
	}

	WEntity* Duplicate(const WEntity& Source)
	{
		// Every copy exists before any property is copied, so that a copy can refer to a copy that is made later.
		// Components hear that they are attached last, when their data is complete.
		WEntity* const Copy = CopyEntityTree(Source, Source.GetParent());
		CopyComponents();
		CopyAllProperties();
		NotifyComponentsAttached();
		return Copy;
	}

private:
	/** An entity or a component, and the copy that was made of it. */
	template <typename ObjectType>
	struct TCopied
	{
		const ObjectType* Original = nullptr;
		ObjectType* Copy = nullptr;
	};

	/** Creates the copy of an entity under CopyParent, and below it the copies of the entity's descendants. */
	WEntity* CopyEntityTree(const WEntity& Original, WEntity* CopyParent)
	{
		WEntity* const Copy = Scene.CreateEntity(std::string(), CopyParent);
		Entities.push_back({&Original, Copy});
		Copies.emplace(&Original, Copy);

		// The copies are added to the children of other copies, never to the list that is gone through here.
		for (const TObjectPtr<WEntity>& Child : Original.GetChildren())
		{
			if (Child != nullptr)
			{
				CopyEntityTree(*Child, Copy);
			}
		}
		return Copy;
	}

	void CopyComponents()
	{
		for (const TCopied<WEntity>& Entity : Entities)
		{
			for (const TObjectPtr<WComponent>& Original : Entity.Original->GetComponents())
			{
				if (Original == nullptr)
				{
					continue;
				}

				WComponent* const Copy = Entity.Copy->CreateComponentWithId(Original->GetClass(), Scene.AllocateId());
				Components.push_back({Original.Get(), Copy});
				Copies.emplace(Original.Get(), Copy);

				// A component whose class is not available has no properties to copy by reflection: it is what
				// the scene file said, and so is its copy.
				if (const WMissingComponent* const Missing = Cast<WMissingComponent>(Original.Get()))
				{
					WMissingComponent* const MissingCopy = static_cast<WMissingComponent*>(Copy);
					MissingCopy->MissingClassName = Missing->MissingClassName;
					MissingCopy->SavedProperties = Missing->SavedProperties;
				}
			}
		}
	}

	void CopyAllProperties()
	{
		for (const TCopied<WEntity>& Entity : Entities)
		{
			CopyProperties(*Entity.Original, *Entity.Copy);
		}
		for (const TCopied<WComponent>& Component : Components)
		{
			if (Cast<WMissingComponent>(Component.Original) == nullptr)
			{
				CopyProperties(*Component.Original, *Component.Copy);
			}
		}
	}

	/** Gives Copy the value of every saved property of Original, which is of the same class. */
	void CopyProperties(const WObject& Original, WObject& Copy)
	{
		for (const FProperty* const Property : FObjectProperties::GetSaved(*Original.GetClass()))
		{
			References.clear();
			const FJsonValue Value = Property->ExportJson(Property->GetValuePtr(&Original), *this);

			// A value that a property has just written is one that it can read.
			std::string Error;
			const bool bCopied = Property->ImportJson(Property->GetValuePtr(&Copy), Value, *this, Error);
			WW_CHECKF(bCopied,
				"The property {} of a {} could not be copied: {}",
				Property->GetName(),
				Original.GetClass()->GetName(),
				Error);
		}
	}

	void NotifyComponentsAttached()
	{
		for (const TCopied<WEntity>& Entity : Entities)
		{
			for (const TObjectPtr<WComponent>& Component : Entity.Copy->GetComponents())
			{
				Entity.Copy->NotifyComponentAttached(*Component);
			}
		}
	}

	// IPropertyJsonContext
	FJsonValue ExportObjectReference(const WObject& Object) override
	{
		References.push_back(&Object);
		return FJsonValue(static_cast<int64>(References.size() - 1));
	}

	bool ImportObjectReference(const FJsonValue& Json, WObject*& OutObject, std::string& OutError) override
	{
		const bool bIsKnown = Json.IsInteger() && Json.GetInteger() >= 0 &&
			static_cast<std::size_t>(Json.GetInteger()) < References.size();
		if (!bIsKnown)
		{
			OutError = "the reference is not one that was written while the property was copied";
			return false;
		}

		const WObject* const Original = References[static_cast<std::size_t>(Json.GetInteger())];
		const auto Copied = Copies.find(Original);
		// What was not copied is referred to as the original refers to it, which is through a property that does
		// not hold its object as constant.
		OutObject = Copied != Copies.end() ? Copied->second : const_cast<WObject*>(Original);
		return true;
	}

	WScene& Scene;

	/** The copies that were made, in the order of the tree: parents before children. */
	std::vector<TCopied<WEntity>> Entities;
	std::vector<TCopied<WComponent>> Components;

	/** The copy of every entity and component that was copied. */
	std::unordered_map<const WObject*, WObject*> Copies;

	/** The objects that the property being copied refers to, in the order it wrote them. */
	std::vector<const WObject*> References;
};
FJsonValue FSceneSerializer::SaveToJson(const WScene& Scene)
{
	FSaver Saver(Scene);
	return Saver.Save();
}

std::string FSceneSerializer::SaveToText(const WScene& Scene)
{
	return FJsonWriter::Write(SaveToJson(Scene), EJsonFormat::Pretty) + "\n";
}

bool FSceneSerializer::SaveToFile(const WScene& Scene, const std::filesystem::path& Path, std::string& OutError)
{
	return FFileHelper::SaveStringToFile(SaveToText(Scene), Path, OutError);
}

WScene* FSceneSerializer::LoadFromJson(
	const FJsonValue& Json, std::string& OutError, std::vector<std::string>* OutWarnings)
{
	FLoader Loader(Json, OutWarnings);
	return Loader.Load(OutError);
}

WScene* FSceneSerializer::LoadFromText(
	std::string_view Text, std::string& OutError, std::vector<std::string>* OutWarnings)
{
	FJsonValue Json;
	std::string ParseError;
	if (!FJsonReader::Parse(Text, Json, ParseError))
	{
		OutError = std::format("the scene is not valid JSON: {}", ParseError);
		return nullptr;
	}

	return LoadFromJson(Json, OutError, OutWarnings);
}

WScene* FSceneSerializer::LoadFromFile(
	const std::filesystem::path& Path, std::string& OutError, std::vector<std::string>* OutWarnings)
{
	std::string Text;
	if (!FFileHelper::LoadFileToString(Path, Text, OutError))
	{
		return nullptr;
	}

	std::string LoadError;
	WScene* const Scene = LoadFromText(Text, LoadError, OutWarnings);
	if (Scene == nullptr)
	{
		OutError = std::format("{}: {}", FFileHelper::PathToUtf8(Path), LoadError);
	}
	return Scene;
}

WEntity* FSceneSerializer::DuplicateEntity(const WEntity& Source)
{
	WScene* const Scene = IsValid(&Source) ? Source.GetScene() : nullptr;
	if (Scene == nullptr)
	{
		WW_LOG(LogScene, Warning, "An entity that has been destroyed cannot be copied");
		return nullptr;
	}

	FDuplicator Duplicator(*Scene);
	return Duplicator.Duplicate(Source);
}
