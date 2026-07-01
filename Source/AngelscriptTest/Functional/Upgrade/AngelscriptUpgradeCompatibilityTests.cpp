#include "CQTest.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_string.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS



TEST_CLASS_WITH_FLAGS(
	FAngelscriptUpgradeCompatibilityTests,
	"Angelscript.TestModule.Functional.Upgrade",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
static constexpr int32 P9BAutomaticImportsPropertyId = 41;
static constexpr int32 P9BTypecheckSwitchEnumsPropertyId = 42;
static constexpr int32 P9BAllowDoubleTypePropertyId = 43;
static constexpr int32 P9BFloatIsFloat64PropertyId = 44;
static constexpr int32 P9BWarnOnFloatConstantsForDoublesPropertyId = 45;
static constexpr int32 P9BWarnIntegerDivisionPropertyId = 46;
static constexpr uint64 P9BScriptObjectFlag = (uint64(1) << 21);
static constexpr uint64 P9BSharedFlag = (uint64(1) << 22);
static constexpr uint64 P9BNoInheritFlag = (uint64(1) << 23);
static constexpr uint64 P9BFuncdefFlag = (uint64(1) << 24);
static constexpr uint64 P9BListPatternFlag = (uint64(1) << 25);
static constexpr uint64 P9BEnumFlag = (uint64(1) << 26);
static constexpr uint64 P9BTemplateSubtypeFlag = (uint64(1) << 27);
static constexpr uint64 P9BTypedefFlag = (uint64(1) << 28);
static constexpr uint64 P9BAbstractFlag = (uint64(1) << 29);
static constexpr uint64 P9BStockMoreConstructorsFlag = (uint64(1) << 31);
static constexpr uint64 P9BStockUnionFlag = (uint64(1) << 32);
static constexpr uint64 P9BCovariantSubtypeFlag = (uint64(1) << 48);
static constexpr uint64 P9BDeterminesSizeFlag = (uint64(1) << 49);
static constexpr uint64 P9BDisallowInstantiationFlag = (uint64(1) << 50);
static constexpr uint64 P9BBasicMathTypeFlag = (uint64(1) << 51);
static constexpr uint64 P9BEditorOnlyFlag = (uint64(1) << 52);

inline static bool GUpgradeMessageCallbackInvoked = false;
inline static FString GUpgradeMessageText;
inline static asEMsgType GUpgradeMessageType = asMSGTYPE_INFORMATION;
inline static int32 GUpgradeMessageCallbackACount = 0;
inline static int32 GUpgradeMessageCallbackBCount = 0;
inline static FString GUpgradeMessageCallbackAText;
inline static FString GUpgradeMessageCallbackBText;

static void CaptureUpgradeMessage(asSMessageInfo* Message)
{
	GUpgradeMessageCallbackInvoked = true;
	GUpgradeMessageText = UTF8_TO_TCHAR(Message->message);
	GUpgradeMessageType = Message->type;
}

static void CaptureUpgradeMessageA(asSMessageInfo* Message)
{
	++GUpgradeMessageCallbackACount;
	GUpgradeMessageCallbackAText = UTF8_TO_TCHAR(Message->message);
}

static void CaptureUpgradeMessageB(asSMessageInfo* Message)
{
	++GUpgradeMessageCallbackBCount;
	GUpgradeMessageCallbackBText = UTF8_TO_TCHAR(Message->message);
}

public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	TEST_METHOD(HeaderCompatibility)
	{
ASSERT_THAT(AreEqual(23300, ANGELSCRIPT_VERSION, TEXT("Embedded Angelscript version should remain pinned to 2.33.0 until the 2.38 upgrade resumes")));
		ASSERT_THAT(AreEqual(FString(TEXT("2.33.0 WIP")), FString(ANSI_TO_TCHAR(ANGELSCRIPT_VERSION_STRING)), TEXT("Embedded Angelscript version string should report 2.33.0 WIP")));

		ASSERT_THAT(AreEqual(29, static_cast<int32>(asEP_INIT_STACK_SIZE), TEXT("Stock 2.38 init stack size property id should remain available")));
		ASSERT_THAT(AreEqual(30, static_cast<int32>(asEP_INIT_CALL_STACK_SIZE), TEXT("Stock 2.38 init call stack size property id should remain available")));
		ASSERT_THAT(AreEqual(31, static_cast<int32>(asEP_MAX_CALL_STACK_SIZE), TEXT("Stock 2.38 max call stack size property id should remain available")));
		ASSERT_THAT(AreEqual(32, static_cast<int32>(asEP_IGNORE_DUPLICATE_SHARED_INTF), TEXT("Stock 2.38 duplicate shared interface property id should remain available")));
		ASSERT_THAT(AreEqual(33, static_cast<int32>(asEP_NO_DEBUG_OUTPUT), TEXT("Stock 2.38 no debug output property id should remain available")));
		ASSERT_THAT(AreEqual(34, static_cast<int32>(asEP_DISABLE_SCRIPT_CLASS_GC), TEXT("Stock 2.38 disable script class GC property id should remain available")));
		ASSERT_THAT(AreEqual(35, static_cast<int32>(asEP_JIT_INTERFACE_VERSION), TEXT("Stock 2.38 JIT interface version property id should remain available")));
		ASSERT_THAT(AreEqual(36, static_cast<int32>(asEP_ALWAYS_IMPL_DEFAULT_COPY), TEXT("Stock 2.38 default copy property id should remain available")));
		ASSERT_THAT(AreEqual(37, static_cast<int32>(asEP_ALWAYS_IMPL_DEFAULT_COPY_CONSTRUCT), TEXT("Stock 2.38 default copy construct property id should remain available")));
		ASSERT_THAT(AreEqual(38, static_cast<int32>(asEP_MEMBER_INIT_MODE), TEXT("Stock 2.38 member init mode property id should remain available")));
		ASSERT_THAT(AreEqual(39, static_cast<int32>(asEP_BOOL_CONVERSION_MODE), TEXT("Stock 2.38 bool conversion mode property id should remain available")));
		ASSERT_THAT(AreEqual(40, static_cast<int32>(asEP_FOREACH_SUPPORT), TEXT("Stock 2.38 foreach support property id should remain available")));
		ASSERT_THAT(AreEqual(P9BAutomaticImportsPropertyId, static_cast<int32>(asEP_AUTOMATIC_IMPORTS), TEXT("APV2 automatic imports property id should move off the stock 2.38 property range")));
		ASSERT_THAT(AreEqual(P9BTypecheckSwitchEnumsPropertyId, static_cast<int32>(asEP_TYPECHECK_SWITCH_ENUMS), TEXT("APV2 typecheck switch enums property id should move off the stock 2.38 property range")));
		ASSERT_THAT(AreEqual(P9BAllowDoubleTypePropertyId, static_cast<int32>(asEP_ALLOW_DOUBLE_TYPE), TEXT("APV2 allow double type property id should move off the stock 2.38 property range")));
		ASSERT_THAT(AreEqual(P9BFloatIsFloat64PropertyId, static_cast<int32>(asEP_FLOAT_IS_FLOAT64), TEXT("APV2 float64 compatibility property id should move off the stock 2.38 property range")));
		ASSERT_THAT(AreEqual(P9BWarnOnFloatConstantsForDoublesPropertyId, static_cast<int32>(asEP_WARN_ON_FLOAT_CONSTANTS_FOR_DOUBLES), TEXT("APV2 float warning property id should move off the stock 2.38 property range")));
		ASSERT_THAT(AreEqual(P9BWarnIntegerDivisionPropertyId, static_cast<int32>(asEP_WARN_INTEGER_DIVISION), TEXT("APV2 integer division warning property id should move off the stock 2.38 property range")));

		ASSERT_THAT(AreEqual(static_cast<int32>(sizeof(asQWORD)), static_cast<int32>(sizeof(asEObjTypeFlags)), TEXT("asEObjTypeFlags should widen to preserve stock 2.38 and APV2 high-bit object flags")));
		ASSERT_THAT(AreEqual(P9BScriptObjectFlag, static_cast<uint64>(asOBJ_SCRIPT_OBJECT), TEXT("Stock 2.38 should restore asOBJ_SCRIPT_OBJECT to bit 21")));
		ASSERT_THAT(AreEqual(P9BSharedFlag, static_cast<uint64>(asOBJ_SHARED), TEXT("Stock 2.38 should restore asOBJ_SHARED to bit 22")));
		ASSERT_THAT(AreEqual(P9BNoInheritFlag, static_cast<uint64>(asOBJ_NOINHERIT), TEXT("Stock 2.38 should restore asOBJ_NOINHERIT to bit 23")));
		ASSERT_THAT(AreEqual(P9BFuncdefFlag, static_cast<uint64>(asOBJ_FUNCDEF), TEXT("Stock 2.38 should restore asOBJ_FUNCDEF to bit 24")));
		ASSERT_THAT(AreEqual(P9BListPatternFlag, static_cast<uint64>(asOBJ_LIST_PATTERN), TEXT("Stock 2.38 should restore asOBJ_LIST_PATTERN to bit 25")));
		ASSERT_THAT(AreEqual(P9BEnumFlag, static_cast<uint64>(asOBJ_ENUM), TEXT("Stock 2.38 should restore asOBJ_ENUM to bit 26")));
		ASSERT_THAT(AreEqual(P9BTemplateSubtypeFlag, static_cast<uint64>(asOBJ_TEMPLATE_SUBTYPE), TEXT("Stock 2.38 should restore asOBJ_TEMPLATE_SUBTYPE to bit 27")));
		ASSERT_THAT(AreEqual(P9BTypedefFlag, static_cast<uint64>(asOBJ_TYPEDEF), TEXT("Stock 2.38 should restore asOBJ_TYPEDEF to bit 28")));
		ASSERT_THAT(AreEqual(P9BAbstractFlag, static_cast<uint64>(asOBJ_ABSTRACT), TEXT("Stock 2.38 should restore asOBJ_ABSTRACT to bit 29")));
		ASSERT_THAT(AreEqual(P9BStockMoreConstructorsFlag, static_cast<uint64>(asOBJ_APP_CLASS_MORE_CONSTRUCTORS), TEXT("Stock 2.38 should preserve asOBJ_APP_CLASS_MORE_CONSTRUCTORS on bit 31")));
		ASSERT_THAT(AreEqual(P9BStockUnionFlag, static_cast<uint64>(asOBJ_APP_CLASS_UNION), TEXT("Stock 2.38 should preserve asOBJ_APP_CLASS_UNION on bit 32")));
		ASSERT_THAT(AreEqual(P9BCovariantSubtypeFlag, static_cast<uint64>(asOBJ_TEMPLATE_SUBTYPE_COVARIANT), TEXT("APV2 covariant subtype flag should move to the high-bit private range")));
		ASSERT_THAT(AreEqual(P9BDeterminesSizeFlag, static_cast<uint64>(asOBJ_TEMPLATE_SUBTYPE_DETERMINES_SIZE), TEXT("APV2 template-size flag should move to the high-bit private range")));
		ASSERT_THAT(AreEqual(P9BDisallowInstantiationFlag, static_cast<uint64>(asOBJ_DISALLOW_INSTANTIATION), TEXT("APV2 disallow-instantiation flag should move to the high-bit private range")));
		ASSERT_THAT(AreEqual(P9BBasicMathTypeFlag, static_cast<uint64>(asOBJ_BASICMATHTYPE), TEXT("APV2 basic-math flag should move to the high-bit private range")));
		ASSERT_THAT(AreEqual(P9BEditorOnlyFlag, static_cast<uint64>(asOBJ_EDITOR_ONLY), TEXT("APV2 editor-only flag should move to the high-bit private range")));
		ASSERT_THAT(AreEqual(10, static_cast<int32>(asTYPEID_FLOAT32), TEXT("APV2 float32 alias should preserve the stock float type id")));
		ASSERT_THAT(AreEqual(11, static_cast<int32>(asTYPEID_FLOAT64), TEXT("APV2 float64 alias should preserve the stock double type id")));
		ASSERT_THAT(AreEqual(212, static_cast<int32>(asBC_MAXBYTECODE), TEXT("APV2 custom bytecodes should keep the extended bytecode max")));
	}

	TEST_METHOD(EngineProperties)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

	asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
	ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Upgrade property test should create a script engine")));

	const int64 PreviousAutomaticImports = static_cast<int64>(ScriptEngine->GetEngineProperty(asEP_AUTOMATIC_IMPORTS));
	ScriptEngine->SetEngineProperty(asEP_AUTOMATIC_IMPORTS, 1);
	ASSERT_THAT(AreEqual(int64(1), static_cast<int64>(ScriptEngine->GetEngineProperty(asEP_AUTOMATIC_IMPORTS)), TEXT("APV2 automatic imports property should round-trip through the getter")));
	ScriptEngine->SetEngineProperty(asEP_AUTOMATIC_IMPORTS, PreviousAutomaticImports);

	ASSERT_THAT(AreEqual(int64(1), static_cast<int64>(ScriptEngine->GetEngineProperty(asEP_ALWAYS_IMPL_DEFAULT_COPY)), TEXT("Stock 2.38 default copy property should be forced to the APV2 compatibility value during engine initialization")));
	ASSERT_THAT(AreEqual(int64(1), static_cast<int64>(ScriptEngine->GetEngineProperty(asEP_ALWAYS_IMPL_DEFAULT_COPY_CONSTRUCT)), TEXT("Stock 2.38 default copy construct property should be forced to the APV2 compatibility value during engine initialization")));
	ASSERT_THAT(AreEqual(int64(0), static_cast<int64>(ScriptEngine->GetEngineProperty(asEP_MEMBER_INIT_MODE)), TEXT("Stock 2.38 member init mode should be forced to the APV2 compatibility value during engine initialization")));
	ASSERT_THAT(AreEqual(int64(0), static_cast<int64>(ScriptEngine->GetEngineProperty(asEP_PROPERTY_ACCESSOR_MODE)), TEXT("Property accessor mode should be disabled during engine initialization")));
	ASSERT_THAT(AreEqual(int64(1), static_cast<int64>(ScriptEngine->GetEngineProperty(asEP_TYPECHECK_SWITCH_ENUMS)), TEXT("APV2 typecheck switch enums property should still be enabled")));
	ASSERT_THAT(AreEqual(int64(1), static_cast<int64>(ScriptEngine->GetEngineProperty(asEP_ALLOW_DOUBLE_TYPE)), TEXT("APV2 double type property should remain enabled when settings allow it")));

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

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Cases); ++Index)
	{
		const FPropertyCase& Case = Cases[Index];
		ScriptEngine->SetEngineProperty(Case.Property, Case.Value);
		ASSERT_THAT(AreEqual(static_cast<int64>(Case.Value), static_cast<int64>(ScriptEngine->GetEngineProperty(Case.Property)), FString::Printf(TEXT("Stock engine property %d should round-trip through the getter"), static_cast<int32>(Case.Property))));
	}

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Cases); ++Index)
	{
		ScriptEngine->SetEngineProperty(Cases[Index].Property, PreviousValues[Index]);
	}

	}

	TEST_METHOD(EngineProperties_CallStackLimitOverflow)
	{
	TUniquePtr<FAngelscriptEngine> IsolatedEngine = CreateIsolatedCloneEngine();
	ASSERT_THAT(IsNotNull(IsolatedEngine.Get(), TEXT("Upgrade.EngineProperties.CallStackLimitOverflow should create an isolated clone engine")));

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

	asIScriptModule* Module = BuildModule(*TestRunner, Engine, "UpgradeCallStackLimit", RecursiveScript);
	ASSERT_THAT(IsNotNull(Module));

	asIScriptFunction* RunFunction = GetFunctionByDecl(*TestRunner, *Module, TEXT("void Recursive(int)"));
	ASSERT_THAT(IsNotNull(RunFunction));

	asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
	ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Upgrade.EngineProperties.CallStackLimitOverflow should expose the backing script engine")));

	asIScriptContext* Context = Engine.CreateContext();
	ASSERT_THAT(IsNotNull(Context, TEXT("Upgrade.EngineProperties.CallStackLimitOverflow should create an execution context")));
	ON_SCOPE_EXIT
	{
		Context->Release();
	};

	ScriptEngine->SetEngineProperty(asEP_INIT_CALL_STACK_SIZE, 1);
	ScriptEngine->SetEngineProperty(asEP_MAX_CALL_STACK_SIZE, 1);
	ScriptEngine->SetEngineProperty(asEP_MAX_NESTED_CALLS, 1);

	const int PrepareResult = Context->Prepare(RunFunction);
	ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), PrepareResult, TEXT("Upgrade.EngineProperties.CallStackLimitOverflow should prepare the recursive entry point")));
	ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Context->SetArgDWord(0, 1000), TEXT("Upgrade.EngineProperties.CallStackLimitOverflow should bind the recursive depth argument")));

	TestRunner->AddExpectedError(TEXT("Stack overflow: potential infinite recursion detected?"), EAutomationExpectedErrorFlags::Contains, 1);
	TestRunner->AddExpectedError(TEXT("UpgradeCallStackLimit"), EAutomationExpectedErrorFlags::Contains, 1);
	TestRunner->AddExpectedError(TEXT("void Recursive(int) | Line"), EAutomationExpectedErrorFlags::Contains, -1);

	const int ExecuteResult = Context->Execute();
	const char* ExceptionStringAnsi = Context->GetExceptionString();
	const FString ExceptionString = ExceptionStringAnsi != nullptr ? UTF8_TO_TCHAR(ExceptionStringAnsi) : FString();
	ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), ExecuteResult, TEXT("Upgrade.EngineProperties.CallStackLimitOverflow should raise an execution exception once the migrated call-stack properties are enforced")));
	ASSERT_THAT(IsFalse(ExceptionString.IsEmpty(), TEXT("Upgrade.EngineProperties.CallStackLimitOverflow should expose a non-empty exception string after the overflow")));
	}

	TEST_METHOD(MessageCallback)
	{
FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
	FAngelscriptEngineScope Scope(Engine);

	asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
	ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Upgrade message callback test should create a script engine")));

	GUpgradeMessageCallbackInvoked = false;
	GUpgradeMessageText.Reset();
	GUpgradeMessageType = asMSGTYPE_INFORMATION;

	ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ScriptEngine->SetMessageCallback(asFUNCTION(CaptureUpgradeMessage), nullptr, asCALL_CDECL), TEXT("SetMessageCallback should succeed for the upgrade compatibility callback")));

	asSFuncPtr CallbackPtr = {};
	void* CallbackObject = nullptr;
	asDWORD CallConv = 0;
	ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ScriptEngine->GetMessageCallback(&CallbackPtr, &CallbackObject, &CallConv), TEXT("GetMessageCallback should report the registered callback")));

	ASSERT_THAT(AreEqual(static_cast<int32>(asCALL_CDECL), static_cast<int32>(CallConv), TEXT("GetMessageCallback should preserve the original call convention")));

	ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ScriptEngine->WriteMessage("Upgrade", 1, 1, asMSGTYPE_WARNING, "CallbackRoundtrip"), TEXT("WriteMessage should succeed after restoring the stock callback getter ABI")));

	ASSERT_THAT(IsTrue(GUpgradeMessageCallbackInvoked, TEXT("The registered upgrade callback should receive WriteMessage diagnostics")));
	ASSERT_THAT(AreEqual(FString(TEXT("CallbackRoundtrip")), GUpgradeMessageText, TEXT("The registered upgrade callback should receive the expected message text")));
	ASSERT_THAT(AreEqual(static_cast<int32>(asMSGTYPE_WARNING), static_cast<int32>(GUpgradeMessageType), TEXT("The registered upgrade callback should receive the expected message type")));
	}

	TEST_METHOD(MessageCallback_ClearAndReRegister)
	{
TUniquePtr<FAngelscriptEngine> IsolatedEngine = CreateIsolatedCloneEngine();
	ASSERT_THAT(IsNotNull(IsolatedEngine.Get(), TEXT("Upgrade.MessageCallback.ClearAndReRegister should create an isolated clone engine")));

	FAngelscriptEngine& Engine = *IsolatedEngine;
	FAngelscriptEngineScope EngineScope(Engine);
	asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
	ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Upgrade.MessageCallback.ClearAndReRegister should expose the backing script engine")));

	GUpgradeMessageCallbackACount = 0;
	GUpgradeMessageCallbackBCount = 0;
	GUpgradeMessageCallbackAText.Reset();
	GUpgradeMessageCallbackBText.Reset();

	ON_SCOPE_EXIT
	{
		ScriptEngine->ClearMessageCallback();
	};

	ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ScriptEngine->SetMessageCallback(asFUNCTION(CaptureUpgradeMessageA), nullptr, asCALL_CDECL), TEXT("Upgrade.MessageCallback.ClearAndReRegister should register callback A")));
	ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ScriptEngine->WriteMessage("Upgrade", 1, 1, asMSGTYPE_WARNING, "CallbackA"), TEXT("Upgrade.MessageCallback.ClearAndReRegister should dispatch the first message successfully")));
	ASSERT_THAT(AreEqual(1, GUpgradeMessageCallbackACount, TEXT("Upgrade.MessageCallback.ClearAndReRegister should deliver the first message to callback A exactly once")));
	ASSERT_THAT(AreEqual(FString(TEXT("CallbackA")), GUpgradeMessageCallbackAText, TEXT("Upgrade.MessageCallback.ClearAndReRegister should preserve the first callback payload")));
	ASSERT_THAT(AreEqual(0, GUpgradeMessageCallbackBCount, TEXT("Upgrade.MessageCallback.ClearAndReRegister should keep callback B untouched before re-registration")));

	ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ScriptEngine->ClearMessageCallback(), TEXT("Upgrade.MessageCallback.ClearAndReRegister should clear the active callback successfully")));
	ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ScriptEngine->WriteMessage("Upgrade", 1, 1, asMSGTYPE_WARNING, "CallbackAfterClear"), TEXT("Upgrade.MessageCallback.ClearAndReRegister should allow WriteMessage after the callback is cleared")));
	ASSERT_THAT(AreEqual(1, GUpgradeMessageCallbackACount, TEXT("Upgrade.MessageCallback.ClearAndReRegister should keep callback A count unchanged after ClearMessageCallback")));
	ASSERT_THAT(AreEqual(0, GUpgradeMessageCallbackBCount, TEXT("Upgrade.MessageCallback.ClearAndReRegister should keep callback B count unchanged while no callback is registered")));

	ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ScriptEngine->SetMessageCallback(asFUNCTION(CaptureUpgradeMessageB), nullptr, asCALL_CDECL), TEXT("Upgrade.MessageCallback.ClearAndReRegister should register callback B after the clear")));

	asSFuncPtr CallbackPtr = {};
	void* CallbackObject = nullptr;
	asDWORD CallConv = 0;
	ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ScriptEngine->GetMessageCallback(&CallbackPtr, &CallbackObject, &CallConv), TEXT("Upgrade.MessageCallback.ClearAndReRegister should report a registered callback after re-registration")));
	ASSERT_THAT(AreEqual(static_cast<int32>(asCALL_CDECL), static_cast<int32>(CallConv), TEXT("Upgrade.MessageCallback.ClearAndReRegister should preserve the callback B call convention")));
	ASSERT_THAT(IsNull(CallbackObject, TEXT("Upgrade.MessageCallback.ClearAndReRegister should not attach an object instance when re-registering a free function")));

	ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ScriptEngine->WriteMessage("Upgrade", 1, 1, asMSGTYPE_WARNING, "CallbackB"), TEXT("Upgrade.MessageCallback.ClearAndReRegister should dispatch the third message successfully")));
	ASSERT_THAT(AreEqual(1, GUpgradeMessageCallbackACount, TEXT("Upgrade.MessageCallback.ClearAndReRegister should keep callback A count frozen after callback B takes over")));
	ASSERT_THAT(AreEqual(1, GUpgradeMessageCallbackBCount, TEXT("Upgrade.MessageCallback.ClearAndReRegister should deliver the third message to callback B exactly once")));
	ASSERT_THAT(AreEqual(FString(TEXT("CallbackB")), GUpgradeMessageCallbackBText, TEXT("Upgrade.MessageCallback.ClearAndReRegister should preserve the re-registered callback payload")));
	}

	TEST_METHOD(RegisterObjectTypeFlags)
	{
asQWORD Flags = 0;
	FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
	FAngelscriptEngineScope Scope(Engine);

	asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
	ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Upgrade object-type registration test should create a script engine")));

	static const char* TypeName = "FUpgradeEditorOnlyRegisteredType";
	const int RegistrationResult = ScriptEngine->RegisterObjectType(TypeName, 4, asOBJ_VALUE | asOBJ_POD | asOBJ_APP_PRIMITIVE | asOBJ_EDITOR_ONLY);
	ASSERT_THAT(IsTrue(RegistrationResult >= 0, TEXT("RegisterObjectType should accept a migrated high-bit editor-only flag on an application value type")));

	asITypeInfo* TypeInfo = ScriptEngine->GetTypeInfoByName(TypeName);
	ASSERT_THAT(IsNotNull(TypeInfo, TEXT("RegisterObjectType should expose the registered type by name")));

	Flags = TypeInfo->GetFlags();

	ASSERT_THAT(IsTrue((Flags & asOBJ_EDITOR_ONLY) != 0, TEXT("The registered type should preserve the migrated editor-only high-bit flag")));
	ASSERT_THAT(IsFalse((Flags & asOBJ_APP_CLASS_MORE_CONSTRUCTORS) != 0, TEXT("The registered type should not alias the stock more-constructors bit when using the migrated editor-only flag")));
	}

	TEST_METHOD(CStringHash)
	{
	const asCString MixedCase("AlphaBeta");
	const asCString LowerCase("alphabeta");
	const asCString DifferentValue("gamma");

	const uint32 MixedHash = GetTypeHash(MixedCase);
	const uint32 LowerHash = GetTypeHash(LowerCase);
	const uint32 DifferentHash = GetTypeHash(DifferentValue);

	ASSERT_THAT(AreEqual(LowerHash, MixedHash, TEXT("asCString hashing should remain case-insensitive for equal content")));
	ASSERT_THAT(IsTrue(MixedHash != DifferentHash, TEXT("asCString hashing should still distinguish different content")));
	}
};

#endif
