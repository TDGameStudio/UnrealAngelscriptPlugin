#include "../../AngelscriptRuntime/Core/Helper_Reification.h"

#include "CQTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	bool ExpectReifyType(
		FAutomationTestBase& Test,
		const TCHAR* Context,
		const int32 ActualType,
		const EReifiedType ExpectedType)
	{
		return Test.TestEqual(
			Context,
			ActualType,
			static_cast<int32>(ExpectedType));
	}
}


TEST_CLASS_WITH_FLAGS(FAngelscriptDebugReificationTests,
	"Angelscript.TestModule.AngelScriptSDK.DebugReification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(TypeMapAndFallback)
	{
		const int32 Int32Type = GetReifyType<int32>();
		const int32 DoubleType = GetReifyType<double>();
		const int32 NameType = GetReifyType<FName>();
		const int32 ObjectType = GetReifyType<UObject*>();
		const int32 UnknownType = GetReifyType<FIntPoint>();

		ExpectReifyType(
			*TestRunner,
			TEXT("Debug reification should keep an unregistered type in the Unknown bucket"),
			UnknownType,
			EReifiedType::Unknown);

#if WITH_AS_DEBUGVALUES
		ExpectReifyType(
			*TestRunner,
			TEXT("Debug reification should map int32 to the int32 debugger type"),
			Int32Type,
			EReifiedType::_Enum_int32);
		ExpectReifyType(
			*TestRunner,
			TEXT("Debug reification should map double to the double debugger type"),
			DoubleType,
			EReifiedType::_Enum_double);
		ExpectReifyType(
			*TestRunner,
			TEXT("Debug reification should map FName to the FName debugger type"),
			NameType,
			EReifiedType::_Enum_FName);
		ExpectReifyType(
			*TestRunner,
			TEXT("Debug reification should map UObject* to the UObject debugger type"),
			ObjectType,
			EReifiedType::_Enum_UObject);
		TestRunner->TestNotEqual(
			TEXT("Debug reification should keep int32 and double on distinct debugger types"),
			Int32Type,
			DoubleType);
		TestRunner->TestNotEqual(
			TEXT("Debug reification should keep FName and UObject* on distinct debugger types"),
			NameType,
			ObjectType);
#else
		ExpectReifyType(
			*TestRunner,
			TEXT("Debug reification fallback should collapse int32 to Unknown"),
			Int32Type,
			EReifiedType::Unknown);
		ExpectReifyType(
			*TestRunner,
			TEXT("Debug reification fallback should collapse double to Unknown"),
			DoubleType,
			EReifiedType::Unknown);
		ExpectReifyType(
			*TestRunner,
			TEXT("Debug reification fallback should collapse FName to Unknown"),
			NameType,
			EReifiedType::Unknown);
		ExpectReifyType(
			*TestRunner,
			TEXT("Debug reification fallback should collapse UObject* to Unknown"),
			ObjectType,
			EReifiedType::Unknown);
#endif
	}
};

#endif
