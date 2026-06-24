// ============================================================================
// AngelscriptClassBindingsTests.cpp
//
// UClass / TSubclassOf / TSoftClassPtr / StaticClass binding coverage.
// All 10 Automation IDs are grouped under the Class. prefix so a single
// `RunTests.ps1 -Prefix Angelscript.TestModule.Bindings.Class` invocation
// covers everything in one Editor startup.
//
//   - Angelscript.TestModule.Bindings.Class.ClassLookupCompat
//   - Angelscript.TestModule.Bindings.Class.TSubclassOfCompat
//   - Angelscript.TestModule.Bindings.Class.TSubclassOfRejectsUnrelatedClass
//   - Angelscript.TestModule.Bindings.Class.TSoftClassPtrCompat
//   - Angelscript.TestModule.Bindings.Class.StaticClassCompat
//   - Angelscript.TestModule.Bindings.Class.NativeStaticClassNamespace
//   - Angelscript.TestModule.Bindings.Class.NativeStaticTypeGlobal
//   - Angelscript.TestModule.Bindings.Class.UClassReflectionCompat
//   - Angelscript.TestModule.Bindings.Class.TSoftClassPtrRejectsUnrelatedClass
//   - Angelscript.TestModule.Bindings.Class.ClassReturnTypeAndLogDiag
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestEngineHelper.h"

#include "Camera/CameraActor.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "UObject/Package.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_context.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ============================================================================
// Sections
// ============================================================================

namespace
{
	// -----------------------------------------------------------------------
	// Section: ClassLookup — FindClass / GetAllClasses
	// -----------------------------------------------------------------------
	bool RunClassLookupSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASClass_Lookup"), TEXT(R"(
int FindClass_NotNull()
{
	UClass C = FindClass("AActor");
	return (C == null) ? 0 : 1;
}
int FindClass_MissingIsNull()
{
	UClass C = FindClass("DefinitelyNotAClass_xyz_42");
	return (C == null) ? 1 : 0;
}
int GetAllClasses_NonEmpty()
{
	TArray<UClass> All;
	GetAllClasses(All);
	return (All.Num() > 0) ? 1 : 0;
}
int GetAllClasses_ContainsActor()
{
	UClass ActorClass = FindClass("AActor");
	TArray<UClass> All;
	GetAllClasses(All);
	foreach (UClass C : All)
	{
		if (C == ActorClass)
			return 1;
	}
	return 0;
}
int IsChildOf_ChildToParent()
{
	UClass Camera = ACameraActor::StaticClass();
	UClass Actor = AActor::StaticClass();
	return Camera.IsChildOf(Actor) ? 1 : 0;
}
int IsChildOf_ParentToChildFalse()
{
	UClass Camera = ACameraActor::StaticClass();
	UClass Actor = AActor::StaticClass();
	return Actor.IsChildOf(Camera) ? 1 : 0;
}
int GetSuperClass_CameraIsActor()
{
	UClass Camera = ACameraActor::StaticClass();
	return (Camera.GetSuperClass() == AActor::StaticClass()) ? 1 : 0;
}
int GetSuperClass_ActorNotNull()
{
	UClass Actor = AActor::StaticClass();
	return (Actor.GetSuperClass() != null) ? 1 : 0;
}
)"));
		if (!ModuleScope.IsValid()) return false;
		asIScriptModule& Module = ModuleScope.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int FindClass_NotNull()"),         TEXT("FindClass(AActor) should not be null"),               1 },
			{ TEXT("int FindClass_MissingIsNull()"),    TEXT("FindClass on missing name should return null"),       1 },
			{ TEXT("int GetAllClasses_NonEmpty()"),     TEXT("GetAllClasses should populate at least one entry"),    1 },
			{ TEXT("int GetAllClasses_ContainsActor()"),TEXT("GetAllClasses should include AActor"),                  1 },
			{ TEXT("int IsChildOf_ChildToParent()"),    TEXT("ACameraActor should be child of AActor"),              1 },
			{ TEXT("int IsChildOf_ParentToChildFalse()"),TEXT("AActor should NOT be child of ACameraActor"),         0 },
			{ TEXT("int GetSuperClass_CameraIsActor()"),TEXT("ACameraActor.GetSuperClass() should be AActor"),       1 },
			{ TEXT("int GetSuperClass_ActorNotNull()"), TEXT("AActor.GetSuperClass() should not be null"),           1 },
		};
		return ExpectGlobalInts(Test, Engine, Module,  Cases);
	}

	// -----------------------------------------------------------------------
	// Section: TSubclassOf — basic ops + arg passing + GetDefaultObject
	// -----------------------------------------------------------------------
	bool RunTSubclassOfSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASClass_TSubclassOf"), TEXT(R"(
TSubclassOf<AActor> EchoSubclass(TSubclassOf<AActor> Value)
{
	return Value;
}
UClass EchoSubclassClass(TSubclassOf<AActor> Value)
{
	return Value;
}

int Empty_IsValid()
{
	TSubclassOf<AActor> S;
	return S.IsValid() ? 1 : 0;
}
int Empty_GetIsNull()
{
	TSubclassOf<AActor> S;
	return (S.Get() == null) ? 1 : 0;
}
int AssignActor_IsValid()
{
	TSubclassOf<AActor> S;
	S = AActor::StaticClass();
	return S.IsValid() ? 1 : 0;
}
int AssignActor_GetMatch()
{
	TSubclassOf<AActor> S;
	S = AActor::StaticClass();
	return (S.Get() == AActor::StaticClass()) ? 1 : 0;
}
int AssignActor_OpEqualsClass()
{
	TSubclassOf<AActor> S;
	S = AActor::StaticClass();
	return (S == AActor::StaticClass()) ? 1 : 0;
}
int ImplicitFromClassArg_OpEquals()
{
	TSubclassOf<AActor> R = EchoSubclass(AActor::StaticClass());
	return (R == AActor::StaticClass()) ? 1 : 0;
}
int ImplicitClassArg_RoundTrip()
{
	UClass R = EchoSubclassClass(ACameraActor::StaticClass());
	return (R == ACameraActor::StaticClass()) ? 1 : 0;
}
int Narrowed_CopyOpEquals()
{
	TSubclassOf<AActor> N = ACameraActor::StaticClass();
	TSubclassOf<AActor> Copy = N;
	return (Copy == ACameraActor::StaticClass()) ? 1 : 0;
}
int GetDefaultObject_IsValid()
{
	TSubclassOf<AActor> N = ACameraActor::StaticClass();
	AActor D = N.GetDefaultObject();
	return IsValid(D) ? 1 : 0;
}
int GetDefaultObject_IsACameraActor()
{
	TSubclassOf<AActor> N = ACameraActor::StaticClass();
	AActor D = N.GetDefaultObject();
	return D.IsA(ACameraActor::StaticClass()) ? 1 : 0;
}
int Array_LiteralNum()
{
	TArray<TSubclassOf<AActor>> H;
	H.Add(AActor::StaticClass());
	H.Add(ACameraActor::StaticClass());
	return H.Num();
}
int Array_LiteralIndex()
{
	TArray<TSubclassOf<AActor>> H;
	H.Add(AActor::StaticClass());
	H.Add(ACameraActor::StaticClass());
	return (H[1] == ACameraActor::StaticClass()) ? 1 : 0;
}
int IsChildOf_CameraToActor()
{
	TSubclassOf<AActor> S = ACameraActor::StaticClass();
	return S.IsChildOf(AActor::StaticClass()) ? 1 : 0;
}
int IsChildOf_ActorToCameraFalse()
{
	TSubclassOf<AActor> S = AActor::StaticClass();
	return S.IsChildOf(ACameraActor::StaticClass()) ? 1 : 0;
}
int OpEquals_TwoSubclassOf()
{
	TSubclassOf<AActor> A = AActor::StaticClass();
	TSubclassOf<AActor> B = AActor::StaticClass();
	return (A == B) ? 1 : 0;
}
int SetKey_Contains()
{
	TSet<TSubclassOf<AActor>> S;
	S.Add(AActor::StaticClass());
	S.Add(ACameraActor::StaticClass());
	return S.Contains(ACameraActor::StaticClass()) ? 1 : 0;
}
int MapKey_FindValue()
{
	TMap<TSubclassOf<AActor>, int> M;
	M.Add(AActor::StaticClass(), 42);
	M.Add(ACameraActor::StaticClass(), 99);
	int Value = 0;
	if (M.Find(ACameraActor::StaticClass(), Value))
		return Value;
	return 0;
}
)"));
		if (!ModuleScope.IsValid()) return false;
		asIScriptModule& Module = ModuleScope.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int Empty_IsValid()"),                TEXT("Empty TSubclassOf should not be valid"),                  0 },
			{ TEXT("int Empty_GetIsNull()"),               TEXT("Empty TSubclassOf.Get() should be null"),                  1 },
			{ TEXT("int AssignActor_IsValid()"),           TEXT("Assign(AActor) should make TSubclassOf valid"),            1 },
			{ TEXT("int AssignActor_GetMatch()"),          TEXT("Assign(AActor).Get() should equal AActor::StaticClass()"), 1 },
			{ TEXT("int AssignActor_OpEqualsClass()"),     TEXT("opEquals should compare TSubclassOf with raw UClass"),     1 },
			{ TEXT("int ImplicitFromClassArg_OpEquals()"), TEXT("Implicit conversion from UClass arg should round-trip"),   1 },
			{ TEXT("int ImplicitClassArg_RoundTrip()"),    TEXT("TSubclassOf -> UClass arg should round-trip"),             1 },
			{ TEXT("int Narrowed_CopyOpEquals()"),         TEXT("Copy ctor should preserve narrowed class"),                1 },
			{ TEXT("int GetDefaultObject_IsValid()"),      TEXT("GetDefaultObject of camera class should be valid"),        1 },
			{ TEXT("int GetDefaultObject_IsACameraActor()"),TEXT("GetDefaultObject should be a ACameraActor"),                1 },
			{ TEXT("int Array_LiteralNum()"),              TEXT("TArray<TSubclassOf<AActor>> Num after Add x2 should be 2"), 2 },
			{ TEXT("int Array_LiteralIndex()"),            TEXT("Array index access should preserve narrowed class"),       1 },
			{ TEXT("int IsChildOf_CameraToActor()"),       TEXT("TSubclassOf(Camera).IsChildOf(Actor) should be true"),     1 },
			{ TEXT("int IsChildOf_ActorToCameraFalse()"),  TEXT("TSubclassOf(Actor).IsChildOf(Camera) should be false"),    0 },
			{ TEXT("int OpEquals_TwoSubclassOf()"),        TEXT("Two TSubclassOf with same class should be equal"),          1 },
			{ TEXT("int SetKey_Contains()"),                TEXT("TSet<TSubclassOf<AActor>> should support Contains"),        1 },
			{ TEXT("int MapKey_FindValue()"),               TEXT("TMap<TSubclassOf<AActor>, int> should find value by key"),  99 },
		};
		return ExpectGlobalInts(Test, Engine, Module,  Cases);
	}

	// -----------------------------------------------------------------------
	// Section: TSubclassOfReject — exception path on unrelated class assignment
	//
	// Two negative paths:
	//   1) Implicit construction with unrelated class throws
	//   2) Assignment via UClass arg with unrelated class throws and resets dest
	// Plus one positive sanity path:
	//   3) Assignment with null UClass arg cleanly resets dest
	// -----------------------------------------------------------------------
	bool RunTSubclassOfRejectSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		// Register all expected error patterns up front. The AS engine logs
		// the diagnostic + the call-stack frame, so cover both.
		const FString DiagnosticText(TEXT("Class set to TSubclassOf<> was not a child of templated class."));
		Test.AddExpectedErrorPlain(DiagnosticText, EAutomationExpectedErrorFlags::Contains, 0);
		Test.AddExpectedErrorPlain(
			FString(TEXT("ASClass_Reject")),
			EAutomationExpectedErrorFlags::Contains, 0);
		Test.AddExpectedErrorPlain(TEXT("TriggerInvalidImplicitCtor"),
			EAutomationExpectedErrorFlags::Contains, 0);
		Test.AddExpectedErrorPlain(TEXT("AssignClass"),
			EAutomationExpectedErrorFlags::Contains, 0);

		FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASClass_Reject"), TEXT(R"(
void TriggerInvalidImplicitCtor()
{
	TSubclassOf<AActor> Invalid = UPackage::StaticClass();
}

void AssignClass(TSubclassOf<AActor>& OutValue, UClass NewClass)
{
	OutValue = NewClass;
}
)"));
		if (!ModuleScope.IsValid()) return false;
		asIScriptModule& Module = ModuleScope.GetModule();

		bool bPassed = true;
		FNoDiscardAsserter LocalAssert(Test);

		// (3) Positive sanity: null assignment via the AssignClass(out, null) path.
		// Drive AssignClass directly with FASGlobalFunctionInvoker so we can pass
		// a TSubclassOf& out-ref + a UClass* arg.
		{
			TSubclassOf<AActor> ResetTarget = AActor::StaticClass();

			FASGlobalFunctionInvoker NullInvoker(
				Test, Engine, Module,
				TEXT("void AssignClass(TSubclassOf<AActor>& OutValue, UClass NewClass)"));
			if (NullInvoker.IsValid())
			{
				NullInvoker.AddArgRef(ResetTarget);
				NullInvoker.AddArgObject(nullptr);
				bPassed &= LocalAssert.IsTrue(
					NullInvoker.Call(),
					TEXT("[Class] AssignClass(out, null) should execute"));

				bPassed &= LocalAssert.IsNull(
					ResetTarget.Get(),
					TEXT("[Class] AssignClass(null) should clear stored class pointer"));
				bPassed &= LocalAssert.IsNull(
					ResetTarget.GetDefaultObject(),
					TEXT("[Class] AssignClass(null) should clear default object path"));
			}
			else
			{
				bPassed = false;
			}
		}

		// (1) Negative: implicit ctor with unrelated UClass should throw.
		bPassed &= ExecuteFunctionExpectingScriptException(
			Test, Engine, Module, 
			TEXT("void TriggerInvalidImplicitCtor()"),
			TEXT("Implicit ctor from unrelated UClass should raise script exception"),
			DiagnosticText);

		// (2) Negative: AssignClass with unrelated class should throw + reset dest.
		// We must drive the call manually because we need to pass arguments and
		// inspect post-state — ExecuteFunctionExpectingScriptException is the
		// no-arg shortcut. Use the lower-level invoker.
		{
			TSubclassOf<AActor> AssignmentTarget = AActor::StaticClass();

			FASGlobalFunctionInvoker BadInvoker(
				Test, Engine, Module,
				TEXT("void AssignClass(TSubclassOf<AActor>& OutValue, UClass NewClass)"));
			if (BadInvoker.IsValid())
			{
				BadInvoker.AddArgRef(AssignmentTarget);
				BadInvoker.AddArgObject(UPackage::StaticClass());
				const bool bCalled = BadInvoker.Call();
				// Call() returns false on script exception — that's the desired
				// outcome here. Don't TestTrue it.
				(void)bCalled;

				bPassed &= LocalAssert.IsNull(
					AssignmentTarget.Get(),
					TEXT("[Class] AssignClass(unrelated) should clear stored class pointer"));
				bPassed &= LocalAssert.IsNull(
					AssignmentTarget.GetDefaultObject(),
					TEXT("[Class] AssignClass(unrelated) should clear default object path"));
			}
			else
			{
				bPassed = false;
			}
		}

		return bPassed;
	}

	// -----------------------------------------------------------------------
	// Section: TSoftClassPtr — soft references to UClass
	// -----------------------------------------------------------------------
	bool RunTSoftClassPtrSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASClass_TSoftClassPtr"), TEXT(R"(
TSoftClassPtr<AActor> EchoSoftClass(TSoftClassPtr<AActor> Value)
{
	return Value;
}
UClass EchoSoftClassClass(TSoftClassPtr<AActor> Value)
{
	return Value.Get();
}

int Empty_IsNull()
{
	TSoftClassPtr<AActor> S;
	return S.IsNull() ? 1 : 0;
}
int Empty_IsValid()
{
	TSoftClassPtr<AActor> S;
	return S.IsValid() ? 1 : 0;
}
int Constructed_OpEquals()
{
	TSoftClassPtr<AActor> S(AActor::StaticClass());
	return (S == AActor::StaticClass()) ? 1 : 0;
}
int Constructed_GetMatch()
{
	TSoftClassPtr<AActor> S(AActor::StaticClass());
	return (S.Get() == AActor::StaticClass()) ? 1 : 0;
}
int Constructed_IsValid()
{
	TSoftClassPtr<AActor> S(AActor::StaticClass());
	return S.IsValid() ? 1 : 0;
}
int Constructed_ToStringNonEmpty()
{
	TSoftClassPtr<AActor> S(AActor::StaticClass());
	return S.ToString().IsEmpty() ? 0 : 1;
}
int ImplicitArg_OpEquals()
{
	TSoftClassPtr<AActor> R = EchoSoftClass(TSoftClassPtr<AActor>(AActor::StaticClass()));
	return (R == AActor::StaticClass()) ? 1 : 0;
}
int Echo_GetMatch()
{
	UClass R = EchoSoftClassClass(TSoftClassPtr<AActor>(ACameraActor::StaticClass()));
	return (R == ACameraActor::StaticClass()) ? 1 : 0;
}
int Assigned_OpEquals()
{
	TSoftClassPtr<AActor> A;
	A = AActor::StaticClass();
	return (A == AActor::StaticClass()) ? 1 : 0;
}
int Array_LiteralNum()
{
	TArray<TSoftClassPtr<AActor>> H;
	H.Add(TSoftClassPtr<AActor>(AActor::StaticClass()));
	H.Add(TSoftClassPtr<AActor>(ACameraActor::StaticClass()));
	return H.Num();
}
int Array_LiteralIndex()
{
	TArray<TSoftClassPtr<AActor>> H;
	H.Add(TSoftClassPtr<AActor>(AActor::StaticClass()));
	H.Add(TSoftClassPtr<AActor>(ACameraActor::StaticClass()));
	return (H[1] == ACameraActor::StaticClass()) ? 1 : 0;
}
int Reset_IsNull()
{
	TSoftClassPtr<AActor> A(AActor::StaticClass());
	A.Reset();
	return A.IsNull() ? 1 : 0;
}
int FromTSubclassOf_Ctor()
{
	TSubclassOf<AActor> Sub = AActor::StaticClass();
	TSoftClassPtr<AActor> S(Sub);
	return (S == AActor::StaticClass()) ? 1 : 0;
}
int FromTSubclassOf_Assign()
{
	TSubclassOf<AActor> Sub = ACameraActor::StaticClass();
	TSoftClassPtr<AActor> S;
	S = Sub;
	return (S == ACameraActor::StaticClass()) ? 1 : 0;
}
int OpEquals_TSubclassOf()
{
	TSubclassOf<AActor> Sub = AActor::StaticClass();
	TSoftClassPtr<AActor> S(AActor::StaticClass());
	return (S == Sub) ? 1 : 0;
}
int Get_ReturnsSubclassOf_IsValid()
{
	TSoftClassPtr<AActor> S(AActor::StaticClass());
	TSubclassOf<AActor> G = S.Get();
	return G.IsValid() ? 1 : 0;
}
int ToSoftObjectPath_NonEmpty()
{
	TSoftClassPtr<AActor> S(AActor::StaticClass());
	FSoftObjectPath P = S.ToSoftObjectPath();
	return P.ToString().IsEmpty() ? 0 : 1;
}
int GetAssetName_NonEmpty()
{
	TSoftClassPtr<AActor> S(AActor::StaticClass());
	return S.GetAssetName().IsEmpty() ? 0 : 1;
}
)"));
		if (!ModuleScope.IsValid()) return false;
		asIScriptModule& Module = ModuleScope.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int Empty_IsNull()"),               TEXT("Empty TSoftClassPtr should be null"),               1 },
			{ TEXT("int Empty_IsValid()"),               TEXT("Empty TSoftClassPtr should not be valid"),         0 },
			{ TEXT("int Constructed_OpEquals()"),        TEXT("Constructed TSoftClassPtr opEquals UClass"),        1 },
			{ TEXT("int Constructed_GetMatch()"),        TEXT("Constructed TSoftClassPtr.Get() should match"),     1 },
			{ TEXT("int Constructed_IsValid()"),         TEXT("Constructed TSoftClassPtr should be valid"),        1 },
			{ TEXT("int Constructed_ToStringNonEmpty()"),TEXT("Constructed ToString should not be empty"),         1 },
			{ TEXT("int ImplicitArg_OpEquals()"),        TEXT("Implicit construction in arg should round-trip"),   1 },
			{ TEXT("int Echo_GetMatch()"),               TEXT("Echo via .Get() should preserve narrowed class"),   1 },
			{ TEXT("int Assigned_OpEquals()"),           TEXT("opAssign(UClass) should make value equal"),         1 },
			{ TEXT("int Array_LiteralNum()"),            TEXT("TArray<TSoftClassPtr<AActor>> Num should be 2"),    2 },
			{ TEXT("int Array_LiteralIndex()"),          TEXT("Array element [1] should equal camera class"),       1 },
			{ TEXT("int Reset_IsNull()"),                TEXT("Reset should make TSoftClassPtr null"),              1 },
			{ TEXT("int FromTSubclassOf_Ctor()"),        TEXT("TSoftClassPtr from TSubclassOf ctor should match"), 1 },
			{ TEXT("int FromTSubclassOf_Assign()"),      TEXT("TSoftClassPtr = TSubclassOf should match"),         1 },
			{ TEXT("int OpEquals_TSubclassOf()"),         TEXT("TSoftClassPtr == TSubclassOf should be true"),      1 },
			{ TEXT("int Get_ReturnsSubclassOf_IsValid()"),TEXT("TSoftClassPtr.Get() as TSubclassOf should be valid"), 1 },
			{ TEXT("int ToSoftObjectPath_NonEmpty()"),    TEXT("ToSoftObjectPath().ToString() should not be empty"), 1 },
			{ TEXT("int GetAssetName_NonEmpty()"),        TEXT("GetAssetName() should not be empty"),                1 },
		};
		return ExpectGlobalInts(Test, Engine, Module,  Cases);
	}

	// -----------------------------------------------------------------------
	// Section: StaticClass — both plain native classes and annotated ASClass
	//
	// Three sub-modules:
	//   (a) Plain native StaticClass syntax
	//   (b) Annotated ASClass compiled from memory + reflective UFUNCTION call
	//   (c) Follow-up plain module that resolves the generated class
	// -----------------------------------------------------------------------
	bool RunStaticClassSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		bool bPassed = true;
		FNoDiscardAsserter LocalAssert(Test);

		// (a) Plain native StaticClass module.
		{
			FScopedAngelscriptModule PlainScope(Test, Engine, TEXT("ASClass_StaticClassPlain"), TEXT(R"(
int Plain_ActorIsValid()
{
	UClass C = AActor::StaticClass();
	return IsValid(C) ? 1 : 0;
}
int Plain_TSubclassOfIsValid()
{
	TSubclassOf<AActor> S = AActor::StaticClass();
	return S.IsValid() ? 1 : 0;
}
int Plain_IsChildOfSelfBoth()
{
	UClass C = AActor::StaticClass();
	TSubclassOf<AActor> S = AActor::StaticClass();
	return (C.IsChildOf(S) && S.IsChildOf(C)) ? 1 : 0;
}
int Plain_DefaultObjectIsValid()
{
	UClass C = AActor::StaticClass();
	return IsValid(C.GetDefaultObject()) ? 1 : 0;
}
)"));
			if (!PlainScope.IsValid()) return false;
			asIScriptModule& PlainModule = PlainScope.GetModule();

			const FExpectedGlobalInt PlainCases[] = {
				{ TEXT("int Plain_ActorIsValid()"),         TEXT("Plain native StaticClass should produce a valid UClass"), 1 },
				{ TEXT("int Plain_TSubclassOfIsValid()"),    TEXT("Plain TSubclassOf from StaticClass should be valid"),     1 },
				{ TEXT("int Plain_IsChildOfSelfBoth()"),     TEXT("UClass <-> TSubclassOf IsChildOf should be reflexive"),   1 },
				{ TEXT("int Plain_DefaultObjectIsValid()"),  TEXT("StaticClass GetDefaultObject should be valid"),           1 },
			};
			bPassed &= ExpectGlobalInts(Test, Engine, PlainModule,  PlainCases);
		}

		// (b) Annotated ASClass + reflective UFUNCTION call.
		// CompileAnnotatedModuleFromMemory bypasses our coverage scope; do
		// the cleanup manually after invocation.
		{
			const TCHAR* AnnotatedModuleName = TEXT("ASAnnotatedStaticClassCompat");
			const bool bAnnotatedCompiled = CompileAnnotatedModuleFromMemory(
				&Engine,
				AnnotatedModuleName,
				TEXT("ASAnnotatedStaticClassCompat.as"),
				TEXT(R"(
UCLASS()
class ABindingStaticClassActor : AActor
{
	UFUNCTION()
	int ReadStaticClassCompat()
	{
		UClass SelfClass = ABindingStaticClassActor::StaticClass();
		TSubclassOf<ABindingStaticClassActor> CompatClass = ABindingStaticClassActor::StaticClass();

		if (!IsValid(SelfClass) || !CompatClass.IsValid())
			return 10;
		if (!SelfClass.IsChildOf(GetClass()) || !GetClass().IsChildOf(SelfClass))
			return 20;
		if (!CompatClass.IsChildOf(SelfClass) || !SelfClass.IsChildOf(CompatClass))
			return 30;

		return IsValid(SelfClass.GetDefaultObject()) ? 1 : 40;
	}
}
)"));
			ON_SCOPE_EXIT { Engine.DiscardModule(AnnotatedModuleName); };

			bPassed &= LocalAssert.IsTrue(
				bAnnotatedCompiled,
				TEXT("[Class] Annotated StaticClass compat module should compile"));

			if (bAnnotatedCompiled)
			{
				UClass* RuntimeActorClass = FindGeneratedClass(&Engine, TEXT("ABindingStaticClassActor"));
				bPassed &= LocalAssert.IsNotNull(
					RuntimeActorClass,
					TEXT("[Class] Generated actor class for StaticClass compat should exist"));

				UFunction* ReadStaticClassCompatFunction = RuntimeActorClass != nullptr
					? FindGeneratedFunction(RuntimeActorClass, TEXT("ReadStaticClassCompat"))
					: nullptr;
				bPassed &= LocalAssert.IsNotNull(
					ReadStaticClassCompatFunction,
					TEXT("[Class] StaticClass compat function should exist"));

				if (RuntimeActorClass != nullptr && ReadStaticClassCompatFunction != nullptr)
				{
					AActor* RuntimeActor = NewObject<AActor>(GetTransientPackage(), RuntimeActorClass);
					bPassed &= LocalAssert.IsNotNull(
						RuntimeActor,
						TEXT("[Class] Generated actor for StaticClass compat should instantiate"));

					if (RuntimeActor != nullptr)
					{
						int32 AnnotatedResult = 0;
						const bool bExecuted = ExecuteGeneratedIntEventOnGameThread(
							&Engine, RuntimeActor, ReadStaticClassCompatFunction, AnnotatedResult);
						bPassed &= LocalAssert.IsTrue(
							bExecuted,
							TEXT("[Class] Reflected call should execute on game thread"));
						bPassed &= LocalAssert.AreEqual(
							1,
							AnnotatedResult,
							TEXT("[Class] Annotated module StaticClass should report success (1)"));
					}
				}
			}
		}

		// (c) Follow-up plain module resolves the generated class.
		{
			FScopedAngelscriptModule QueryScope(Test, Engine, TEXT("ASClass_StaticClassQuery"), TEXT(R"(
int Query_FindGeneratedIsValid()
{
	UClass C = FindClass("ABindingStaticClassActor");
	return IsValid(C) ? 1 : 0;
}
int Query_TSubclassOfFromFindIsValid()
{
	TSubclassOf<AActor> S = FindClass("ABindingStaticClassActor");
	return S.IsValid() ? 1 : 0;
}
int Query_IsChildOfActor()
{
	UClass C = FindClass("ABindingStaticClassActor");
	return C.IsChildOf(AActor::StaticClass()) ? 1 : 0;
}
int Query_TSubclassOf_IsChildOfBoth()
{
	UClass C = FindClass("ABindingStaticClassActor");
	TSubclassOf<AActor> S = FindClass("ABindingStaticClassActor");
	return (C.IsChildOf(S) && S.IsChildOf(C)) ? 1 : 0;
}
)"));
			if (!QueryScope.IsValid()) return false;
			asIScriptModule& QueryModule = QueryScope.GetModule();

			const FExpectedGlobalInt QueryCases[] = {
				{ TEXT("int Query_FindGeneratedIsValid()"),       TEXT("FindClass on generated class should be valid"),   1 },
				{ TEXT("int Query_TSubclassOfFromFindIsValid()"), TEXT("TSubclassOf from FindClass should be valid"),     1 },
				{ TEXT("int Query_IsChildOfActor()"),             TEXT("Generated class should be child of AActor"),      1 },
				{ TEXT("int Query_TSubclassOf_IsChildOfBoth()"),  TEXT("Generated class TSubclassOf round-trip"),         1 },
			};
			bPassed &= ExpectGlobalInts(Test, Engine, QueryModule,  QueryCases);
		}

		return bPassed;
	}

	// -----------------------------------------------------------------------
	// Section: NativeStaticClassNamespace — engine-level introspection,
	// no script module construction.
	// -----------------------------------------------------------------------
	bool RunNativeStaticClassNamespaceSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		Test.AddInfo(FString(TEXT("Probe AActor::StaticClass via default namespace")));

		FAngelscriptEngineScope EngineScope(Engine);
		asIScriptEngine* ScriptEngine = Engine.Engine;
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(ScriptEngine, TEXT("[Class] AS engine pointer should exist")))
		{
			return false;
		}

		bool bPassed = true;
		bPassed &= LocalAssert.IsTrue(
			ScriptEngine->SetDefaultNamespace("AActor") >= 0,
			TEXT("[Class] Set namespace to AActor should succeed"));

		asIScriptFunction* StaticClassFunction = ScriptEngine->GetGlobalFunctionByDecl("UClass StaticClass()");
		bPassed &= LocalAssert.IsNotNull(
			StaticClassFunction,
			TEXT("[Class] Native AActor namespace should expose StaticClass"));

		if (StaticClassFunction != nullptr)
		{
			bPassed &= LocalAssert.AreEqual(
				AActor::StaticClass(),
				static_cast<UClass*>(StaticClassFunction->GetUserData()),
				TEXT("[Class] StaticClass userdata should equal AActor::StaticClass()"));
		}

		bPassed &= LocalAssert.IsTrue(
			ScriptEngine->SetDefaultNamespace("") >= 0,
			TEXT("[Class] Restoring global namespace should succeed"));

		return bPassed;
	}

	// -----------------------------------------------------------------------
	// Section: NativeStaticTypeGlobal — __StaticType_AActor global symbol
	// -----------------------------------------------------------------------
	bool RunNativeStaticTypeGlobalSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASClass_StaticTypeGlobal"), TEXT(R"(
int StaticType_IsValid()
{
	return __StaticType_AActor.IsValid() ? 1 : 0;
}
int StaticType_GetMatch()
{
	return (__StaticType_AActor.Get() == AActor::StaticClass()) ? 1 : 0;
}
int StaticType_OpEqualsClass()
{
	return (__StaticType_AActor == AActor::StaticClass()) ? 1 : 0;
}
int StaticType_IsChildOfSelf()
{
	return __StaticType_AActor.IsChildOf(AActor::StaticClass()) ? 1 : 0;
}
int StaticType_DefaultObjectIsValid()
{
	return IsValid(__StaticType_AActor.GetDefaultObject()) ? 1 : 0;
}
int StaticType_RoundTrip_MatchNamespace()
{
	return (__StaticType_AActor.Get() == AActor::StaticClass()) ? 1 : 0;
}
)"));
		if (!ModuleScope.IsValid()) return false;
		asIScriptModule& Module = ModuleScope.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int StaticType_IsValid()"),              TEXT("__StaticType_AActor should be valid"),               1 },
			{ TEXT("int StaticType_GetMatch()"),             TEXT("__StaticType_AActor.Get() should equal AActor::StaticClass()"), 1 },
			{ TEXT("int StaticType_OpEqualsClass()"),         TEXT("__StaticType_AActor opEquals AActor::StaticClass()"),  1 },
			{ TEXT("int StaticType_IsChildOfSelf()"),         TEXT("__StaticType_AActor.IsChildOf(self) should be true"),  1 },
			{ TEXT("int StaticType_DefaultObjectIsValid()"),  TEXT("__StaticType_AActor.GetDefaultObject should be valid"),1 },
			{ TEXT("int StaticType_RoundTrip_MatchNamespace()"),TEXT("__StaticType_AActor.Get() should match AActor::StaticClass() round-trip"), 1 },
		};
		return ExpectGlobalInts(Test, Engine, Module,  Cases);
	}

	// -----------------------------------------------------------------------
	// Section: UClassReflection — annotated ASClass reflection methods
	//
	// Covers: GetSourceFilePath, GetScriptModuleName, GetScriptTypeDeclaration,
	//         IsFunctionImplementedInScript, FindFunctionByName, IsAbstract
	// -----------------------------------------------------------------------
	bool RunUClassReflectionSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		bool bPassed = true;
		FNoDiscardAsserter LocalAssert(Test);

		// (a) Annotated ASClass — reflection queries that require a script class.
		{
			const TCHAR* ReflModuleName = TEXT("ASAnnotatedReflectionCompat");
			const bool bCompiled = CompileAnnotatedModuleFromMemory(
				&Engine,
				ReflModuleName,
				TEXT("ASAnnotatedReflectionCompat.as"),
				TEXT(R"(
UCLASS()
class ABindingReflectionActor : AActor
{
	UFUNCTION()
	int ReadReflection()
	{
		UClass SelfClass = ABindingReflectionActor::StaticClass();

		// GetSourceFilePath — for script classes should be non-empty
		FString SourcePath = SelfClass.GetSourceFilePath();
		if (SourcePath.IsEmpty())
			return 10;

		// GetScriptModuleName — should be non-empty
		FString ModName = SelfClass.GetScriptModuleName();
		if (ModName.IsEmpty())
			return 20;

		// GetScriptTypeDeclaration — should contain class name
		FString TypeDecl = SelfClass.GetScriptTypeDeclaration();
		if (!TypeDecl.Contains("BindingReflectionActor"))
			return 30;

		// IsFunctionImplementedInScript — ReadReflection should be true
		if (!SelfClass.IsFunctionImplementedInScript(n"ReadReflection"))
			return 40;

		// FindFunctionByName — ReadReflection should not be null
		UFunction Func = SelfClass.FindFunctionByName(n"ReadReflection");
		if (Func == null)
			return 50;

		return 1;
	}
}
)"));
			ON_SCOPE_EXIT { Engine.DiscardModule(ReflModuleName); };

			bPassed &= LocalAssert.IsTrue(
				bCompiled,
				TEXT("[Class] Reflection compat module should compile"));

			if (bCompiled)
			{
				UClass* ReflActorClass = FindGeneratedClass(&Engine, TEXT("ABindingReflectionActor"));
				bPassed &= LocalAssert.IsNotNull(
					ReflActorClass,
					TEXT("[Class] Generated reflection actor class should exist"));

				UFunction* ReadReflectionFunc = ReflActorClass != nullptr
					? FindGeneratedFunction(ReflActorClass, TEXT("ReadReflection"))
					: nullptr;
				bPassed &= LocalAssert.IsNotNull(
					ReadReflectionFunc,
					TEXT("[Class] ReadReflection function should exist"));

				if (ReflActorClass != nullptr && ReadReflectionFunc != nullptr)
				{
					AActor* ReflActor = NewObject<AActor>(GetTransientPackage(), ReflActorClass);
					bPassed &= LocalAssert.IsNotNull(
						ReflActor,
						TEXT("[Class] Reflection actor should instantiate"));

					if (ReflActor != nullptr)
					{
						int32 Result = 0;
						const bool bExecuted = ExecuteGeneratedIntEventOnGameThread(
							&Engine, ReflActor, ReadReflectionFunc, Result);
						bPassed &= LocalAssert.IsTrue(
							bExecuted,
							TEXT("[Class] Reflection call should execute"));
						bPassed &= LocalAssert.AreEqual(
							1,
							Result,
							TEXT("[Class] Reflection queries should all pass (1)"));
					}
				}
			}
		}

		// (b) Plain module — IsAbstract on native classes.
		{
			FScopedAngelscriptModule AbstractScope(Test, Engine, TEXT("ASClass_IsAbstract"), TEXT(R"(
int IsAbstract_AActor_False()
{
	UClass C = AActor::StaticClass();
	return C.IsAbstract() ? 1 : 0;
}
int IsAbstract_FindAbstract()
{
	// UNavigationData is abstract in most UE builds
	UClass C = FindClass("ANavigationData");
	if (C == null)
		return -1;
	return C.IsAbstract() ? 1 : 0;
}
)"));
			if (!AbstractScope.IsValid()) return false;
			asIScriptModule& AbstractModule = AbstractScope.GetModule();

			bPassed &= ExpectGlobalInt(Test, Engine, AbstractModule, 
				TEXT("int IsAbstract_AActor_False()"),
				TEXT("AActor should NOT be abstract"),
				0);
			// IsAbstract_FindAbstract may return -1 if ANavigationData not found;
			// just verify execution succeeds (non-crash). Skip value check if -1.
			{
				FASGlobalFunctionInvoker Invoker(
					Test, Engine, AbstractModule,
					TEXT("int IsAbstract_FindAbstract()"));
				if (Invoker.IsValid())
				{
					const int32 Val = Invoker.CallAndReturn<int32>(INDEX_NONE);
					if (Val >= 0)
					{
						bPassed &= LocalAssert.AreEqual(
							1,
							Val,
							TEXT("[Class] ANavigationData should be abstract"));
					}
					else
					{
						Test.AddInfo(TEXT("[Class] ANavigationData not found — skipping IsAbstract positive test"));
					}
				}
			}
		}

		return bPassed;
	}

	// -----------------------------------------------------------------------
	// Section: TSoftClassPtrReject — exception path on unrelated class assign
	// -----------------------------------------------------------------------
	bool RunTSoftClassPtrRejectSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		const FString DiagnosticText(TEXT("Provided class is does not inherit from TSoftClassPtr subtype."));
		Test.AddExpectedErrorPlain(DiagnosticText, EAutomationExpectedErrorFlags::Contains, 0);
		Test.AddExpectedErrorPlain(
			FString(TEXT("ASClass_SoftClassReject")),
			EAutomationExpectedErrorFlags::Contains, 0);
		Test.AddExpectedErrorPlain(TEXT("TriggerBadSoftAssign"),
			EAutomationExpectedErrorFlags::Contains, 0);

		FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASClass_SoftClassReject"), TEXT(R"(
void TriggerBadSoftAssign()
{
	TSoftClassPtr<AActor> S;
	S = UPackage::StaticClass();
}
)"));
		if (!ModuleScope.IsValid()) return false;
		asIScriptModule& Module = ModuleScope.GetModule();

		return ExecuteFunctionExpectingScriptException(
			Test, Engine, Module, 
			TEXT("void TriggerBadSoftAssign()"),
			TEXT("TSoftClassPtr opAssign with unrelated class should raise exception"),
			DiagnosticText);
	}

	// -----------------------------------------------------------------------
	// Section: ClassReturnType — return type matrix coverage
	// -----------------------------------------------------------------------
	bool RunClassReturnTypeSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASClass_ReturnType"), TEXT(R"(
bool ClassRet_Bool_FindClassIsValid()
{
	UClass C = FindClass("AActor");
	return C != null;
}
bool ClassRet_Bool_IsChildOf()
{
	UClass Camera = ACameraActor::StaticClass();
	return Camera.IsChildOf(AActor::StaticClass());
}
int ClassRet_FString_ClassNameLen()
{
	UClass C = AActor::StaticClass();
	FString Name = C.GetName().ToString();
	return Name.Len();
}
int ClassRet_FString_SuperClassNameLen()
{
	UClass C = ACameraActor::StaticClass();
	UClass Super = C.GetSuperClass();
	return Super.GetName().ToString().Len();
}
int ClassRet_UClass_Echo()
{
	UClass C = AActor::StaticClass();
	return (C == AActor::StaticClass()) ? 1 : 0;
}
int ClassRet_SubclassOf_Echo()
{
	TSubclassOf<AActor> S = ACameraActor::StaticClass();
	return (S.Get() == ACameraActor::StaticClass()) ? 1 : 0;
}
)"));
		if (!ModuleScope.IsValid()) return false;
		asIScriptModule& Module = ModuleScope.GetModule();

		bool bPassed = true;
		bPassed &= ExpectGlobalReturnBool(Test, Engine, Module, 
			TEXT("bool ClassRet_Bool_FindClassIsValid()"),
			TEXT("FindClass should return valid (bool return path)"),
			true);
		bPassed &= ExpectGlobalReturnBool(Test, Engine, Module, 
			TEXT("bool ClassRet_Bool_IsChildOf()"),
			TEXT("IsChildOf should return true (bool return path)"),
			true);

		// FString return — verify via length proxy (> 0)
		bPassed &= ExpectGlobalIntAtLeast(Test, Engine, Module, 
			TEXT("int ClassRet_FString_ClassNameLen()"),
			TEXT("AActor class name length should be > 0"),
			1);
		bPassed &= ExpectGlobalIntAtLeast(Test, Engine, Module, 
			TEXT("int ClassRet_FString_SuperClassNameLen()"),
			TEXT("CameraActor super class name length should be > 0"),
			1);

		// UClass / TSubclassOf echo paths
		bPassed &= ExpectGlobalInt(Test, Engine, Module, 
			TEXT("int ClassRet_UClass_Echo()"),
			TEXT("UClass echo should round-trip"),
			1);
		bPassed &= ExpectGlobalInt(Test, Engine, Module, 
			TEXT("int ClassRet_SubclassOf_Echo()"),
			TEXT("TSubclassOf echo should round-trip"),
			1);

		return bPassed;
	}

	// -----------------------------------------------------------------------
	// Section: ClassLogDiagnostic — class string diagnostics in Log()
	// -----------------------------------------------------------------------
	bool RunClassLogDiagnosticSection(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		FScopedAngelscriptModule ModuleScope(Test, Engine, TEXT("ASClass_LogDiag"), TEXT(R"(
int ClassLog_Types()
{
	UClass C = AActor::StaticClass();
	TSubclassOf<AActor> S = AActor::StaticClass();
	TSoftClassPtr<AActor> Soft(AActor::StaticClass());

	Log("UClass: " + C);
	Log("TSubclassOf: " + S);
	Log("TSoftClassPtr: " + Soft.ToString());

	return 1;
}
)"));
		if (!ModuleScope.IsValid()) return false;
		asIScriptModule& Module = ModuleScope.GetModule();

		return ExpectGlobalInt(Test, Engine, Module, 
			TEXT("int ClassLog_Types()"),
			TEXT("Log() with class string diagnostics should succeed"),
			1);
	}
}

// ============================================================================
// Test class
// ============================================================================

TEST_CLASS_WITH_FLAGS(FAngelscriptClassBindingsTest,
	"Angelscript.TestModule.Bindings.Class",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	TEST_METHOD(ClassLookupCompat)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		RunClassLookupSection(*TestRunner, Engine);
	}

	TEST_METHOD(TSubclassOfCompat)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		RunTSubclassOfSection(*TestRunner, Engine);
	}

	TEST_METHOD(TSubclassOfRejectsUnrelatedClass)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		RunTSubclassOfRejectSection(*TestRunner, Engine);
	}

	TEST_METHOD(TSoftClassPtrCompat)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		RunTSoftClassPtrSection(*TestRunner, Engine);
	}

	TEST_METHOD(StaticClassCompat)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		RunStaticClassSection(*TestRunner, Engine);
	}

	TEST_METHOD(NativeStaticClassNamespace)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		RunNativeStaticClassNamespaceSection(*TestRunner, Engine);
	}

	TEST_METHOD(NativeStaticTypeGlobal)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		RunNativeStaticTypeGlobalSection(*TestRunner, Engine);
	}

	TEST_METHOD(UClassReflectionCompat)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		RunUClassReflectionSection(*TestRunner, Engine);
	}

	TEST_METHOD(TSoftClassPtrRejectsUnrelatedClass)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		RunTSoftClassPtrRejectSection(*TestRunner, Engine);
	}

	TEST_METHOD(ClassReturnTypeAndLogDiag)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		if (!RunClassReturnTypeSection(*TestRunner, Engine))
		{
			return;
		}
		RunClassLogDiagnosticSection(*TestRunner, Engine);
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
