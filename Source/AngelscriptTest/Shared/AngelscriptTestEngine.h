#pragma once

#include "CoreMinimal.h"
#include "AngelscriptEngine.h"

/**
 * Test-module-owned helper that hosts the supported test-side engine
 * lifecycle: lazy-singleton shared engine for fast suite execution, fresh
 * isolated engine creation, and a module-level reset that preserves
 * type/bind databases between tests.
 *
 * Implemented as a static-method-only struct (no inheritance) so the
 * runtime layout of `FAngelscriptEngine` is unaffected. The previous
 * subclass-based design was reverted because no instance methods needed
 * protected access to `FAngelscriptEngine` internals; all operations go
 * through the public engine API.
 */
struct ANGELSCRIPTTEST_API FAngelscriptTestEngine
{
	/**
	 * Create a fresh, isolated, uncompiled Full test engine.
	 *
	 * Sets `Config.bSkipInitialCompile = true` on a local copy of `Config` and
	 * delegates to `FAngelscriptEngine::Create(LocalConfig, Dependencies)`.
	 * The wrapper exists so test sites express their intent ("create a test
	 * engine") instead of setting the flag inline; the runtime factory
	 * (`FAngelscriptEngine::Create`) dispatches on the flag — see OpenSpec
	 * `refactor-as-engine-clone-removal` D8.
	 */
	static TUniquePtr<FAngelscriptEngine> Create(
		const FAngelscriptEngineConfig& Config,
		const FAngelscriptEngineDependencies& Dependencies);

	/** Get-or-create the long-lived shared Full test engine singleton. */
	static FAngelscriptEngine& GetSharedEngine();

	/** Destroy the shared engine singleton; next GetSharedEngine() will recreate. */
	static void DestroySharedEngine();

	/**
	 * Discard all compiled script modules on the given engine while preserving
	 * the engine's type binding databases and VM instance.
	 *
	 * After this call the engine is ready to compile a fresh module set
	 * without re-running type binding.
	 */
	static void ResetModules(FAngelscriptEngine& Engine);
};
