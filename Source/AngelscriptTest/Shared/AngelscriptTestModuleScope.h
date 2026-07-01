#pragma once

#include "CoreMinimal.h"
#include "AngelscriptEngine.h"
#include "Misc/AutomationTest.h"

#include "AngelscriptTestUtilities.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptengine.h"
#include "source/as_module.h"
#include "EndAngelscriptHeaders.h"

/**
 * FCoverageModuleScope — RAII wrapper around `BuildModule`
 * that automatically discards the named module when the scope ends.
 *
 * Why this exists:
 *  - Legacy bindings tests scattered `Engine.DiscardModule(TEXT("ASXxx"))`
 *    across `ON_SCOPE_EXIT` blocks, often with the module name duplicated
 *    near a build call. This scope keeps the build/discard name in exactly
 *    one place.
 *  - Bindings sections should be drop-in: declare scope, get the module,
 *    use it, return. Cleanup happens automatically.
 *
 * This type deliberately does not know about Coverage profiles. CQTest already
 * owns the test identity (Automation path + TEST_METHOD), and module lifetime
 * only needs an explicit AS module name plus source.
 */

#if WITH_ANGELSCRIPT_UNITTESTS

/**
 * Build an AS module under the given name and keep it alive for the
 * lifetime of the scope. On destruction the module is discarded from the
 * engine (idempotent — safe even if `BuildModule` itself failed).
 *
 * Typical usage in a Section function:
 *
 *   FCoverageModuleScope ModuleScope(Test, Engine, TEXT("ASOptional_Basics"), TEXT(R"(
 *       int EchoEmpty()         { TOptional<int> O; return O.IsSet() ? 1 : 0; }
 *       int EchoEmptyFallback() { TOptional<int> O; return O.Get(7); }
 *   )"));
 *   if (!ModuleScope.IsValid()) { return false; }
 *   asIScriptModule& Module = ModuleScope.GetModule();
 *   ExpectGlobalInt(Test, Engine, Module,
 *       TEXT("int EchoEmpty()"), TEXT("Empty Optional should not be set"), 0);
 */
struct FScopedAngelscriptModule
{
	FScopedAngelscriptModule(
		FAutomationTestBase& InTest,
		FAngelscriptEngine& InEngine,
		const TCHAR* InModuleName,
		const FString& Source)
		: Engine(InEngine)
		, ModuleName(InModuleName)
	{
		const FString ModuleNameAnsi = ModuleName;
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleNameAnsi);
		Module = BuildModule(InTest, InEngine, ModuleNameUtf8.Get(), Source);
	}

	~FScopedAngelscriptModule()
	{
		// BuildModule registers the module under `ModuleName` (the AS
		// preprocessor uses the requested filename stem as the module
		// name). Discarding by that same name is idempotent: even if
		// compile failed and Module is null, DiscardModule on a missing
		// name is a no-op in the engine.
		Engine.DiscardModule(*ModuleName);
	}

	FScopedAngelscriptModule(const FScopedAngelscriptModule&) = delete;
	FScopedAngelscriptModule& operator=(const FScopedAngelscriptModule&) = delete;

	bool IsValid() const { return Module != nullptr; }

	/** Resolved AS module. Caller must check IsValid() first. */
	asIScriptModule& GetModule() const
	{
		check(Module != nullptr);
		return *Module;
	}

	/** Composed module name (also queryable for AddExpectedError registrations). */
	const FString& GetModuleName() const { return ModuleName; }

private:
	FAngelscriptEngine& Engine;
	FString ModuleName;
	asIScriptModule* Module = nullptr;
};


#endif // WITH_ANGELSCRIPT_UNITTESTS
