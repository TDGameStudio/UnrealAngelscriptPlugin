// ============================================================================
// AngelscriptReflectiveFallbackCacheTests.cpp
//
// Functional regression coverage for the per-UFunction parameter cache that
// backs the BlueprintCallable reflective-fallback path. The cache lives inside
// FBlueprintCallableReflectiveSignature::CachedParams and is populated lazily
// on the first reflective call, then reused for every subsequent dispatch via
// `Function->Invoke` (no ProcessEvent).
//
// Automation IDs:
//   Angelscript.TestModule.Bindings.ReflectiveFallbackCache.*
//
// Sections cover the dispatch matrix called out in
// `Documents/Plans/Plan_ReflectiveFallbackCache.md`:
//   PODScalar    - int32/bool/double fast-path memcpy + POD return
//   NonPOD       - FString copy semantics
//   OutParam     - pure out-param writeback through the FOutParmRec chain
//   Return       - non-POD FString return
//   MixinObject  - static UFUNCTION binding with bInjectMixinObject==true
//                  (every BPLib free function exercises this path)
//   CacheReuse   - same UFunction called many times in one AS run; second and
//                  later calls must reuse the cached metadata and remain
//                  correct (verified by counting outputs across iterations).
//   Eligibility  - direct C++ check that the representative BPLib UFUNCTION
//                  is still accepted by the reflective fallback gate.
//
// Why we use BlueprintPathsLibrary for these tests:
//   The UHT summary (`AS_FunctionTable_Summary.json`) reports
//   BlueprintPathsLibrary entries as stubs (100% reflective fallback). That
//   makes path functions a stable core-plugin signal that the cache is on the
//   critical path without depending on optional plugin bindings.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptReflectiveAccess.h"
#include "Binds/BlueprintCallableReflectiveFallback.h"
#include "Binds/Helper_FunctionSignature.h"

#include "Kismet/BlueprintPathsLibrary.h"
#include "HAL/IConsoleManager.h"

#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptTest_Bindings_ReflectiveFallbackCache_Private
{
	// Build the AS namespace prefix used for static UFUNCTIONs that reach the
	// reflective fallback.
	FString GetPathsLibraryCallPrefix(FAutomationTestBase& Test)
	{
		UClass* LibraryClass = UBlueprintPathsLibrary::StaticClass();
		UFunction* RepresentativeFunction = LibraryClass->FindFunctionByName(TEXT("GetBaseFilename"));
		TSharedPtr<FAngelscriptType> LibraryType = FAngelscriptType::GetByClass(LibraryClass);
		if (!Test.TestTrue(TEXT("BlueprintPathsLibrary type should resolve"), LibraryType.IsValid()))
		{
			return FString();
		}

		if (!Test.TestNotNull(TEXT("GetBaseFilename should exist on BlueprintPathsLibrary"), RepresentativeFunction))
		{
			return FString();
		}

		const FString Namespace = FAngelscriptFunctionSignature::GetScriptNamespaceForClass(LibraryType.ToSharedRef(), RepresentativeFunction);
		return Namespace.IsEmpty() ? FString() : Namespace + TEXT("::");
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptReflectiveFallbackCacheTest,
	"Angelscript.TestModule.Bindings.ReflectiveFallbackCache",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	// ====================================================================
	// Section: PODScalar  - memcpy fast path for POD scalar args/return.
	// ====================================================================

	TEST_METHOD(PODScalar)
	{
		using namespace AngelscriptTest_Bindings_ReflectiveFallbackCache_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		FAngelscriptEngineScope Scope(Engine);

		const FString CallPrefix = GetPathsLibraryCallPrefix(*TestRunner);
		if (CallPrefix.IsEmpty()) return;

		FString Script = FString::Printf(TEXT(R"(
int RunPODScalar()
{
	return %sIsRelative("Relative/Cache.txt") ? 1 : 0;
}
)"), *CallPrefix);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASRefCachePODScalar", Script);
		if (Module == nullptr) return;

		asIScriptFunction* Function = GetFunctionByDecl(*TestRunner, *Module, TEXT("int RunPODScalar()"));
		if (Function == nullptr) return;

		int32 Result = 0;
		if (!ExecuteIntFunction(*TestRunner, Engine, *Function, Result)) return;

		TestRunner->TestEqual(TEXT("POD scalar reflective fallback should return bool via memcpy fast path"), Result, 1);
	}

	// ====================================================================
	// Section: NonPOD  - exercises virtual CopySingleValue path for FName/FString.
	// ====================================================================

	TEST_METHOD(NonPOD)
	{
		using namespace AngelscriptTest_Bindings_ReflectiveFallbackCache_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		FAngelscriptEngineScope Scope(Engine);

		const FString CallPrefix = GetPathsLibraryCallPrefix(*TestRunner);
		if (CallPrefix.IsEmpty()) return;

		FString Script = FString::Printf(TEXT(R"(
int RunNonPOD()
{
	FString Clean = %sGetCleanFilename("C:/Reflective/Fallback/Cache.txt");
	if (Clean != "Cache.txt") return 10;
	return 1;
}
)"), *CallPrefix);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASRefCacheNonPOD", Script);
		if (Module == nullptr) return;

		asIScriptFunction* Function = GetFunctionByDecl(*TestRunner, *Module, TEXT("int RunNonPOD()"));
		if (Function == nullptr) return;

		int32 Result = 0;
		if (!ExecuteIntFunction(*TestRunner, Engine, *Function, Result)) return;

		TestRunner->TestEqual(TEXT("Non-POD reflective fallback should round-trip FString via CopySingleValue"), Result, 1);
	}

	// ====================================================================
	// Section: OutParam  - exercises FOutParmRec chain for `out` writeback.
	//
	// `Split(const FString&, FString&, FString&, FString&)` is a
	// reflective-fallback function with pure out FString refs - the cached
	// invoker must write the path parts back to AS storage.
	// ====================================================================

	TEST_METHOD(OutParam)
	{
		using namespace AngelscriptTest_Bindings_ReflectiveFallbackCache_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		FAngelscriptEngineScope Scope(Engine);

		const FString CallPrefix = GetPathsLibraryCallPrefix(*TestRunner);
		if (CallPrefix.IsEmpty()) return;

		FString Script = FString::Printf(TEXT(R"(
int RunOutParam()
{
	FString Path;
	FString Filename;
	FString Extension;
	%sSplit("C:/Reflective/Fallback/Cache.txt", Path, Filename, Extension);
	if (Path != "C:/Reflective/Fallback") return 10;
	if (Filename != "Cache") return 20;
	if (Extension != "txt") return 30;
	return 1;
}
)"), *CallPrefix);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASRefCacheOutParam", Script);
		if (Module == nullptr) return;

		asIScriptFunction* Function = GetFunctionByDecl(*TestRunner, *Module, TEXT("int RunOutParam()"));
		if (Function == nullptr) return;

		int32 Result = 0;
		if (!ExecuteIntFunction(*TestRunner, Engine, *Function, Result)) return;

		TestRunner->TestEqual(TEXT("Reflective fallback should write UPARAM(ref) parameters back to script storage"), Result, 1);
	}

	// ====================================================================
	// Section: Return  - non-POD FString return.
	// ====================================================================

	TEST_METHOD(Return)
	{
		using namespace AngelscriptTest_Bindings_ReflectiveFallbackCache_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		FAngelscriptEngineScope Scope(Engine);

		const FString CallPrefix = GetPathsLibraryCallPrefix(*TestRunner);
		if (CallPrefix.IsEmpty()) return;

		FString Script = FString::Printf(TEXT(R"(
int RunReturn()
{
	FString Base = %sGetBaseFilename("C:/Reflective/Fallback/Cache.txt", true);
	if (Base != "Cache") return 10;
	return 1;
}
)"), *CallPrefix);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASRefCacheReturn", Script);
		if (Module == nullptr) return;

		asIScriptFunction* Function = GetFunctionByDecl(*TestRunner, *Module, TEXT("int RunReturn()"));
		if (Function == nullptr) return;

		int32 Result = 0;
		if (!ExecuteIntFunction(*TestRunner, Engine, *Function, Result)) return;

		TestRunner->TestEqual(TEXT("Reflective fallback should return non-POD FString values correctly"), Result, 1);
	}

	// ====================================================================
	// Section: MixinObject  - static UFUNCTION bound with bInjectMixinObject=true.
	//
	// All BlueprintPathsLibrary functions are static BPLib statics that
	// reach the reflective fallback path with bInjectMixinObject==true. This
	// section just confirms the mixin-object branch of the cached invoker
	// (where the first parameter slot is fed from Generic->GetObject()) keeps
	// returning sane values across multiple BPLib calls in one script.
	// ====================================================================

	TEST_METHOD(MixinObject)
	{
		using namespace AngelscriptTest_Bindings_ReflectiveFallbackCache_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		FAngelscriptEngineScope Scope(Engine);

		const FString CallPrefix = GetPathsLibraryCallPrefix(*TestRunner);
		if (CallPrefix.IsEmpty()) return;

		FString Script = FString::Printf(TEXT(R"(
int RunMixin()
{
	FString Clean = %sGetCleanFilename("C:/Reflective/Fallback/Cache.txt");
	bool bRelative = %sIsRelative("Relative/Cache.txt");
	if (Clean != "Cache.txt") return 10;
	if (!bRelative) return 20;
	return 1;
}
)"), *CallPrefix, *CallPrefix);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASRefCacheMixin", Script);
		if (Module == nullptr) return;

		asIScriptFunction* Function = GetFunctionByDecl(*TestRunner, *Module, TEXT("int RunMixin()"));
		if (Function == nullptr) return;

		int32 Result = 0;
		if (!ExecuteIntFunction(*TestRunner, Engine, *Function, Result)) return;

		TestRunner->TestEqual(TEXT("Mixin-object reflective fallback should still produce correct results across multiple calls"), Result, 1);
	}

	// ====================================================================
	// Section: CacheReuse  - hammer the same UFunction many times.
	//
	// On the first call the cache is built; every later call must reuse it.
	// We do not (and cannot) reach into FBlueprintCallableReflectiveSignature::
	// CachedParams from this test (it lives in an anonymous namespace inside
	// BlueprintCallableReflectiveFallback.cpp), so we infer cache health by
	// running the same function many times and demanding consistent output.
	// ====================================================================

	TEST_METHOD(CacheReuse)
	{
		using namespace AngelscriptTest_Bindings_ReflectiveFallbackCache_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		FAngelscriptEngineScope Scope(Engine);

		const FString CallPrefix = GetPathsLibraryCallPrefix(*TestRunner);
		if (CallPrefix.IsEmpty()) return;

		FString Script = FString::Printf(TEXT(R"(
int RunCacheReuse()
{
	int TotalCount = 0;
	for (int Index = 0; Index < 32; ++Index)
	{
		FString Clean = %sGetCleanFilename("C:/Reflective/Fallback/Cache.txt");
		TotalCount += Clean.Len();
	}
	if (TotalCount != 288) return 10;
	return 1;
}
)"), *CallPrefix);

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASRefCacheReuse", Script);
		if (Module == nullptr) return;

		asIScriptFunction* Function = GetFunctionByDecl(*TestRunner, *Module, TEXT("int RunCacheReuse()"));
		if (Function == nullptr) return;

		int32 Result = 0;
		if (!ExecuteIntFunction(*TestRunner, Engine, *Function, Result)) return;

		TestRunner->TestEqual(TEXT("Reflective fallback cache should produce correct results across 32 repeated calls"), Result, 1);
	}

	// ====================================================================
	// Section: FallbackEligibility
	//
	// Structural verification: the representative BPLib UFUNCTION remains
	// classifiable by the reflective fallback gate.
	// ====================================================================

	TEST_METHOD(FuncNetEligibility)
	{
		const UFunction* BaseFilenameFunction = UBlueprintPathsLibrary::StaticClass()
			->FindFunctionByName(TEXT("GetBaseFilename"));
		ASSERT_THAT(IsNotNull(BaseFilenameFunction));
		TestRunner->TestEqual(
			TEXT("BPLib UFUNCTIONs reaching reflective fallback should remain eligible after the cache lands"),
			EvaluateReflectionFallback(BaseFilenameFunction),
			EReflectionFallbackResult::Success);
	}

	// ====================================================================
	// Section: CVarParityCachedVsProcessEvent
	//
	// Toggles `as.ReflectiveFallback.UseCache` mid-test to verify both
	// dispatch strategies produce identical observable results. Combines
	// POD scalar + non-POD return + out-param writeback + repeated calls
	// into one composite checksum so a single integer encodes the full
	// behavioural surface. The CVar is captured + restored to keep the
	// rest of the suite running with whatever default the project chose.
	// ====================================================================

	TEST_METHOD(CVarParityCachedVsProcessEvent)
	{
		using namespace AngelscriptTest_Bindings_ReflectiveFallbackCache_Private;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		FAngelscriptEngineScope Scope(Engine);

		const FString CallPrefix = GetPathsLibraryCallPrefix(*TestRunner);
		if (CallPrefix.IsEmpty()) return;

		// Composite checksum exercising cached and legacy dispatch with
		// repeated POD scalar and FString-return calls. Out-param writeback is
		// covered by the dedicated OutParam section above.
		FString Script = FString::Printf(TEXT(R"(
int RunParity()
{
	int Acc = 0;
	for (int Index = 0; Index < 8; ++Index)
	{
		bool bRelative = %sIsRelative("Relative/Cache.txt");
		Acc += bRelative ? 100 : 0;

		FString Clean = %sGetCleanFilename("C:/Reflective/Fallback/Cache.txt");
		Acc += Clean.Len();

		FString Base = %sGetBaseFilename("C:/Reflective/Fallback/Cache.txt", true);
		Acc += Base.Len() * 7;
	}
	return Acc;
}
)"),
			*CallPrefix, *CallPrefix, *CallPrefix);

		// Capture the CVar so we leave it exactly as we found it. The CVar is
		// owned by AngelscriptRuntime (registered in BlueprintCallableReflectiveFallback.cpp).
		IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("as.ReflectiveFallback.UseCache"));
		ASSERT_THAT(IsNotNull(CVar));
		const bool bOriginal = CVar->GetBool();
		ON_SCOPE_EXIT { CVar->Set(bOriginal ? 1 : 0, ECVF_SetByCode); };

		// --- Cached path (CVar = 1) ---
		CVar->Set(1, ECVF_SetByCode);
		asIScriptModule* CachedModule = BuildModule(*TestRunner, Engine, "ASRefCacheParityCached", Script);
		if (CachedModule == nullptr) return;
		asIScriptFunction* CachedFunction = GetFunctionByDecl(*TestRunner, *CachedModule, TEXT("int RunParity()"));
		if (CachedFunction == nullptr) return;
		int32 CachedResult = 0;
		if (!ExecuteIntFunction(*TestRunner, Engine, *CachedFunction, CachedResult)) return;

		// --- Legacy ProcessEvent path (CVar = 0) ---
		CVar->Set(0, ECVF_SetByCode);
		asIScriptModule* LegacyModule = BuildModule(*TestRunner, Engine, "ASRefCacheParityLegacy", Script);
		if (LegacyModule == nullptr) return;
		asIScriptFunction* LegacyFunction = GetFunctionByDecl(*TestRunner, *LegacyModule, TEXT("int RunParity()"));
		if (LegacyFunction == nullptr) return;
		int32 LegacyResult = 0;
		if (!ExecuteIntFunction(*TestRunner, Engine, *LegacyFunction, LegacyResult)) return;

		TestRunner->TestEqual(
			TEXT("Cached and ProcessEvent reflective fallback paths must produce identical composite checksum"),
			CachedResult,
			LegacyResult);

		// Sanity bound: a zero would indicate both paths silently failed in
		// lockstep, defeating the equality check above.
		TestRunner->TestTrue(
			TEXT("Composite checksum should be non-zero"),
			CachedResult > 0);
	}
};

#endif
