#include "AngelscriptTestAdapter.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;
using namespace AngelscriptSDKTestSupport;

namespace
{
	static const char* RecursiveScript = R"(
void recursive(int n)
{
	if (n > 0)
	{
		recursive(n - 1);
	}
}
)";

	int FindGlobalVarIndexByName(asIScriptModule* Module, const char* Name)
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

	int FindGlobalVarIndexByDeclaration(asIScriptModule* Module, const char* Declaration)
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
}


TEST_CLASS_WITH_FLAGS(FAngelscriptSDKGlobalVarTests,
	"Angelscript.TestModule.AngelScriptSDK.GlobalVar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(Enumerate)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK global-var enumeration test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildNativeModule(
			ScriptEngine,
			"SDKGlobalVarEnumerate",
			"const int a = 1; const double b = 2.0; const double c = 35.2; const uint d = 0xC0DE;");
		if (!TestRunner->TestNotNull(TEXT("SDK global-var enumeration test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		if (!TestRunner->TestEqual(TEXT("SDK global-var enumeration test should expose four globals"), static_cast<int32>(Module->GetGlobalVarCount()), 4))
		{
			return;
		}

		const char* Declaration = Module->GetGlobalVarDeclaration(0);
		if (!TestRunner->TestNotNull(TEXT("SDK global-var enumeration test should report the first declaration"), Declaration))
		{
			return;
		}

		if (!TestRunner->TestEqual(TEXT("SDK global-var enumeration test should preserve the first declaration"), FString(UTF8_TO_TCHAR(Declaration)), FString(TEXT("const int a"))))
		{
			return;
		}

		if (!TestRunner->TestEqual(TEXT("SDK global-var enumeration test should find globals by name"), FindGlobalVarIndexByName(Module, "b"), 1))
		{
			return;
		}

		const int ConstGlobalIndex = FindGlobalVarIndexByName(Module, "c");
		if (!TestRunner->TestEqual(TEXT("SDK global-var enumeration test should find the const global by name"), ConstGlobalIndex, 2))
		{
			return;
		}

		const char* ConstGlobalName = nullptr;
		int ConstGlobalTypeId = 0;
		bool bIsConstGlobal = false;
		if (!TestRunner->TestTrue(TEXT("SDK global-var enumeration test should read metadata for the const global"), Module->GetGlobalVar(ConstGlobalIndex, &ConstGlobalName, nullptr, &ConstGlobalTypeId, &bIsConstGlobal) >= 0))
		{
			return;
		}

		if (!TestRunner->TestEqual(TEXT("SDK global-var enumeration test should preserve the const global name"), FString(UTF8_TO_TCHAR(ConstGlobalName != nullptr ? ConstGlobalName : "")), FString(TEXT("c"))))
		{
			return;
		}

		if (!TestRunner->TestTrue(TEXT("SDK global-var enumeration test should mark the third global as const"), bIsConstGlobal))
		{
			return;
		}

		const char* GlobalName = nullptr;
		Module->GetGlobalVar(3, &GlobalName);
		if (!TestRunner->TestEqual(TEXT("SDK global-var enumeration test should expose the global name for index 3"), FString(UTF8_TO_TCHAR(GlobalName != nullptr ? GlobalName : "")), FString(TEXT("d"))))
		{
			return;
		}

		asUINT* DValue = static_cast<asUINT*>(Module->GetAddressOfGlobalVar(3));
		if (!TestRunner->TestNotNull(TEXT("SDK global-var enumeration test should expose the uint storage"), DValue))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("SDK global-var enumeration test should preserve the uint initializer"), static_cast<uint32>(*DValue), static_cast<uint32>(0xC0DE));
	}

	TEST_METHOD(ResetState)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK global-var reset test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildNativeModule(
			ScriptEngine,
			"SDKGlobalVarReset",
			"const double First = 2.0; const double Second = 5.0;");
		if (!TestRunner->TestNotNull(TEXT("SDK global-var reset test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		if (!TestRunner->TestEqual(TEXT("SDK global-var reset test should reset globals successfully"), Module->ResetGlobalVars(), static_cast<int32>(asSUCCESS)))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("SDK global-var reset test should keep both const globals after reset"), static_cast<int32>(Module->GetGlobalVarCount()), 2);
	}

	TEST_METHOD(RemoveBeforeDiscard)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK global-var remove test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildNativeModule(
			ScriptEngine,
			"SDKGlobalVarRemove",
			"const int First = 1; const int Second = 2;");
		if (!TestRunner->TestNotNull(TEXT("SDK global-var remove test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		if (!TestRunner->TestEqual(TEXT("SDK global-var remove test should start with two globals"), static_cast<int32>(Module->GetGlobalVarCount()), 2))
		{
			return;
		}

		if (!TestRunner->TestTrue(TEXT("SDK global-var remove test should remove the first global successfully"), Module->RemoveGlobalVar(0) >= 0))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("SDK global-var remove test should leave one global after removal"), static_cast<int32>(Module->GetGlobalVarCount()), 1);
	}

	TEST_METHOD(InitializerExpression)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK global-var initializer test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		// Global initializer expression should be evaluated at module build time.
		asIScriptModule* Module = BuildNativeModule(
			ScriptEngine,
			"SDKGlobalVarInitializer",
			"const int computed = 10 * 3 + 7;");
		if (!TestRunner->TestNotNull(TEXT("SDK global-var initializer test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		const int ComputedIndex = FindGlobalVarIndexByName(Module, "computed");
		if (!TestRunner->TestTrue(TEXT("SDK global-var initializer test should find the computed global"), ComputedIndex >= 0))
		{
			return;
		}

		const int* ComputedValue = static_cast<const int*>(Module->GetAddressOfGlobalVar(ComputedIndex));
		if (!TestRunner->TestNotNull(TEXT("SDK global-var initializer test should expose the storage"), ComputedValue))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("SDK global-var initializer test should evaluate 10*3+7 = 37 at build time"), *ComputedValue, 37);
	}

	TEST_METHOD(ConstReadAccess)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK global-var const-read test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		// This fork requires all script globals to be const. C++ reads the const value.
		asIScriptModule* Module = BuildNativeModule(
			ScriptEngine,
			"SDKGlobalVarConstRead",
			R"(
const int limit = 200;

int Entry()
{
	return limit * 2;
}
)");
		if (!TestRunner->TestNotNull(TEXT("SDK global-var const-read test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		const int LimitIndex = FindGlobalVarIndexByName(Module, "limit");
		if (!TestRunner->TestTrue(TEXT("SDK global-var const-read test should find the limit global"), LimitIndex >= 0))
		{
			return;
		}

		const int* Limit = static_cast<const int*>(Module->GetAddressOfGlobalVar(LimitIndex));
		if (!TestRunner->TestNotNull(TEXT("SDK global-var const-read test should expose const storage"), Limit))
		{
			return;
		}

		if (!TestRunner->TestEqual(TEXT("SDK global-var const-read test should read limit = 200"), *Limit, 200))
		{
			return;
		}

		int32 Result = 0;
		if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module, "int Entry()", Result))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("SDK global-var const-read test should compute limit*2 = 400"), Result, 400);
	}

	TEST_METHOD(DeclarationString)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK global-var declaration-string test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		// Verify GetGlobalVarDeclaration returns a non-empty string for each global.
		asIScriptModule* Module = BuildNativeModule(
			ScriptEngine,
			"SDKGlobalVarDeclString",
			"const double pi = 3.14159; const int answer = 42;");
		if (!TestRunner->TestNotNull(TEXT("SDK global-var declaration-string test should compile the module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		const asUINT GlobalCount = Module->GetGlobalVarCount();
		if (!TestRunner->TestEqual(TEXT("SDK global-var declaration-string test should have 2 globals"), static_cast<int32>(GlobalCount), 2))
		{
			return;
		}

		for (asUINT i = 0; i < GlobalCount; ++i)
		{
			const char* Decl = Module->GetGlobalVarDeclaration(i);
			if (!TestRunner->TestNotNull(TEXT("SDK global-var declaration-string test should return a declaration string"), Decl))
			{
				return;
			}
			const size_t Len = std::strlen(Decl);
			if (!TestRunner->TestTrue(TEXT("SDK global-var declaration-string test should return a non-empty declaration"), Len > 0))
			{
				return;
			}
		}

		TestRunner->TestTrue(TEXT("SDK global-var declaration-string test verified all declarations are non-empty"), true);
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
		if (!TestRunner->TestNotNull(TEXT("SDK stack data-limit test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKStackDataLimit", RecursiveScript);
		if (!TestRunner->TestNotNull(TEXT("SDK stack data-limit test should compile the recursive module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		asIScriptContext* Context = ScriptEngine->CreateContext();
		if (!TestRunner->TestNotNull(TEXT("SDK stack data-limit test should create a context"), Context))
		{
			return;
		}

		ScriptEngine->SetEngineProperty(asEP_INIT_STACK_SIZE, 256);
		ScriptEngine->SetEngineProperty(asEP_MAX_STACK_SIZE, 256);
		Context->Prepare(GetNativeFunctionByDecl(Module, "void recursive(int)"));
		Context->SetArgDWord(0, 100);
		const int ExecuteResult = Context->Execute();
		Context->Release();
		TestRunner->TestEqual(TEXT("SDK stack data-limit test should raise an execution exception when the data stack overflows"), ExecuteResult, static_cast<int32>(asEXECUTION_EXCEPTION));
	}

	TEST_METHOD(CallLimit)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK stack call-limit test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		asIScriptModule* Module = BuildNativeModule(ScriptEngine, "SDKStackCallLimit", RecursiveScript);
		if (!TestRunner->TestNotNull(TEXT("SDK stack call-limit test should compile the recursive module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		asIScriptContext* Context = ScriptEngine->CreateContext();
		if (!TestRunner->TestNotNull(TEXT("SDK stack call-limit test should create a context"), Context))
		{
			return;
		}

		ScriptEngine->SetEngineProperty(asEP_INIT_CALL_STACK_SIZE, 1);
		ScriptEngine->SetEngineProperty(asEP_MAX_CALL_STACK_SIZE, 1);
		ScriptEngine->SetEngineProperty(asEP_MAX_NESTED_CALLS, 1);
		Context->Prepare(GetNativeFunctionByDecl(Module, "void recursive(int)"));
		Context->SetArgDWord(0, 1000);
		const int ExecuteResult = Context->Execute();
		Context->Release();
		TestRunner->TestEqual(TEXT("SDK stack call-limit test should raise an execution exception when the call stack overflows"), ExecuteResult, static_cast<int32>(asEXECUTION_EXCEPTION));
	}

	TEST_METHOD(ExceptionLocation)
	{
		FNativeMessageCollector Messages;
		asIScriptEngine* ScriptEngine = CreateNativeEngine(&Messages);
		if (!TestRunner->TestNotNull(TEXT("SDK stack exception-location test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		ON_SCOPE_EXIT
		{
			DestroyNativeEngine(ScriptEngine);
		};

		ScriptEngine->SetEngineProperty(asEP_MAX_STACK_SIZE, 256);
		asIScriptModule* Module = BuildNativeModule(
			ScriptEngine,
			"SDKStackExceptionLocation",
			RecursiveScript);
		if (!TestRunner->TestNotNull(TEXT("SDK stack exception-location test should compile the overflow module"), Module))
		{
			TestRunner->AddInfo(CollectMessages(Messages));
			return;
		}

		asIScriptContext* Context = ScriptEngine->CreateContext();
		if (!TestRunner->TestNotNull(TEXT("SDK stack exception-location test should create a context"), Context))
		{
			return;
		}

		const int PrepareResult = Context->Prepare(GetNativeFunctionByDecl(Module, "void recursive(int)"));
		if (!TestRunner->TestEqual(TEXT("SDK stack exception-location test should prepare the entry point"), PrepareResult, static_cast<int32>(asSUCCESS)))
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

		if (!TestRunner->TestEqual(TEXT("SDK stack exception-location test should raise an execution exception"), ExecuteResult, static_cast<int32>(asEXECUTION_EXCEPTION)))
		{
			return;
		}

		if (!TestRunner->TestEqual(TEXT("SDK stack exception-location test should surface the stack overflow reason"), ExceptionString, FString(TEXT("Stack overflow"))))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("SDK stack exception-location test should report the recursive function as the overflow site"), ExceptionFunctionName, FString(TEXT("recursive")));
	}
};

#endif
