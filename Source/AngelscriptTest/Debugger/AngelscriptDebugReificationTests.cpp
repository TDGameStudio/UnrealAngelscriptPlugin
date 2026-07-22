#include "../../AngelscriptRuntime/Core/Helper_Reification.h"

// Debugger reification ownership coverage.

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FDebugReificationTests,
	"Angelscript.TestModule.Debugger.Reification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static bool ExpectReifyType(
		const int32 ActualType,
		const EReifiedType ExpectedType)
	{
		return ActualType == static_cast<int32>(ExpectedType);
	}

public:
	TEST_METHOD(TypeMapAndFallback)
	{
		const int32 Int32Type = GetReifyType<int32>();
		const int32 DoubleType = GetReifyType<double>();
		const int32 NameType = GetReifyType<FName>();
		const int32 ObjectType = GetReifyType<UObject*>();
		const int32 UnknownType = GetReifyType<FIntPoint>();

		ASSERT_THAT(IsTrue(ExpectReifyType(UnknownType, EReifiedType::Unknown),
			TEXT("Debug reification should keep an unregistered type in the Unknown bucket")));

#if WITH_AS_DEBUGVALUES
		ASSERT_THAT(IsTrue(ExpectReifyType(Int32Type, EReifiedType::_Enum_int32),
			TEXT("Debug reification should map int32 to the int32 debugger type")));
		ASSERT_THAT(IsTrue(ExpectReifyType(DoubleType, EReifiedType::_Enum_double),
			TEXT("Debug reification should map double to the double debugger type")));
		ASSERT_THAT(IsTrue(ExpectReifyType(NameType, EReifiedType::_Enum_FName),
			TEXT("Debug reification should map FName to the FName debugger type")));
		ASSERT_THAT(IsTrue(ExpectReifyType(ObjectType, EReifiedType::_Enum_UObject),
			TEXT("Debug reification should map UObject* to the UObject debugger type")));
		ASSERT_THAT(AreNotEqual(Int32Type, DoubleType,
			TEXT("Debug reification should keep int32 and double on distinct debugger types")));
		ASSERT_THAT(AreNotEqual(NameType, ObjectType,
			TEXT("Debug reification should keep FName and UObject* on distinct debugger types")));
#else
		ASSERT_THAT(IsTrue(ExpectReifyType(Int32Type, EReifiedType::Unknown),
			TEXT("Debug reification fallback should collapse int32 to Unknown")));
		ASSERT_THAT(IsTrue(ExpectReifyType(DoubleType, EReifiedType::Unknown),
			TEXT("Debug reification fallback should collapse double to Unknown")));
		ASSERT_THAT(IsTrue(ExpectReifyType(NameType, EReifiedType::Unknown),
			TEXT("Debug reification fallback should collapse FName to Unknown")));
		ASSERT_THAT(IsTrue(ExpectReifyType(ObjectType, EReifiedType::Unknown),
			TEXT("Debug reification fallback should collapse UObject* to Unknown")));
#endif
	}
};

#endif
