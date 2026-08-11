#include "Cache/AngelscriptCacheExactStartup.h"
#include "Cache/AngelscriptCacheCompilerBridge.h"
#include "Cache/AngelscriptCacheService.h"
#include "Cache/AngelscriptCacheStableSymbolIdentity.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Shared/AngelscriptReflectiveAccess.h"
#include "Shared/AngelscriptTestEngineAcquisition.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"

#include "as_objecttype.h"
#include "as_scriptfunction.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheProductionClassPropertyCompilerReuseTests,
	"Angelscript.TestModule.Cache.ProductionClassPropertyCompilerReuse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	class FScopedProjectRoot final
	{
	public:
		FScopedProjectRoot()
		{
			Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("Automation/AngelscriptCacheProductionClassPropertyCompilerReuse"),
				FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			FPaths::NormalizeDirectoryName(Root);
			check(Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheProductionClassPropertyCompilerReuse/")));
			ScriptRoot = Root / TEXT("Script");
			CacheRoot = Root / TEXT("CacheV2");
			check(IFileManager::Get().MakeDirectory(*ScriptRoot, true));
		}

		~FScopedProjectRoot()
		{
			if (Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheProductionClassPropertyCompilerReuse/")))
			{
				IFileManager::Get().DeleteDirectory(*Root, false, true);
			}
		}

		bool WriteSource(
			const FString& RelativePath,
			const FString& Source) const
		{
			return FFileHelper::SaveStringToFile(
				Source,
				*(ScriptRoot / RelativePath),
				FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		}

		FString Root;
		FString ScriptRoot;
		FString CacheRoot;
	};

	struct FClassPropertyCoordinates final
	{
		FAngelscriptStableModuleKey ModuleKey;
		FAngelscriptStableTypeKey TypeKey;
		FAngelscriptStablePropertyKey PropertyKey;
		FAngelscriptStableFunctionKey UnchangedFunctionKey;
		FAngelscriptStableFunctionKey ChangedFunctionKey;
		int32 VmPropertyOffset = INDEX_NONE;
		int32 ReflectedPropertyOffset = INDEX_NONE;
	};

	static TUniquePtr<FAngelscriptEngine> CreateEngine(
		const FScopedProjectRoot& Project)
	{
		FAngelscriptEngineConfig Config;
		Config.bIsEditor = true;
		Config.bDevelopmentMode = true;
		Config.bSkipThreadedInitialize = true;
		Config.CacheV2RootOverride = Project.CacheRoot;

		FAngelscriptEngineDependencies Dependencies =
			FAngelscriptEngineDependencies::CreateDefault();
		Dependencies.GetProjectDir = [&Project]()
		{
			return Project.Root;
		};
		Dependencies.GetEnabledPluginScriptRoots = []()
		{
			return TArray<FString>();
		};
		Dependencies.GetEnabledPluginScriptRootDescriptors = []()
		{
			return TArray<FAngelscriptPluginScriptRoot>();
		};
		return CreateScriptScanFreeFullEngineForTesting(Config, Dependencies);
	}

	static bool TryBuildCoordinates(
		const TSharedRef<FAngelscriptModuleDesc>& Module,
		FClassPropertyCoordinates& OutCoordinates,
		FString& OutFailure)
	{
		OutCoordinates = {};
		OutFailure.Reset();
		if (Module->ScriptModule == nullptr || Module->Classes.Num() != 1)
		{
			OutFailure = TEXT("The module has no unique compiled class");
			return false;
		}
		const TSharedRef<FAngelscriptClassDesc>& Class = Module->Classes[0];
		asCObjectType* ScriptType = static_cast<asCObjectType*>(Class->ScriptType);
		if (ScriptType == nullptr || ScriptType->localProperties.GetLength() != 1
			|| ScriptType->localProperties[0] == nullptr)
		{
			OutFailure = TEXT("The class has no unique local VM property");
			return false;
		}
		asCScriptFunction* UnchangedFunction = static_cast<asCScriptFunction*>(
			ScriptType->GetMethodByDecl("int ReadUnchanged()"));
		asCScriptFunction* ChangedFunction = static_cast<asCScriptFunction*>(
			ScriptType->GetMethodByDecl("int ReadChanged()"));
		if (UnchangedFunction == nullptr || ChangedFunction == nullptr)
		{
			OutFailure = TEXT("The class methods are incomplete");
			return false;
		}
		if (!FAngelscriptCacheStableSymbolIdentity::TryBuildModuleKey(
			*Module, OutCoordinates.ModuleKey, &OutFailure)
			|| !FAngelscriptCacheStableSymbolIdentity::TryBuildLocalTypeKey(
				OutCoordinates.ModuleKey, *ScriptType, OutCoordinates.TypeKey)
			|| !FAngelscriptCacheStableSymbolIdentity::TryBuildFunctionKey(
				OutCoordinates.ModuleKey, *UnchangedFunction,
				OutCoordinates.UnchangedFunctionKey, &OutFailure)
			|| !FAngelscriptCacheStableSymbolIdentity::TryBuildFunctionKey(
				OutCoordinates.ModuleKey, *ChangedFunction,
				OutCoordinates.ChangedFunctionKey, &OutFailure))
		{
			if (OutFailure.IsEmpty())
			{
				OutFailure = TEXT("Stable class/function identity failed");
			}
			return false;
		}

		const asCObjectProperty* ScriptProperty = ScriptType->localProperties[0];
		const FString PropertyName =
			UTF8_TO_TCHAR(ScriptProperty->name.AddressOf());
		if (!PropertyName.Equals(TEXT("StoredValue"), ESearchCase::CaseSensitive))
		{
			OutFailure = FString::Printf(
				TEXT("Unexpected VM property %s"), *PropertyName);
			return false;
		}
		FAngelscriptPropertyIdentityDescriptor PropertyIdentity;
		PropertyIdentity.OwnerTypeKey = OutCoordinates.TypeKey;
		PropertyIdentity.Kind = EAngelscriptArtifactEntityKind::Property;
		PropertyIdentity.Name = PropertyName;
		PropertyIdentity.CanonicalType = TEXT("int");
		OutCoordinates.PropertyKey =
			FAngelscriptArtifactIdentityBuilder::BuildPropertyKey(
				PropertyIdentity);
		OutCoordinates.VmPropertyOffset = ScriptProperty->byteOffset;
		const FIntProperty* ReflectedProperty = FindFProperty<FIntProperty>(
			Class->Class, TEXT("StoredValue"));
		if (ReflectedProperty == nullptr)
		{
			OutFailure = TEXT("Generated UClass has no reflected StoredValue");
			return false;
		}
		OutCoordinates.ReflectedPropertyOffset =
			ReflectedProperty->GetOffset_ForInternal();
		return !OutCoordinates.PropertyKey.Hash.IsZero();
	}

	static const FAngelscriptCacheDecisionEvent* FindFunctionEvent(
		const FAngelscriptCacheDecisionTraceSnapshot& Trace,
		const FAngelscriptStableFunctionKey& FunctionKey,
		const EAngelscriptCacheDecisionOutcome Outcome)
	{
		return Trace.Events.FindByPredicate(
			[&FunctionKey, Outcome](
				const FAngelscriptCacheDecisionEvent& Event)
			{
				return Event.Stage
						== EAngelscriptCacheDecisionStage::FunctionLookup
					&& Event.Outcome == Outcome
					&& Event.FunctionKey.IsSet()
					&& Event.FunctionKey->Hash == FunctionKey.Hash;
			});
	}

public:
	TEST_METHOD(BodyOnlyEditPreservesPropertyAuthorityAndRestoresUnchangedMethod)
	{
		FScopedProjectRoot Project;
		const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const FString ColdTemplate = ASTEST_AS(R"AS(
			UCLASS()
			class UCacheProductionPropertyClass%s : UObject
			{
				UPROPERTY()
				int StoredValue = 31;

				UFUNCTION()
				int ReadUnchanged()
				{
					return StoredValue + 1100;
				}

				UFUNCTION()
				int ReadChanged()
				{
					return StoredValue + 1101;
				}
			}
			)AS");
		const FString WarmTemplate = ASTEST_AS(R"AS(
			UCLASS()
			class UCacheProductionPropertyClass%s : UObject
			{
				UPROPERTY()
				int StoredValue = 31;

				UFUNCTION()
				int ReadUnchanged()
				{
					return StoredValue + 1100;
				}

				UFUNCTION()
				int ReadChanged()
				{
					return StoredValue + 2101;
				}
			}
			)AS");
		const FString ColdSource = ColdTemplate.Replace(
			TEXT("%s"), *Unique, ESearchCase::CaseSensitive);
		const FString WarmSource = WarmTemplate.Replace(
			TEXT("%s"), *Unique, ESearchCase::CaseSensitive);
		ASSERT_THAT(IsTrue(Project.WriteSource(
			TEXT("ClassPropertyBodyEdit.as"), ColdSource)));

		FAngelscriptHash256 ColdGenerationId;
		FAngelscriptHash256 ColdSourceSnapshot;
		FClassPropertyCoordinates ColdCoordinates;
		{
			TUniquePtr<FAngelscriptEngine> Cold = CreateEngine(Project);
			ASSERT_THAT(IsNotNull(Cold.Get()));
			FAngelscriptEngineScope ColdScope(*Cold);
			Cold->InitialCompile();
			const TSharedPtr<FAngelscriptModuleDesc> ColdModule =
				Cold->GetModuleByModuleName(TEXT("ClassPropertyBodyEdit"));
			ASSERT_THAT(IsTrue(ColdModule.IsValid()));
			FString CoordinateFailure;
			ASSERT_THAT(IsTrue(TryBuildCoordinates(
				ColdModule.ToSharedRef(), ColdCoordinates, CoordinateFailure),
				*CoordinateFailure));

			const FAngelscriptCacheLifecyclePublications ColdPublications =
				Cold->GetCacheService()->GetLifecyclePublications();
			ASSERT_THAT(IsTrue(ColdPublications.Current.IsValid()));
			ASSERT_THAT(AreEqual(1, ColdPublications.Current->Modules.Num(),
				TEXT("cold property class capture must publish the admitted module")));
			ColdSourceSnapshot = ColdPublications.Current->SourceSnapshot;
			const FAngelscriptCacheFlushApiResult Flush =
				FlushAngelscriptCacheToStore(Cold.Get(), 5.0);
			ASSERT_THAT(IsTrue(Flush.IsSuccess(), *Flush.Detail));
			ASSERT_THAT(IsTrue(Flush.Flush.Current.IsSuccess()));
			ColdGenerationId = Flush.Flush.Current.GenerationId;
		}

		ASSERT_THAT(IsTrue(Project.WriteSource(
			TEXT("ClassPropertyBodyEdit.as"), WarmSource)));
		FAngelscriptHash256 WarmGenerationId;
		FAngelscriptHash256 WarmSourceSnapshot;
		FClassPropertyCoordinates WarmCoordinates;
		{
		TUniquePtr<FAngelscriptEngine> Warm = CreateEngine(Project);
		ASSERT_THAT(IsNotNull(Warm.Get()));
		FAngelscriptEngineScope WarmScope(*Warm);
		FAngelscriptCacheService* Service = Warm->GetCacheService();
		ASSERT_THAT(IsNotNull(Service));
		Service->ConfigureDecisionTrace(true, 256);
		Warm->InitialCompile();

		const TSharedPtr<FAngelscriptModuleDesc> WarmModule =
			Warm->GetModuleByModuleName(TEXT("ClassPropertyBodyEdit"));
		ASSERT_THAT(IsTrue(WarmModule.IsValid()));
		FString CoordinateFailure;
		ASSERT_THAT(IsTrue(TryBuildCoordinates(
			WarmModule.ToSharedRef(), WarmCoordinates, CoordinateFailure),
			*CoordinateFailure));
		ASSERT_THAT(IsTrue(
			ColdCoordinates.ModuleKey.Hash == WarmCoordinates.ModuleKey.Hash));
		ASSERT_THAT(IsTrue(
			ColdCoordinates.TypeKey.Hash == WarmCoordinates.TypeKey.Hash));
		ASSERT_THAT(IsTrue(
			ColdCoordinates.PropertyKey.Hash == WarmCoordinates.PropertyKey.Hash));
		ASSERT_THAT(AreEqual(
			ColdCoordinates.VmPropertyOffset,
			WarmCoordinates.VmPropertyOffset));
		ASSERT_THAT(AreEqual(
			ColdCoordinates.ReflectedPropertyOffset,
			WarmCoordinates.ReflectedPropertyOffset));

		const TSharedRef<FAngelscriptClassDesc>& Class =
			WarmModule->Classes[0];
		const FIntProperty* StoredValueProperty = FindFProperty<FIntProperty>(
			Class->Class, TEXT("StoredValue"));
		ASSERT_THAT(IsNotNull(StoredValueProperty));
		UObject* DefaultObject = Class->Class->GetDefaultObject();
		ASSERT_THAT(IsNotNull(DefaultObject));
		UObject* Instance = NewObject<UObject>(
			GetTransientPackage(), Class->Class);
		ASSERT_THAT(IsNotNull(Instance));
		const int32 DefaultStoredValue =
			StoredValueProperty->GetPropertyValue_InContainer(DefaultObject);
		const int32 InstanceStoredValue =
			StoredValueProperty->GetPropertyValue_InContainer(Instance);
		FFunctionInvoker UnchangedInvoker(
			*TestRunner, Instance, FName(TEXT("ReadUnchanged")));
		FFunctionInvoker ChangedInvoker(
			*TestRunner, Instance, FName(TEXT("ReadChanged")));
		ASSERT_THAT(IsTrue(UnchangedInvoker.IsValid()));
		ASSERT_THAT(IsTrue(ChangedInvoker.IsValid()));
		const int32 UnchangedValue =
			UnchangedInvoker.CallAndReturn<int32>(INDEX_NONE);
		const int32 ChangedValue =
			ChangedInvoker.CallAndReturn<int32>(INDEX_NONE);

		const FAngelscriptCacheDecisionTraceSnapshot Trace =
			Service->CaptureDecisionTrace();
		const FAngelscriptCacheDecisionEvent* DirectInputMiss =
			Trace.Events.FindByPredicate(
				[](const FAngelscriptCacheDecisionEvent& Event)
				{
					return Event.Stage
							== EAngelscriptCacheDecisionStage::StartupRestore
						&& Event.Outcome
							== EAngelscriptCacheDecisionOutcome::Miss
						&& Event.ReasonCode == static_cast<uint32>(
							EAngelscriptCacheExactStartupReason::
								DirectInputMismatch);
				});
		const FAngelscriptCacheDecisionEvent* UnchangedHit =
			FindFunctionEvent(
				Trace, WarmCoordinates.UnchangedFunctionKey,
				EAngelscriptCacheDecisionOutcome::Restored);
		const FAngelscriptCacheDecisionEvent* ChangedCompile =
			FindFunctionEvent(
				Trace, WarmCoordinates.ChangedFunctionKey,
				EAngelscriptCacheDecisionOutcome::Compiled);
		for (const FAngelscriptCacheDecisionEvent& Event : Trace.Events)
		{
			TestRunner->AddInfo(FString::Printf(
				TEXT("Property-class reuse trace: Stage=%u Outcome=%u Domain=%u Reason=%u Module=%s Function=%s Candidate=%s Detail=%s"),
				static_cast<uint32>(Event.Stage),
				static_cast<uint32>(Event.Outcome),
				static_cast<uint32>(Event.ReasonDomain),
				Event.ReasonCode,
				Event.ModuleKeys.IsEmpty()
					? TEXT("none")
					: *Event.ModuleKeys[0].Hash.ToHexString(),
				Event.FunctionKey.IsSet()
					? *Event.FunctionKey->Hash.ToHexString()
					: TEXT("none"),
				Event.ExpectedCoordinate.IsSet()
					? *Event.ExpectedCoordinate->ToHexString()
					: TEXT("none"),
				*Event.Detail));
		}
		const FAngelscriptCacheLifecyclePublications WarmPublications =
			Service->GetLifecyclePublications();
		TestRunner->AddInfo(FString::Printf(
			TEXT("Production property-class observation: Candidate=%s ColdSource=%s CurrentSource=%s Module=%s Type=%s Property=%s Unchanged=%s Changed=%s VmOffset=%d ReflectedOffset=%d Defaults=%d/%d Values=%d/%d TraceEvents=%d Restored=%d Compiled=%d PublishedModules=%d"),
			*ColdGenerationId.ToHexString(),
			*ColdSourceSnapshot.ToHexString(),
			WarmPublications.Current.IsValid()
				? *WarmPublications.Current->SourceSnapshot.ToHexString()
				: TEXT("none"),
			*WarmCoordinates.ModuleKey.Hash.ToHexString(),
			*WarmCoordinates.TypeKey.Hash.ToHexString(),
			*WarmCoordinates.PropertyKey.Hash.ToHexString(),
			*WarmCoordinates.UnchangedFunctionKey.Hash.ToHexString(),
			*WarmCoordinates.ChangedFunctionKey.Hash.ToHexString(),
			WarmCoordinates.VmPropertyOffset,
			WarmCoordinates.ReflectedPropertyOffset,
			DefaultStoredValue,
			InstanceStoredValue,
			UnchangedValue,
			ChangedValue,
			Trace.Events.Num(),
			UnchangedHit != nullptr ? 1 : 0,
			ChangedCompile != nullptr ? 1 : 0,
			WarmPublications.Current.IsValid()
				? WarmPublications.Current->Modules.Num() : 0));

		ASSERT_THAT(AreEqual(31, DefaultStoredValue));
		ASSERT_THAT(AreEqual(31, InstanceStoredValue));
		ASSERT_THAT(AreEqual(1131, UnchangedValue));
		ASSERT_THAT(AreEqual(2132, ChangedValue));
		ASSERT_THAT(IsNotNull(DirectInputMiss));
		ASSERT_THAT(IsNotNull(UnchangedHit,
			TEXT("unchanged reflected method reading a stable property must restore before its compiler closure")));
		ASSERT_THAT(IsNotNull(ChangedCompile,
			TEXT("body-edited reflected method must compile against current property authority")));
		ASSERT_THAT(IsTrue(UnchangedHit->ExpectedCoordinate.IsSet()));
		ASSERT_THAT(IsTrue(
			UnchangedHit->ExpectedCoordinate.GetValue() == ColdGenerationId));
		ASSERT_THAT(IsTrue(ChangedCompile->ExpectedCoordinate.IsSet()));
		ASSERT_THAT(IsTrue(
			ChangedCompile->ExpectedCoordinate.GetValue() == ColdGenerationId));
		ASSERT_THAT(IsNull(FindFunctionEvent(
			Trace, WarmCoordinates.ChangedFunctionKey,
			EAngelscriptCacheDecisionOutcome::Restored)));
		ASSERT_THAT(IsNull(FindFunctionEvent(
			Trace, WarmCoordinates.UnchangedFunctionKey,
			EAngelscriptCacheDecisionOutcome::Compiled)));
		ASSERT_THAT(IsTrue(WarmPublications.Current.IsValid()));
		ASSERT_THAT(AreEqual(1, WarmPublications.Current->Modules.Num()));
		ASSERT_THAT(IsTrue(
			WarmPublications.Current->SourceSnapshot != ColdSourceSnapshot));
		ASSERT_THAT(IsFalse(WarmPublications.Current->bRestoredFromStore));
		WarmSourceSnapshot = WarmPublications.Current->SourceSnapshot;
		const FAngelscriptCacheFlushApiResult WarmFlush =
			FlushAngelscriptCacheToStore(Warm.Get(), 5.0);
		ASSERT_THAT(IsTrue(WarmFlush.IsSuccess(), *WarmFlush.Detail));
		ASSERT_THAT(IsTrue(WarmFlush.Flush.Current.IsSuccess()));
		WarmGenerationId = WarmFlush.Flush.Current.GenerationId;
		}

		TUniquePtr<FAngelscriptEngine> Third = CreateEngine(Project);
		ASSERT_THAT(IsNotNull(Third.Get()));
		FAngelscriptEngineScope ThirdScope(*Third);
		FAngelscriptCacheService* ThirdService = Third->GetCacheService();
		ASSERT_THAT(IsNotNull(ThirdService));
		ThirdService->ConfigureDecisionTrace(true, 256);
		Third->InitialCompile();

		const TSharedPtr<FAngelscriptModuleDesc> ThirdModule =
			Third->GetModuleByModuleName(TEXT("ClassPropertyBodyEdit"));
		ASSERT_THAT(IsTrue(ThirdModule.IsValid()));
		FClassPropertyCoordinates ThirdCoordinates;
		FString ThirdCoordinateFailure;
		ASSERT_THAT(IsTrue(TryBuildCoordinates(
			ThirdModule.ToSharedRef(), ThirdCoordinates,
			ThirdCoordinateFailure), *ThirdCoordinateFailure));

		const TSharedRef<FAngelscriptClassDesc>& ThirdClass =
			ThirdModule->Classes[0];
		const FIntProperty* ThirdStoredValueProperty =
			FindFProperty<FIntProperty>(
				ThirdClass->Class, TEXT("StoredValue"));
		ASSERT_THAT(IsNotNull(ThirdStoredValueProperty));
		UObject* ThirdDefaultObject = ThirdClass->Class->GetDefaultObject();
		ASSERT_THAT(IsNotNull(ThirdDefaultObject));
		UObject* ThirdInstance = NewObject<UObject>(
			GetTransientPackage(), ThirdClass->Class);
		ASSERT_THAT(IsNotNull(ThirdInstance));
		const int32 ThirdDefaultStoredValue =
			ThirdStoredValueProperty->GetPropertyValue_InContainer(
				ThirdDefaultObject);
		const int32 ThirdInstanceStoredValue =
			ThirdStoredValueProperty->GetPropertyValue_InContainer(
				ThirdInstance);
		FFunctionInvoker ThirdUnchangedInvoker(
			*TestRunner, ThirdInstance, FName(TEXT("ReadUnchanged")));
		FFunctionInvoker ThirdChangedInvoker(
			*TestRunner, ThirdInstance, FName(TEXT("ReadChanged")));
		ASSERT_THAT(IsTrue(ThirdUnchangedInvoker.IsValid()));
		ASSERT_THAT(IsTrue(ThirdChangedInvoker.IsValid()));
		const int32 ThirdUnchangedValue =
			ThirdUnchangedInvoker.CallAndReturn<int32>(INDEX_NONE);
		const int32 ThirdChangedValue =
			ThirdChangedInvoker.CallAndReturn<int32>(INDEX_NONE);

		const FAngelscriptCacheDecisionTraceSnapshot ThirdTrace =
			ThirdService->CaptureDecisionTrace();
		const FAngelscriptCacheDecisionEvent* ExactModuleIneligible =
			ThirdTrace.Events.FindByPredicate(
				[](const FAngelscriptCacheDecisionEvent& Event)
				{
					return Event.Stage
							== EAngelscriptCacheDecisionStage::StartupRestore
						&& Event.Outcome
							== EAngelscriptCacheDecisionOutcome::Miss
						&& Event.ReasonCode == static_cast<uint32>(
							EAngelscriptCacheExactStartupReason::ModuleIneligible);
				});
		const FAngelscriptCacheDecisionEvent* ThirdUnchangedHit =
			FindFunctionEvent(
				ThirdTrace, ThirdCoordinates.UnchangedFunctionKey,
				EAngelscriptCacheDecisionOutcome::Restored);
		const FAngelscriptCacheDecisionEvent* ThirdChangedHit =
			FindFunctionEvent(
				ThirdTrace, ThirdCoordinates.ChangedFunctionKey,
				EAngelscriptCacheDecisionOutcome::Restored);
		const FAngelscriptCacheDecisionEvent* StaticClassNotCacheable =
			ThirdTrace.Events.FindByPredicate(
				[](const FAngelscriptCacheDecisionEvent& Event)
				{
					return Event.Stage
							== EAngelscriptCacheDecisionStage::FunctionLookup
						&& Event.Outcome
							== EAngelscriptCacheDecisionOutcome::NotCacheable
						&& Event.Detail.Contains(
							TEXT("Declaration=UClass StaticClass()"));
				});
		const FAngelscriptCacheDecisionEvent* StaticClassCompile =
			StaticClassNotCacheable != nullptr
				&& StaticClassNotCacheable->FunctionKey.IsSet()
			? FindFunctionEvent(
				ThirdTrace,
				StaticClassNotCacheable->FunctionKey.GetValue(),
				EAngelscriptCacheDecisionOutcome::Compiled)
			: nullptr;
		int32 RestoredFunctionCount = 0;
		for (const FAngelscriptCacheDecisionEvent& Event : ThirdTrace.Events)
		{
			if (Event.Stage
					== EAngelscriptCacheDecisionStage::FunctionLookup
				&& Event.Outcome
					== EAngelscriptCacheDecisionOutcome::Restored)
			{
				++RestoredFunctionCount;
			}
		}
		const FAngelscriptCacheDecisionEvent* UnexplainedFunctionKeyMiss =
			ThirdTrace.Events.FindByPredicate(
				[](const FAngelscriptCacheDecisionEvent& Event)
				{
					return Event.Stage
							== EAngelscriptCacheDecisionStage::FunctionLookup
						&& Event.Outcome
							== EAngelscriptCacheDecisionOutcome::Miss
						&& Event.ReasonCode == static_cast<uint32>(
							EAngelscriptCacheFunctionCandidateLookupStatus::
								FunctionKeyMiss);
				});
		for (const FAngelscriptCacheDecisionEvent& Event : ThirdTrace.Events)
		{
			TestRunner->AddInfo(FString::Printf(
				TEXT("Property-class third-generation trace: Stage=%u Outcome=%u Domain=%u Reason=%u Candidate=%s Detail=%s"),
				static_cast<uint32>(Event.Stage),
				static_cast<uint32>(Event.Outcome),
				static_cast<uint32>(Event.ReasonDomain),
				Event.ReasonCode,
				Event.ExpectedCoordinate.IsSet()
					? *Event.ExpectedCoordinate->ToHexString()
					: TEXT("none"),
				*Event.Detail));
		}
		const FAngelscriptCacheLifecyclePublications ThirdPublications =
			ThirdService->GetLifecyclePublications();
		TestRunner->AddInfo(FString::Printf(
			TEXT("Property-class third-generation observation: WarmGeneration=%s WarmSource=%s Module=%s Type=%s Property=%s Offsets=%d/%d Defaults=%d/%d Values=%d/%d TraceEvents=%d ExactModuleIneligible=%d RestoredFunctions=%d StaticClassNotCacheable=%d StaticClassCompiled=%d UnexplainedFunctionKeyMiss=%d RestoredFromStore=%d"),
			*WarmGenerationId.ToHexString(),
			*WarmSourceSnapshot.ToHexString(),
			*ThirdCoordinates.ModuleKey.Hash.ToHexString(),
			*ThirdCoordinates.TypeKey.Hash.ToHexString(),
			*ThirdCoordinates.PropertyKey.Hash.ToHexString(),
			ThirdCoordinates.VmPropertyOffset,
			ThirdCoordinates.ReflectedPropertyOffset,
			ThirdDefaultStoredValue,
			ThirdInstanceStoredValue,
			ThirdUnchangedValue,
			ThirdChangedValue,
			ThirdTrace.Events.Num(),
			ExactModuleIneligible != nullptr ? 1 : 0,
			RestoredFunctionCount,
			StaticClassNotCacheable != nullptr ? 1 : 0,
			StaticClassCompile != nullptr ? 1 : 0,
			UnexplainedFunctionKeyMiss != nullptr ? 1 : 0,
			ThirdPublications.Current.IsValid()
				&& ThirdPublications.Current->bRestoredFromStore ? 1 : 0));

		ASSERT_THAT(IsTrue(
			ThirdCoordinates.ModuleKey.Hash == WarmCoordinates.ModuleKey.Hash));
		ASSERT_THAT(IsTrue(
			ThirdCoordinates.TypeKey.Hash == WarmCoordinates.TypeKey.Hash));
		ASSERT_THAT(IsTrue(
			ThirdCoordinates.PropertyKey.Hash == WarmCoordinates.PropertyKey.Hash));
		ASSERT_THAT(AreEqual(
			WarmCoordinates.VmPropertyOffset,
			ThirdCoordinates.VmPropertyOffset));
		ASSERT_THAT(AreEqual(
			WarmCoordinates.ReflectedPropertyOffset,
			ThirdCoordinates.ReflectedPropertyOffset));
		ASSERT_THAT(AreEqual(31, ThirdDefaultStoredValue));
		ASSERT_THAT(AreEqual(31, ThirdInstanceStoredValue));
		ASSERT_THAT(AreEqual(1131, ThirdUnchangedValue));
		ASSERT_THAT(AreEqual(2132, ThirdChangedValue));
		ASSERT_THAT(IsNotNull(ExactModuleIneligible));
		ASSERT_THAT(AreEqual(4, RestoredFunctionCount));
		ASSERT_THAT(IsNotNull(ThirdUnchangedHit));
		ASSERT_THAT(IsNotNull(ThirdChangedHit));
		ASSERT_THAT(IsTrue(ThirdUnchangedHit->ExpectedCoordinate.IsSet()));
		ASSERT_THAT(IsTrue(
			ThirdUnchangedHit->ExpectedCoordinate.GetValue()
				== WarmGenerationId));
		ASSERT_THAT(IsTrue(ThirdChangedHit->ExpectedCoordinate.IsSet()));
		ASSERT_THAT(IsTrue(
			ThirdChangedHit->ExpectedCoordinate.GetValue() == WarmGenerationId));
		ASSERT_THAT(IsNotNull(StaticClassNotCacheable));
		ASSERT_THAT(IsNotNull(StaticClassCompile));
		ASSERT_THAT(IsNull(UnexplainedFunctionKeyMiss));
		ASSERT_THAT(IsTrue(ThirdPublications.Current.IsValid()));
		ASSERT_THAT(IsFalse(ThirdPublications.Current->bRestoredFromStore,
			TEXT("class modules use typed hybrid fallback outside the first exact-start enum/global vertical")));
		ASSERT_THAT(IsTrue(
			ThirdPublications.Current->SourceSnapshot == WarmSourceSnapshot));
		ASSERT_THAT(AreEqual(1, ThirdPublications.Current->Modules.Num()));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
