#pragma once

// ============================================================================
// AngelscriptTestFixture
// ============================================================================
//
// Themed sub-header split out of `AngelscriptTestUtilities.h` (Phase 1 of
// OpenSpec change `refactor-as-test-shared-layout-and-naming`).
//
// Responsibility:
//   - `ETestEngineMode` enum — distinguishes the three engine acquisition
//     strategies the fixture supports (SharedClone / IsolatedFull /
//     ProductionLike).
//   - `FAngelscriptTestFixture` — RAII fixture that owns an
//     `FAngelscriptEngine`, an `FAngelscriptEngineScope`, and provides
//     convenience methods (`BuildModule`, `ExecuteInt`, `ExecuteInt64`)
//     that delegate to the corresponding free functions in
//     `AngelscriptTestModuleBuilder.h` / `AngelscriptTestExecute.h`.
//
// Dependency layering:
//   - This is the **only** Shared/* header that depends on the other
//     themed sub-headers. Its purpose is to compose them into a single
//     ergonomic test-side facade. Sub-headers themselves remain
//     independent and may be included individually by tests that only
//     need one capability.
//
// Original location: AngelscriptTestUtilities.h lines 1009-1091.
// ============================================================================

#include "AngelscriptTestEngineAcquisition.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptEngine.h"
#include "Misc/AutomationTest.h"

enum class ETestEngineMode : uint8
{
	SharedClone,
	IsolatedFull,
	ProductionLike,
};

struct FAngelscriptTestFixture
{
	FAngelscriptTestFixture(FAutomationTestBase& InTest, ETestEngineMode InMode = ETestEngineMode::SharedClone)
		: Test(InTest)
		, Mode(InMode)
	{
		switch (Mode)
		{
		case ETestEngineMode::SharedClone:
		{
			FAngelscriptEngine& SharedEngine = AcquireCleanSharedCloneEngine();
			Engine = &SharedEngine;
			EngineScope = MakeUnique<FAngelscriptEngineScope>(SharedEngine);
			break;
		}
		case ETestEngineMode::IsolatedFull:
		{
			OwnedEngine = CreateIsolatedFullEngine();
			if (OwnedEngine.IsValid())
			{
				Engine = OwnedEngine.Get();
				EngineScope = MakeUnique<FAngelscriptEngineScope>(*Engine);
			}
			break;
		}
		case ETestEngineMode::ProductionLike:
		{
			FResolvedProductionLikeEngine Resolved;
			if (AcquireProductionLikeEngine(InTest, TEXT("FAngelscriptTestFixture failed to acquire production-like engine"), Resolved))
			{
				OwnedEngine = MoveTemp(Resolved.OwnedEngine);
				EngineScope = MoveTemp(Resolved.EngineScope);
				Engine = Resolved.Engine;
			}
			break;
		}
		}
	}

	~FAngelscriptTestFixture()
	{
		EngineScope.Reset();
	}

	FAngelscriptTestFixture(const FAngelscriptTestFixture&) = delete;
	FAngelscriptTestFixture& operator=(const FAngelscriptTestFixture&) = delete;

	bool IsValid() const { return Engine != nullptr; }
	FAngelscriptEngine& GetEngine() const { check(Engine != nullptr); return *Engine; }
	FAutomationTestBase& GetTest() const { return Test; }

	asIScriptModule* BuildModule(const char* ModuleName, const FString& Source)
	{
		check(Engine != nullptr);
		return ::BuildModule(Test, *Engine, ModuleName, Source);
	}

	bool ExecuteInt(asIScriptFunction& Function, int32& OutResult)
	{
		check(Engine != nullptr);
		return ExecuteIntFunction(Test, *Engine, Function, OutResult);
	}

	bool ExecuteInt64(asIScriptFunction& Function, int64& OutResult)
	{
		check(Engine != nullptr);
		return ExecuteInt64Function(Test, *Engine, Function, OutResult);
	}

private:
	FAutomationTestBase& Test;
	ETestEngineMode Mode;
	FAngelscriptEngine* Engine = nullptr;
	TUniquePtr<FAngelscriptEngine> OwnedEngine;
	TUniquePtr<FAngelscriptEngineScope> EngineScope;
};

