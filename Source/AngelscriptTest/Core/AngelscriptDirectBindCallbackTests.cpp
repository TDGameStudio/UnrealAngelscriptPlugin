#include "CQTest.h"

#include "Core/AngelscriptBinds.h"
#include "Core/AngelscriptBindsInternal.h"
#include "AngelscriptTestEngine.h"
#include "Testing/AngelscriptBindExecutionObservation.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace
{
	using UE::Angelscript::Private::FAngelscriptBindCollection;
	using ::FAngelscriptBindRecord;

	static int32 CallbackInvocationCount = 0;
	static int32 LaterProviderInvocationCount = 0;
	static TArray<FName> CallbackSequence;

	static int32 CDECL ReturnFailureFixtureValue()
	{
		return 7;
	}

	static int32 CDECL ReturnPostFailureFixtureValue()
	{
		return 11;
	}

	static void RegisterDuplicateGlobalFunction(FAngelscriptBinds& Binds)
	{
		Binds.BindGlobalFunctionForTarget("int DirectBindFailureFixture()", &ReturnFailureFixtureValue);
		Binds.BindGlobalFunctionForTarget("int DirectBindFailureFixture()", &ReturnFailureFixtureValue);
		Binds.BindGlobalFunctionForTarget("int DirectBindPostFailureFixture()", &ReturnPostFailureFixtureValue);
	}

	static void RecordLaterProvider(FAngelscriptBinds& Binds)
	{
		(void)Binds;
		++LaterProviderInvocationCount;
	}

	static void RecordInvocation(FAngelscriptBinds& Binds)
	{
		(void)Binds;
		++CallbackInvocationCount;
	}

	static void RecordAlpha(FAngelscriptBinds& Binds)
	{
		(void)Binds;
		CallbackSequence.Add(TEXT("Alpha"));
	}

	static void RecordBeta(FAngelscriptBinds& Binds)
	{
		(void)Binds;
		CallbackSequence.Add(TEXT("Beta"));
	}

	static void RecordGamma(FAngelscriptBinds& Binds)
	{
		(void)Binds;
		CallbackSequence.Add(TEXT("Gamma"));
	}

	static FAngelscriptBindRecord MakeRecord(
		const TCHAR* OwnerModule,
		const TCHAR* BindName,
		const EAngelscriptBindPhase Phase,
		FAngelscriptBindCallback Callback,
		const ANSICHAR* SourceFile,
		const int32 SourceLine)
	{
		FAngelscriptBindRecord Record;
		Record.OwnerModule = FName(OwnerModule);
		Record.BindName = FName(BindName);
		Record.Phase = Phase;
		Record.SourceFile = SourceFile;
		Record.SourceLine = SourceLine;
		Record.Callback = Callback;
		return Record;
	}

	static bool LoadRuntimeSource(const TCHAR* RelativePath, FString& OutSource)
	{
		return FFileHelper::LoadFileToString(
			OutSource,
			*FPaths::Combine(
				FPaths::ProjectPluginsDir(),
				TEXT("Angelscript/Source/AngelscriptRuntime"),
				RelativePath));
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptDirectBindCallbackCollectionTests,
	"Angelscript.TestModule.Engine.BindingArchitecture.Collection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ConstructionCapturesMetadataWithoutExecutingCallback)
	{
		FAngelscriptBindCollection Collection;
		CallbackInvocationCount = 0;
		const int32 FirstLine = __LINE__ + 1;
		FAngelscriptBind FirstBind(Collection, TEXT("First"), EAngelscriptBindPhase::ExplicitBindings, &RecordInvocation);
		const int32 SecondLine = __LINE__ + 1;
		FAngelscriptBind SecondBind(Collection, TEXT("Second"), EAngelscriptBindPhase::Finalization, &RecordInvocation);

		const TConstArrayView<FAngelscriptBindRecord> Records = Collection.GetRecords();
		bool bPassed = true;
		bPassed &= TestRunner->TestEqual(TEXT("Two binds declared in one source file should append two records"), Records.Num(), 2);
		if (Records.Num() != 2)
		{
			return;
		}

		bPassed &= TestRunner->TestEqual(TEXT("The first logical bind name should be retained"), Records[0].BindName, FName(TEXT("First")));
		bPassed &= TestRunner->TestEqual(TEXT("The second logical bind name should be retained"), Records[1].BindName, FName(TEXT("Second")));
		bPassed &= TestRunner->TestEqual(TEXT("The declaring module should be captured automatically"), Records[0].OwnerModule, FName(TEXT("AngelscriptTest")));
		bPassed &= TestRunner->TestEqual(TEXT("The declared phase should be retained"), Records[0].Phase, EAngelscriptBindPhase::ExplicitBindings);
		bPassed &= TestRunner->TestTrue(TEXT("The declaring source file should be captured automatically"), FString(ANSI_TO_TCHAR(Records[0].SourceFile)).EndsWith(TEXT("AngelscriptDirectBindCallbackTests.cpp")));
		bPassed &= TestRunner->TestEqual(TEXT("The first source line should identify its declaration"), Records[0].SourceLine, FirstLine);
		bPassed &= TestRunner->TestEqual(TEXT("The second source line should identify its declaration"), Records[1].SourceLine, SecondLine);
		bPassed &= TestRunner->TestTrue(TEXT("The callback should be stored as a process-lifetime function pointer"), Records[0].Callback == &RecordInvocation);
		bPassed &= TestRunner->TestEqual(TEXT("Static metadata construction must not execute the callback"), CallbackInvocationCount, 0);
		TestRunner->TestTrue(TEXT("Direct bind metadata capture should pass"), bPassed);
	}

	TEST_METHOD(FinalizationRejectsMissingRequiredMetadata)
	{
		FAngelscriptBindCollection Collection;
		FAngelscriptBind MissingName(Collection, NAME_None, EAngelscriptBindPhase::ExplicitBindings, &RecordInvocation, "TestOwner", "MissingName.cpp", 17);
		FAngelscriptBind MissingCallback(Collection, TEXT("MissingCallback"), EAngelscriptBindPhase::ExplicitBindings, nullptr, "TestOwner", "MissingCallback.cpp", 29);
		FAngelscriptBind InvalidPhase(Collection, TEXT("InvalidPhase"), static_cast<EAngelscriptBindPhase>(255), &RecordInvocation, "TestOwner", "InvalidPhase.cpp", 41);

		FString Diagnostic;
		bool bPassed = true;
		bPassed &= TestRunner->TestFalse(TEXT("A collection with invalid records must fail finalization"), Collection.Finalize(Diagnostic));
		bPassed &= TestRunner->TestTrue(TEXT("The diagnostic should identify the first invalid record owner"), Diagnostic.Contains(TEXT("TestOwner")));
		bPassed &= TestRunner->TestTrue(TEXT("The diagnostic should identify missing logical-name metadata"), Diagnostic.Contains(TEXT("bind name")));
		bPassed &= TestRunner->TestTrue(TEXT("A failed finalization must not seal the collection"), !Collection.IsSealed());
		bPassed &= TestRunner->TestEqual(TEXT("Validation must not execute callbacks"), CallbackInvocationCount, 0);
		TestRunner->TestTrue(TEXT("Direct bind validation should fail closed"), bPassed);
	}

	TEST_METHOD(FinalizationSortsByPhaseAndStableMetadata)
	{
		FAngelscriptBindCollection Collection;
		FString Diagnostic;
		Collection.Append(MakeRecord(TEXT("Zulu"), TEXT("Same"), EAngelscriptBindPhase::ExplicitBindings, &RecordInvocation, "Z.cpp", 10), Diagnostic);
		Collection.Append(MakeRecord(TEXT("Alpha"), TEXT("Zulu"), EAngelscriptBindPhase::ExplicitBindings, &RecordInvocation, "A.cpp", 30), Diagnostic);
		Collection.Append(MakeRecord(TEXT("Alpha"), TEXT("Alpha"), EAngelscriptBindPhase::Finalization, &RecordInvocation, "A.cpp", 10), Diagnostic);
		Collection.Append(MakeRecord(TEXT("Alpha"), TEXT("Charlie"), EAngelscriptBindPhase::ExplicitBindings, &RecordInvocation, "B.cpp", 20), Diagnostic);
		Collection.Append(MakeRecord(TEXT("Alpha"), TEXT("Alpha"), EAngelscriptBindPhase::TypeDeclarations, &RecordInvocation, "C.cpp", 40), Diagnostic);
		Collection.Append(MakeRecord(TEXT("Alpha"), TEXT("Bravo"), EAngelscriptBindPhase::ExplicitBindings, &RecordInvocation, "A.cpp", 50), Diagnostic);
		Collection.Append(MakeRecord(TEXT("Alpha"), TEXT("Alpha"), EAngelscriptBindPhase::ExplicitBindings, &RecordInvocation, "A.cpp", 10), Diagnostic);

		bool bPassed = true;
		bPassed &= TestRunner->TestTrue(TEXT("A valid collection should finalize"), Collection.Finalize(Diagnostic));
		bPassed &= TestRunner->TestTrue(TEXT("Successful finalization should clear diagnostics"), Diagnostic.IsEmpty());
		bPassed &= TestRunner->TestTrue(TEXT("Finalization should be idempotent"), Collection.Finalize(Diagnostic));
		const TConstArrayView<FAngelscriptBindRecord> Records = Collection.GetRecords();
		if (!TestRunner->TestEqual(TEXT("All sortable records should remain present"), Records.Num(), 7))
		{
			return;
		}

		bPassed &= TestRunner->TestEqual(TEXT("Phase should be the primary sort key"), Records[0].Phase, EAngelscriptBindPhase::TypeDeclarations);
		bPassed &= TestRunner->TestEqual(TEXT("Owner module should be the first same-phase key"), Records[1].OwnerModule, FName(TEXT("Alpha")));
		bPassed &= TestRunner->TestEqual(TEXT("Bind name should be the second same-phase key"), Records[1].BindName, FName(TEXT("Alpha")));
		bPassed &= TestRunner->TestTrue(TEXT("Source provenance should remain attached after sorting"), FCStringAnsi::Strcmp(Records[1].SourceFile, "A.cpp") == 0 && Records[1].SourceLine == 10);
		bPassed &= TestRunner->TestEqual(TEXT("The next logical bind name should follow"), Records[2].BindName, FName(TEXT("Bravo")));
		bPassed &= TestRunner->TestEqual(TEXT("Logical bind names should sort lexically within an owner"), Records[3].BindName, FName(TEXT("Charlie")));
		bPassed &= TestRunner->TestEqual(TEXT("Bind-name ordering should precede owner-module changes"), Records[4].BindName, FName(TEXT("Zulu")));
		bPassed &= TestRunner->TestEqual(TEXT("Owner-module ordering should be deterministic"), Records[5].OwnerModule, FName(TEXT("Zulu")));
		bPassed &= TestRunner->TestEqual(TEXT("Finalization phase should execute last"), Records[6].Phase, EAngelscriptBindPhase::Finalization);
		TestRunner->TestTrue(TEXT("Direct bind ordering should be deterministic"), bPassed);
	}

	TEST_METHOD(DuplicateIdentityAndLateAppendFailClosed)
	{
		FAngelscriptBindCollection DuplicateCollection;
		FString Diagnostic;
		DuplicateCollection.Append(MakeRecord(TEXT("Owner"), TEXT("Duplicate"), EAngelscriptBindPhase::ExplicitBindings, &RecordInvocation, "Second.cpp", 20), Diagnostic);
		DuplicateCollection.Append(MakeRecord(TEXT("Owner"), TEXT("Duplicate"), EAngelscriptBindPhase::ExplicitBindings, &RecordInvocation, "First.cpp", 20), Diagnostic);
		DuplicateCollection.Append(MakeRecord(TEXT("Owner"), TEXT("Duplicate"), EAngelscriptBindPhase::ExplicitBindings, &RecordInvocation, "First.cpp", 10), Diagnostic);

		bool bPassed = true;
		bPassed &= TestRunner->TestFalse(TEXT("Duplicate stable identities must fail finalization"), DuplicateCollection.Finalize(Diagnostic));
		bPassed &= TestRunner->TestTrue(TEXT("Duplicate diagnostics should identify the owner"), Diagnostic.Contains(TEXT("Owner")));
		bPassed &= TestRunner->TestTrue(TEXT("Duplicate diagnostics should identify the logical bind name"), Diagnostic.Contains(TEXT("Duplicate")));
		bPassed &= TestRunner->TestTrue(TEXT("Duplicate diagnostics should identify the phase"), Diagnostic.Contains(TEXT("ExplicitBindings")));
		bPassed &= TestRunner->TestTrue(TEXT("Duplicate diagnostics should include both declaration sites"), Diagnostic.Contains(TEXT("First.cpp:10")) && Diagnostic.Contains(TEXT("First.cpp:20")));
		const TConstArrayView<FAngelscriptBindRecord> DuplicateRecords = DuplicateCollection.GetRecords();
		bPassed &= TestRunner->TestTrue(TEXT("Source file and line should deterministically order duplicate diagnostics"), DuplicateRecords.Num() == 3 && FCStringAnsi::Strcmp(DuplicateRecords[0].SourceFile, "First.cpp") == 0 && DuplicateRecords[0].SourceLine == 10 && DuplicateRecords[1].SourceLine == 20 && FCStringAnsi::Strcmp(DuplicateRecords[2].SourceFile, "Second.cpp") == 0);
		bPassed &= TestRunner->TestFalse(TEXT("A duplicate collection must remain unsealed"), DuplicateCollection.IsSealed());

		FAngelscriptBindCollection SealedCollection;
		SealedCollection.Append(MakeRecord(TEXT("Owner"), TEXT("Initial"), EAngelscriptBindPhase::ExplicitBindings, &RecordInvocation, "Initial.cpp", 30), Diagnostic);
		bPassed &= TestRunner->TestTrue(TEXT("The valid collection should seal"), SealedCollection.Finalize(Diagnostic));
		const void* const BackingData = SealedCollection.GetRecords().GetData();
		bPassed &= TestRunner->TestFalse(TEXT("Appending after finalization must fail"), SealedCollection.Append(MakeRecord(TEXT("LateOwner"), TEXT("LateBind"), EAngelscriptBindPhase::GeneratedBindings, &RecordInvocation, "Late.cpp", 40), Diagnostic));
		bPassed &= TestRunner->TestTrue(TEXT("Late-append diagnostics should require restart"), Diagnostic.Contains(TEXT("restart")));
		bPassed &= TestRunner->TestTrue(TEXT("Late-append diagnostics should retain all provenance"), Diagnostic.Contains(TEXT("LateOwner")) && Diagnostic.Contains(TEXT("LateBind")) && Diagnostic.Contains(TEXT("GeneratedBindings")) && Diagnostic.Contains(TEXT("Late.cpp:40")));
		bPassed &= TestRunner->TestEqual(TEXT("Late append must not mutate the sealed collection"), SealedCollection.GetRecords().Num(), 1);
		bPassed &= TestRunner->TestTrue(TEXT("Late append must not replace the sealed backing array"), SealedCollection.GetRecords().GetData() == BackingData);
		TestRunner->TestTrue(TEXT("Duplicate and late direct binds should fail closed"), bPassed);
	}

	TEST_METHOD(RepeatedExecutionReusesTheSealedBackingCollection)
	{
		FAngelscriptBindCollection Collection;
		FString Diagnostic;
		Collection.Append(MakeRecord(TEXT("Owner"), TEXT("Gamma"), EAngelscriptBindPhase::Finalization, &RecordGamma, "C.cpp", 30), Diagnostic);
		Collection.Append(MakeRecord(TEXT("Owner"), TEXT("Beta"), EAngelscriptBindPhase::ExplicitBindings, &RecordBeta, "B.cpp", 20), Diagnostic);
		Collection.Append(MakeRecord(TEXT("Owner"), TEXT("Alpha"), EAngelscriptBindPhase::TypeDeclarations, &RecordAlpha, "A.cpp", 10), Diagnostic);
		if (!TestRunner->TestTrue(TEXT("The execution fixture should finalize"), Collection.Finalize(Diagnostic)))
		{
			return;
		}

		const void* const BackingData = Collection.GetRecords().GetData();
		FAngelscriptBinds Binds(FAngelscriptTestEngine::GetSharedEngine());
		CallbackSequence.Reset();
		bool bPassed = true;
		bPassed &= TestRunner->TestTrue(TEXT("The first engine pass should execute"), Collection.Execute(Binds, Diagnostic));
		bPassed &= TestRunner->TestTrue(TEXT("The first engine pass should use phase order"), CallbackSequence == TArray<FName>({TEXT("Alpha"), TEXT("Beta"), TEXT("Gamma")}));
		CallbackSequence.Reset();
		bPassed &= TestRunner->TestTrue(TEXT("The second engine pass should execute"), Collection.Execute(Binds, Diagnostic));
		bPassed &= TestRunner->TestTrue(TEXT("The second engine pass should use the identical order"), CallbackSequence == TArray<FName>({TEXT("Alpha"), TEXT("Beta"), TEXT("Gamma")}));
		bPassed &= TestRunner->TestTrue(TEXT("Repeated execution should retain the same backing collection"), Collection.GetRecords().GetData() == BackingData);
		TestRunner->TestTrue(TEXT("Direct bind replay should reuse immutable metadata"), bPassed);
	}

	TEST_METHOD(LateRegistrationFailureRemainsStickyAndBlocksFutureEnginePreparation)
	{
		FAngelscriptBindCollection Collection;
		FString Diagnostic;
		Collection.Append(
			MakeRecord(
				TEXT("Owner"),
				TEXT("Initial"),
				EAngelscriptBindPhase::ExplicitBindings,
				&RecordInvocation,
				"Initial.cpp",
				30),
			Diagnostic);
		if (!TestRunner->TestTrue(TEXT("The late-registration fixture should seal"), Collection.Finalize(Diagnostic)))
		{
			return;
		}

		const void* const BackingData = Collection.GetRecords().GetData();
		const bool bLateAppendAccepted = Collection.Append(
			MakeRecord(
				TEXT("LateOwner"),
				TEXT("LateBind"),
				EAngelscriptBindPhase::GeneratedBindings,
				&RecordInvocation,
				"Late.cpp",
				40),
			Diagnostic);
		const FString FirstLateDiagnostic = Diagnostic;

		int32 ModuleLoadCount = 0;
		const TArray<FString> ModuleNames = {TEXT("MustNotLoad")};
		const bool bPrepared = Collection.PrepareForEngineInitialization(
			ModuleNames,
			[&ModuleLoadCount](const FName, FString&)
			{
				++ModuleLoadCount;
				return true;
			},
			Diagnostic);

		FAngelscriptBinds Binds(FAngelscriptTestEngine::GetSharedEngine());
		CallbackInvocationCount = 0;
		FString ReplayDiagnostic;
		const bool bReplayed = Collection.Execute(Binds, ReplayDiagnostic);

		bool bPassed = true;
		bPassed &= TestRunner->TestFalse(TEXT("A bind declared after sealing must be rejected"), bLateAppendAccepted);
		bPassed &= TestRunner->TestFalse(TEXT("A remembered late bind must block future engine preparation"), bPrepared);
		bPassed &= TestRunner->TestEqual(TEXT("Sticky late failure must stop generated-module loading"), ModuleLoadCount, 0);
		bPassed &= TestRunner->TestFalse(TEXT("A remembered late bind must block future callback replay"), bReplayed);
		bPassed &= TestRunner->TestEqual(TEXT("Blocked replay must not invoke an existing callback"), CallbackInvocationCount, 0);
		bPassed &= TestRunner->TestTrue(
			TEXT("Preparation should preserve the first restart-required late-bind diagnostic"),
			Diagnostic == FirstLateDiagnostic
				&& Diagnostic.Contains(TEXT("restart"))
				&& Diagnostic.Contains(TEXT("LateOwner"))
				&& Diagnostic.Contains(TEXT("LateBind"))
				&& Diagnostic.Contains(TEXT("Late.cpp:40")));
		bPassed &= TestRunner->TestEqual(
			TEXT("Replay should report the same first late-bind failure"),
			ReplayDiagnostic,
			FirstLateDiagnostic);
		bPassed &= TestRunner->TestTrue(
			TEXT("Sticky failure must not mutate or replace the sealed callback backing"),
			Collection.GetRecords().Num() == 1 && Collection.GetRecords().GetData() == BackingData);
		TestRunner->TestTrue(TEXT("Late direct-bind registration should remain fail-closed until restart"), bPassed);
	}

	TEST_METHOD(PhaseWindowsExecuteEachCallbackExactlyOnceAcrossMixedMigration)
	{
		FAngelscriptBindCollection Collection;
		FString Diagnostic;
		Collection.Append(MakeRecord(TEXT("Owner"), TEXT("Gamma"), EAngelscriptBindPhase::Finalization, &RecordGamma, "C.cpp", 30), Diagnostic);
		Collection.Append(MakeRecord(TEXT("Owner"), TEXT("Beta"), EAngelscriptBindPhase::ExplicitBindings, &RecordBeta, "B.cpp", 20), Diagnostic);
		Collection.Append(MakeRecord(TEXT("Owner"), TEXT("Alpha"), EAngelscriptBindPhase::TypeDeclarations, &RecordAlpha, "A.cpp", 10), Diagnostic);
		if (!TestRunner->TestTrue(TEXT("The phase-window fixture should finalize"), Collection.Finalize(Diagnostic)))
		{
			return;
		}

		FAngelscriptBinds Binds(FAngelscriptTestEngine::GetSharedEngine());
		CallbackSequence.Reset();
		bool bPassed = true;
		bPassed &= TestRunner->TestTrue(
			TEXT("The early phase window should execute"),
			Collection.Execute(Binds, EAngelscriptBindPhase::TypeDeclarations, EAngelscriptBindPhase::TypeInfrastructure, Diagnostic));
		bPassed &= TestRunner->TestTrue(
			TEXT("Only type-declaration callbacks should run before legacy compatibility callbacks"),
			CallbackSequence == TArray<FName>({TEXT("Alpha")}));
		bPassed &= TestRunner->TestTrue(
			TEXT("The late phase window should execute"),
			Collection.Execute(Binds, EAngelscriptBindPhase::ExplicitBindings, EAngelscriptBindPhase::Finalization, Diagnostic));
		bPassed &= TestRunner->TestTrue(
			TEXT("The two non-overlapping windows should execute every callback exactly once and in phase order"),
			CallbackSequence == TArray<FName>({TEXT("Alpha"), TEXT("Beta"), TEXT("Gamma")}));
		bPassed &= TestRunner->TestFalse(
			TEXT("An inverted phase window must fail closed"),
			Collection.Execute(Binds, EAngelscriptBindPhase::Finalization, EAngelscriptBindPhase::ExplicitBindings, Diagnostic));
		bPassed &= TestRunner->TestTrue(TEXT("The inverted-window diagnostic should identify the invalid range"), Diagnostic.Contains(TEXT("inverted")));
		TestRunner->TestTrue(TEXT("Direct bind phase-window replay should support mixed migration"), bPassed);
	}

	TEST_METHOD(RegistrationFailureStopsLaterProvidersAndRetainsContext)
	{
		FAngelscriptBindCollection Collection;
		FString Diagnostic;
		Collection.Append(MakeRecord(TEXT("FailureOwner"), TEXT("FailureProvider"), EAngelscriptBindPhase::ExplicitBindings, &RegisterDuplicateGlobalFunction, "FailureProvider.cpp", 73), Diagnostic);
		Collection.Append(MakeRecord(TEXT("LaterOwner"), TEXT("LaterProvider"), EAngelscriptBindPhase::Finalization, &RecordLaterProvider, "LaterProvider.cpp", 91), Diagnostic);
		if (!TestRunner->TestTrue(TEXT("The failure fixture should finalize"), Collection.Finalize(Diagnostic)))
		{
			return;
		}

		const FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = FAngelscriptTestEngine::Create(Config, Dependencies);
		if (!TestRunner->TestTrue(TEXT("The failure fixture should create an explicit engine"), Engine.IsValid()))
		{
			return;
		}

		FAngelscriptBinds Binds(*Engine);
		LaterProviderInvocationCount = 0;
		TestRunner->AddExpectedErrorPlain(
			TEXT("Failed in call to function 'RegisterGlobalFunction' with 'int DirectBindFailureFixture()'"),
			EAutomationExpectedErrorFlags::Contains,
			1);
		bool bPassed = true;
		bPassed &= TestRunner->TestFalse(TEXT("A rejected direct registration should fail collection execution"), Collection.Execute(Binds, Diagnostic));
		bPassed &= TestRunner->TestEqual(TEXT("Providers after the first failure must not execute"), LaterProviderInvocationCount, 0);
		bPassed &= TestRunner->TestTrue(
			TEXT("Registrations later in the failing provider must not mutate the engine"),
			Engine->GetScriptEngine()->GetGlobalFunctionByDecl("int DirectBindPostFailureFixture()") == nullptr);
		bPassed &= TestRunner->TestTrue(TEXT("The first failure should identify the provider"), Diagnostic.Contains(TEXT("FailureOwner")) && Diagnostic.Contains(TEXT("FailureProvider")));
		bPassed &= TestRunner->TestTrue(TEXT("The first failure should identify phase and source"), Diagnostic.Contains(TEXT("ExplicitBindings")) && Diagnostic.Contains(TEXT("FailureProvider.cpp:73")));
		bPassed &= TestRunner->TestTrue(TEXT("The first failure should retain the rejected declaration"), Diagnostic.Contains(TEXT("DirectBindFailureFixture")));
		TestRunner->TestTrue(TEXT("Direct bind registration failures should stop execution with actionable context"), bPassed);
	}

	TEST_METHOD(CollectionFinalizationIsObservedOnceWithExactSevenPhaseTotals)
	{
		FAngelscriptBindExecutionObservation::Reset();
		FAngelscriptBindCollection Collection;
		FString Diagnostic;
		static const EAngelscriptBindPhase ReversePhases[] = {
			EAngelscriptBindPhase::Finalization,
			EAngelscriptBindPhase::PostReflectionBindings,
			EAngelscriptBindPhase::ReflectionBindings,
			EAngelscriptBindPhase::GeneratedBindings,
			EAngelscriptBindPhase::ExplicitBindings,
			EAngelscriptBindPhase::TypeInfrastructure,
			EAngelscriptBindPhase::TypeDeclarations,
		};
		for (int32 PhaseIndex = 0; PhaseIndex < UE_ARRAY_COUNT(ReversePhases); ++PhaseIndex)
		{
			Collection.Append(
				MakeRecord(
					TEXT("ObservationOwner"),
					*FString::Printf(TEXT("Provider%d"), PhaseIndex),
					ReversePhases[PhaseIndex],
					&RecordInvocation,
					"Observation.cpp",
					10 + PhaseIndex),
				Diagnostic);
		}

		ASSERT_THAT(IsTrue(Collection.Finalize(Diagnostic), TEXT("The seven-phase observation fixture should finalize")));
		ASSERT_THAT(IsTrue(Collection.Finalize(Diagnostic), TEXT("Repeated finalization should remain idempotent")));
		const FAngelscriptBindExecutionSnapshot FirstRead =
			FAngelscriptBindExecutionObservation::GetLastSnapshot();
		const FAngelscriptBindExecutionSnapshot SecondRead =
			FAngelscriptBindExecutionObservation::GetLastSnapshot();

		bool bPassed = true;
		bPassed &= TestRunner->TestEqual(
			TEXT("Only the real collection finalization should be observed"),
			FirstRead.CollectionFinalizationCount,
			1);
		bPassed &= TestRunner->TestTrue(
			TEXT("Successful finalization should expose the sealed provider count"),
			FirstRead.bCollectionFinalized && FirstRead.FinalizedProviderCount == 7);
		bPassed &= TestRunner->TestEqual(
			TEXT("Collection observation should expose all seven phase totals"),
			FirstRead.CollectionPhaseProviderCounts.Num(),
			7);
		for (int32 PhaseIndex = 0; PhaseIndex < FirstRead.CollectionPhaseProviderCounts.Num(); ++PhaseIndex)
		{
			bPassed &= TestRunner->TestEqual(
				*FString::Printf(TEXT("Finalized phase %d should contain exactly one provider"), PhaseIndex),
				FirstRead.CollectionPhaseProviderCounts[PhaseIndex],
				1);
		}
		bPassed &= TestRunner->TestTrue(
			TEXT("Repeated observation reads should not mutate or rebuild collection data"),
			SecondRead.CollectionFinalizationCount == FirstRead.CollectionFinalizationCount
				&& SecondRead.FinalizedProviderOrder == FirstRead.FinalizedProviderOrder
				&& SecondRead.CollectionPhaseProviderCounts == FirstRead.CollectionPhaseProviderCounts);
		TestRunner->TestTrue(TEXT("One-time collection finalization observation should be deterministic"), bPassed);
	}

	TEST_METHOD(PerEngineExecutionRecordsProvidersPhaseTotalsEpochAndTopNLines)
	{
		const FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> EngineA = FAngelscriptTestEngine::Create(Config, Dependencies);
		TUniquePtr<FAngelscriptEngine> EngineB = FAngelscriptTestEngine::Create(Config, Dependencies);
		ASSERT_THAT(IsTrue(EngineA.IsValid() && EngineB.IsValid(), TEXT("The per-engine observation test should create two engines")));

		FAngelscriptBindExecutionObservation::Reset();
		FAngelscriptBindCollection Collection;
		FString Diagnostic;
		Collection.Append(MakeRecord(TEXT("Owner"), TEXT("Type"), EAngelscriptBindPhase::TypeDeclarations, &RecordAlpha, "A.cpp", 10), Diagnostic);
		Collection.Append(MakeRecord(TEXT("Owner"), TEXT("Manual"), EAngelscriptBindPhase::ExplicitBindings, &RecordBeta, "B.cpp", 20), Diagnostic);
		Collection.Append(MakeRecord(TEXT("Owner"), TEXT("Final"), EAngelscriptBindPhase::Finalization, &RecordGamma, "C.cpp", 30), Diagnostic);
		ASSERT_THAT(IsTrue(Collection.Finalize(Diagnostic), TEXT("The per-engine observation fixture should finalize")));

		FAngelscriptBinds BindsA(*EngineA);
		ASSERT_THAT(IsTrue(Collection.Execute(BindsA, Diagnostic), TEXT("Engine A should execute the observed callbacks")));
		const FAngelscriptBindExecutionSnapshot SnapshotA =
			FAngelscriptBindExecutionObservation::GetLastSnapshot();
		FAngelscriptBinds BindsB(*EngineB);
		ASSERT_THAT(IsTrue(Collection.Execute(BindsB, Diagnostic), TEXT("Engine B should execute the observed callbacks")));
		const FAngelscriptBindExecutionSnapshot SnapshotB =
			FAngelscriptBindExecutionObservation::GetLastSnapshot();
		const TArray<FString> TopLines =
			FAngelscriptBindExecutionObservation::BuildTopCallbackLogLines(SnapshotB, 2);

		bool bPassed = true;
		bPassed &= TestRunner->TestEqual(TEXT("Each engine pass should record all providers"), SnapshotB.ProviderRecords.Num(), 3);
		bPassed &= TestRunner->TestTrue(
			TEXT("Two engines should receive distinct observation identities and epochs"),
			SnapshotA.EngineIdentity != SnapshotB.EngineIdentity
				&& SnapshotA.ExecutionEpoch != SnapshotB.ExecutionEpoch);
		bPassed &= TestRunner->TestEqual(TEXT("Execution observation should expose seven phase totals"), SnapshotB.PhaseTotals.Num(), 7);
		bPassed &= TestRunner->TestTrue(
			TEXT("Selected phase totals should exactly match attempted and successful providers"),
			SnapshotB.PhaseTotals[static_cast<int32>(EAngelscriptBindPhase::TypeDeclarations)].AttemptedCount == 1
				&& SnapshotB.PhaseTotals[static_cast<int32>(EAngelscriptBindPhase::ExplicitBindings)].SucceededCount == 1
				&& SnapshotB.PhaseTotals[static_cast<int32>(EAngelscriptBindPhase::Finalization)].SucceededCount == 1);
		bPassed &= TestRunner->TestTrue(
			TEXT("A successful callback pass should be publication-eligible but not claim actual publication"),
			SnapshotB.PublicationEligibility == EAngelscriptBindPublicationEligibility::Eligible
				&& !SnapshotB.bPublicationResultRecorded);
		bPassed &= TestRunner->TestEqual(TEXT("Top-N formatting should honor the requested limit"), TopLines.Num(), 2);
		for (const FString& Line : TopLines)
		{
			bPassed &= TestRunner->TestTrue(
				TEXT("Top-N lines should expose stable engine/epoch/owner/bind/phase/status/duration columns"),
				Line.Contains(TEXT("engine="))
					&& Line.Contains(TEXT("epoch="))
					&& Line.Contains(TEXT("owner="))
					&& Line.Contains(TEXT("bind="))
					&& Line.Contains(TEXT("phase="))
					&& Line.Contains(TEXT("status="))
					&& Line.Contains(TEXT("duration_ms=")));
		}
		TestRunner->TestTrue(TEXT("Per-engine direct callback observation should be exact and queryable"), bPassed);
	}

	TEST_METHOD(AbortedExecutionRecordsFirstFailureAndBlocksPublication)
	{
		const FAngelscriptEngineConfig Config;
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = FAngelscriptTestEngine::Create(Config, Dependencies);
		ASSERT_THAT(IsTrue(Engine.IsValid(), TEXT("The aborted observation test should create an explicit engine")));

		FAngelscriptBindExecutionObservation::Reset();
		FAngelscriptBindCollection Collection;
		FString Diagnostic;
		Collection.Append(MakeRecord(TEXT("FailureOwner"), TEXT("FailureProvider"), EAngelscriptBindPhase::ExplicitBindings, &RegisterDuplicateGlobalFunction, "FailureProvider.cpp", 73), Diagnostic);
		Collection.Append(MakeRecord(TEXT("LaterOwner"), TEXT("LaterProvider"), EAngelscriptBindPhase::Finalization, &RecordLaterProvider, "LaterProvider.cpp", 91), Diagnostic);
		ASSERT_THAT(IsTrue(Collection.Finalize(Diagnostic), TEXT("The aborted observation fixture should finalize")));

		FAngelscriptBinds Binds(*Engine);
		LaterProviderInvocationCount = 0;
		TestRunner->AddExpectedErrorPlain(
			TEXT("Failed in call to function 'RegisterGlobalFunction' with 'int DirectBindFailureFixture()'"),
			EAutomationExpectedErrorFlags::Contains,
			1);
		ASSERT_THAT(IsFalse(Collection.Execute(Binds, Diagnostic), TEXT("The duplicate registration should abort execution")));
		FAngelscriptBindExecutionObservation::RecordPublicationResult(Engine.Get(), false);
		const FAngelscriptBindExecutionSnapshot Snapshot =
			FAngelscriptBindExecutionObservation::GetLastSnapshot();

		bool bPassed = true;
		bPassed &= TestRunner->TestTrue(
			TEXT("The failed callback should be the only reported provider"),
			Snapshot.ProviderRecords.Num() == 1
				&& Snapshot.ProviderRecords[0].Status == EAngelscriptBindProviderStatus::Failed);
		bPassed &= TestRunner->TestTrue(
			TEXT("Aborted execution should retain the first actionable failure"),
			Snapshot.bExecutionAborted
				&& Snapshot.FirstFailureDiagnostic.Contains(TEXT("DirectBindFailureFixture")));
		bPassed &= TestRunner->TestTrue(
			TEXT("The failure phase should report one attempt, zero successes, and one failure"),
			Snapshot.PhaseTotals[static_cast<int32>(EAngelscriptBindPhase::ExplicitBindings)].AttemptedCount == 1
				&& Snapshot.PhaseTotals[static_cast<int32>(EAngelscriptBindPhase::ExplicitBindings)].SucceededCount == 0
				&& Snapshot.PhaseTotals[static_cast<int32>(EAngelscriptBindPhase::ExplicitBindings)].FailedCount == 1);
		bPassed &= TestRunner->TestTrue(
			TEXT("A failed pass should be publication-blocked and record the actual not-published decision"),
			Snapshot.PublicationEligibility == EAngelscriptBindPublicationEligibility::Blocked
				&& Snapshot.bPublicationResultRecorded
				&& !Snapshot.bPublished
				&& LaterProviderInvocationCount == 0);
		TestRunner->TestTrue(TEXT("Aborted direct callback observation should fail closed"), bPassed);
	}

	TEST_METHOD(DetailedProviderObservationIsCompiledOutWithoutDevAutomationTests)
	{
		FString ObservationHeader;
		FString ObservationImplementation;
		FString BindsHeader;
		FString BindsInternalHeader;
		FString BindsImplementation;
		ASSERT_THAT(IsTrue(LoadRuntimeSource(TEXT("Testing/AngelscriptBindExecutionObservation.h"), ObservationHeader)));
		ASSERT_THAT(IsTrue(LoadRuntimeSource(TEXT("Testing/AngelscriptBindExecutionObservation.cpp"), ObservationImplementation)));
		ASSERT_THAT(IsTrue(LoadRuntimeSource(TEXT("Core/AngelscriptBinds.h"), BindsHeader)));
		ASSERT_THAT(IsTrue(LoadRuntimeSource(TEXT("Core/AngelscriptBindsInternal.h"), BindsInternalHeader)));
		ASSERT_THAT(IsTrue(LoadRuntimeSource(TEXT("Core/AngelscriptBinds.cpp"), BindsImplementation)));

		const int32 HeaderGate = ObservationHeader.Find(TEXT("#if WITH_DEV_AUTOMATION_TESTS"));
		const int32 ProviderStorage = ObservationHeader.Find(TEXT("ProviderRecords"));
		const int32 ImplementationGate = ObservationImplementation.Find(TEXT("#if WITH_DEV_AUTOMATION_TESTS"));
		const int32 ProviderClock = ObservationImplementation.Find(TEXT("ProviderStartTime"));
		bool bPassed = true;
		bPassed &= TestRunner->TestTrue(
			TEXT("Detailed observation types should live entirely behind the development automation gate"),
			HeaderGate != INDEX_NONE && ProviderStorage > HeaderGate);
		bPassed &= TestRunner->TestTrue(
			TEXT("Per-provider clocks should live entirely behind the development automation gate"),
			ImplementationGate != INDEX_NONE && ProviderClock > ImplementationGate);
		bPassed &= TestRunner->TestFalse(
			TEXT("The compact process callback record must not retain provider clocks or execution observations"),
			BindsInternalHeader.Contains(TEXT("Duration"))
				|| BindsInternalHeader.Contains(TEXT("ProviderRecords"))
				|| BindsInternalHeader.Contains(TEXT("StartTime")));
		bPassed &= TestRunner->TestFalse(
			TEXT("Engine-owned bind state must not retain per-provider clocks or expanded observation records"),
			BindsHeader.Contains(TEXT("ProviderRecords"))
				|| BindsHeader.Contains(TEXT("ProviderStartTime")));
		bPassed &= TestRunner->TestTrue(
			TEXT("Provider observation calls in callback execution should remain compile-time gated"),
			BindsImplementation.Contains(TEXT("#if WITH_DEV_AUTOMATION_TESTS"))
				&& BindsImplementation.Contains(TEXT("BeginProvider"))
				&& BindsImplementation.Contains(TEXT("EndProvider")));
		TestRunner->TestTrue(TEXT("Non-development builds should have no per-provider observation storage or clocks"), bPassed);
	}
};

#endif
