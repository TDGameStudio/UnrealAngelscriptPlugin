#include "AngelscriptTestAdapter.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;
using namespace AngelscriptSDKTestSupport;

TEST_CLASS_WITH_FLAGS(FAngelscriptSDKGlobalVarTests,
	"Angelscript.TestModule.AngelScriptSDK.GlobalVar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	inline static const char* RecursiveScript = R"(
void recursive(int n)
{
	if (n > 0)
	{
		recursive(n - 1);
	}
}
)";

	static int FindGlobalVarIndexByName(asIScriptModule* Module, const char* Name)
	{
		if (Module == nullptr || Name == nullptr)
		{
			return -1;
		}

		const asUINT GlobalVarCount = Module->GetGlobalVarCount();
		for (asUINT Index = 0; Index < GlobalVarCount; ++Index)
		{
			const char* GlobalVarName = nullptr;
			if (Module->GetGlobalVar(Index, &GlobalVarName) >= 0 && GlobalVarName != nullptr && std::strcmp(GlobalVarName, Name) == 0)
			{
				return static_cast<int>(Index);
			}
		}

		return -1;
	}

	static int FindGlobalVarIndexByDeclaration(asIScriptModule* Module, const char* Declaration)
	{
		if (Module == nullptr || Declaration == nullptr)
		{
			return -1;
		}

		const asUINT GlobalVarCount = Module->GetGlobalVarCount();
		for (asUINT Index = 0; Index < GlobalVarCount; ++Index)
		{
			const char* GlobalDeclaration = Module->GetGlobalVarDeclaration(Index);
			if (GlobalDeclaration != nullptr && std::strcmp(GlobalDeclaration, Declaration) == 0)
			{
				return static_cast<int>(Index);
			}
		}

		return -1;
	}

	inline static FNativeTestEngine Engine;

	BEFORE_ALL()
	{
		Engine.Create(*TestRunner);
	}

	AFTER_ALL()
	{
		Engine.Destroy();
	}

	BEFORE_EACH()
	{
		Engine.ResetMessages();
	}

	TEST_METHOD(Enumerate)
	{
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK global-var enumeration test should create a standalone engine")));

		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"SDKGlobalVarEnumerate",
			"const int a = 1; const double b = 2.0; const double c = 35.2; const uint d = 0xC0DE;");
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		ASSERT_THAT(AreEqual(4, static_cast<int32>(Module->GetGlobalVarCount()), TEXT("SDK global-var enumeration test should expose four globals")));

		const char* Declaration = Module->GetGlobalVarDeclaration(0);
		ASSERT_THAT(IsNotNull(Declaration, TEXT("SDK global-var enumeration test should report the first declaration")));

		ASSERT_THAT(AreEqual(FString(TEXT("const int a")), FString(UTF8_TO_TCHAR(Declaration)), TEXT("SDK global-var enumeration test should preserve the first declaration")));

		ASSERT_THAT(AreEqual(1, FindGlobalVarIndexByName(Module, "b"), TEXT("SDK global-var enumeration test should find globals by name")));

		const int ConstGlobalIndex = FindGlobalVarIndexByName(Module, "c");
		ASSERT_THAT(AreEqual(2, ConstGlobalIndex, TEXT("SDK global-var enumeration test should find the const global by name")));

		const char* ConstGlobalName = nullptr;
		int ConstGlobalTypeId = 0;
		bool bIsConstGlobal = false;
		ASSERT_THAT(IsTrue(Module->GetGlobalVar(ConstGlobalIndex, &ConstGlobalName, nullptr, &ConstGlobalTypeId, &bIsConstGlobal) >= 0, TEXT("SDK global-var enumeration test should read metadata for the const global")));

		ASSERT_THAT(AreEqual(FString(TEXT("c")), FString(UTF8_TO_TCHAR(ConstGlobalName != nullptr ? ConstGlobalName : "")), TEXT("SDK global-var enumeration test should preserve the const global name")));

		ASSERT_THAT(IsTrue(bIsConstGlobal, TEXT("SDK global-var enumeration test should mark the third global as const")));

		const char* GlobalName = nullptr;
		Module->GetGlobalVar(3, &GlobalName);
		ASSERT_THAT(AreEqual(FString(TEXT("d")), FString(UTF8_TO_TCHAR(GlobalName != nullptr ? GlobalName : "")), TEXT("SDK global-var enumeration test should expose the global name for index 3")));

		asUINT* DValue = static_cast<asUINT*>(Module->GetAddressOfGlobalVar(3));
		ASSERT_THAT(IsNotNull(DValue, TEXT("SDK global-var enumeration test should expose the uint storage")));

		ASSERT_THAT(AreEqual(static_cast<uint32>(0xC0DE), static_cast<uint32>(*DValue), TEXT("SDK global-var enumeration test should preserve the uint initializer")));
	}

	TEST_METHOD(ResetState)
	{
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK global-var reset test should create a standalone engine")));

		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"SDKGlobalVarReset",
			"const double First = 2.0; const double Second = 5.0;");
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), Module->ResetGlobalVars(), TEXT("SDK global-var reset test should reset globals successfully")));

		ASSERT_THAT(AreEqual(2, static_cast<int32>(Module->GetGlobalVarCount()), TEXT("SDK global-var reset test should keep both const globals after reset")));
	}

	TEST_METHOD(RemoveBeforeDiscard)
	{
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK global-var remove test should create a standalone engine")));

		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"SDKGlobalVarRemove",
			"const int First = 1; const int Second = 2;");
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		ASSERT_THAT(AreEqual(2, static_cast<int32>(Module->GetGlobalVarCount()), TEXT("SDK global-var remove test should start with two globals")));

		ASSERT_THAT(IsTrue(Module->RemoveGlobalVar(0) >= 0, TEXT("SDK global-var remove test should remove the first global successfully")));

		ASSERT_THAT(AreEqual(1, static_cast<int32>(Module->GetGlobalVarCount()), TEXT("SDK global-var remove test should leave one global after removal")));
	}

	TEST_METHOD(InitializerExpression)
	{
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK global-var initializer test should create a standalone engine")));

		// Global initializer expression should be evaluated at module build time.
		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"SDKGlobalVarInitializer",
			"const int computed = 10 * 3 + 7;");
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		const int ComputedIndex = FindGlobalVarIndexByName(Module, "computed");
		ASSERT_THAT(IsTrue(ComputedIndex >= 0, TEXT("SDK global-var initializer test should find the computed global")));

		const int* ComputedValue = static_cast<const int*>(Module->GetAddressOfGlobalVar(ComputedIndex));
		ASSERT_THAT(IsNotNull(ComputedValue, TEXT("SDK global-var initializer test should expose the storage")));

		ASSERT_THAT(AreEqual(37, *ComputedValue, TEXT("SDK global-var initializer test should evaluate 10*3+7 = 37 at build time")));
	}

	TEST_METHOD(ConstReadAccess)
	{
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK global-var const-read test should create a standalone engine")));

		// This fork requires all script globals to be const. C++ reads the const value.
		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"SDKGlobalVarConstRead",
			R"(
const int limit = 200;

int Entry()
{
	return limit * 2;
}
)");
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		const int LimitIndex = FindGlobalVarIndexByName(Module, "limit");
		ASSERT_THAT(IsTrue(LimitIndex >= 0, TEXT("SDK global-var const-read test should find the limit global")));

		const int* Limit = static_cast<const int*>(Module->GetAddressOfGlobalVar(LimitIndex));
		ASSERT_THAT(IsNotNull(Limit, TEXT("SDK global-var const-read test should expose const storage")));

		ASSERT_THAT(AreEqual(200, *Limit, TEXT("SDK global-var const-read test should read limit = 200")));

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Entry()", Result))
		{
			return;
		}

		ASSERT_THAT(AreEqual(400, Result, TEXT("SDK global-var const-read test should compute limit*2 = 400")));
	}

	TEST_METHOD(DeclarationString)
	{
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK global-var declaration-string test should create a standalone engine")));

		// Verify GetGlobalVarDeclaration returns a non-empty string for each global.
		FScopedNativeModule Module(
			*TestRunner,
			Engine,
			"SDKGlobalVarDeclString",
			"const double pi = 3.14159; const int answer = 42;");
		if (!Module.IsValid())
		{
			TestRunner->AddInfo(Engine.GetMessagesText());
			return;
		}

		const asUINT GlobalCount = Module->GetGlobalVarCount();
		ASSERT_THAT(AreEqual(2, static_cast<int32>(GlobalCount), TEXT("SDK global-var declaration-string test should have 2 globals")));

		for (asUINT i = 0; i < GlobalCount; ++i)
		{
			const char* Decl = Module->GetGlobalVarDeclaration(i);
			ASSERT_THAT(IsNotNull(Decl, TEXT("SDK global-var declaration-string test should return a declaration string")));
			const size_t Len = std::strlen(Decl);
			ASSERT_THAT(IsTrue(Len > 0, TEXT("SDK global-var declaration-string test should return a non-empty declaration")));
		}

		ASSERT_THAT(IsTrue(true, TEXT("SDK global-var declaration-string test verified all declarations are non-empty")));
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptSDKStackTests,
	"Angelscript.TestModule.AngelScriptSDK.Stack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(DataLimit)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK stack data-limit test should create a standalone engine")));

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKStackDataLimit", FAngelscriptSDKGlobalVarTests::RecursiveScript);
		if (!this->Assert.IsNotNull(Module, TEXT("SDK stack data-limit test should compile the recursive module")))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		asIScriptContext* Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("SDK stack data-limit test should create a context")));

		ScriptEngine->SetEngineProperty(asEP_INIT_STACK_SIZE, 256);
		ScriptEngine->SetEngineProperty(asEP_MAX_STACK_SIZE, 256);
		Context->Prepare(GetNativeFunctionByDecl(Module, "void recursive(int)"));
		Context->SetArgDWord(0, 100);
		const int ExecuteResult = Context->Execute();
		Context->Release();
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), ExecuteResult, TEXT("SDK stack data-limit test should raise an execution exception when the data stack overflows")));
	}

	TEST_METHOD(CallLimit)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK stack call-limit test should create a standalone engine")));

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKStackCallLimit", FAngelscriptSDKGlobalVarTests::RecursiveScript);
		if (!this->Assert.IsNotNull(Module, TEXT("SDK stack call-limit test should compile the recursive module")))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		asIScriptContext* Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("SDK stack call-limit test should create a context")));

		ScriptEngine->SetEngineProperty(asEP_INIT_CALL_STACK_SIZE, 1);
		ScriptEngine->SetEngineProperty(asEP_MAX_CALL_STACK_SIZE, 1);
		ScriptEngine->SetEngineProperty(asEP_MAX_NESTED_CALLS, 1);
		Context->Prepare(GetNativeFunctionByDecl(Module, "void recursive(int)"));
		Context->SetArgDWord(0, 1000);
		const int ExecuteResult = Context->Execute();
		Context->Release();
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), ExecuteResult, TEXT("SDK stack call-limit test should raise an execution exception when the call stack overflows")));
	}

	TEST_METHOD(ExceptionLocation)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK stack exception-location test should create a standalone engine")));

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		ScriptEngine->SetEngineProperty(asEP_MAX_STACK_SIZE, 256);
		asIScriptModule* Module = BuildNativeModule(
			ScriptEngine,
			"SDKStackExceptionLocation",
			FAngelscriptSDKGlobalVarTests::RecursiveScript);
		if (!this->Assert.IsNotNull(Module, TEXT("SDK stack exception-location test should compile the overflow module")))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		asIScriptContext* Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("SDK stack exception-location test should create a context")));

		const int PrepareResult = Context->Prepare(GetNativeFunctionByDecl(Module, "void recursive(int)"));
		if (!this->Assert.AreEqual(static_cast<int32>(asSUCCESS), PrepareResult, TEXT("SDK stack exception-location test should prepare the entry point")))
		{
			Context->Release();
			return;
		}

		Context->SetArgDWord(0, 100);
		const int ExecuteResult = Context->Execute();
		const FString ExceptionString = UTF8_TO_TCHAR(Context->GetExceptionString() != nullptr ? Context->GetExceptionString() : "");
		const FString ExceptionFunctionName = Context->GetExceptionFunction() != nullptr
			? UTF8_TO_TCHAR(Context->GetExceptionFunction()->GetName())
			: FString();
		Context->Release();

		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION), ExecuteResult, TEXT("SDK stack exception-location test should raise an execution exception")));

		ASSERT_THAT(AreEqual(FString(TEXT("Stack overflow")), ExceptionString, TEXT("SDK stack exception-location test should surface the stack overflow reason")));

		ASSERT_THAT(AreEqual(FString(TEXT("recursive")), ExceptionFunctionName, TEXT("SDK stack exception-location test should report the recursive function as the overflow site")));
	}
};

#endif
