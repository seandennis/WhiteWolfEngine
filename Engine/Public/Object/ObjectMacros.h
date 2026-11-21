#pragma once

#include "CoreMinimal.h"

#include "Object/Class.h"

#include <new>
#include <type_traits>

/**
 * Builds the registration data for a class written in C++.
 *
 * A class that can be default-constructed gets a construct function; an abstract class does not, which is what
 * makes NewObject refuse to create it.
 */
template <typename ClassType>
FNativeClassDescription MakeNativeClassDescription(const char* Name, void (*RegisterMembers)(WClass& Class))
{
	FNativeClassDescription Description;
	Description.Name = Name;
	Description.Size = sizeof(ClassType);
	Description.Alignment = alignof(ClassType);
	Description.GetSuperClass = &ClassType::Super::StaticClass;
	Description.RegisterMembers = RegisterMembers;
	if constexpr (!std::is_abstract_v<ClassType>)
	{
		Description.Construct = [](void* Memory) -> WObject*
		{
			return ::new (Memory) ClassType();
		};
	}
	return Description;
}

/** Registers a class when the module that contains it is loaded, so the class exists even if no code names it. */
class FNativeClassAutoRegistration
{
public:
	explicit FNativeClassAutoRegistration(WClass* (*GetStaticClass)())
	{
		GetStaticClass();
	}
};

/**
 * Declares the reflection members of a WObject class. Put it first in the class body:
 *
 *     class ENGINE_API WDoor : public WObject
 *     {
 *         WW_DECLARE_CLASS(WDoor, WObject)
 *
 *     public:
 *         TObjectPtr<WHinge> Hinge;
 *     };
 *
 * It declares StaticClass(), the Super and ThisClass aliases and RegisterClassMembers(), and leaves the access
 * level private. The class needs a matching WW_IMPLEMENT_CLASS and a definition of RegisterClassMembers in its
 * source file.
 */
#define WW_DECLARE_CLASS(ClassName, SuperClassName) \
public: \
	using Super = SuperClassName; \
	using ThisClass = ClassName; \
	static WClass* StaticClass(); \
\
private: \
	static void RegisterClassMembers(WClass& Class);

/**
 * Defines StaticClass() for a class declared with WW_DECLARE_CLASS and registers the class when its module loads.
 * Put it in the class's source file, together with the definition of RegisterClassMembers:
 *
 *     WW_IMPLEMENT_CLASS(WDoor)
 *
 *     void WDoor::RegisterClassMembers(WClass& Class)
 *     {
 *         Class.AddProperty(MakeProperty<&WDoor::Hinge>("Hinge"));
 *     }
 */
#define WW_IMPLEMENT_CLASS(ClassName) \
	WClass* ClassName::StaticClass() \
	{ \
		static WClass* const Class = WClass::RegisterNativeClass( \
			MakeNativeClassDescription<ClassName>(#ClassName, &ClassName::RegisterClassMembers)); \
		return Class; \
	} \
	static const FNativeClassAutoRegistration ClassName##AutoRegistration(&ClassName::StaticClass);
