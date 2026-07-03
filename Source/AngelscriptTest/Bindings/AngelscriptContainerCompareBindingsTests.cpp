// ============================================================================
// AngelscriptContainerCompareBindingsTests.cpp
//
// Container compare/debugger contract smoke. Broad container comparison
// semantics live in Coverage (`03-containers`).
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"
#include "../../AngelscriptRuntime/Binds/Bind_TMap.h"
#include "../../AngelscriptRuntime/Binds/Bind_TOptional.h"

#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptContainerCompareBindingsTest,
	"Angelscript.TestModule.Bindings.ContainerCompare",
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

	TEST_METHOD(SetAndMapCompareContractSmoke)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASContainerCompare_ContractSmoke"), ASTEST_AS(R"AS(
			int VerifySetAndMapCompareContractSmoke()
			{
				TSet<int> LeftSet;
				LeftSet.Add(1);
				LeftSet.Add(4);

				TSet<int> RightSet;
				RightSet.Add(4);
				RightSet.Add(1);

				TMap<FName, int> LeftMap;
				LeftMap.Add(FName("Alpha"), 2);
				LeftMap.Add(FName("Beta"), 5);

				TMap<FName, int> RightMap;
				RightMap.Add(FName("Beta"), 5);
				RightMap.Add(FName("Alpha"), 2);

				TMap<FName, int> DifferentMap = RightMap;
				DifferentMap.Add(FName("Alpha"), 99);

				return (LeftSet == RightSet)
					&& (LeftMap == RightMap)
					&& !(LeftMap == DifferentMap) ? 1 : 0;
			}
			)AS"));
		if (!ModuleScope.IsValid()) return;

		ASSERT_THAT(IsTrue(ExpectGlobalInt(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("int VerifySetAndMapCompareContractSmoke()"),
			TEXT("TSet and TMap compare bindings should dispatch"),
			1)));
	}

	TEST_METHOD(OptionalTypeCompare)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FAngelscriptTypeUsage IntUsage(FAngelscriptType::GetByAngelscriptTypeName(TEXT("int")));
		FAngelscriptTypeUsage OptionalUsage(FAngelscriptType::GetByAngelscriptTypeName(TEXT("TOptional")));
		OptionalUsage.SubTypes.Add(IntUsage);

		ASSERT_THAT(IsTrue(OptionalUsage.CanCompare(), TEXT("TOptional<int> should report type-level compare support")));

		const SIZE_T OptionalSize = static_cast<SIZE_T>(OptionalUsage.GetValueSize());
		const uint32 OptionalAlignment = static_cast<uint32>(OptionalUsage.GetValueAlignment());

		void* LeftStorage = FMemory::Malloc(OptionalSize, OptionalAlignment);
		void* RightStorage = FMemory::Malloc(OptionalSize, OptionalAlignment);
		ON_SCOPE_EXIT
		{
			FMemory::Free(LeftStorage);
			FMemory::Free(RightStorage);
		};

		OptionalUsage.ConstructValue(LeftStorage);
		OptionalUsage.ConstructValue(RightStorage);

		FOptionalOperations OptionalOps(IntUsage);
		FAngelscriptOptional& LeftOptional = *static_cast<FAngelscriptOptional*>(LeftStorage);
		FAngelscriptOptional& RightOptional = *static_cast<FAngelscriptOptional*>(RightStorage);

		ASSERT_THAT(IsTrue(OptionalUsage.IsValueEqual(&LeftOptional, &RightOptional), TEXT("Two unset optionals should compare equal")));

		int32 LeftValue = 7;
		int32 RightValue = 7;
		OptionalOps.Set(LeftOptional, &LeftValue);
		OptionalOps.Set(RightOptional, &RightValue);
		ASSERT_THAT(IsTrue(OptionalUsage.IsValueEqual(&LeftOptional, &RightOptional), TEXT("Two equal set optionals should compare equal")));

		RightValue = 9;
		OptionalOps.Set(RightOptional, &RightValue);
		ASSERT_THAT(IsFalse(OptionalUsage.IsValueEqual(&LeftOptional, &RightOptional), TEXT("Different set optionals should compare unequal")));

		OptionalOps.Reset(RightOptional);
		const bool bSetVsUnsetEqual = OptionalUsage.IsValueEqual(&LeftOptional, &RightOptional);
		OptionalUsage.DestructValue(LeftStorage);
		OptionalUsage.DestructValue(RightStorage);

		ASSERT_THAT(IsFalse(bSetVsUnsetEqual, TEXT("Set and unset optionals should compare unequal")));
	}

	TEST_METHOD(MapDebugger)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FAngelscriptTypeUsage KeyUsage(FAngelscriptType::GetByAngelscriptTypeName(TEXT("FName")));
		FAngelscriptTypeUsage ValueUsage(FAngelscriptType::GetByAngelscriptTypeName(TEXT("int")));
		FAngelscriptTypeUsage MapUsage(FAngelscriptType::GetByAngelscriptTypeName(TEXT("TMap")));
		MapUsage.SubTypes.Add(KeyUsage);
		MapUsage.SubTypes.Add(ValueUsage);
		MapUsage.TypeIndex = 0;

		FScriptMap TestMap;
		FMapOperations MapOps(KeyUsage, ValueUsage);
		FName AlphaName(TEXT("Alpha"));
		FName BetaName(TEXT("Beta"));
		int32 AlphaValue = 2;
		int32 BetaValue = 5;
		MapOps.Add(TestMap, &AlphaName, &AlphaValue);
		MapOps.Add(TestMap, &BetaName, &BetaValue);

		FDebuggerValue SummaryValue;
		if (!this->Assert.IsTrue(MapUsage.GetDebuggerValue(&TestMap, SummaryValue), TEXT("TMap debugger summary should be available")))
		{
			MapOps.Empty(TestMap, 0);
			return;
		}
		ASSERT_THAT(AreEqual(FString(TEXT("Num = 2")), SummaryValue.Value, TEXT("TMap debugger summary should show element count")));
		ASSERT_THAT(IsTrue(SummaryValue.bHasMembers, TEXT("TMap debugger summary should report child members")));

		FDebuggerValue NumValue;
		if (!this->Assert.IsTrue(MapUsage.GetDebuggerMember(&TestMap, TEXT("Num"), NumValue), TEXT("TMap debugger should expose Num member")))
		{
			MapOps.Empty(TestMap, 0);
			return;
		}
		ASSERT_THAT(AreEqual(FString(TEXT("2")), NumValue.Value, TEXT("TMap debugger Num member should match element count")));

		FDebuggerValue AlphaDebugValue;
		const bool bAlphaFound = MapUsage.GetDebuggerMember(&TestMap, TEXT("[Alpha]"), AlphaDebugValue);
		MapOps.Empty(TestMap, 0);
		ASSERT_THAT(IsTrue(bAlphaFound, TEXT("TMap debugger should expose FName-keyed members by string identifier")));

		ASSERT_THAT(AreEqual(FString(TEXT("2")), AlphaDebugValue.Value, TEXT("TMap debugger key lookup should return the mapped value")));
	}
};

#endif
