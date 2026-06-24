#include "CQTest.h"
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


TEST_CLASS_WITH_FLAGS(
	FAngelscriptHandleTests,
	"Angelscript.TestModule.Functional.Handles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(Basic)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		const FString ScriptFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NegativeCompileIsolation"), TEXT("ASHandleBasic.as"));
		FAngelscriptEngineScope Scope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& Module : ActiveModules)
			{
				Engine.DiscardModule(*Module->ModuleName);
			}
		};

		ECompileResult CompileResult = ECompileResult::FullyHandled;
		UE_SET_LOG_VERBOSITY(Angelscript, Fatal);
		ON_SCOPE_EXIT { UE_SET_LOG_VERBOSITY(Angelscript, Log); };
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::SoftReloadOnly,
			TEXT("ASHandleBasic"),
			ScriptFilename,
			TEXT("class HandleBasicObject { int Value; } int Test() { HandleBasicObject@ First = HandleBasicObject(); First.Value = 10; HandleBasicObject@ Second = First; return First.Value + Second.Value; }"),
			CompileResult);

		ASSERT_THAT(IsFalse(bCompiled, TEXT("Handles.Basic should remain unsupported because script-class handle declarations are not available on this branch")));
		ASSERT_THAT(AreEqual(ECompileResult::Error, CompileResult, TEXT("Handles.Basic should surface a compile error")));
	}

	TEST_METHOD(Implicit)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const bool bCompiled = CompileModuleFromMemory(
			&Engine,
			TEXT("ASHandleImplicit"),
			TEXT("ASHandleImplicit.as"),
			TEXT("class HandleImplicitObject { int Value; } void SetValue(HandleImplicitObject ObjectRef) { ObjectRef.Value = 42; } int Test() { HandleImplicitObject ValueHolder; SetValue(ValueHolder); return ValueHolder.Value; }"));
		ASSERT_THAT(IsTrue(bCompiled, TEXT("Handles.Implicit should compile through the shared non-preprocessor path")));

		TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModuleByModuleName(TEXT("ASHandleImplicit"));
		asIScriptModule* Module = ModuleDesc.IsValid() ? ModuleDesc->ScriptModule : nullptr;
		ASSERT_THAT(IsNotNull(Module, TEXT("Handles.Implicit should expose the compiled module")));

		asIScriptFunction* Function = GetFunctionByDecl(*TestRunner, *Module, TEXT("int Test()"));
		if (Function == nullptr)
		{
			return;
		}

		// This branch still faults when a script-class parameter is passed by value,
		// so keep the unsupported runtime path explicit instead of implying success.
		ASSERT_THAT(IsTrue(ExecuteIntFunctionExpectingScriptException(
			*TestRunner,
			Engine,
			*Function,
			TEXT("Handles.Implicit"),
			TEXT("Null pointer access"),
			TEXT("void SetValue(HandleImplicitObject)"),
			TEXT("int Test()"))));
	}

	TEST_METHOD(Auto)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		const FString ScriptFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NegativeCompileIsolation"), TEXT("ASHandleAuto.as"));
		FAngelscriptEngineScope Scope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& Module : ActiveModules)
			{
				Engine.DiscardModule(*Module->ModuleName);
			}
		};

		ECompileResult CompileResult = ECompileResult::FullyHandled;
		UE_SET_LOG_VERBOSITY(Angelscript, Fatal);
		ON_SCOPE_EXIT { UE_SET_LOG_VERBOSITY(Angelscript, Log); };
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::SoftReloadOnly,
			TEXT("ASHandleAuto"),
			ScriptFilename,
			TEXT("class HandleAutoObject { int Value; } HandleAutoObject@ Create() { HandleAutoObject Instance; Instance.Value = 42; return Instance; } int Test() { HandleAutoObject@ Created = Create(); return Created.Value; }"),
			CompileResult);

		ASSERT_THAT(IsFalse(bCompiled, TEXT("Handles.Auto should remain unsupported because factory-style script-class handles are not available on this branch")));
		ASSERT_THAT(AreEqual(ECompileResult::Error, CompileResult, TEXT("Handles.Auto should surface a compile error")));
	}

	TEST_METHOD(RefArgument)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const bool bCompiled = CompileModuleFromMemory(
			&Engine,
			TEXT("ASHandleRefArgument"),
			TEXT("ASHandleRefArgument.as"),
			TEXT("void Modify(int &out Value) { Value = 42; } int Test() { int Value = 0; Modify(Value); return Value; }"));
		ASSERT_THAT(IsTrue(bCompiled, TEXT("Handles.RefArgument should compile through the shared non-preprocessor path")));

		TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModuleByModuleName(TEXT("ASHandleRefArgument"));
		asIScriptModule* Module = ModuleDesc.IsValid() ? ModuleDesc->ScriptModule : nullptr;
		ASSERT_THAT(IsNotNull(Module, TEXT("Handles.RefArgument should expose the compiled module")));

		asIScriptFunction* Function = GetFunctionByDecl(*TestRunner, *Module, TEXT("int Test()"));
		if (Function == nullptr)
		{
			return;
		}

		int32 Result = 0;
		ASSERT_THAT(IsTrue(ExecuteIntFunction(*TestRunner, Engine, *Function, Result)));
		ASSERT_THAT(AreEqual(42, Result, TEXT("Handles.RefArgument should propagate out-ref writes back to the caller")));
	}
};

TEST_CLASS_WITH_FLAGS(
	FAngelscriptHandleNativeObjectArgumentTests,
	"Angelscript.TestModule.Functional.Handles.NativeObjectArgument",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static constexpr ANSICHAR HandleNativeObjectArgumentModuleName[] = "ASHandleNativeObjectArgument";

	static bool ExecuteNativeObjectArgumentCase(
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

public:
	TEST_METHOD(NullAndNonNull)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(TEXT("ASHandleNativeObjectArgument"));
		};

		asIScriptModule* Module = BuildModule(
			*TestRunner,
			Engine,
			HandleNativeObjectArgumentModuleName,
			TEXT(R"(
int Test(UObject Value)
{
	return Value != nullptr ? 1 : 0;
}
)"));
		ASSERT_THAT(IsNotNull(Module, TEXT("Handles.NativeObjectArgument.NullAndNonNull should compile the test module")));

		asIScriptFunction* Function = GetFunctionByDecl(*TestRunner, *Module, TEXT("int Test(UObject)"));
		if (Function == nullptr)
		{
			return;
		}

		asIScriptContext* Context = Engine.CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("Handles.NativeObjectArgument.NullAndNonNull should create a context")));

		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		UObject* Instance = NewObject<UAngelscriptNativeScriptTestObject>(GetTransientPackage());
		ASSERT_THAT(IsNotNull(Instance, TEXT("Handles.NativeObjectArgument.NullAndNonNull should create a native UObject instance")));

		ASSERT_THAT(IsTrue(ExecuteNativeObjectArgumentCase(
			*TestRunner,
			*Context,
			*Function,
			Instance,
			TEXT("Handles.NativeObjectArgument.NullAndNonNull(non-null)"),
			1)));

		ASSERT_THAT(IsTrue(ExecuteNativeObjectArgumentCase(
			*TestRunner,
			*Context,
			*Function,
			nullptr,
			TEXT("Handles.NativeObjectArgument.NullAndNonNull(null)"),
			0)));
	}
};

#endif
