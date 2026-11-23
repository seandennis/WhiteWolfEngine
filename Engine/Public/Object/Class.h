#pragma once

#include "CoreMinimal.h"

#include "Object/Object.h"
#include "Reflection/Function.h"
#include "Reflection/Property.h"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

/** Everything the engine needs to know to register a class written in C++. */
struct FNativeClassDescription
{
	/** The class name, including its prefix, for example "WStaticMeshComponent". Must be unique. */
	const char* Name = nullptr;

	/** sizeof and alignof the class. */
	std::size_t Size = 0;
	std::size_t Alignment = 0;

	/** Returns the class of the direct base class. Null only for WObject. */
	WClass* (*GetSuperClass)() = nullptr;

	/**
	 * Constructs an instance in the given memory, which has the size and alignment above, and returns it. Null if
	 * the class cannot be instantiated.
	 */
	WObject* (*Construct)(void* Memory) = nullptr;

	/** Adds the class's reflected members to the class object. May be null. */
	void (*RegisterMembers)(WClass& Class) = nullptr;
};

/**
 * Describes a class of objects at run time: its name, its base class, how to create an instance and its reflected
 * properties. Every WObject class has exactly one WClass, returned by its StaticClass() and by GetClass() on its
 * instances.
 *
 * Class objects are created by registration, never with NewObject.
 */
class ENGINE_API WClass : public WObject
{
public:
	WClass();

	/** Returns the class that describes WClass itself. */
	static WClass* StaticClass();

	/**
	 * Registers a class written in C++ and returns its class object. The base class is registered first if it has
	 * not been yet. Registering two classes with the same name is a fatal error.
	 *
	 * Called by the code that WW_IMPLEMENT_CLASS generates; there is normally no reason to call it directly.
	 */
	static WClass* RegisterNativeClass(const FNativeClassDescription& Description);

	/** Returns the registered class with the given name, or null if there is none. */
	static WClass* FindByName(std::string_view ClassName);

	/**
	 * Returns every registered class, of every module that is loaded, in the order they were registered: a class
	 * always comes after its base class. This is how to find the classes that derive from one, such as the
	 * components there are, without naming them.
	 */
	static std::vector<WClass*> GetAllClasses();

	/** Returns the class name, including its prefix. */
	const std::string& GetName() const
	{
		return Name;
	}

	/** Returns the direct base class, or null for WObject's class. */
	WClass* GetSuperClass() const
	{
		return SuperClass;
	}

	/** Returns true if this class is Other or derives from it. */
	bool IsChildOf(const WClass* Other) const;

	/** Returns true if NewObject cannot create instances of this class. */
	bool IsAbstract() const
	{
		return ConstructFunction == nullptr;
	}

	/**
	 * Adds a reflected property to this class. Only valid while the class is being registered. The name must not
	 * be empty or already used by this class or one of its base classes.
	 */
	void AddProperty(std::unique_ptr<FProperty> Property);

	/** Returns the properties this class declares itself, in declaration order, without inherited ones. */
	const std::vector<std::unique_ptr<FProperty>>& GetOwnProperties() const
	{
		return OwnProperties;
	}

	/** Returns the property with the given name, declared by this class or a base class, or null. */
	const FProperty* FindProperty(std::string_view PropertyName) const;

	/**
	 * Returns every property, inherited ones included, whose value can hold strong object references. This is the
	 * list the garbage collector walks for each instance.
	 */
	const std::vector<const FProperty*>& GetReferenceProperties() const
	{
		return ReferenceProperties;
	}

	/**
	 * Adds a reflected function to this class. Only valid while the class is being registered. The name must not
	 * be empty or already used by a function of this class or one of its base classes.
	 */
	void AddFunction(std::unique_ptr<FFunction> Function);

	/** Returns the functions this class declares itself, in declaration order, without inherited ones. */
	const std::vector<std::unique_ptr<FFunction>>& GetOwnFunctions() const
	{
		return OwnFunctions;
	}

	/** Returns the function with the given name, declared by this class or a base class, or null. */
	const FFunction* FindFunction(std::string_view FunctionName) const;

private:
	friend class FObjectSystem;

	std::string Name;
	WClass* SuperClass = nullptr;
	std::size_t Size = 0;
	std::size_t Alignment = 0;
	WObject* (*ConstructFunction)(void* Memory) = nullptr;
	std::vector<std::unique_ptr<FProperty>> OwnProperties;
	std::vector<const FProperty*> ReferenceProperties;
	std::vector<std::unique_ptr<FFunction>> OwnFunctions;
	bool bIsRegistering = false;
};
