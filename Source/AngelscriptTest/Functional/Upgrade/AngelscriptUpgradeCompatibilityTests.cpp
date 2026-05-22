#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptTestMacros.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_string.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;

namespace AngelscriptTest_Angelscript_AngelscriptUpgradeCompatibilityTests_Private
{
	constexpr int32 P9BAutomaticImportsPropertyId = 41;
	constexpr int32 P9BTypecheckSwitchEnumsPropertyId = 42;
	constexpr int32 P9BAllowDoubleTypePropertyId = 43;
	constexpr int32 P9BFloatIsFloat64PropertyId = 44;
	constexpr int32 P9BWarnOnFloatConstantsForDoublesPropertyId = 45;
	constexpr int32 P9BWarnIntegerDivisionPropertyId = 46;
	constexpr uint64 P9BScriptObjectFlag = (uint64(1) << 21);
	constexpr uint64 P9BSharedFlag = (uint64(1) << 22);
	constexpr uint64 P9BNoInheritFlag = (uint64(1) << 23);
	constexpr uint64 P9BFuncdefFlag = (uint64(1) << 24);
	constexpr uint64 P9BListPatternFlag = (uint64(1) << 25);
	constexpr uint64 P9BEnumFlag = (uint64(1) << 26);
	constexpr uint64 P9BTemplateSubtypeFlag = (uint64(1) << 27);
	constexpr uint64 P9BTypedefFlag = (uint64(1) << 28);
	constexpr uint64 P9BAbstractFlag = (uint64(1) << 29);
	constexpr uint64 P9BStockMoreConstructorsFlag = (uint64(1) << 31);
	constexpr uint64 P9BStockUnionFlag = (uint64(1) << 32);
	constexpr uint64 P9BCovariantSubtypeFlag = (uint64(1) << 48);
	constexpr uint64 P9BDeterminesSizeFlag = (uint64(1) << 49);
	constexpr uint64 P9BDisallowInstantiationFlag = (uint64(1) << 50);
	constexpr uint64 P9BBasicMathTypeFlag = (uint64(1) << 51);
	constexpr uint64 P9BEditorOnlyFlag = (uint64(1) << 52);

	bool GUpgradeMessageCallbackInvoked = false;
	FString GUpgradeMessageText;
	asEMsgType GUpgradeMessageType = asMSGTYPE_INFORMATION;
	int32 GUpgradeMessageCallbackACount = 0;
	int32 GUpgradeMessageCallbackBCount = 0;
	FString GUpgradeMessageCallbackAText;
	FString GUpgradeMessageCallbackBText;

	void CaptureUpgradeMessage(asSMessageInfo* Message)
	{
		GUpgradeMessageCallbackInvoked = true;
		GUpgradeMessageText = UTF8_TO_TCHAR(Message->message);
		GUpgradeMessageType = Message->type;
	}

	void CaptureUpgradeMessageA(asSMessageInfo* Message)
	{
		++GUpgradeMessageCallbackACount;
		GUpgradeMessageCallbackAText = UTF8_TO_TCHAR(Message->message);
	}

	void CaptureUpgradeMessageB(asSMessageInfo* Message)
	{
		++GUpgradeMessageCallbackBCount;
		GUpgradeMessageCallbackBText = UTF8_TO_TCHAR(Message->message);
	}
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptUpgradeHeaderCompatibilityTest,
	"Angelscript.TestModule.Functional.Upgrade.HeaderCompatibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptUpgradeHeaderCompatibilityTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptUpgradeCompatibilityTests_Private;
	const bool bVersionMatches =
		TestEqual(TEXT("Embedded Angelscript version should remain pinned to 2.33.0 until the 2.38 upgrade resumes"), ANGELSCRIPT_VERSION, 23300) &&
		TestEqual(TEXT("Embedded Angelscript version string should report 2.33.0 WIP"), FString(ANSI_TO_TCHAR(ANGELSCRIPT_VERSION_STRING)), FString(TEXT("2.33.0 WIP")));

	const bool bPropertyIdsMatch =
		TestEqual(TEXT("Stock 2.38 init stack size property id should remain available"), static_cast<int32>(asEP_INIT_STACK_SIZE), 29) &&
		TestEqual(TEXT("Stock 2.38 init call stack size property id should remain available"), static_cast<int32>(asEP_INIT_CALL_STACK_SIZE), 30) &&
		TestEqual(TEXT("Stock 2.38 max call stack size property id should remain available"), static_cast<int32>(asEP_MAX_CALL_STACK_SIZE), 31) &&
		TestEqual(TEXT("Stock 2.38 duplicate shared interface property id should remain available"), static_cast<int32>(asEP_IGNORE_DUPLICATE_SHARED_INTF), 32) &&
		TestEqual(TEXT("Stock 2.38 no debug output property id should remain available"), static_cast<int32>(asEP_NO_DEBUG_OUTPUT), 33) &&
		TestEqual(TEXT("Stock 2.38 disable script class GC property id should remain available"), static_cast<int32>(asEP_DISABLE_SCRIPT_CLASS_GC), 34) &&
		TestEqual(TEXT("Stock 2.38 JIT interface version property id should remain available"), static_cast<int32>(asEP_JIT_INTERFACE_VERSION), 35) &&
		TestEqual(TEXT("Stock 2.38 default copy property id should remain available"), static_cast<int32>(asEP_ALWAYS_IMPL_DEFAULT_COPY), 36) &&
		TestEqual(TEXT("Stock 2.38 default copy construct property id should remain available"), static_cast<int32>(asEP_ALWAYS_IMPL_DEFAULT_COPY_CONSTRUCT), 37) &&
		TestEqual(TEXT("Stock 2.38 member init mode property id should remain available"), static_cast<int32>(asEP_MEMBER_INIT_MODE), 38) &&
		TestEqual(TEXT("Stock 2.38 bool conversion mode property id should remain available"), static_cast<int32>(asEP_BOOL_CONVERSION_MODE), 39) &&
		TestEqual(TEXT("Stock 2.38 foreach support property id should remain available"), static_cast<int32>(asEP_FOREACH_SUPPORT), 40) &&
		TestEqual(TEXT("APV2 automatic imports property id should move off the stock 2.38 property range"), static_cast<int32>(asEP_AUTOMATIC_IMPORTS), P9BAutomaticImportsPropertyId) &&
		TestEqual(TEXT("APV2 typecheck switch enums property id should move off the stock 2.38 property range"), static_cast<int32>(asEP_TYPECHECK_SWITCH_ENUMS), P9BTypecheckSwitchEnumsPropertyId) &&
		TestEqual(TEXT("APV2 allow double type property id should move off the stock 2.38 property range"), static_cast<int32>(asEP_ALLOW_DOUBLE_TYPE), P9BAllowDoubleTypePropertyId) &&
		TestEqual(TEXT("APV2 float64 compatibility property id should move off the stock 2.38 property range"), static_cast<int32>(asEP_FLOAT_IS_FLOAT64), P9BFloatIsFloat64PropertyId) &&
		TestEqual(TEXT("APV2 float warning property id should move off the stock 2.38 property range"), static_cast<int32>(asEP_WARN_ON_FLOAT_CONSTANTS_FOR_DOUBLES), P9BWarnOnFloatConstantsForDoublesPropertyId) &&
		TestEqual(TEXT("APV2 integer division warning property id should move off the stock 2.38 property range"), static_cast<int32>(asEP_WARN_INTEGER_DIVISION), P9BWarnIntegerDivisionPropertyId);

	const bool bTypeAndFlagLayoutMatches =
		TestEqual(TEXT("asEObjTypeFlags should widen to preserve stock 2.38 and APV2 high-bit object flags"), static_cast<int32>(sizeof(asEObjTypeFlags)), static_cast<int32>(sizeof(asQWORD))) &&
		TestEqual(TEXT("Stock 2.38 should restore asOBJ_SCRIPT_OBJECT to bit 21"), static_cast<uint64>(asOBJ_SCRIPT_OBJECT), P9BScriptObjectFlag) &&
		TestEqual(TEXT("Stock 2.38 should restore asOBJ_SHARED to bit 22"), static_cast<uint64>(asOBJ_SHARED), P9BSharedFlag) &&
		TestEqual(TEXT("Stock 2.38 should restore asOBJ_NOINHERIT to bit 23"), static_cast<uint64>(asOBJ_NOINHERIT), P9BNoInheritFlag) &&
		TestEqual(TEXT("Stock 2.38 should restore asOBJ_FUNCDEF to bit 24"), static_cast<uint64>(asOBJ_FUNCDEF), P9BFuncdefFlag) &&
		TestEqual(TEXT("Stock 2.38 should restore asOBJ_LIST_PATTERN to bit 25"), static_cast<uint64>(asOBJ_LIST_PATTERN), P9BListPatternFlag) &&
		TestEqual(TEXT("Stock 2.38 should restore asOBJ_ENUM to bit 26"), static_cast<uint64>(asOBJ_ENUM), P9BEnumFlag) &&
		TestEqual(TEXT("Stock 2.38 should restore asOBJ_TEMPLATE_SUBTYPE to bit 27"), static_cast<uint64>(asOBJ_TEMPLATE_SUBTYPE), P9BTemplateSubtypeFlag) &&
		TestEqual(TEXT("Stock 2.38 should restore asOBJ_TYPEDEF to bit 28"), static_cast<uint64>(asOBJ_TYPEDEF), P9BTypedefFlag) &&
		TestEqual(TEXT("Stock 2.38 should restore asOBJ_ABSTRACT to bit 29"), static_cast<uint64>(asOBJ_ABSTRACT), P9BAbstractFlag) &&
		TestEqual(TEXT("Stock 2.38 should preserve asOBJ_APP_CLASS_MORE_CONSTRUCTORS on bit 31"), static_cast<uint64>(asOBJ_APP_CLASS_MORE_CONSTRUCTORS), P9BStockMoreConstructorsFlag) &&
		TestEqual(TEXT("Stock 2.38 should preserve asOBJ_APP_CLASS_UNION on bit 32"), static_cast<uint64>(asOBJ_APP_CLASS_UNION), P9BStockUnionFlag) &&
		TestEqual(TEXT("APV2 covariant subtype flag should move to the high-bit private range"), static_cast<uint64>(asOBJ_TEMPLATE_SUBTYPE_COVARIANT), P9BCovariantSubtypeFlag) &&
		TestEqual(TEXT("APV2 template-size flag should move to the high-bit private range"), static_cast<uint64>(asOBJ_TEMPLATE_SUBTYPE_DETERMINES_SIZE), P9BDeterminesSizeFlag) &&
		TestEqual(TEXT("APV2 disallow-instantiation flag should move to the high-bit private range"), static_cast<uint64>(asOBJ_DISALLOW_INSTANTIATION), P9BDisallowInstantiationFlag) &&
		TestEqual(TEXT("APV2 basic-math flag should move to the high-bit private range"), static_cast<uint64>(asOBJ_BASICMATHTYPE), P9BBasicMathTypeFlag) &&
		TestEqual(TEXT("APV2 editor-only flag should move to the high-bit private range"), static_cast<uint64>(asOBJ_EDITOR_ONLY), P9BEditorOnlyFlag) &&
		TestEqual(TEXT("APV2 float32 alias should preserve the stock float type id"), static_cast<int32>(asTYPEID_FLOAT32), 10) &&
		TestEqual(TEXT("APV2 float64 alias should preserve the stock double type id"), static_cast<int32>(asTYPEID_FLOAT64), 11) &&
		TestEqual(TEXT("APV2 custom bytecodes should keep the extended bytecode max"), static_cast<int32>(asBC_MAXBYTECODE), 212);

	return bVersionMatches && bPropertyIdsMatch && bTypeAndFlagLayoutMatches;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptUpgradeEnginePropertyCompatibilityTest,
	"Angelscript.TestModule.Functional.Upgrade.EngineProperties",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptUpgradeEnginePropertyCallStackLimitOverflowTest,
	"Angelscript.TestModule.Functional.Upgrade.EngineProperties.CallStackLimitOverflow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptUpgradeMessageCallbackCompatibilityTest,
	"Angelscript.TestModule.Functional.Upgrade.MessageCallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptUpgradeMessageCallbackClearAndReRegisterCompatibilityTest,
	"Angelscript.TestModule.Functional.Upgrade.MessageCallback.ClearAndReRegister",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptUpgradeRegisterObjectTypeFlagCompatibilityTest,
	"Angelscript.TestModule.Functional.Upgrade.RegisterObjectTypeFlags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptUpgradeCStringHashCompatibilityTest,
	"Angelscript.TestModule.Functional.Upgrade.CStringHash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptUpgradeEnginePropertyCompatibilityTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptUpgradeCompatibilityTests_Private;
	bool bCustomPropertiesStillWork = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
	if (!TestNotNull(TEXT("Upgrade property test should create a script engine"), ScriptEngine))
	{
		return false;
	}

	bCustomPropertiesStillWork =
		TestTrue(TEXT("APV2 automatic imports property getter should reflect the value written through SetEngineProperty"),
			[&]()
			{
				const int64 PreviousAutomaticImports = static_cast<int64>(ScriptEngine->GetEngineProperty(asEP_AUTOMATIC_IMPORTS));
				ScriptEngine->SetEngineProperty(asEP_AUTOMATIC_IMPORTS, 1);
				const bool bMatches = TestEqual(TEXT("APV2 automatic imports property should round-trip through the getter"), static_cast<int64>(ScriptEngine->GetEngineProperty(asEP_AUTOMATIC_IMPORTS)), int64(1));
				ScriptEngine->SetEngineProperty(asEP_AUTOMATIC_IMPORTS, PreviousAutomaticImports);
				return bMatches;
			}()) &&
		TestEqual(TEXT("Stock 2.38 default copy property should be forced to the APV2 compatibility value during engine initialization"), static_cast<int64>(ScriptEngine->GetEngineProperty(asEP_ALWAYS_IMPL_DEFAULT_COPY)), int64(1)) &&
		TestEqual(TEXT("Stock 2.38 default copy construct property should be forced to the APV2 compatibility value during engine initialization"), static_cast<int64>(ScriptEngine->GetEngineProperty(asEP_ALWAYS_IMPL_DEFAULT_COPY_CONSTRUCT)), int64(1)) &&
		TestEqual(TEXT("Stock 2.38 member init mode should be forced to the APV2 compatibility value during engine initialization"), static_cast<int64>(ScriptEngine->GetEngineProperty(asEP_MEMBER_INIT_MODE)), int64(0)) &&
		TestEqual(TEXT("Property accessor mode should be disabled during engine initialization"), static_cast<int64>(ScriptEngine->GetEngineProperty(asEP_PROPERTY_ACCESSOR_MODE)), int64(0)) &&
		TestEqual(TEXT("APV2 typecheck switch enums property should still be enabled"), static_cast<int64>(ScriptEngine->GetEngineProperty(asEP_TYPECHECK_SWITCH_ENUMS)), int64(1)) &&
		TestEqual(TEXT("APV2 double type property should remain enabled when settings allow it"), static_cast<int64>(ScriptEngine->GetEngineProperty(asEP_ALLOW_DOUBLE_TYPE)), int64(1)) &&
		TestTrue(TEXT("Stock 2.38 engine properties should round-trip through SetEngineProperty/GetEngineProperty"),
			[&]()
			{
				struct FPropertyCase
				{
					asEEngineProp Property;
					asPWORD Value;
				};

				const FPropertyCase Cases[] = {
					{ asEP_INIT_STACK_SIZE, 2048 },
					{ asEP_INIT_CALL_STACK_SIZE, 32 },
					{ asEP_MAX_CALL_STACK_SIZE, 64 },
					{ asEP_IGNORE_DUPLICATE_SHARED_INTF, 1 },
					{ asEP_NO_DEBUG_OUTPUT, 1 },
					{ asEP_DISABLE_SCRIPT_CLASS_GC, 1 },
					{ asEP_JIT_INTERFACE_VERSION, 2 },
					{ asEP_ALWAYS_IMPL_DEFAULT_COPY, 1 },
					{ asEP_ALWAYS_IMPL_DEFAULT_COPY_CONSTRUCT, 2 },
					{ asEP_MEMBER_INIT_MODE, 1 },
					{ asEP_BOOL_CONVERSION_MODE, 1 },
					{ asEP_FOREACH_SUPPORT, 0 },
				};

				TArray<asPWORD, TInlineAllocator<12>> PreviousValues;
				PreviousValues.Reserve(UE_ARRAY_COUNT(Cases));
				for (const FPropertyCase& Case : Cases)
				{
					PreviousValues.Add(ScriptEngine->GetEngineProperty(Case.Property));
				}

				bool bAllMatched = true;
				for (int32 Index = 0; Index < UE_ARRAY_COUNT(Cases); ++Index)
				{
					const FPropertyCase& Case = Cases[Index];
					ScriptEngine->SetEngineProperty(Case.Property, Case.Value);
					bAllMatched &= TestEqual(FString::Printf(TEXT("Stock engine property %d should round-trip through the getter"), static_cast<int32>(Case.Property)), static_cast<int64>(ScriptEngine->GetEngineProperty(Case.Property)), static_cast<int64>(Case.Value));
				}

				for (int32 Index = 0; Index < UE_ARRAY_COUNT(Cases); ++Index)
				{
					ScriptEngine->SetEngineProperty(Cases[Index].Property, PreviousValues[Index]);
				}

				return bAllMatched;
			}());

	}
	return bCustomPropertiesStillWork;
}

bool FAngelscriptUpgradeEnginePropertyCallStackLimitOverflowTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptUpgradeCompatibilityTests_Private;
	TUniquePtr<FAngelscriptEngine> IsolatedEngine = CreateIsolatedCloneEngine();
	if (!TestNotNull(TEXT("Upgrade.EngineProperties.CallStackLimitOverflow should create an isolated clone engine"), IsolatedEngine.Get()))
	{
		return false;
	}

	FAngelscriptEngine& Engine = *IsolatedEngine;
	FAngelscriptEngineScope EngineScope(Engine);

	const FString RecursiveScript =
		TEXT("void Recursive(int Depth)\n")
		TEXT("{\n")
		TEXT("    if (Depth > 0)\n")
		TEXT("    {\n")
		TEXT("        Recursive(Depth - 1);\n")
		TEXT("    }\n")
		TEXT("}\n");

	asIScriptModule* Module = BuildModule(*this, Engine, "UpgradeCallStackLimit", RecursiveScript);
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* RunFunction = GetFunctionByDecl(*this, *Module, TEXT("void Recursive(int)"));
	if (RunFunction == nullptr)
	{
		return false;
	}

	asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
	if (!TestNotNull(TEXT("Upgrade.EngineProperties.CallStackLimitOverflow should expose the backing script engine"), ScriptEngine))
	{
		return false;
	}

	asIScriptContext* Context = Engine.CreateContext();
	if (!TestNotNull(TEXT("Upgrade.EngineProperties.CallStackLimitOverflow should create an execution context"), Context))
	{
		return false;
	}
	ON_SCOPE_EXIT
	{
		Context->Release();
	};

	ScriptEngine->SetEngineProperty(asEP_INIT_CALL_STACK_SIZE, 1);
	ScriptEngine->SetEngineProperty(asEP_MAX_CALL_STACK_SIZE, 1);
	ScriptEngine->SetEngineProperty(asEP_MAX_NESTED_CALLS, 1);

	const int PrepareResult = Context->Prepare(RunFunction);
	if (!TestEqual(TEXT("Upgrade.EngineProperties.CallStackLimitOverflow should prepare the recursive entry point"), PrepareResult, static_cast<int32>(asSUCCESS)))
	{
		return false;
	}
	if (!TestEqual(TEXT("Upgrade.EngineProperties.CallStackLimitOverflow should bind the recursive depth argument"), Context->SetArgDWord(0, 1000), static_cast<int32>(asSUCCESS)))
	{
		return false;
	}

	AddExpectedError(TEXT("Stack overflow: potential infinite recursion detected?"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("UpgradeCallStackLimit"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("void Recursive(int) | Line"), EAutomationExpectedErrorFlags::Contains, -1);

	const int ExecuteResult = Context->Execute();
	const char* ExceptionStringAnsi = Context->GetExceptionString();
	const FString ExceptionString = ExceptionStringAnsi != nullptr ? UTF8_TO_TCHAR(ExceptionStringAnsi) : FString();
	return TestEqual(TEXT("Upgrade.EngineProperties.CallStackLimitOverflow should raise an execution exception once the migrated call-stack properties are enforced"), ExecuteResult, static_cast<int32>(asEXECUTION_EXCEPTION))
		&& TestFalse(TEXT("Upgrade.EngineProperties.CallStackLimitOverflow should expose a non-empty exception string after the overflow"), ExceptionString.IsEmpty());
}

bool FAngelscriptUpgradeMessageCallbackCompatibilityTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptUpgradeCompatibilityTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
	if (!TestNotNull(TEXT("Upgrade message callback test should create a script engine"), ScriptEngine))
	{
		return false;
	}

	GUpgradeMessageCallbackInvoked = false;
	GUpgradeMessageText.Reset();
	GUpgradeMessageType = asMSGTYPE_INFORMATION;

	if (!TestEqual(TEXT("SetMessageCallback should succeed for the upgrade compatibility callback"), ScriptEngine->SetMessageCallback(asFUNCTION(CaptureUpgradeMessage), nullptr, asCALL_CDECL), asSUCCESS))
	{
		return false;
	}

	asSFuncPtr CallbackPtr = {};
	void* CallbackObject = nullptr;
	asDWORD CallConv = 0;
	if (!TestEqual(TEXT("GetMessageCallback should report the registered callback"), ScriptEngine->GetMessageCallback(&CallbackPtr, &CallbackObject, &CallConv), static_cast<int32>(asSUCCESS)))
	{
		return false;
	}

	if (!TestEqual(TEXT("GetMessageCallback should preserve the original call convention"), static_cast<int32>(CallConv), static_cast<int32>(asCALL_CDECL)))
	{
		return false;
	}

	if (!TestEqual(TEXT("WriteMessage should succeed after restoring the stock callback getter ABI"), ScriptEngine->WriteMessage("Upgrade", 1, 1, asMSGTYPE_WARNING, "CallbackRoundtrip"), asSUCCESS))
	{
		return false;
	}

	}
	return TestTrue(TEXT("The registered upgrade callback should receive WriteMessage diagnostics"), GUpgradeMessageCallbackInvoked)
		&& TestEqual(TEXT("The registered upgrade callback should receive the expected message text"), GUpgradeMessageText, FString(TEXT("CallbackRoundtrip")))
		&& TestEqual(TEXT("The registered upgrade callback should receive the expected message type"), static_cast<int32>(GUpgradeMessageType), static_cast<int32>(asMSGTYPE_WARNING));
}

bool FAngelscriptUpgradeMessageCallbackClearAndReRegisterCompatibilityTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptUpgradeCompatibilityTests_Private;
	TUniquePtr<FAngelscriptEngine> IsolatedEngine = CreateIsolatedCloneEngine();
	if (!TestNotNull(TEXT("Upgrade.MessageCallback.ClearAndReRegister should create an isolated clone engine"), IsolatedEngine.Get()))
	{
		return false;
	}

	FAngelscriptEngine& Engine = *IsolatedEngine;
	FAngelscriptEngineScope EngineScope(Engine);
	asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
	if (!TestNotNull(TEXT("Upgrade.MessageCallback.ClearAndReRegister should expose the backing script engine"), ScriptEngine))
	{
		return false;
	}

	GUpgradeMessageCallbackACount = 0;
	GUpgradeMessageCallbackBCount = 0;
	GUpgradeMessageCallbackAText.Reset();
	GUpgradeMessageCallbackBText.Reset();

	ON_SCOPE_EXIT
	{
		ScriptEngine->ClearMessageCallback();
	};

	bool bPassed = true;
	bPassed &= TestEqual(
		TEXT("Upgrade.MessageCallback.ClearAndReRegister should register callback A"),
		ScriptEngine->SetMessageCallback(asFUNCTION(CaptureUpgradeMessageA), nullptr, asCALL_CDECL),
		asSUCCESS);
	bPassed &= TestEqual(
		TEXT("Upgrade.MessageCallback.ClearAndReRegister should dispatch the first message successfully"),
		ScriptEngine->WriteMessage("Upgrade", 1, 1, asMSGTYPE_WARNING, "CallbackA"),
		asSUCCESS);
	bPassed &= TestEqual(
		TEXT("Upgrade.MessageCallback.ClearAndReRegister should deliver the first message to callback A exactly once"),
		GUpgradeMessageCallbackACount,
		1);
	bPassed &= TestEqual(
		TEXT("Upgrade.MessageCallback.ClearAndReRegister should preserve the first callback payload"),
		GUpgradeMessageCallbackAText,
		FString(TEXT("CallbackA")));
	bPassed &= TestEqual(
		TEXT("Upgrade.MessageCallback.ClearAndReRegister should keep callback B untouched before re-registration"),
		GUpgradeMessageCallbackBCount,
		0);

	bPassed &= TestEqual(
		TEXT("Upgrade.MessageCallback.ClearAndReRegister should clear the active callback successfully"),
		ScriptEngine->ClearMessageCallback(),
		asSUCCESS);
	bPassed &= TestEqual(
		TEXT("Upgrade.MessageCallback.ClearAndReRegister should allow WriteMessage after the callback is cleared"),
		ScriptEngine->WriteMessage("Upgrade", 1, 1, asMSGTYPE_WARNING, "CallbackAfterClear"),
		asSUCCESS);
	bPassed &= TestEqual(
		TEXT("Upgrade.MessageCallback.ClearAndReRegister should keep callback A count unchanged after ClearMessageCallback"),
		GUpgradeMessageCallbackACount,
		1);
	bPassed &= TestEqual(
		TEXT("Upgrade.MessageCallback.ClearAndReRegister should keep callback B count unchanged while no callback is registered"),
		GUpgradeMessageCallbackBCount,
		0);

	bPassed &= TestEqual(
		TEXT("Upgrade.MessageCallback.ClearAndReRegister should register callback B after the clear"),
		ScriptEngine->SetMessageCallback(asFUNCTION(CaptureUpgradeMessageB), nullptr, asCALL_CDECL),
		asSUCCESS);

	asSFuncPtr CallbackPtr = {};
	void* CallbackObject = nullptr;
	asDWORD CallConv = 0;
	bPassed &= TestEqual(
		TEXT("Upgrade.MessageCallback.ClearAndReRegister should report a registered callback after re-registration"),
		ScriptEngine->GetMessageCallback(&CallbackPtr, &CallbackObject, &CallConv),
		asSUCCESS);
	bPassed &= TestEqual(
		TEXT("Upgrade.MessageCallback.ClearAndReRegister should preserve the callback B call convention"),
		static_cast<int32>(CallConv),
		static_cast<int32>(asCALL_CDECL));
	bPassed &= TestNull(
		TEXT("Upgrade.MessageCallback.ClearAndReRegister should not attach an object instance when re-registering a free function"),
		CallbackObject);

	bPassed &= TestEqual(
		TEXT("Upgrade.MessageCallback.ClearAndReRegister should dispatch the third message successfully"),
		ScriptEngine->WriteMessage("Upgrade", 1, 1, asMSGTYPE_WARNING, "CallbackB"),
		asSUCCESS);
	bPassed &= TestEqual(
		TEXT("Upgrade.MessageCallback.ClearAndReRegister should keep callback A count frozen after callback B takes over"),
		GUpgradeMessageCallbackACount,
		1);
	bPassed &= TestEqual(
		TEXT("Upgrade.MessageCallback.ClearAndReRegister should deliver the third message to callback B exactly once"),
		GUpgradeMessageCallbackBCount,
		1);
	bPassed &= TestEqual(
		TEXT("Upgrade.MessageCallback.ClearAndReRegister should preserve the re-registered callback payload"),
		GUpgradeMessageCallbackBText,
		FString(TEXT("CallbackB")));

	return bPassed;
}

bool FAngelscriptUpgradeRegisterObjectTypeFlagCompatibilityTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptUpgradeCompatibilityTests_Private;
	asQWORD Flags = 0;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
	if (!TestNotNull(TEXT("Upgrade object-type registration test should create a script engine"), ScriptEngine))
	{
		return false;
	}

	static const char* TypeName = "FUpgradeEditorOnlyRegisteredType";
	const int RegistrationResult = ScriptEngine->RegisterObjectType(TypeName, 4, asOBJ_VALUE | asOBJ_POD | asOBJ_APP_PRIMITIVE | asOBJ_EDITOR_ONLY);
	if (!TestTrue(TEXT("RegisterObjectType should accept a migrated high-bit editor-only flag on an application value type"), RegistrationResult >= 0))
	{
		return false;
	}

	asITypeInfo* TypeInfo = ScriptEngine->GetTypeInfoByName(TypeName);
	if (!TestNotNull(TEXT("RegisterObjectType should expose the registered type by name"), TypeInfo))
	{
		return false;
	}

	Flags = TypeInfo->GetFlags();

	}
	return TestTrue(TEXT("The registered type should preserve the migrated editor-only high-bit flag"), (Flags & asOBJ_EDITOR_ONLY) != 0)
		&& TestFalse(TEXT("The registered type should not alias the stock more-constructors bit when using the migrated editor-only flag"), (Flags & asOBJ_APP_CLASS_MORE_CONSTRUCTORS) != 0);
}

bool FAngelscriptUpgradeCStringHashCompatibilityTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptUpgradeCompatibilityTests_Private;
	const asCString MixedCase("AlphaBeta");
	const asCString LowerCase("alphabeta");
	const asCString DifferentValue("gamma");

	const uint32 MixedHash = GetTypeHash(MixedCase);
	const uint32 LowerHash = GetTypeHash(LowerCase);
	const uint32 DifferentHash = GetTypeHash(DifferentValue);

	return TestEqual(TEXT("asCString hashing should remain case-insensitive for equal content"), MixedHash, LowerHash)
		&& TestNotEqual(TEXT("asCString hashing should still distinguish different content"), MixedHash, DifferentHash);
}

#endif
