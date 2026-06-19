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
//   Two legacy automation tests merged into one TEST_CLASS.
//   Both sections use custom compile/execute patterns with FString return values
//   via ExecuteStringGlobalFunction helper. The original structure is largely preserved
//   since these tests do not follow the simple "int Entry()" pattern.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS


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
	bool ExecuteStringGlobalFunction(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		asIScriptModule& Module,
		const TCHAR* FunctionDecl,
		TValue& OutValue)
	{
		FAngelscriptTestExecutor Executor(Test, Engine, Module, FunctionDecl);
		return Executor.ExecuteAndExtractStruct(OutValue);
	}

	asIScriptModule* GetCompiledModule(FAutomationTestBase& Test, FAngelscriptEngine& Engine)
	{
		TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModule(ScriptFunctionLibraryModuleName.ToString());
		FNoDiscardAsserter Assert(Test);
		if (!Assert.IsTrue(
				ModuleDesc.IsValid(),
				TEXT("Script function library global-init test should keep the module registered after compile")))
		{
			return nullptr;
		}

		asIScriptModule* Module = ModuleDesc.IsValid() ? ModuleDesc->ScriptModule : nullptr;
		if (!Assert.IsNotNull(
				Module,
				TEXT("Script function library global-init test should expose the backing asIScriptModule")))
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

		if (!this->Assert.AreEqual(
			1,
			InitialVersion,
			TEXT("Should load initial module before hot reload")))
		{
			return;
		}

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

		if (!this->Assert.AreEqual(
			2,
			ReloadedVersion,
			TEXT("Should expose the reloaded module version")))
		{
			return;
		}

		FString PlainNameCapture;
		FString PlainNamespaceCapture;
		FString PlainModuleCapture;
		FString ScopedNameCapture;
		FString ScopedNamespaceCapture;
		FString ScopedModuleCapture;
		FString OutsideInitName;
		FString OutsideInitNamespace;
		FString OutsideInitModule;

		ExecuteStringGlobalFunction(*TestRunner, Engine, *Module, TEXT("FString GetPlainNameCapture()"), PlainNameCapture);
		ExecuteStringGlobalFunction(*TestRunner, Engine, *Module, TEXT("FString GetPlainNamespaceCapture()"), PlainNamespaceCapture);
		ExecuteStringGlobalFunction(*TestRunner, Engine, *Module, TEXT("FString GetPlainModuleCapture()"), PlainModuleCapture);
		ExecuteStringGlobalFunction(*TestRunner, Engine, *Module, TEXT("FString GetScopedNameCapture()"), ScopedNameCapture);
		ExecuteStringGlobalFunction(*TestRunner, Engine, *Module, TEXT("FString GetScopedNamespaceCapture()"), ScopedNamespaceCapture);
		ExecuteStringGlobalFunction(*TestRunner, Engine, *Module, TEXT("FString GetScopedModuleCapture()"), ScopedModuleCapture);
		ExecuteStringGlobalFunction(*TestRunner, Engine, *Module, TEXT("FString GetOutsideInitName()"), OutsideInitName);
		ExecuteStringGlobalFunction(*TestRunner, Engine, *Module, TEXT("FString GetOutsideInitNamespace()"), OutsideInitNamespace);
		ExecuteStringGlobalFunction(*TestRunner, Engine, *Module, TEXT("FString GetOutsideInitModule()"), OutsideInitModule);

		const FString ExpectedHotReloadPrefix = ScriptFunctionLibraryModuleName.ToString() + HotReloadMarker;

		ASSERT_THAT(AreEqual(FString(TEXT("PlainNameCapture")), PlainNameCapture, TEXT("Plain global init should report variable name")));
		ASSERT_THAT(AreEqual(FString(TEXT("")), PlainNamespaceCapture, TEXT("Plain global init should report empty namespace")));
		ASSERT_THAT(AreEqual(FString(TEXT("ScopedNameCapture")), ScopedNameCapture, TEXT("Namespaced global init should report variable name")));
		ASSERT_THAT(AreEqual(ScriptFunctionLibraryNamespace, ScopedNamespaceCapture, TEXT("Namespaced global init should report its namespace")));
		ASSERT_THAT(AreEqual(PlainModuleCapture, ScopedModuleCapture, TEXT("Plain and namespaced module captures should agree")));
		ASSERT_THAT(IsTrue(PlainModuleCapture.StartsWith(ExpectedHotReloadPrefix), TEXT("Module capture should preserve hot-reload suffix")));
		ASSERT_THAT(IsTrue(PlainModuleCapture.Contains(TEXT("_HotReload_42_NEW_")), TEXT("Module capture should keep explicit user suffix")));
		ASSERT_THAT(AreEqual(FString(TEXT("")), OutsideInitName, TEXT("Outside init global-name helper should be empty")));
		ASSERT_THAT(AreEqual(FString(TEXT("")), OutsideInitNamespace, TEXT("Outside init namespace helper should be empty")));
		ASSERT_THAT(AreEqual(FString(TEXT("")), OutsideInitModule, TEXT("Outside init module helper should be empty")));
	}

	// ====================================================================
	// Section: GlobalInitContext
	// ====================================================================

	TEST_METHOD(GlobalInitContext)
	{
		using namespace AngelscriptTest_Bindings_AngelscriptScriptFunctionLibraryTests_Private;
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptModule* Module = BuildModule(
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

		ExecuteStringGlobalFunction(*TestRunner, Engine, *Module, TEXT("FString GetPlainNameCapture()"), PlainNameCapture);
		ExecuteStringGlobalFunction(*TestRunner, Engine, *Module, TEXT("FString GetPlainNamespaceCapture()"), PlainNamespaceCapture);
		ExecuteStringGlobalFunction(*TestRunner, Engine, *Module, TEXT("FString GetPlainModuleCapture()"), PlainModuleCapture);
		ExecuteStringGlobalFunction(*TestRunner, Engine, *Module, TEXT("FString GetScopedNameCapture()"), ScopedNameCapture);
		ExecuteStringGlobalFunction(*TestRunner, Engine, *Module, TEXT("FString GetScopedNamespaceCapture()"), ScopedNamespaceCapture);
		ExecuteStringGlobalFunction(*TestRunner, Engine, *Module, TEXT("FString GetScopedModuleCapture()"), ScopedModuleCapture);
		ExecuteStringGlobalFunction(*TestRunner, Engine, *Module, TEXT("FString GetOutsideInitName()"), OutsideInitName);
		ExecuteStringGlobalFunction(*TestRunner, Engine, *Module, TEXT("FString GetOutsideInitNamespace()"), OutsideInitNamespace);
		ExecuteStringGlobalFunction(*TestRunner, Engine, *Module, TEXT("FString GetOutsideInitModule()"), OutsideInitModule);

		ASSERT_THAT(AreEqual(FString(TEXT("PlainNameCapture")), PlainNameCapture, TEXT("Plain global init should report variable name")));
		ASSERT_THAT(AreEqual(FString(TEXT("")), PlainNamespaceCapture, TEXT("Plain global init should report empty namespace")));
		ASSERT_THAT(AreEqual(DirectScriptFunctionLibraryModuleName, PlainModuleCapture, TEXT("Plain global init should report direct module name")));
		ASSERT_THAT(AreEqual(FString(TEXT("ScopedNameCapture")), ScopedNameCapture, TEXT("Namespaced global init should report variable name")));
		ASSERT_THAT(AreEqual(ScriptFunctionLibraryNamespace, ScopedNamespaceCapture, TEXT("Namespaced global init should report its namespace")));
		ASSERT_THAT(AreEqual(DirectScriptFunctionLibraryModuleName, ScopedModuleCapture, TEXT("Namespaced global init should report direct module name")));
		ASSERT_THAT(AreEqual(PlainModuleCapture, ScopedModuleCapture, TEXT("Plain and namespaced should agree on direct module name")));
		ASSERT_THAT(AreEqual(FString(TEXT("")), OutsideInitName, TEXT("Outside init global-name helper should be empty")));
		ASSERT_THAT(AreEqual(FString(TEXT("")), OutsideInitNamespace, TEXT("Outside init namespace helper should be empty")));
		ASSERT_THAT(AreEqual(FString(TEXT("")), OutsideInitModule, TEXT("Outside init module helper should be empty")));
	}
};

#endif
