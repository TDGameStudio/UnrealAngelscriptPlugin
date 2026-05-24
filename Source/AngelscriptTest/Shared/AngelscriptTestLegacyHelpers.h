#pragma once

// ============================================================================
// Legacy Helper Macros (DEPRECATED)
// ============================================================================
//
// These macros use `return false` internally, making them incompatible with
// CQTest TEST_METHOD (which returns void). They exist only to support legacy
// IMPLEMENT_SIMPLE_AUTOMATION_TEST files that have not yet been migrated.
//
// New tests should use FCoverageModuleScope + ExpectGlobalInt / ExpectGlobalInts
// from AngelscriptBindingsAssertions.h instead.
// ============================================================================

#include "AngelscriptTestUtilities.h"

// Compile module + get function + execute int, return false on any failure.
// Requires: *this is FAutomationTestBase (bool RunTest), Engine is FAngelscriptEngine&
#define ASTEST_COMPILE_RUN_INT(Engine, ModuleName, Source, FuncDecl, OutResult) \
	do { \
		asIScriptModule* _Module = BuildModule( \
			*this, Engine, ModuleName, Source); \
		if (_Module == nullptr) { return false; } \
		asIScriptFunction* _Function = GetFunctionByDecl( \
			*this, *_Module, FuncDecl); \
		if (_Function == nullptr) { return false; } \
		if (!ExecuteIntFunction( \
			*this, Engine, *_Function, OutResult)) { return false; } \
	} while (false)

// Same as above but for int64 return type.
#define ASTEST_COMPILE_RUN_INT64(Engine, ModuleName, Source, FuncDecl, OutResult) \
	do { \
		asIScriptModule* _Module = BuildModule( \
			*this, Engine, ModuleName, Source); \
		if (_Module == nullptr) { return false; } \
		asIScriptFunction* _Function = GetFunctionByDecl( \
			*this, *_Module, FuncDecl); \
		if (_Function == nullptr) { return false; } \
		if (!ExecuteInt64Function( \
			*this, Engine, *_Function, OutResult)) { return false; } \
	} while (false)

// Compile only (no execution). Sets OutModulePtr, returns false on failure.
#define ASTEST_BUILD_MODULE(Engine, ModuleName, Source, OutModulePtr) \
	do { \
		OutModulePtr = BuildModule( \
			*this, Engine, ModuleName, Source); \
		if (OutModulePtr == nullptr) { return false; } \
	} while (false)
