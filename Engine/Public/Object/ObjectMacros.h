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
 * Marks the class that follows as reflected:
 *
 *     WCLASS()
 *     class ENGINE_API WDoor : public WObject
 *     {
 *         WW_DECLARE_CLASS(WDoor, WObject)
 *
 *     public:
 *         WPROPERTY(Category = "Door")
 *         TObjectPtr<WHinge> Hinge;
 *
 *         WFUNCTION(BlueprintCallable)
 *         void Open(float Speed);
 *     };
 *
 * The build runs WhiteWolfHeaderTool over the module's headers, and the tool writes the code that registers the
 * class with its properties and functions; nothing has to be written in the source file. The macros themselves
 * expand to nothing.
 */
#define WCLASS(...)

/**
 * Marks the member variable that follows as a reflected property: it is saved and loaded with the object, shown in
 * the editor, and followed by the garbage collector if it holds object references.
 *
 * Specifiers: Transient (not saved), BlueprintReadOnly, BlueprintReadWrite, Category = "Name".
 */
#define WPROPERTY(...)

/**
 * Marks the member function that follows as a reflected function, which can be called by name. The function must
 * not be static or overloaded, and every parameter needs a name.
 *
 * Specifiers: BlueprintCallable, BlueprintPure, Category = "Name".
 */
#define WFUNCTION(...)

/**
 * Declares the reflection members of a WObject class. Put it first in the class body, as shown for WCLASS.
 *
 * It declares StaticClass(), the Super and ThisClass aliases and RegisterClassMembers(), and leaves the access
 * level private. For a class marked with WCLASS() the header tool defines these functions. A class without
 * WCLASS() needs a WW_IMPLEMENT_CLASS and a definition of RegisterClassMembers in its source file.
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
 * The header tool writes this for every class marked with WCLASS(). To register a class by hand instead, put it in
 * the class's source file, together with the definition of RegisterClassMembers:
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
