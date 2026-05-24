#include "AngelscriptTestUtilities.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "UObject/UObjectGlobals.h"
#include "AngelscriptNativeScriptTestObject.h"
#include "AngelscriptTestMacros.h"
// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS


bool CompileModuleFromMemory(FAngelscriptEngine* Engine, FName ModuleName, FString Filename, FString Script);
bool CompileModuleWithResult(FAngelscriptEngine* Engine, ECompileType CompileType, FName ModuleName, FString Filename, FString Script, ECompileResult& OutCompileResult);
void ResetSharedCloneEngine(FAngelscriptEngine& Engine);


namespace AngelscriptTest_Angelscript_AngelscriptHandleTests_Private
{
	static constexpr ANSICHAR HandleNativeObjectArgumentModuleName[] = "ASHandleNativeObjectArgument";

	bool ExecuteNativeObjectArgumentCase(
		FAutomationTestBase& Test,
		asIScriptContext& Context,
		asIScriptFunction& Function,
		UObject* Value,
		const TCHAR* ContextLabel,
		int32 ExpectedReturnValue)
	{
		const int PrepareResult = Context.Prepare(&Function);
		if (!Test.TestEqual(
			*FString::Printf(TEXT("%s should prepare successfully"), ContextLabel),
			PrepareResult,
			static_cast<int32>(asSUCCESS)))
		{
			return false;
		}

		const int SetArgResult = Context.SetArgObject(0, Value);
		if (!Test.TestEqual(
			*FString::Printf(TEXT("%s should bind the UObject argument"), ContextLabel),
			SetArgResult,
			static_cast<int32>(asSUCCESS)))
		{
			return false;
		}

		const int ExecuteResult = Context.Execute();
		if (!Test.TestEqual(
			*FString::Printf(TEXT("%s should execute successfully"), ContextLabel),
			ExecuteResult,
			static_cast<int32>(asEXECUTION_FINISHED)))
		{
			return false;
		}

		return Test.TestEqual(
			*FString::Printf(TEXT("%s should return the expected nullability marker"), ContextLabel),
			static_cast<int32>(Context.GetReturnDWord()),
			ExpectedReturnValue);
	}
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptHandleBasicTest,
	"Angelscript.TestModule.Functional.Handles.Basic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptHandleBasicTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptHandleTests_Private;
	bool bPassed = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	const FString ScriptFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NegativeCompileIsolation"), TEXT("ASHandleBasic.as"));
	{
		FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};
	ECompileResult CompileResult = ECompileResult::FullyHandled;
	UE_SET_LOG_VERBOSITY(Angelscript, Fatal);
	const bool bCompiled = CompileModuleWithResult(
		&Engine,
		ECompileType::SoftReloadOnly,
		TEXT("ASHandleBasic"),
		ScriptFilename,
		TEXT("class HandleBasicObject { int Value; } int Test() { HandleBasicObject@ First = HandleBasicObject(); First.Value = 10; HandleBasicObject@ Second = First; return First.Value + Second.Value; }"),
		CompileResult);
	UE_SET_LOG_VERBOSITY(Angelscript, Log);
	if (!TestFalse(TEXT("Handles.Basic should remain unsupported because script-class handle declarations are not available on this branch"), bCompiled))
	{
		return false;
	}
	bPassed = CompileResult == ECompileResult::Error;
	}

	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptHandleImplicitTest,
	"Angelscript.TestModule.Functional.Handles.Implicit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptHandleImplicitTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptHandleTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	const bool bCompiled = CompileModuleFromMemory(
		&Engine,
		TEXT("ASHandleImplicit"),
		TEXT("ASHandleImplicit.as"),
		TEXT("class HandleImplicitObject { int Value; } void SetValue(HandleImplicitObject ObjectRef) { ObjectRef.Value = 42; } int Test() { HandleImplicitObject ValueHolder; SetValue(ValueHolder); return ValueHolder.Value; }"));
	if (!TestTrue(TEXT("Handles.Implicit should compile through the shared non-preprocessor path"), bCompiled))
	{
		return false;
	}

	TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModuleByModuleName(TEXT("ASHandleImplicit"));
	asIScriptModule* Module = ModuleDesc.IsValid() ? ModuleDesc->ScriptModule : nullptr;
	if (!TestNotNull(TEXT("Handles.Implicit should expose the compiled module"), Module))
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("int Test()"));
	if (Function == nullptr)
	{
		return false;
	}

	// This branch still faults when a script-class parameter is passed by value,
	// so keep the unsupported runtime path explicit instead of implying success.
	if (!ExecuteIntFunctionExpectingScriptException(
		*this,
		Engine,
		*Function,
		TEXT("Handles.Implicit"),
		TEXT("Null pointer access"),
		TEXT("void SetValue(HandleImplicitObject)"),
		TEXT("int Test()")))
	{
		return false;
	}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptHandleAutoTest,
	"Angelscript.TestModule.Functional.Handles.Auto",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptHandleAutoTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptHandleTests_Private;
	bool bPassed = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	const FString ScriptFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NegativeCompileIsolation"), TEXT("ASHandleAuto.as"));
	{
		FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};
	ECompileResult CompileResult = ECompileResult::FullyHandled;
	UE_SET_LOG_VERBOSITY(Angelscript, Fatal);
	const bool bCompiled = CompileModuleWithResult(
		&Engine,
		ECompileType::SoftReloadOnly,
		TEXT("ASHandleAuto"),
		ScriptFilename,
		TEXT("class HandleAutoObject { int Value; } HandleAutoObject@ Create() { HandleAutoObject Instance; Instance.Value = 42; return Instance; } int Test() { HandleAutoObject@ Created = Create(); return Created.Value; }"),
		CompileResult);
	UE_SET_LOG_VERBOSITY(Angelscript, Log);
	if (!TestFalse(TEXT("Handles.Auto should remain unsupported because factory-style script-class handles are not available on this branch"), bCompiled))
	{
		return false;
	}
	bPassed = CompileResult == ECompileResult::Error;
	}

	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptHandleRefArgumentTest,
	"Angelscript.TestModule.Functional.Handles.RefArgument",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptHandleNativeObjectArgumentNullAndNonNullTest,
	"Angelscript.TestModule.Functional.Handles.NativeObjectArgument.NullAndNonNull",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptHandleRefArgumentTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptHandleTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	const bool bCompiled = CompileModuleFromMemory(
		&Engine,
		TEXT("ASHandleRefArgument"),
		TEXT("ASHandleRefArgument.as"),
		TEXT("void Modify(int &out Value) { Value = 42; } int Test() { int Value = 0; Modify(Value); return Value; }"));
	if (!TestTrue(TEXT("Handles.RefArgument should compile through the shared non-preprocessor path"), bCompiled))
	{
		return false;
	}

	TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModuleByModuleName(TEXT("ASHandleRefArgument"));
	asIScriptModule* Module = ModuleDesc.IsValid() ? ModuleDesc->ScriptModule : nullptr;
	if (!TestNotNull(TEXT("Handles.RefArgument should expose the compiled module"), Module))
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("int Test()"));
	if (Function == nullptr)
	{
		return false;
	}

	int32 Result = 0;
	if (!ExecuteIntFunction(*this, Engine, *Function, Result))
	{
		return false;
	}

	TestEqual(TEXT("Handles.RefArgument should propagate out-ref writes back to the caller"), Result, 42);
	}
	return true;
}

bool FAngelscriptHandleNativeObjectArgumentNullAndNonNullTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptHandleTests_Private;
	bool bPassed = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	ON_SCOPE_EXIT
	{
		Engine.DiscardModule(TEXT("ASHandleNativeObjectArgument"));
	};

	do
	{
		asIScriptModule* Module = BuildModule(
			*this,
			Engine,
			HandleNativeObjectArgumentModuleName,
			TEXT(R"(
int Test(UObject Value)
{
	return Value != nullptr ? 1 : 0;
}
)"));
		if (!TestNotNull(TEXT("Handles.NativeObjectArgument.NullAndNonNull should compile the test module"), Module))
		{
			break;
		}

		asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("int Test(UObject)"));
		if (Function == nullptr)
		{
			break;
		}

		asIScriptContext* Context = Engine.CreateContext();
		if (!TestNotNull(TEXT("Handles.NativeObjectArgument.NullAndNonNull should create a context"), Context))
		{
			break;
		}

		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		UObject* Instance = NewObject<UAngelscriptNativeScriptTestObject>(GetTransientPackage());
		if (!TestNotNull(TEXT("Handles.NativeObjectArgument.NullAndNonNull should create a native UObject instance"), Instance))
		{
			break;
		}

		if (!ExecuteNativeObjectArgumentCase(
				*this,
				*Context,
				*Function,
				Instance,
				TEXT("Handles.NativeObjectArgument.NullAndNonNull(non-null)"),
				1))
		{
			break;
		}

		if (!ExecuteNativeObjectArgumentCase(
				*this,
				*Context,
				*Function,
				nullptr,
				TEXT("Handles.NativeObjectArgument.NullAndNonNull(null)"),
				0))
		{
			break;
		}

		bPassed = true;
	}
	while (false);

	}
	return bPassed;
}

#endif
