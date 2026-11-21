#include "Reflection/Function.h"

#include "Object/Class.h"
#include "Object/ObjectGlobals.h"
#include "Object/ObjectPtr.h"
#include "Object/TestObjects.h"
#include "Reflection/FunctionFactory.h"
#include "Reflection/ObjectProperty.h"
#include "Reflection/TestReflectionObjects.h"
#include "TestSupport/FatalErrorException.h"
#include "TestSupport/ScopedLogCapture.h"

#include <doctest/doctest.h>

#include <array>
#include <memory>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace
{
	const FFunction& FindTestFunction(std::string_view Name)
	{
		const FFunction* const Function = WTestValueObject::StaticClass()->FindFunction(Name);
		REQUIRE(Function != nullptr);
		return *Function;
	}
}

TEST_SUITE("FFunction")
{
	TEST_CASE("A class lists the functions it declares, in declaration order")
	{
		const std::vector<std::unique_ptr<FFunction>>& Functions = WTestValueObject::StaticClass()->GetOwnFunctions();

		REQUIRE(Functions.size() == 11);
		CHECK(Functions[0]->GetName() == "Add");
		CHECK(Functions[1]->GetName() == "SetText");
		CHECK(Functions[10]->GetName() == "Describe");
		CHECK(WTestDerivedValueObject::StaticClass()->GetOwnFunctions().empty());
	}

	TEST_CASE("FindFunction finds declared and inherited functions")
	{
		WClass* const Derived = WTestDerivedValueObject::StaticClass();

		CHECK(Derived->FindFunction("Add") == WTestValueObject::StaticClass()->FindFunction("Add"));
		CHECK(Derived->FindFunction("add") == nullptr);
		CHECK(Derived->FindFunction("") == nullptr);
		CHECK(WObject::StaticClass()->FindFunction("Add") == nullptr);
	}

	TEST_CASE("A function knows its owner class, flags and category")
	{
		const FFunction& Add = FindTestFunction("Add");
		const FFunction& SetText = FindTestFunction("SetText");
		const FFunction& CountCall = FindTestFunction("CountCall");

		CHECK(Add.GetOwnerClass() == WTestValueObject::StaticClass());
		CHECK(Add.HasAnyFlags(EFunctionFlags::BlueprintPure));
		CHECK_FALSE(Add.HasAnyFlags(EFunctionFlags::BlueprintCallable));
		CHECK(Add.GetCategory() == "Math");

		CHECK(SetText.HasAnyFlags(EFunctionFlags::BlueprintCallable));
		CHECK(SetText.GetCategory().empty());

		CHECK(CountCall.GetFlags() == EFunctionFlags::None);
	}

	TEST_CASE("A const member function gets the Const flag")
	{
		CHECK(FindTestFunction("Add").HasAnyFlags(EFunctionFlags::Const));
		CHECK(FindTestFunction("GetTextCopy").HasAnyFlags(EFunctionFlags::Const));
		CHECK_FALSE(FindTestFunction("SetText").HasAnyFlags(EFunctionFlags::Const));
	}

	TEST_CASE("Parameters are described by properties named after them")
	{
		const std::vector<std::unique_ptr<FProperty>>& Parameters = FindTestFunction("TryDivide").GetParameters();

		REQUIRE(Parameters.size() == 3);
		CHECK(Parameters[0]->GetName() == "Numerator");
		CHECK(Parameters[0]->GetTypeName() == "int32");
		CHECK_FALSE(Parameters[0]->HasAnyFlags(EPropertyFlags::OutParameter));
		CHECK(Parameters[1]->GetName() == "Denominator");
		CHECK(Parameters[2]->GetName() == "OutQuotient");
		CHECK(Parameters[2]->GetTypeName() == "int32");
		CHECK(Parameters[2]->HasAnyFlags(EPropertyFlags::OutParameter));
	}

	TEST_CASE("A const reference parameter is an input of the plain value type")
	{
		const std::vector<std::unique_ptr<FProperty>>& Parameters = FindTestFunction("SetText").GetParameters();

		REQUIRE(Parameters.size() == 1);
		CHECK(Parameters[0]->GetTypeName() == "std::string");
		CHECK_FALSE(Parameters[0]->HasAnyFlags(EPropertyFlags::OutParameter));
	}

	TEST_CASE("An object pointer parameter or result is described as a TObjectPtr")
	{
		const FFunction& SetStrong = FindTestFunction("SetStrong");
		const FFunction& GetStrong = FindTestFunction("GetStrong");

		REQUIRE(SetStrong.GetParameters().size() == 1);
		CHECK(SetStrong.GetParameters()[0]->GetTypeName() == "TObjectPtr<WTestValueObject>");
		REQUIRE(GetStrong.GetReturnProperty() != nullptr);
		CHECK(GetStrong.GetReturnProperty()->GetTypeName() == "TObjectPtr<WTestValueObject>");
		CHECK(dynamic_cast<const FObjectProperty*>(GetStrong.GetReturnProperty()) != nullptr);
	}

	TEST_CASE("The return property describes the result, and is null for void")
	{
		const FProperty* const AddResult = FindTestFunction("Add").GetReturnProperty();
		REQUIRE(AddResult != nullptr);
		CHECK(AddResult->GetName() == "ReturnValue");
		CHECK(AddResult->GetTypeName() == "int32");

		const FProperty* const ReferenceResult = FindTestFunction("GetTextReference").GetReturnProperty();
		REQUIRE(ReferenceResult != nullptr);
		CHECK(ReferenceResult->GetTypeName() == "std::string");

		CHECK(FindTestFunction("SetText").GetReturnProperty() == nullptr);
		CHECK(FindTestFunction("CountCall").GetReturnProperty() == nullptr);
	}

	TEST_CASE("Invoke passes arguments and returns the result")
	{
		const FScopedObjectTest ObjectTest;
		WTestValueObject* const Object = NewObject<WTestValueObject>();
		int32 A = 19;
		int32 B = 23;
		int32 Sum = 0;
		const std::array<void*, 2> Arguments{&A, &B};

		FindTestFunction("Add").Invoke(Object, Arguments, &Sum);

		CHECK(Sum == 42);
	}

	TEST_CASE("Invoke calls a function with no parameters and no result")
	{
		const FScopedObjectTest ObjectTest;
		WTestValueObject* const Object = NewObject<WTestValueObject>();

		FindTestFunction("CountCall").Invoke(Object, {}, nullptr);
		FindTestFunction("CountCall").Invoke(Object, {}, nullptr);

		CHECK(Object->CallCount == 2);
	}

	TEST_CASE("Invoke changes the object it is called on")
	{
		const FScopedObjectTest ObjectTest;
		WTestValueObject* const Object = NewObject<WTestValueObject>();
		Object->FloatValue = 1.5f;
		std::string NewText = "changed";
		float Factor = 4.0f;
		const std::array<void*, 1> TextArguments{&NewText};
		const std::array<void*, 1> FactorArguments{&Factor};

		FindTestFunction("SetText").Invoke(Object, TextArguments, nullptr);
		FindTestFunction("ScaleFloat").Invoke(Object, FactorArguments, nullptr);

		CHECK(Object->Text == "changed");
		CHECK(Object->FloatValue == 6.0f);
	}

	TEST_CASE("Invoke returns values and references as values")
	{
		const FScopedObjectTest ObjectTest;
		WTestValueObject* const Object = NewObject<WTestValueObject>();
		Object->Text = "stored";
		Object->Location = FVector(1.0, 2.0, 3.0);
		std::string Copy;
		std::string FromReference;
		FVector Delta(10.0, 20.0, 30.0);
		FVector Moved(0.0);
		const std::array<void*, 1> OffsetArguments{&Delta};

		FindTestFunction("GetTextCopy").Invoke(Object, {}, &Copy);
		FindTestFunction("GetTextReference").Invoke(Object, {}, &FromReference);
		FindTestFunction("Offset").Invoke(Object, OffsetArguments, &Moved);

		CHECK(Copy == "stored");
		CHECK(FromReference == "stored");
		CHECK(Moved == FVector(11.0, 22.0, 33.0));
	}

	TEST_CASE("Invoke writes output parameters")
	{
		const FScopedObjectTest ObjectTest;
		WTestValueObject* const Object = NewObject<WTestValueObject>();
		int32 Numerator = 84;
		int32 Denominator = 2;
		int32 Quotient = 0;
		bool bSucceeded = false;
		const std::array<void*, 3> Arguments{&Numerator, &Denominator, &Quotient};

		FindTestFunction("TryDivide").Invoke(Object, Arguments, &bSucceeded);
		CHECK(bSucceeded);
		CHECK(Quotient == 42);

		Denominator = 0;
		FindTestFunction("TryDivide").Invoke(Object, Arguments, &bSucceeded);
		CHECK_FALSE(bSucceeded);
		CHECK(Quotient == 42);
	}

	TEST_CASE("Invoke passes and returns objects as TObjectPtr")
	{
		const FScopedObjectTest ObjectTest;
		WTestValueObject* const Object = NewObject<WTestValueObject>();
		WTestValueObject* const Other = NewObject<WTestValueObject>();
		TObjectPtr<WTestValueObject> NewStrong = Other;
		TObjectPtr<WTestValueObject> Result;
		const std::array<void*, 1> Arguments{&NewStrong};

		FindTestFunction("SetStrong").Invoke(Object, Arguments, nullptr);
		FindTestFunction("GetStrong").Invoke(Object, {}, &Result);

		CHECK(Object->Strong.Get() == Other);
		CHECK(Result.Get() == Other);
	}

	TEST_CASE("Invoke on a derived object calls its override")
	{
		const FScopedObjectTest ObjectTest;
		WTestValueObject* const Base = NewObject<WTestValueObject>();
		WTestValueObject* const Derived = NewObject<WTestDerivedValueObject>();
		const FFunction* const Describe = Derived->GetClass()->FindFunction("Describe");
		REQUIRE(Describe != nullptr);
		std::string BaseDescription;
		std::string DerivedDescription;

		Describe->Invoke(Base, {}, &BaseDescription);
		Describe->Invoke(Derived, {}, &DerivedDescription);

		CHECK(BaseDescription == "base");
		CHECK(DerivedDescription == "derived");
	}

	TEST_CASE("Invoking with the wrong target, argument count or return pointer is a fatal error")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedLogCapture Capture;
		WTestValueObject* const Object = NewObject<WTestValueObject>();
		WTestObject* const WrongClass = NewObject<WTestObject>();
		int32 A = 1;
		int32 B = 2;
		int32 Sum = 0;
		const std::array<void*, 2> Arguments{&A, &B};
		const std::array<void*, 1> TooFewArguments{&A};
		const std::array<void*, 2> NullArgument{&A, nullptr};
		const FFunction& Add = FindTestFunction("Add");

		CHECK_THROWS_AS(Add.Invoke(nullptr, Arguments, &Sum), FFatalErrorException);
		CHECK_THROWS_AS(Add.Invoke(WrongClass, Arguments, &Sum), FFatalErrorException);
		CHECK_THROWS_AS(Add.Invoke(Object, TooFewArguments, &Sum), FFatalErrorException);
		CHECK_THROWS_AS(Add.Invoke(Object, NullArgument, &Sum), FFatalErrorException);
		CHECK_THROWS_AS(Add.Invoke(Object, Arguments, nullptr), FFatalErrorException);
		CHECK_THROWS_AS(FindTestFunction("CountCall").Invoke(Object, {}, &Sum), FFatalErrorException);
		CHECK(Sum == 0);
		CHECK(Object->CallCount == 0);
	}

	TEST_CASE("A function that has not been added to a class cannot be invoked")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedLogCapture Capture;
		WTestValueObject* const Object = NewObject<WTestValueObject>();
		const std::unique_ptr<FFunction> Function = MakeFunction<&WTestValueObject::CountCall>("CountCall");

		CHECK(Function->GetOwnerClass() == nullptr);
		CHECK_THROWS_AS(Function->Invoke(Object, {}, nullptr), FFatalErrorException);
	}

	TEST_CASE("MakeFunction with the wrong number of parameter names is a fatal error")
	{
		const FScopedLogCapture Capture;

		CHECK_THROWS_AS(MakeFunction<&WTestValueObject::Add>("Add", {"A"}), FFatalErrorException);
		CHECK_THROWS_AS(MakeFunction<&WTestValueObject::CountCall>("CountCall", {"Extra"}), FFatalErrorException);
	}

	TEST_CASE("Adding a function to a class after registration is a fatal error")
	{
		const FScopedLogCapture Capture;
		WClass* const Class = WTestValueObject::StaticClass();

		CHECK_THROWS_AS(
			Class->AddFunction(MakeFunction<&WTestValueObject::CountCall>("LateFunction")), FFatalErrorException);
		CHECK(Class->FindFunction("LateFunction") == nullptr);
	}

	TEST_CASE("A class cannot have two functions with the same name, its own or inherited")
	{
		const FScopedObjectTest ObjectTest;
		const FScopedLogCapture Capture;
		FNativeClassDescription Description;
		Description.Name = "WTestDuplicateFunctionObject";
		Description.Size = sizeof(WTestValueObject);
		Description.Alignment = alignof(WTestValueObject);
		Description.GetSuperClass = &WTestValueObject::StaticClass;
		Description.RegisterMembers = [](WClass& Class)
		{
			// "Add" is already a function of the base class.
			Class.AddFunction(MakeFunction<&WTestValueObject::CountCall>("Add"));
		};

		CHECK_THROWS_AS(WClass::RegisterNativeClass(Description), FFatalErrorException);

		CHECK(WClass::FindByName("WTestDuplicateFunctionObject") == nullptr);
	}
}
