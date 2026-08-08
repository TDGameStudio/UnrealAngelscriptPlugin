#include "AngelscriptBinds.h"
#include "AngelscriptBindsInternal.h"
#include "AngelscriptEngine.h"
#include "AngelscriptSettings.h"
#include "AngelscriptUhtCoverageTestTypes.h"
#include "Binds/Helper_FunctionSignature.h"
#include "Testing/AngelscriptUhtOverloadCoverageTypes.h"
#include "ClassGenerator/ASClass.h"
#include "AngelscriptTestUtilities.h"
#include "Testing/AngelscriptBindExecutionObservation.h"
#include "FunctionLibraries/RuntimeFloatCurveMixinLibrary.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "CQTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptBindConfigTests,
	"Angelscript.TestModule.Engine.BindConfig",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FBindCollection = UE::Angelscript::Private::FAngelscriptBindCollection;

	static bool LoadRuntimeSource(const TCHAR* RelativePath, FString& OutSource)
	{
		return FFileHelper::LoadFileToString(
			OutSource,
			*FPaths::Combine(
				FPaths::ProjectPluginsDir(),
				TEXT("Angelscript/Source/AngelscriptRuntime"),
				RelativePath));
	}

	static void NoOpDirectBind(FAngelscriptBinds& Binds)
	{
		(void)Binds;
	}

	static FAngelscriptBindRecord MakeDirectBindRecord(
		const FName BindName,
		const EAngelscriptBindPhase Phase,
		const ANSICHAR* SourceFile,
		const int32 SourceLine)
	{
		FAngelscriptBindRecord Record;
		Record.OwnerModule = TEXT("AngelscriptTest");
		Record.BindName = BindName;
		Record.Phase = Phase;
		Record.SourceFile = SourceFile;
		Record.SourceLine = SourceLine;
		Record.Callback = &NoOpDirectBind;
		return Record;
	}

	static FAngelscriptBindExecutionSnapshot ObserveStartupBindPass()
	{
		FAngelscriptBindExecutionObservation::Reset();
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeFullEngineForTesting(
			FAngelscriptEngineConfig(),
			Dependencies);
		check(Engine.IsValid());
		return FAngelscriptBindExecutionObservation::GetLastSnapshot();
	}

	static bool IsFunctionEntryBound(const FAngelscriptFunctionBinding& Entry)
	{
		FGenericFuncPtr FuncPtr = Entry.FunctionPointer;
		return FuncPtr.IsBound() && Entry.FunctionCaller.IsBound();
	}

	static bool AreFunctionEntriesEqual(
		const FAngelscriptFunctionBinding& Left,
		const FAngelscriptFunctionBinding& Right)
	{
		return FMemory::Memcmp(&Left.FunctionPointer, &Right.FunctionPointer, sizeof(FGenericFuncPtr)) == 0
			&& FMemory::Memcmp(
				&Left.FunctionCaller,
				&Right.FunctionCaller,
				sizeof(ASAutoCaller::FunctionCaller)) == 0;
	}

	static void CDECL NoOpGeneric(asIScriptGeneric* Generic)
	{
		(void)Generic;
	}

public:
	TEST_METHOD(RuntimeConfigurationHasNoBindingFilterSurface)
	{
		FString SettingsHeader;
		FString EngineHeader;
		FString EngineImplementation;
		FString BindsHeader;
		FString BindsImplementation;
		FString ObservationHeader;
		FString ObservationImplementation;
		FString StateDumpImplementation;
		ASSERT_THAT(IsTrue(LoadRuntimeSource(TEXT("Core/AngelscriptSettings.h"), SettingsHeader)));
		ASSERT_THAT(IsTrue(LoadRuntimeSource(TEXT("Core/AngelscriptEngine.h"), EngineHeader)));
		ASSERT_THAT(IsTrue(LoadRuntimeSource(TEXT("Core/AngelscriptEngine.cpp"), EngineImplementation)));
		ASSERT_THAT(IsTrue(LoadRuntimeSource(TEXT("Core/AngelscriptBinds.h"), BindsHeader)));
		ASSERT_THAT(IsTrue(LoadRuntimeSource(TEXT("Core/AngelscriptBinds.cpp"), BindsImplementation)));
		ASSERT_THAT(IsTrue(LoadRuntimeSource(TEXT("Testing/AngelscriptBindExecutionObservation.h"), ObservationHeader)));
		ASSERT_THAT(IsTrue(LoadRuntimeSource(TEXT("Testing/AngelscriptBindExecutionObservation.cpp"), ObservationImplementation)));
		ASSERT_THAT(IsTrue(LoadRuntimeSource(TEXT("Dump/AngelscriptStateDump.cpp"), StateDumpImplementation)));

		const FString RuntimeFilteringSurface = SettingsHeader
			+ EngineHeader
			+ EngineImplementation
			+ BindsHeader
			+ BindsImplementation
			+ ObservationHeader
			+ ObservationImplementation
			+ StateDumpImplementation;
		const TArray<FString> ForbiddenFilteringSymbols = {
			TEXT("DisabledBindNames"),
			TEXT("CollectDisabledBindNames"),
			TEXT("GetBindInfoList"),
			TEXT("GetAllRegisteredBindNames"),
			TEXT("CallBinds("),
			TEXT("struct FBindInfo"),
			TEXT("enum class EOrder"),
			TEXT("BindOrder"),
			TEXT("SkipFingerprint"),
			TEXT("BindAlias"),
			TEXT("ProviderAlias"),
			TEXT("BindDependencies"),
			TEXT("DependentBindNames"),
			TEXT("DependencyCascade"),
		};

		bool bPassed = true;
		bPassed &= TestRunner->TestTrue(
			TEXT("DisabledBindNames should not be exposed as a project setting"),
			FindFProperty<FProperty>(UAngelscriptSettings::StaticClass(), TEXT("DisabledBindNames")) == nullptr);
		for (const FString& ForbiddenSymbol : ForbiddenFilteringSymbols)
		{
			bPassed &= TestRunner->TestFalse(
				*FString::Printf(
					TEXT("Binding runtime sources should not retain filtering symbol '%s'"),
					*ForbiddenSymbol),
				RuntimeFilteringSurface.Contains(ForbiddenSymbol));
		}
		TestRunner->TestTrue(TEXT("Runtime binding configuration should not expose filtering or cascade state"), bPassed);
	}

	TEST_METHOD(BindingSourceChangesRequireRestartAfterCollectionSeal)
	{
		FBindCollection Collection;
		FString Diagnostic;
		Collection.Append(
			MakeDirectBindRecord(
				TEXT("InitialProvider"),
				EAngelscriptBindPhase::ManualBindings,
				"InitialProvider.cpp",
				17),
			Diagnostic);
		ASSERT_THAT(IsTrue(Collection.Finalize(Diagnostic), TEXT("The direct callback collection should seal")));
		const void* const SealedBacking = Collection.GetRecords().GetData();

		const bool bAcceptedAfterSeal = Collection.Append(
			MakeDirectBindRecord(
				TEXT("ChangedProvider"),
				EAngelscriptBindPhase::GeneratedBindings,
				"ChangedProvider.cpp",
				29),
			Diagnostic);

		bool bPassed = true;
		bPassed &= TestRunner->TestFalse(TEXT("A source provider cannot join the process after seal"), bAcceptedAfterSeal);
		bPassed &= TestRunner->TestTrue(
			TEXT("A late source provider should require process restart and identify its declaration"),
			Diagnostic.Contains(TEXT("restart"))
				&& Diagnostic.Contains(TEXT("AngelscriptTest"))
				&& Diagnostic.Contains(TEXT("ChangedProvider"))
				&& Diagnostic.Contains(TEXT("GeneratedBindings"))
				&& Diagnostic.Contains(TEXT("ChangedProvider.cpp:29")));
		bPassed &= TestRunner->TestTrue(
			TEXT("A late source provider should not rebuild or mutate the sealed collection"),
			Collection.GetRecords().Num() == 1 && Collection.GetRecords().GetData() == SealedBacking);
		TestRunner->TestTrue(TEXT("Binding source changes should take effect only after rebuild and restart"), bPassed);
	}

	TEST_METHOD(CollectionValidationAndSevenDirectPhasesOwnLifecycle)
	{
		FBindCollection InvalidCollection;
		FString Diagnostic;
		FAngelscriptBindRecord InvalidRecord = MakeDirectBindRecord(
			TEXT("InvalidProvider"),
			EAngelscriptBindPhase::ManualBindings,
			"InvalidProvider.cpp",
			11);
		InvalidRecord.Callback = nullptr;
		InvalidCollection.Append(MoveTemp(InvalidRecord), Diagnostic);

		bool bPassed = true;
		bPassed &= TestRunner->TestFalse(
			TEXT("Collection validation should reject incomplete provider metadata"),
			InvalidCollection.Finalize(Diagnostic));
		bPassed &= TestRunner->TestFalse(
			TEXT("A rejected collection should remain unsealed"),
			InvalidCollection.IsSealed());

		FBindCollection ValidCollection;
		static const EAngelscriptBindPhase ReversePhases[] = {
			EAngelscriptBindPhase::Finalization,
			EAngelscriptBindPhase::PostReflectionBindings,
			EAngelscriptBindPhase::ReflectionBindings,
			EAngelscriptBindPhase::GeneratedBindings,
			EAngelscriptBindPhase::ManualBindings,
			EAngelscriptBindPhase::TypeInfrastructure,
			EAngelscriptBindPhase::TypeDeclarations,
		};
		for (int32 PhaseIndex = 0; PhaseIndex < UE_ARRAY_COUNT(ReversePhases); ++PhaseIndex)
		{
			ValidCollection.Append(
				MakeDirectBindRecord(
					FName(*FString::Printf(TEXT("Phase%d"), PhaseIndex)),
					ReversePhases[PhaseIndex],
					"PhaseProvider.cpp",
					20 + PhaseIndex),
				Diagnostic);
		}
		bPassed &= TestRunner->TestTrue(
			TEXT("A complete seven-phase collection should finalize"),
			ValidCollection.Finalize(Diagnostic));

		const TConstArrayView<FAngelscriptBindRecord> Records = ValidCollection.GetRecords();
		bPassed &= TestRunner->TestEqual(TEXT("All seven direct phases should remain represented"), Records.Num(), 7);
		if (Records.Num() == 7)
		{
			for (int32 PhaseIndex = 0; PhaseIndex < Records.Num(); ++PhaseIndex)
			{
				bPassed &= TestRunner->TestEqual(
					*FString::Printf(TEXT("Direct phase %d should use fixed enum order"), PhaseIndex),
					static_cast<int32>(Records[PhaseIndex].Phase),
					PhaseIndex);
			}
		}
		TestRunner->TestTrue(TEXT("Collection validation and fixed direct phases should own binding lifecycle"), bPassed);
	}

	TEST_METHOD(DirectPostReflectionProvidersUseStableLexicalOrder)
	{
		const FAngelscriptBindExecutionSnapshot Snapshot = ObserveStartupBindPass();
		const TArray<FName> ExpectedProviderOrder = {
			TEXT("AActor.PostReflection"),
			TEXT("FunctionLibraryMixins.PostReflection"),
			TEXT("Subsystems.PostReflection"),
			TEXT("UActorComponent.PostReflection"),
		};

		TArray<int32> ExecutionIndices;
		ExecutionIndices.Reserve(ExpectedProviderOrder.Num());
		for (const FName ProviderName : ExpectedProviderOrder)
		{
			const int32 ExecutionIndex = Snapshot.ExecutedBindNames.IndexOfByKey(ProviderName);
			if (!this->Assert.IsTrue(
				ExecutionIndex != INDEX_NONE,
				*FString::Printf(
					TEXT("BindConfig.DirectPostReflectionProvidersUseStableLexicalOrder should observe direct provider '%s' during startup"),
					*ProviderName.ToString())))
			{
				return;
			}
			ExecutionIndices.Add(ExecutionIndex);
		}

		bool bOk = true;
		for (int32 ProviderIndex = 1; ProviderIndex < ExecutionIndices.Num(); ++ProviderIndex)
		{
			bOk &= this->Assert.IsTrue(
				ExecutionIndices[ProviderIndex - 1] < ExecutionIndices[ProviderIndex],
				*FString::Printf(
					TEXT("BindConfig.DirectPostReflectionProvidersUseStableLexicalOrder should execute '%s' before '%s'"),
					*ExpectedProviderOrder[ProviderIndex - 1].ToString(),
					*ExpectedProviderOrder[ProviderIndex].ToString()));
		}
		(void)bOk;
	}

	TEST_METHOD(ExecutionObservationOnlyRecordsWithinActivePass)
	{
		const FName BeforePassName(TEXT("Automation.BindConfig.Observation.BeforePass"));
		const FName DuringPassName(TEXT("Automation.BindConfig.Observation.DuringPass"));
		const FName AfterPassName(TEXT("Automation.BindConfig.Observation.AfterPass"));

		FAngelscriptBindExecutionObservation::Reset();
		ON_SCOPE_EXIT { FAngelscriptBindExecutionObservation::Reset(); };
		FAngelscriptBindExecutionObservation::RecordExecutedBind(BeforePassName);
		FAngelscriptBindExecutionObservation::BeginObservationPass();
		FAngelscriptBindExecutionObservation::RecordExecutedBind(DuringPassName);
		FAngelscriptBindExecutionObservation::EndObservationPass();
		FAngelscriptBindExecutionObservation::RecordExecutedBind(AfterPassName);

		const FAngelscriptBindExecutionSnapshot Snapshot = FAngelscriptBindExecutionObservation::GetLastSnapshot();
		bool bOk = true;
		bOk &= this->Assert.IsFalse(Snapshot.ExecutedBindNames.Contains(BeforePassName), TEXT("BindConfig.ExecutionObservationOnlyRecordsWithinActivePass should ignore records before BeginObservationPass"));
		bOk &= this->Assert.IsTrue(Snapshot.ExecutedBindNames.Contains(DuringPassName), TEXT("BindConfig.ExecutionObservationOnlyRecordsWithinActivePass should accept records while the pass is active"));
		bOk &= this->Assert.IsFalse(Snapshot.ExecutedBindNames.Contains(AfterPassName), TEXT("BindConfig.ExecutionObservationOnlyRecordsWithinActivePass should ignore records after EndObservationPass"));
		(void)bOk;
	}

	TEST_METHOD(GeneratedBlueprintCallableEntriesPopulateClassMaps)
	{
		UFunction* DestroyActorFunction = AActor::StaticClass()->FindFunctionByName(TEXT("K2_DestroyActor"));
		UFunction* GetPlayerControllerFunction = UGameplayStatics::StaticClass()->FindFunctionByName(TEXT("GetPlayerController"));
		UFunction* IsDeveloperOnlyFunction = UASClass::StaticClass()->FindFunctionByName(TEXT("IsDeveloperOnly"));
		if (!this->Assert.IsNotNull(DestroyActorFunction, TEXT("GeneratedBlueprintCallableEntriesPopulateClassMaps should find AActor::K2_DestroyActor"))
			|| !this->Assert.IsNotNull(GetPlayerControllerFunction, TEXT("GeneratedBlueprintCallableEntriesPopulateClassMaps should find UGameplayStatics::GetPlayerController"))
			|| !this->Assert.IsNotNull(IsDeveloperOnlyFunction, TEXT("GeneratedBlueprintCallableEntriesPopulateClassMaps should find UASClass::IsDeveloperOnly")))
		{ return; }

		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeFullEngineForTesting(FAngelscriptEngineConfig(), Dependencies);
		if (!this->Assert.IsTrue(Engine.IsValid(), TEXT("GeneratedBlueprintCallableEntriesPopulateClassMaps should create a testing engine"))) { return; }
		FAngelscriptEngineScope EngineScope(*Engine);
		FAngelscriptBinds Binds(*Engine);

		auto& ClassFunctionBindings = Binds.GetTargetBindState().ClassFunctionBindings;
		const TMap<FString, FAngelscriptFunctionBinding>* ActorEntries = ClassFunctionBindings.Find(AActor::StaticClass());
		const TMap<FString, FAngelscriptFunctionBinding>* GameplayStaticsEntries = ClassFunctionBindings.Find(UGameplayStatics::StaticClass());
		const TMap<FString, FAngelscriptFunctionBinding>* ScriptClassEntries = ClassFunctionBindings.Find(UASClass::StaticClass());
		if (!this->Assert.IsNotNull(ActorEntries, TEXT("GeneratedBlueprintCallableEntriesPopulateClassMaps should populate entries for AActor"))
			|| !this->Assert.IsNotNull(GameplayStaticsEntries, TEXT("GeneratedBlueprintCallableEntriesPopulateClassMaps should populate entries for UGameplayStatics"))
			|| !this->Assert.IsNotNull(ScriptClassEntries, TEXT("GeneratedBlueprintCallableEntriesPopulateClassMaps should populate entries for UASClass")))
		{ return; }

		const FAngelscriptFunctionBinding* DestroyActorEntry = ActorEntries->Find(DestroyActorFunction->GetName());
		const FAngelscriptFunctionBinding* GetPlayerControllerEntry = GameplayStaticsEntries->Find(GetPlayerControllerFunction->GetName());
		const FAngelscriptFunctionBinding* IsDeveloperOnlyEntry = ScriptClassEntries->Find(IsDeveloperOnlyFunction->GetName());
		if (!this->Assert.IsNotNull(DestroyActorEntry, TEXT("GeneratedBlueprintCallableEntriesPopulateClassMaps should register AActor::K2_DestroyActor"))
			|| !this->Assert.IsNotNull(GetPlayerControllerEntry, TEXT("GeneratedBlueprintCallableEntriesPopulateClassMaps should register UGameplayStatics::GetPlayerController"))
			|| !this->Assert.IsNotNull(IsDeveloperOnlyEntry, TEXT("GeneratedBlueprintCallableEntriesPopulateClassMaps should register UASClass::IsDeveloperOnly")))
		{ return; }

		(void)this->Assert.IsTrue(IsFunctionEntryBound(*IsDeveloperOnlyEntry), TEXT("GeneratedBlueprintCallableEntriesPopulateClassMaps should bind UASClass::IsDeveloperOnly to a direct native function entry"));
	}

	TEST_METHOD(RegisterFunctionBindingPreservesFirstRegistration)
	{
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeFullEngineForTesting(
			FAngelscriptEngineConfig(),
			Dependencies);
		ASSERT_THAT(IsTrue(Engine.IsValid(), TEXT("The first-registration test should create an explicit engine")));
		FAngelscriptBinds Binds(*Engine);

		const FString FunctionName = TEXT("Automation_FirstRegistrationWins");
		const FAngelscriptFunctionBinding FirstEntry = { ERASE_METHOD_PTR(AActor, K2_DestroyActor, (), ERASE_ARGUMENT_PACK(void)) };
		const FAngelscriptFunctionBinding SecondEntry = { ERASE_NO_FUNCTION() };
		Binds.RegisterFunctionBindingForTarget(AActor::StaticClass(), FunctionName, FirstEntry);
		Binds.RegisterFunctionBindingForTarget(AActor::StaticClass(), FunctionName, SecondEntry);

		const TMap<FString, FAngelscriptFunctionBinding>* ActorEntries =
			Binds.GetTargetBindState().ClassFunctionBindings.Find(AActor::StaticClass());
		if (!this->Assert.IsNotNull(ActorEntries, TEXT("RegisterFunctionBindingPreservesFirstRegistration should create a function entry map for AActor"))) { return; }
		const FAngelscriptFunctionBinding* StoredEntry = ActorEntries->Find(FunctionName);
		if (!this->Assert.IsNotNull(StoredEntry, TEXT("RegisterFunctionBindingPreservesFirstRegistration should keep the first function entry"))) { return; }

		bool bOk = true;
		bOk &= this->Assert.IsTrue(IsFunctionEntryBound(*StoredEntry), TEXT("RegisterFunctionBindingPreservesFirstRegistration should keep the first registration bound"));
		bOk &= this->Assert.IsTrue(AreFunctionEntriesEqual(*StoredEntry, FirstEntry), TEXT("RegisterFunctionBindingPreservesFirstRegistration should preserve the first stored function pointer and caller"));
		bOk &= this->Assert.IsFalse(AreFunctionEntriesEqual(*StoredEntry, SecondEntry), TEXT("RegisterFunctionBindingPreservesFirstRegistration should ignore the later duplicate registration"));
		(void)bOk;
	}

	TEST_METHOD(BlueprintInternalUseOnlyCanBeOverriddenForAngelscript)
	{
UFunction* WithOverride = UAngelscriptUhtCoverageTestLibrary::StaticClass()->FindFunctionByName(TEXT("InternalCallableWithOverride"));
		UFunction* WithoutOverride = UAngelscriptUhtCoverageTestLibrary::StaticClass()->FindFunctionByName(TEXT("InternalCallableWithoutOverride"));
		if (!this->Assert.IsNotNull(WithOverride, TEXT("BlueprintInternalUseOnlyCanBeOverriddenForAngelscript should find the override test function"))
			|| !this->Assert.IsNotNull(WithoutOverride, TEXT("BlueprintInternalUseOnlyCanBeOverriddenForAngelscript should find the control test function")))
		{ return; }

		bool bOk = true;
		bOk &= this->Assert.IsTrue(WithoutOverride->HasMetaData(TEXT("BlueprintInternalUseOnly")), TEXT("BlueprintInternalUseOnlyCanBeOverriddenForAngelscript should keep the control function marked as BlueprintInternalUseOnly"));
		bOk &= this->Assert.IsTrue(WithOverride->HasMetaData(TEXT("UsableInAngelscript")), TEXT("BlueprintInternalUseOnlyCanBeOverriddenForAngelscript should mark the override function as UsableInAngelscript"));
		bOk &= this->Assert.IsFalse(FAngelscriptBinds::ShouldSkipBlueprintCallableFunction(WithOverride), TEXT("BlueprintInternalUseOnlyCanBeOverriddenForAngelscript should not skip override-marked functions"));
		bOk &= this->Assert.IsTrue(FAngelscriptBinds::ShouldSkipBlueprintCallableFunction(WithoutOverride), TEXT("BlueprintInternalUseOnlyCanBeOverriddenForAngelscript should still skip BlueprintInternalUseOnly functions without an override"));
		(void)bOk;
	}

	TEST_METHOD(FunctionLevelScriptMethodUsesFirstParameterAsMixin)
	{
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeFullEngineForTesting(FAngelscriptEngineConfig(), Dependencies);
		if (!this->Assert.IsTrue(Engine.IsValid(), TEXT("FunctionLevelScriptMethodUsesFirstParameterAsMixin should create a testing engine"))) { return; }
		FAngelscriptEngineScope EngineScope(*Engine);
		FAngelscriptBinds Binds(*Engine);

		TSharedPtr<FAngelscriptType> HostType = FAngelscriptType::GetByClass(
			Binds.GetTargetTypeDatabase(),
			UObject::StaticClass());
		UFunction* ScriptMethodFunction = UAngelscriptUhtCoverageTestLibrary::StaticClass()->FindFunctionByName(TEXT("GetCoverageValue"));
		if (!this->Assert.IsTrue(HostType.IsValid(), TEXT("FunctionLevelScriptMethodUsesFirstParameterAsMixin should resolve a host type for signature construction"))
			|| !this->Assert.IsNotNull(ScriptMethodFunction, TEXT("FunctionLevelScriptMethodUsesFirstParameterAsMixin should find the ScriptMethod test function")))
		{ return; }

		FAngelscriptFunctionSignature Signature(HostType.ToSharedRef(), ScriptMethodFunction);
		bool bOk = true;
		bOk &= this->Assert.IsTrue(Signature.bStaticInUnreal, TEXT("FunctionLevelScriptMethodUsesFirstParameterAsMixin should keep the Unreal function static"));
		bOk &= this->Assert.IsFalse(Signature.bStaticInScript, TEXT("FunctionLevelScriptMethodUsesFirstParameterAsMixin should bind ScriptMethod functions as script members"));
		bOk &= this->Assert.AreEqual(0, Signature.ArgumentTypes.Num(), TEXT("FunctionLevelScriptMethodUsesFirstParameterAsMixin should remove the first parameter from the exposed signature"));
		bOk &= this->Assert.IsTrue(Signature.Declaration.Contains(TEXT("const")), TEXT("FunctionLevelScriptMethodUsesFirstParameterAsMixin should expose a const member declaration when the first parameter is const"));
		bOk &= this->Assert.IsTrue(Signature.Declaration.Contains(TEXT("GetCoverageValue")), TEXT("FunctionLevelScriptMethodUsesFirstParameterAsMixin should keep the generated script name"));
		(void)bOk;
	}

	TEST_METHOD(CallableWithoutWorldContextKeepsHiddenWorldContextButClearsTrait)
	{
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeFullEngineForTesting(FAngelscriptEngineConfig(), Dependencies);
		if (!this->Assert.IsTrue(Engine.IsValid(), TEXT("CallableWithoutWorldContextKeepsHiddenWorldContextButClearsTrait should create a testing engine"))) { return; }
		FAngelscriptEngineScope EngineScope(*Engine);
		FAngelscriptBinds Binds(*Engine);

		TSharedPtr<FAngelscriptType> HostType = FAngelscriptType::GetByClass(
			Binds.GetTargetTypeDatabase(),
			UObject::StaticClass());
		UFunction* RequiredWorldContextFunction = UAngelscriptUhtCoverageTestLibrary::StaticClass()->FindFunctionByName(TEXT("RequiresWorldContext"));
		UFunction* OptionalWorldContextFunction = UAngelscriptUhtCoverageTestLibrary::StaticClass()->FindFunctionByName(TEXT("CallableWithoutWorldContext"));
		if (!this->Assert.IsTrue(HostType.IsValid(), TEXT("CallableWithoutWorldContextKeepsHiddenWorldContextButClearsTrait should resolve a host type for signature construction"))
			|| !this->Assert.IsNotNull(RequiredWorldContextFunction, TEXT("CallableWithoutWorldContextKeepsHiddenWorldContextButClearsTrait should find the required world-context function"))
			|| !this->Assert.IsNotNull(OptionalWorldContextFunction, TEXT("CallableWithoutWorldContextKeepsHiddenWorldContextButClearsTrait should find the optional world-context function")))
		{ return; }

		FAngelscriptFunctionSignature RequiredSignature(HostType.ToSharedRef(), RequiredWorldContextFunction);
		FAngelscriptFunctionSignature OptionalSignature(HostType.ToSharedRef(), OptionalWorldContextFunction);
		FAngelscriptBoundFunction RequiredBoundFunction =
			Binds.BindGlobalGenericFunctionForTarget(RequiredSignature.Declaration, &NoOpGeneric);
		FAngelscriptBoundFunction OptionalBoundFunction =
			Binds.BindGlobalGenericFunctionForTarget(OptionalSignature.Declaration, &NoOpGeneric);
		RequiredSignature.ModifyScriptFunction(RequiredBoundFunction);
		OptionalSignature.ModifyScriptFunction(OptionalBoundFunction);

		auto* RequiredScriptFunction = reinterpret_cast<asCScriptFunction*>(
			Engine->GetScriptEngine()->GetFunctionById(RequiredBoundFunction.GetFunctionId()));
		auto* OptionalScriptFunction = reinterpret_cast<asCScriptFunction*>(
			Engine->GetScriptEngine()->GetFunctionById(OptionalBoundFunction.GetFunctionId()));
		if (!this->Assert.IsNotNull(RequiredScriptFunction, TEXT("CallableWithoutWorldContextKeepsHiddenWorldContextButClearsTrait should create a script function for the required world-context case"))
			|| !this->Assert.IsNotNull(OptionalScriptFunction, TEXT("CallableWithoutWorldContextKeepsHiddenWorldContextButClearsTrait should create a script function for the optional world-context case")))
		{ return; }

		bool bOk = true;
		bOk &= this->Assert.AreEqual(0, RequiredScriptFunction->hiddenArgumentIndex, TEXT("CallableWithoutWorldContextKeepsHiddenWorldContextButClearsTrait should hide the world-context argument for required functions"));
		bOk &= this->Assert.AreEqual(0, OptionalScriptFunction->hiddenArgumentIndex, TEXT("CallableWithoutWorldContextKeepsHiddenWorldContextButClearsTrait should hide the world-context argument for callable-without-world-context functions"));
		bOk &= this->Assert.IsTrue(RequiredScriptFunction->traits.GetTrait(asTRAIT_USES_WORLDCONTEXT), TEXT("CallableWithoutWorldContextKeepsHiddenWorldContextButClearsTrait should mark required world-context functions with the world-context trait"));
		bOk &= this->Assert.IsFalse(OptionalScriptFunction->traits.GetTrait(asTRAIT_USES_WORLDCONTEXT), TEXT("CallableWithoutWorldContextKeepsHiddenWorldContextButClearsTrait should not mark callable-without-world-context functions with the world-context trait"));
		(void)bOk;
	}

	TEST_METHOD(ScriptAllowTemporaryThisAppendsAcceptTemporaryThis)
	{
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeFullEngineForTesting(FAngelscriptEngineConfig(), Dependencies);
		if (!this->Assert.IsTrue(Engine.IsValid(), TEXT("ScriptAllowTemporaryThisAppendsAcceptTemporaryThis should create a testing engine"))) { return; }
		FAngelscriptEngineScope EngineScope(*Engine);
		FAngelscriptBinds Binds(*Engine);

		TSharedPtr<FAngelscriptType> HostType = FAngelscriptType::GetByClass(
			Binds.GetTargetTypeDatabase(),
			UObject::StaticClass());
		UFunction* TemporaryThisFunction = UAngelscriptUhtCoverageTestLibrary::StaticClass()->FindFunctionByName(TEXT("GetTemporaryThisValue"));
		if (!this->Assert.IsTrue(HostType.IsValid(), TEXT("ScriptAllowTemporaryThisAppendsAcceptTemporaryThis should resolve the host type"))
			|| !this->Assert.IsNotNull(TemporaryThisFunction, TEXT("ScriptAllowTemporaryThisAppendsAcceptTemporaryThis should find the test function")))
		{ return; }

		FAngelscriptFunctionSignature Signature(HostType.ToSharedRef(), TemporaryThisFunction);
		bool bOk = true;
		bOk &= this->Assert.IsTrue(!Signature.bStaticInScript, TEXT("ScriptAllowTemporaryThisAppendsAcceptTemporaryThis should bind ScriptMethod functions as members"));
		bOk &= this->Assert.IsTrue(Signature.Declaration.Contains(TEXT(" accept_temporary_this")), TEXT("ScriptAllowTemporaryThisAppendsAcceptTemporaryThis should append accept_temporary_this to the declaration"));
		(void)bOk;
	}

	TEST_METHOD(UnsafeDuringActorConstructionSetsUnsafeTrait)
	{
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeFullEngineForTesting(FAngelscriptEngineConfig(), Dependencies);
		if (!this->Assert.IsTrue(Engine.IsValid(), TEXT("UnsafeDuringActorConstructionSetsUnsafeTrait should create a testing engine"))) { return; }
		FAngelscriptEngineScope EngineScope(*Engine);
		FAngelscriptBinds Binds(*Engine);

		TSharedPtr<FAngelscriptType> HostType = FAngelscriptType::GetByClass(
			Binds.GetTargetTypeDatabase(),
			UObject::StaticClass());
		UFunction* UnsafeFunction = UAngelscriptUhtCoverageTestLibrary::StaticClass()->FindFunctionByName(TEXT("UnsafeDuringConstruction"));
		UFunction* SafeFunction = UAngelscriptUhtCoverageTestLibrary::StaticClass()->FindFunctionByName(TEXT("SafeDuringConstruction"));
		if (!this->Assert.IsTrue(HostType.IsValid(), TEXT("UnsafeDuringActorConstructionSetsUnsafeTrait should resolve the host type"))
			|| !this->Assert.IsNotNull(UnsafeFunction, TEXT("UnsafeDuringActorConstructionSetsUnsafeTrait should find the unsafe test function"))
			|| !this->Assert.IsNotNull(SafeFunction, TEXT("UnsafeDuringActorConstructionSetsUnsafeTrait should find the safe test function")))
		{ return; }

		FAngelscriptFunctionSignature UnsafeSignature(HostType.ToSharedRef(), UnsafeFunction);
		FAngelscriptFunctionSignature SafeSignature(HostType.ToSharedRef(), SafeFunction);
		FAngelscriptBoundFunction UnsafeBoundFunction =
			Binds.BindGlobalGenericFunctionForTarget(UnsafeSignature.Declaration, &NoOpGeneric);
		FAngelscriptBoundFunction SafeBoundFunction =
			Binds.BindGlobalGenericFunctionForTarget(SafeSignature.Declaration, &NoOpGeneric);
		UnsafeSignature.ModifyScriptFunction(UnsafeBoundFunction);
		SafeSignature.ModifyScriptFunction(SafeBoundFunction);

		auto* UnsafeScriptFunction = reinterpret_cast<asCScriptFunction*>(
			Engine->GetScriptEngine()->GetFunctionById(UnsafeBoundFunction.GetFunctionId()));
		auto* SafeScriptFunction = reinterpret_cast<asCScriptFunction*>(
			Engine->GetScriptEngine()->GetFunctionById(SafeBoundFunction.GetFunctionId()));
		if (!this->Assert.IsNotNull(UnsafeScriptFunction, TEXT("UnsafeDuringActorConstructionSetsUnsafeTrait should create the unsafe script function"))
			|| !this->Assert.IsNotNull(SafeScriptFunction, TEXT("UnsafeDuringActorConstructionSetsUnsafeTrait should create the safe script function")))
		{ return; }

		bool bOk = true;
		bOk &= this->Assert.IsTrue(UnsafeScriptFunction->traits.GetTrait(asTRAIT_UNSAFE_DURING_CONSTRUCTION), TEXT("UnsafeDuringActorConstructionSetsUnsafeTrait should mark meta-present functions as unsafe during construction"));
		bOk &= this->Assert.IsFalse(SafeScriptFunction->traits.GetTrait(asTRAIT_UNSAFE_DURING_CONSTRUCTION), TEXT("UnsafeDuringActorConstructionSetsUnsafeTrait should not mark explicit false meta functions as unsafe during construction"));
		(void)bOk;
	}

	TEST_METHOD(OverloadedExportedFunctionsCanRecoverDirectBind)
	{
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeFullEngineForTesting(FAngelscriptEngineConfig(), Dependencies);
		if (!this->Assert.IsTrue(Engine.IsValid(), TEXT("OverloadedExportedFunctionsCanRecoverDirectBind should create a testing engine"))) { return; }
		FAngelscriptEngineScope EngineScope(*Engine);
		FAngelscriptBinds Binds(*Engine);

		UFunction* OverloadFunction = UAngelscriptUhtOverloadCoverageLibrary::StaticClass()->FindFunctionByName(TEXT("ResolveCoverageOverload"));
		if (!this->Assert.IsNotNull(OverloadFunction, TEXT("OverloadedExportedFunctionsCanRecoverDirectBind should find the reflected overload function"))) { return; }

		const TMap<FString, FAngelscriptFunctionBinding>* OverloadEntries =
			Binds.GetTargetBindState().ClassFunctionBindings.Find(UAngelscriptUhtOverloadCoverageLibrary::StaticClass());
		if (!this->Assert.IsNotNull(OverloadEntries, TEXT("OverloadedExportedFunctionsCanRecoverDirectBind should populate entries for the overload test library"))) { return; }

		const FAngelscriptFunctionBinding* OverloadEntry = OverloadEntries->Find(OverloadFunction->GetName());
		if (!this->Assert.IsNotNull(OverloadEntry, TEXT("OverloadedExportedFunctionsCanRecoverDirectBind should register the reflected overload function"))) { return; }

		(void)this->Assert.IsTrue(IsFunctionEntryBound(*OverloadEntry), TEXT("OverloadedExportedFunctionsCanRecoverDirectBind should recover a direct bind instead of ERASE_NO_FUNCTION"));
	}

	TEST_METHOD(InlineDefinitionFunctionsCanRecoverDirectBind)
	{
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeFullEngineForTesting(FAngelscriptEngineConfig(), Dependencies);
		if (!this->Assert.IsTrue(Engine.IsValid(), TEXT("InlineDefinitionFunctionsCanRecoverDirectBind should create a testing engine"))) { return; }
		FAngelscriptEngineScope EngineScope(*Engine);
		FAngelscriptBinds Binds(*Engine);

		UFunction* InlineFunction = URuntimeFloatCurveMixinLibrary::StaticClass()->FindFunctionByName(TEXT("GetNumKeys"));
		if (!this->Assert.IsNotNull(InlineFunction, TEXT("InlineDefinitionFunctionsCanRecoverDirectBind should find the reflected inline function"))) { return; }
		const TMap<FString, FAngelscriptFunctionBinding>* InlineEntries =
			Binds.GetTargetBindState().ClassFunctionBindings.Find(URuntimeFloatCurveMixinLibrary::StaticClass());
		if (!this->Assert.IsNotNull(InlineEntries, TEXT("InlineDefinitionFunctionsCanRecoverDirectBind should populate entries for the inline function library"))) { return; }
		const FAngelscriptFunctionBinding* InlineEntry = InlineEntries->Find(InlineFunction->GetName());
		if (!this->Assert.IsNotNull(InlineEntry, TEXT("InlineDefinitionFunctionsCanRecoverDirectBind should register the reflected inline function"))) { return; }

		(void)this->Assert.IsTrue(IsFunctionEntryBound(*InlineEntry), TEXT("InlineDefinitionFunctionsCanRecoverDirectBind should recover a direct bind instead of ERASE_NO_FUNCTION"));
	}

	TEST_METHOD(InlineOutRefFunctionsCanRecoverDirectBind)
	{
		const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
		TUniquePtr<FAngelscriptEngine> Engine = CreateScriptScanFreeFullEngineForTesting(FAngelscriptEngineConfig(), Dependencies);
		if (!this->Assert.IsTrue(Engine.IsValid(), TEXT("InlineOutRefFunctionsCanRecoverDirectBind should create a testing engine"))) { return; }
		FAngelscriptEngineScope EngineScope(*Engine);
		FAngelscriptBinds Binds(*Engine);

		UFunction* InlineFunction = URuntimeFloatCurveMixinLibrary::StaticClass()->FindFunctionByName(TEXT("GetTimeRange"));
		if (!this->Assert.IsNotNull(InlineFunction, TEXT("InlineOutRefFunctionsCanRecoverDirectBind should find the reflected out-ref function"))) { return; }
		const TMap<FString, FAngelscriptFunctionBinding>* InlineEntries =
			Binds.GetTargetBindState().ClassFunctionBindings.Find(URuntimeFloatCurveMixinLibrary::StaticClass());
		if (!this->Assert.IsNotNull(InlineEntries, TEXT("InlineOutRefFunctionsCanRecoverDirectBind should populate entries for the inline function library"))) { return; }
		const FAngelscriptFunctionBinding* InlineEntry = InlineEntries->Find(InlineFunction->GetName());
		if (!this->Assert.IsNotNull(InlineEntry, TEXT("InlineOutRefFunctionsCanRecoverDirectBind should register the reflected out-ref function"))) { return; }

		(void)this->Assert.IsTrue(IsFunctionEntryBound(*InlineEntry), TEXT("InlineOutRefFunctionsCanRecoverDirectBind should recover a direct bind instead of ERASE_NO_FUNCTION"));
	}
};

#endif
