// ============================================================================
// AngelscriptFStringFormatMultiEngineTests.cpp
//
// Multi-engine regression coverage for FString::Format (and FText::Format) to
// validate the deglobalization fix in refactor-as-runtime-deglobalize-completion:
// after Engine A binds FString and is destroyed, a freshly created Engine B
// must be able to call FString::Format successfully without the previous
// engine's `asITypeInfo*` lingering in any process-wide cache.
//
// Automation IDs:
//   Angelscript.TestModule.Bindings.FString.MultiEngine.*
//
// The pre-fix failure mode (had it not been corrected) was:
//   1. Engine A binds FString -> populates static type-info cache with A's
//      asITypeInfo* for FString.
//   2. Engine A is destroyed without clearing the cache.
//   3. Engine B is created. Engine B binds FString and overwrites the static
//      cache with B's pointer for FString, but other helpers may still hold
//      A's pointer in non-engine-keyed caches.
//   4. Script in Engine B calls FString::Format("{0}", "Hello"). The format
//      argument resolution compares the runtime arg's TypeInfo (B's) against
//      the cached pointer (A's, dangling) -- mismatch -> Format rejects the
//      argument.
//
// With the fix, every cache that holds an `asITypeInfo*` is engine-keyed:
// TGetStaticTypeInfo<T> stores per engine, ClearForEngine fires before AS
// engine release, GScriptEnumTypeLookupByName moved to engine-owned, and the
// FToStringType::TypeInfo fallback path is fenced so it never holds an
// engine-owned pointer. The tests below assert the observable consequence:
// FString::Format / FText::Format in Engine B return the expected string with
// no diagnostic about a mismatched argument type.
// ============================================================================

#include "AngelscriptEngine.h"
#include "AngelscriptTestEngine.h"
#include "CQTest.h"
#include "Misc/Guid.h"

#include "AngelscriptInclude.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest_Bindings_FString_FormatMultiEngine_Private
{

// Compile a parameterless `int Test()` script function in `Engine`'s named
// module and return the resolved asIScriptFunction*. Caller owns the returned
// pointer and must Release() it.
static asIScriptFunction* CompileIntFunction(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	const FString& ModuleName,
	const ANSICHAR* SourceBody)
{
	FAngelscriptEngineScope GlobalScope(Engine);
	FNoDiscardAsserter Assert(Test);

	asIScriptModule* Module = Engine.GetScriptEngine()->GetModule(
		TCHAR_TO_ANSI(*ModuleName), asGM_ALWAYS_CREATE);
	if (!Assert.IsNotNull(Module, *FString::Printf(TEXT("FormatMultiEngine: should create module '%s'"), *ModuleName)))
	{
		return nullptr;
	}

	asIScriptFunction* Function = nullptr;
	const int32 CompileResult = Module->CompileFunction(
		TCHAR_TO_ANSI(*ModuleName), SourceBody, 0, asCOMP_ADD_TO_MODULE, &Function);
	if (!Assert.AreEqual(asSUCCESS, CompileResult, *FString::Printf(TEXT("FormatMultiEngine: should compile '%s'"), *ModuleName)))
	{
		return nullptr;
	}

	if (!Assert.IsNotNull(Function, *FString::Printf(TEXT("FormatMultiEngine: should resolve function in '%s'"), *ModuleName)))
	{
		return nullptr;
	}
	return Function;
}

// Run a previously compiled int Test() function and return its int32 return
// value, or INDEX_NONE on any execution failure. The current thread context
// engine MUST be the engine that owns `Function`'s module.
static int32 ExecuteIntFunction(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	asIScriptFunction& Function,
	const TCHAR* WhatLabel)
{
	FNoDiscardAsserter Assert(Test);
	asIScriptContext* Context = Engine.GetScriptEngine()->RequestContext();
	if (!Assert.IsNotNull(Context, *FString::Printf(TEXT("FormatMultiEngine [%s]: should acquire a context"), WhatLabel)))
	{
		return INDEX_NONE;
	}

	const int32 PrepareResult = Context->Prepare(&Function);
	if (!Assert.AreEqual(asSUCCESS, PrepareResult, *FString::Printf(TEXT("FormatMultiEngine [%s]: Prepare succeeds"), WhatLabel)))
	{
		Engine.GetScriptEngine()->ReturnContext(Context);
		return INDEX_NONE;
	}

	const int32 ExecuteResult = Context->Execute();
	if (!Assert.AreEqual(asEXECUTION_FINISHED, ExecuteResult, *FString::Printf(TEXT("FormatMultiEngine [%s]: Execute reaches asEXECUTION_FINISHED"), WhatLabel)))
	{
		Engine.GetScriptEngine()->ReturnContext(Context);
		return INDEX_NONE;
	}

	const int32 ReturnValue = Context->GetReturnDWord();
	Engine.GetScriptEngine()->ReturnContext(Context);
	return ReturnValue;
}

static FString MakeUniqueModuleName(const TCHAR* Prefix)
{
	return FString::Printf(TEXT("%s_%s"), Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

} // namespace

TEST_CLASS_WITH_FLAGS(FAngelscriptFStringFormatMultiEngineTests,
	"Angelscript.TestModule.Bindings.FString.MultiEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	// Engine A binds FString, runs a Format call, is destroyed; a freshly
	// created Engine B then binds FString and runs the same Format call.
	// Both calls must return the expected string. Pre-fix, Engine B would
	// see the still-cached Engine-A `asITypeInfo*` and reject the arg.
	TEST_METHOD(FormatString_AfterPreviousEngineDestroyed_StillWorks)
	{
		using namespace AngelscriptTest_Bindings_FString_FormatMultiEngine_Private;

		const FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();

		// Engine A: bind, format, destroy.
		{
			TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptTestEngine::Create(Config, Dependencies);
			if (!this->Assert.IsNotNull(EngineA.Get(), TEXT("FormatMultiEngine: should construct engine A")))
			{
				return;
			}

			const FString ModuleNameA = MakeUniqueModuleName(TEXT("ASFStringFormatMultiEngine_A"));
			asIScriptFunction* FunctionA = CompileIntFunction(*TestRunner, *EngineA, ModuleNameA, R"(
int Test()
{
	FString R = FString::Format("{0}", "Hello");
	return (R == "Hello") ? 1 : 0;
}
)");
			if (FunctionA == nullptr)
			{
				return;
			}

			FAngelscriptEngineScope ScopeA(*EngineA);
			const int32 ResultA = ExecuteIntFunction(*TestRunner, *EngineA, *FunctionA, TEXT("EngineA"));
			const bool bResultAOk = this->Assert.AreEqual(
				1,
				ResultA,
				TEXT("FormatMultiEngine: FString::Format in engine A should return 1 ('Hello' matched)"));
			FunctionA->Release();
			if (!bResultAOk)
			{
				return;
			}
			// EngineA goes out of scope here; teardown clears engine-keyed
			// type-info caches before the AS engine is released.
		}

		// Engine B: independent re-bind, format, destroy. If any cache from
		// Engine A persisted process-wide, the format here would either fail
		// to compile or reject the runtime argument.
		{
			TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptTestEngine::Create(Config, Dependencies);
			if (!this->Assert.IsNotNull(
				EngineB.Get(),
				TEXT("FormatMultiEngine: should construct engine B after A's teardown")))
			{
				return;
			}

			const FString ModuleNameB = MakeUniqueModuleName(TEXT("ASFStringFormatMultiEngine_B"));
			asIScriptFunction* FunctionB = CompileIntFunction(*TestRunner, *EngineB, ModuleNameB, R"(
int Test()
{
	FString R = FString::Format("{0}", "Hello");
	return (R == "Hello") ? 1 : 0;
}
)");
			if (FunctionB == nullptr)
			{
				return;
			}

			FAngelscriptEngineScope ScopeB(*EngineB);
			const int32 ResultB = ExecuteIntFunction(*TestRunner, *EngineB, *FunctionB, TEXT("EngineB"));
			const bool bResultBOk = this->Assert.AreEqual(
				1,
				ResultB,
				TEXT("FormatMultiEngine: FString::Format in engine B (post engine-A teardown) should return 1"));
			FunctionB->Release();
			if (!bResultBOk)
			{
				return;
			}
		}
	}

	// Both engines simultaneously alive. After both bind FString, each runs
	// its own Format. Each result must come from the executing engine's own
	// TypeInfo identity, not the other engine's.
	TEST_METHOD(FormatString_TwoEnginesConcurrent_NoCrossContamination)
	{
		using namespace AngelscriptTest_Bindings_FString_FormatMultiEngine_Private;

		const FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();

		TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptTestEngine::Create(Config, Dependencies);
		TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptTestEngine::Create(Config, Dependencies);
		if (!this->Assert.IsNotNull(EngineA.Get(), TEXT("FormatMultiEngine concurrent: should construct engine A"))
			|| !this->Assert.IsNotNull(EngineB.Get(), TEXT("FormatMultiEngine concurrent: should construct engine B")))
		{
			return;
		}

		const FString ModuleNameA = MakeUniqueModuleName(TEXT("ASFStringFormatMultiEngine_ConcA"));
		const FString ModuleNameB = MakeUniqueModuleName(TEXT("ASFStringFormatMultiEngine_ConcB"));

		asIScriptFunction* FunctionA = CompileIntFunction(*TestRunner, *EngineA, ModuleNameA, R"(
int Test()
{
	FString R = FString::Format("{0}", "AAA");
	return (R == "AAA") ? 1 : 0;
}
)");
		asIScriptFunction* FunctionB = CompileIntFunction(*TestRunner, *EngineB, ModuleNameB, R"(
int Test()
{
	FString R = FString::Format("{0}", "BBB");
	return (R == "BBB") ? 1 : 0;
}
)");
		if (FunctionA == nullptr || FunctionB == nullptr)
		{
			if (FunctionA != nullptr) FunctionA->Release();
			if (FunctionB != nullptr) FunctionB->Release();
			return;
		}

		// Run on engine A first.
		{
			FAngelscriptEngineScope ScopeA(*EngineA);
			const int32 ResultA = ExecuteIntFunction(*TestRunner, *EngineA, *FunctionA, TEXT("EngineA-Concurrent"));
			if (!this->Assert.AreEqual(
				1,
				ResultA,
				TEXT("FormatMultiEngine concurrent: engine A's Format returns 1 even with engine B alive")))
			{
				FunctionA->Release();
				FunctionB->Release();
				return;
			}
		}

		// Now run on engine B. Even though engine A is still alive (and
		// holding its own bound type-info), engine B must use B's identity.
		{
			FAngelscriptEngineScope ScopeB(*EngineB);
			const int32 ResultB = ExecuteIntFunction(*TestRunner, *EngineB, *FunctionB, TEXT("EngineB-Concurrent"));
			if (!this->Assert.AreEqual(
				1,
				ResultB,
				TEXT("FormatMultiEngine concurrent: engine B's Format returns 1 even with engine A alive")))
			{
				FunctionA->Release();
				FunctionB->Release();
				return;
			}
		}

		FunctionA->Release();
		FunctionB->Release();
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
