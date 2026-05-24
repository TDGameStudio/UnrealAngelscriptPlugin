// ============================================================================
// AngelscriptScriptFunctionLibraryTests.cpp
//
// Script function library binding coverage — CQTest refactor. Automation IDs:
//   Angelscript.TestModule.FunctionLibraries.Script.FAngelscriptScriptFunctionLibraryTest.*
//
// Sections:
//   GlobalInitContextHotReloadName — hot-reload module name propagation
//   GlobalInitContext              — direct module name propagation
//
// CQTest adaptation notes:
//   Two IMPLEMENT_SIMPLE_AUTOMATION_TEST merged into one TEST_CLASS.
//   Both sections use custom compile/execute patterns with FString return values
//   via ExecuteValueFunction helper. The original structure is largely preserved
//   since these tests do not follow the simple "int Entry()" pattern.
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptTestEngineHelper.h"
#include "Shared/AngelscriptBindingsModuleBuilder.h"
#include "Shared/AngelscriptBindingsAssertions.h"

#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;
using namespace AngelscriptTestBindings;
using namespace AngelscriptReflectiveAccess;

// ----------------------------------------------------------------------------
// Helper utilities (retained from original)
// ----------------------------------------------------------------------------

namespace AngelscriptTest_Bindings_AngelscriptScriptFunctionLibraryTests_Private
{
	static const FName ScriptFunctionLibraryModuleName(TEXT("ASGlobalInitContext_HotReload_42"));
	static const FString ScriptFunctionLibraryFilename(TEXT("ASGlobalInitContext_HotReload_42.as"));
	static const FString DirectScriptFunctionLibraryModuleName(TEXT("ASGlobalInitContext_Stable"));
	static const FString ScriptFunctionLibraryNamespace(TEXT("ScopedContext"));
	static const FString HotReloadMarker(TEXT("_NEW_"));

	template <typename TValue>
	bool ReadReturnValue(FAutomationTestBase& Test, asIScriptContext& Context, TValue& OutValue)
	{
		void* ReturnValueAddress = Context.GetAddressOfReturnValue();
		return Test.TestNotNull(
				TEXT("Script function library global-init test should expose return value storage"),
				ReturnValueAddress)
			&& (OutValue = *static_cast<TValue*>(ReturnValueAddress), true);
	}

	// TODO(refactor-as-test-shared-layout-and-naming): migrate ExecuteValueFunction<T> to Shared/AngelscriptTestExecute.h once the Execute* naming family lands in Phase 3.
	template <typename TValue>
	bool ExecuteValueFunction(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		asIScriptModule& Module,
		const FString& FunctionDecl,
		TValue& OutValue)
	{
		asIScriptFunction* Function = GetFunctionByDecl(Test, Module, FunctionDecl);
		if (Function == nullptr)
		{
			return false;
		}

		asIScriptContext* Context = Engine.CreateContext();
		if (!Test.TestNotNull(
				TEXT("Script function library global-init test should create an execution context"),
				Context))
		{
			return false;
		}

		const int PrepareResult = Context->Prepare(Function);
		if (!Test.TestEqual(
				TEXT("Script function library global-init test should prepare the target function"),
				PrepareResult,
				asSUCCESS))
		{
			Context->Release();
			return false;
		}

		const int ExecuteResult = Context->Execute();
		if (!Test.TestEqual(
				TEXT("Script function library global-init test should execute the target function"),
				ExecuteResult,
				asEXECUTION_FINISHED))
		{
			if (ExecuteResult == asEXECUTION_EXCEPTION && Context->GetExceptionString() != nullptr)
			{
				Test.AddError(FString::Printf(
					TEXT("Script function library global-init test saw a script exception: %s"),
					UTF8_TO_TCHAR(Context->GetExceptionString())));
			}

			Context->Release();
			return false;
		}

		const bool bReadReturnValue = ReadReturnValue(Test, *Context, OutValue);
		Context->Release();
		return bReadReturnValue;
	}

	asIScriptModule* GetCompiledModule(FAutomationTestBase& Test, FAngelscriptEngine& Engine)
	{
		TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModule(ScriptFunctionLibraryModuleName.ToString());
		if (!Test.TestTrue(
				TEXT("Script function library global-init test should keep the module registered after compile"),
				ModuleDesc.IsValid()))
		{
			return nullptr;
		}

		asIScriptModule* Module = ModuleDesc.IsValid() ? ModuleDesc->ScriptModule : nullptr;
		if (!Test.TestNotNull(
				TEXT("Script function library global-init test should expose the backing asIScriptModule"),
				Module))
		{
			return nullptr;
		}

		return Module;
	}

	FString BuildScriptSource(const int32 Version)
	{
		return FString::Printf(TEXT(R"(
const FString PlainNameCapture = Script::GetNameOfGlobalVariableBeingInitialized();
const FString PlainNamespaceCapture = Script::GetNamespaceOfGlobalVariableBeingInitialized();
const FString PlainModuleCapture = Script::GetModuleNameOfGlobalVariableBeingInitialized();

namespace %s
{
	const FString ScopedNameCapture = Script::GetNameOfGlobalVariableBeingInitialized();
	const FString ScopedNamespaceCapture = Script::GetNamespaceOfGlobalVariableBeingInitialized();
	const FString ScopedModuleCapture = Script::GetModuleNameOfGlobalVariableBeingInitialized();
}

FString GetPlainNameCapture() { return PlainNameCapture; }
FString GetPlainNamespaceCapture() { return PlainNamespaceCapture; }
FString GetPlainModuleCapture() { return PlainModuleCapture; }

FString GetScopedNameCapture() { return %s::ScopedNameCapture; }
FString GetScopedNamespaceCapture() { return %s::ScopedNamespaceCapture; }
FString GetScopedModuleCapture() { return %s::ScopedModuleCapture; }

FString GetOutsideInitName() { return Script::GetNameOfGlobalVariableBeingInitialized(); }
FString GetOutsideInitNamespace() { return Script::GetNamespaceOfGlobalVariableBeingInitialized(); }
FString GetOutsideInitModule() { return Script::GetModuleNameOfGlobalVariableBeingInitialized(); }

int GetVersion() { return %d; }
)"),
			*ScriptFunctionLibraryNamespace,
			*ScriptFunctionLibraryNamespace,
			*ScriptFunctionLibraryNamespace,
			*ScriptFunctionLibraryNamespace,
			Version);
	}

	FString BuildDirectContextScriptSource()
	{
		return FString::Printf(TEXT(R"(
const FString PlainNameCapture = Script::GetNameOfGlobalVariableBeingInitialized();
const FString PlainNamespaceCapture = Script::GetNamespaceOfGlobalVariableBeingInitialized();
const FString PlainModuleCapture = Script::GetModuleNameOfGlobalVariableBeingInitialized();

namespace %s
{
	const FString ScopedNameCapture = Script::GetNameOfGlobalVariableBeingInitialized();
	const FString ScopedNamespaceCapture = Script::GetNamespaceOfGlobalVariableBeingInitialized();
	const FString ScopedModuleCapture = Script::GetModuleNameOfGlobalVariableBeingInitialized();
}

FString GetPlainNameCapture() { return PlainNameCapture; }
FString GetPlainNamespaceCapture() { return PlainNamespaceCapture; }
FString GetPlainModuleCapture() { return PlainModuleCapture; }

FString GetScopedNameCapture() { return %s::ScopedNameCapture; }
FString GetScopedNamespaceCapture() { return %s::ScopedNamespaceCapture; }
FString GetScopedModuleCapture() { return %s::ScopedModuleCapture; }

FString GetOutsideInitName() { return Script::GetNameOfGlobalVariableBeingInitialized(); }
FString GetOutsideInitNamespace() { return Script::GetNamespaceOfGlobalVariableBeingInitialized(); }
FString GetOutsideInitModule() { return Script::GetModuleNameOfGlobalVariableBeingInitialized(); }
)"),
			*ScriptFunctionLibraryNamespace,
			*ScriptFunctionLibraryNamespace,
			*ScriptFunctionLibraryNamespace,
			*ScriptFunctionLibraryNamespace);
	}
}


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptScriptFunctionLibraryTest,
	"Angelscript.TestModule.FunctionLibraries.Script",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: GlobalInitContextHotReloadName
	// ====================================================================

	TEST_METHOD(GlobalInitContextHotReloadName)
	{
		using namespace AngelscriptTest_Bindings_AngelscriptScriptFunctionLibraryTests_Private;
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ScriptFunctionLibraryModuleName.ToString());
		};

		ECompileResult InitialCompileResult = ECompileResult::Error;
		if (!CompileModuleWithResult(
				&Engine,
				ECompileType::Initial,
				ScriptFunctionLibraryModuleName,
				ScriptFunctionLibraryFilename,
				BuildScriptSource(1),
				InitialCompileResult))
		{
			return;
		}

		int32 InitialVersion = 0;
		if (!ExecuteIntFunction(&Engine, ScriptFunctionLibraryModuleName, TEXT("int GetVersion()"), InitialVersion))
		{
			return;
		}

		TestRunner->TestEqual(
			TEXT("Should load initial module before hot reload"),
			InitialVersion,
			1);

		ECompileResult ReloadCompileResult = ECompileResult::Error;
		if (!CompileModuleWithResult(
				&Engine,
				ECompileType::FullReload,
				ScriptFunctionLibraryModuleName,
				ScriptFunctionLibraryFilename,
				BuildScriptSource(2),
				ReloadCompileResult))
		{
			return;
		}

		asIScriptModule* Module = GetCompiledModule(*TestRunner, Engine);
		if (Module == nullptr)
		{
			return;
		}

		int32 ReloadedVersion = 0;
		if (!ExecuteIntFunction(&Engine, ScriptFunctionLibraryModuleName, TEXT("int GetVersion()"), ReloadedVersion))
		{
			return;
		}

		TestRunner->TestEqual(
			TEXT("Should expose the reloaded module version"),
			ReloadedVersion,
			2);

		FString PlainNameCapture;
		FString PlainNamespaceCapture;
		FString PlainModuleCapture;
		FString ScopedNameCapture;
		FString ScopedNamespaceCapture;
		FString ScopedModuleCapture;
		FString OutsideInitName;
		FString OutsideInitNamespace;
		FString OutsideInitModule;

		ExecuteValueFunction(*TestRunner, Engine, *Module, TEXT("FString GetPlainNameCapture()"), PlainNameCapture);
		ExecuteValueFunction(*TestRunner, Engine, *Module, TEXT("FString GetPlainNamespaceCapture()"), PlainNamespaceCapture);
		ExecuteValueFunction(*TestRunner, Engine, *Module, TEXT("FString GetPlainModuleCapture()"), PlainModuleCapture);
		ExecuteValueFunction(*TestRunner, Engine, *Module, TEXT("FString GetScopedNameCapture()"), ScopedNameCapture);
		ExecuteValueFunction(*TestRunner, Engine, *Module, TEXT("FString GetScopedNamespaceCapture()"), ScopedNamespaceCapture);
		ExecuteValueFunction(*TestRunner, Engine, *Module, TEXT("FString GetScopedModuleCapture()"), ScopedModuleCapture);
		ExecuteValueFunction(*TestRunner, Engine, *Module, TEXT("FString GetOutsideInitName()"), OutsideInitName);
		ExecuteValueFunction(*TestRunner, Engine, *Module, TEXT("FString GetOutsideInitNamespace()"), OutsideInitNamespace);
		ExecuteValueFunction(*TestRunner, Engine, *Module, TEXT("FString GetOutsideInitModule()"), OutsideInitModule);

		const FString ExpectedHotReloadPrefix = ScriptFunctionLibraryModuleName.ToString() + HotReloadMarker;

		TestRunner->TestEqual(TEXT("Plain global init should report variable name"), PlainNameCapture, TEXT("PlainNameCapture"));
		TestRunner->TestEqual(TEXT("Plain global init should report empty namespace"), PlainNamespaceCapture, TEXT(""));
		TestRunner->TestEqual(TEXT("Namespaced global init should report variable name"), ScopedNameCapture, TEXT("ScopedNameCapture"));
		TestRunner->TestEqual(TEXT("Namespaced global init should report its namespace"), ScopedNamespaceCapture, ScriptFunctionLibraryNamespace);
		TestRunner->TestEqual(TEXT("Plain and namespaced module captures should agree"), ScopedModuleCapture, PlainModuleCapture);
		TestRunner->TestTrue(TEXT("Module capture should preserve hot-reload suffix"), PlainModuleCapture.StartsWith(ExpectedHotReloadPrefix));
		TestRunner->TestTrue(TEXT("Module capture should keep explicit user suffix"), PlainModuleCapture.Contains(TEXT("_HotReload_42_NEW_")));
		TestRunner->TestEqual(TEXT("Outside init global-name helper should be empty"), OutsideInitName, TEXT(""));
		TestRunner->TestEqual(TEXT("Outside init namespace helper should be empty"), OutsideInitNamespace, TEXT(""));
		TestRunner->TestEqual(TEXT("Outside init module helper should be empty"), OutsideInitModule, TEXT(""));
	}

	// ====================================================================
	// Section: GlobalInitContext
	// ====================================================================

	TEST_METHOD(GlobalInitContext)
	{
		using namespace AngelscriptTest_Bindings_AngelscriptScriptFunctionLibraryTests_Private;
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = AngelscriptTestSupport::BuildModule(
			*TestRunner, Engine, "ASGlobalInitContext_Stable", BuildDirectContextScriptSource());
		if (Module == nullptr) return;

		FString PlainNameCapture;
		FString PlainNamespaceCapture;
		FString PlainModuleCapture;
		FString ScopedNameCapture;
		FString ScopedNamespaceCapture;
		FString ScopedModuleCapture;
		FString OutsideInitName;
		FString OutsideInitNamespace;
		FString OutsideInitModule;

		ExecuteValueFunction(*TestRunner, Engine, *Module, TEXT("FString GetPlainNameCapture()"), PlainNameCapture);
		ExecuteValueFunction(*TestRunner, Engine, *Module, TEXT("FString GetPlainNamespaceCapture()"), PlainNamespaceCapture);
		ExecuteValueFunction(*TestRunner, Engine, *Module, TEXT("FString GetPlainModuleCapture()"), PlainModuleCapture);
		ExecuteValueFunction(*TestRunner, Engine, *Module, TEXT("FString GetScopedNameCapture()"), ScopedNameCapture);
		ExecuteValueFunction(*TestRunner, Engine, *Module, TEXT("FString GetScopedNamespaceCapture()"), ScopedNamespaceCapture);
		ExecuteValueFunction(*TestRunner, Engine, *Module, TEXT("FString GetScopedModuleCapture()"), ScopedModuleCapture);
		ExecuteValueFunction(*TestRunner, Engine, *Module, TEXT("FString GetOutsideInitName()"), OutsideInitName);
		ExecuteValueFunction(*TestRunner, Engine, *Module, TEXT("FString GetOutsideInitNamespace()"), OutsideInitNamespace);
		ExecuteValueFunction(*TestRunner, Engine, *Module, TEXT("FString GetOutsideInitModule()"), OutsideInitModule);

		TestRunner->TestEqual(TEXT("Plain global init should report variable name"), PlainNameCapture, TEXT("PlainNameCapture"));
		TestRunner->TestEqual(TEXT("Plain global init should report empty namespace"), PlainNamespaceCapture, TEXT(""));
		TestRunner->TestEqual(TEXT("Plain global init should report direct module name"), PlainModuleCapture, DirectScriptFunctionLibraryModuleName);
		TestRunner->TestEqual(TEXT("Namespaced global init should report variable name"), ScopedNameCapture, TEXT("ScopedNameCapture"));
		TestRunner->TestEqual(TEXT("Namespaced global init should report its namespace"), ScopedNamespaceCapture, ScriptFunctionLibraryNamespace);
		TestRunner->TestEqual(TEXT("Namespaced global init should report direct module name"), ScopedModuleCapture, DirectScriptFunctionLibraryModuleName);
		TestRunner->TestEqual(TEXT("Plain and namespaced should agree on direct module name"), ScopedModuleCapture, PlainModuleCapture);
		TestRunner->TestEqual(TEXT("Outside init global-name helper should be empty"), OutsideInitName, TEXT(""));
		TestRunner->TestEqual(TEXT("Outside init namespace helper should be empty"), OutsideInitNamespace, TEXT(""));
		TestRunner->TestEqual(TEXT("Outside init module helper should be empty"), OutsideInitModule, TEXT(""));
	}
};

#endif
