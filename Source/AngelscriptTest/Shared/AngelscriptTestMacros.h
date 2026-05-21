#pragma once

#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestEnginePool.h"
#include "AngelscriptTestEngineHelper.h"
#include "Shared/AngelscriptTestEngine.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

// ============================================================================
// Angelscript Test Macros
// ============================================================================
//
// Engine Creation:
//   ASTEST_CREATE_ENGINE()       — shared engine, reset to clean state
//   ASTEST_GET_ENGINE()          — shared engine, no reset (use in TEST_METHOD)
//   ASTEST_CREATE_ENGINE_FULL()  — fresh isolated full engine
//   ASTEST_CREATE_ENGINE_NATIVE()— raw asIScriptEngine* from SDK
//
// Engine Reset:
//   ASTEST_RESET_ENGINE(Engine)  — reset shared engine (use in AFTER_ALL)
//
// CQTest standard pattern:
//   BEFORE_ALL()  { ASTEST_CREATE_ENGINE(); }
//   TEST_METHOD() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ... }
//   AFTER_ALL()   { FAngelscriptEngine& E = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(E); }
// ============================================================================

// ============================================================================
// Engine Creation Macros
// ============================================================================

// CREATE - Acquires the shared engine after resetting it to a clean state.
// Use for: first-time acquisition in BEFORE_ALL, or standalone tests needing
//          a guaranteed clean shared engine.
// Returns: FAngelscriptEngine&
#define ASTEST_CREATE_ENGINE() \
	([]() -> FAngelscriptEngine& { \
		FAngelscriptEngine& Engine = FAngelscriptTestEngine::GetSharedEngine(); \
		FAngelscriptTestEngine::ResetModules(Engine); \
		return Engine; \
	}())

// GET - Acquires the shared engine without resetting.
// Use for: TEST_METHOD bodies where the engine was already cleaned by
//          BEFORE_ALL / ASTEST_CREATE_ENGINE(). Pair with FCoverageModuleScope
//          for per-test module isolation.
// Returns: FAngelscriptEngine&
#define ASTEST_GET_ENGINE() \
	FAngelscriptTestEngine::GetSharedEngine()

// FULL - Creates a fresh isolated Full engine each time.
// Use for: engine core self-tests, bind environment testing, hot-reload tests.
// Returns: FAngelscriptEngine&
#define ASTEST_CREATE_ENGINE_FULL() \
	AngelscriptTestSupport::AcquireTransientFullTestEngine()

// NATIVE - Raw asIScriptEngine from the AngelScript SDK.
// Use for: testing AngelScript SDK APIs directly.
// Returns: asIScriptEngine*
#define ASTEST_CREATE_ENGINE_NATIVE() \
	asCreateScriptEngine(ANGELSCRIPT_VERSION)

// ============================================================================
// Engine Reset Macro
// ============================================================================

// RESET - Resets the shared engine to a clean state.
// Use for: AFTER_ALL / AFTER_EACH to leave the shared engine clean for
//          subsequent test classes.
#define ASTEST_RESET_ENGINE(Engine) \
	FAngelscriptTestEngine::ResetModules(Engine)
