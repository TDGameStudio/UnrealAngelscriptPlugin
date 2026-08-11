#include "Cache/AngelscriptCacheCompilerBridge.h"
#include "Cache/AngelscriptCacheExactStartup.h"
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

namespace AngelscriptCacheProductionClassGraphCompilerReuseTests_Private
{
	class FScopedProjectRoot final
	{
	public:
		FScopedProjectRoot()
		{
			Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("Automation/AngelscriptCacheProductionClassGraphCompilerReuse"),
				FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			FPaths::NormalizeDirectoryName(Root);
			check(Root.Contains(TEXT(
				"/Saved/Automation/AngelscriptCacheProductionClassGraphCompilerReuse/")));
			ScriptRoot = Root / TEXT("Script");
			CacheRoot = Root / TEXT("CacheV2");
			check(IFileManager::Get().MakeDirectory(*ScriptRoot, true));
		}

		~FScopedProjectRoot()
		{
			if (Root.Contains(TEXT(
				"/Saved/Automation/AngelscriptCacheProductionClassGraphCompilerReuse/")))
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

	static const TSharedRef<FAngelscriptClassDesc>* FindClass(
		const FAngelscriptModuleDesc& Module,
		const FStringView ClassName)
	{
		const FString OwnedClassName(ClassName);
		return Module.Classes.FindByPredicate(
			[&OwnedClassName](const TSharedRef<FAngelscriptClassDesc>& Candidate)
			{
				return Candidate->ClassName.Equals(
					OwnedClassName, ESearchCase::CaseSensitive);
			});
	}

	struct FGraphCoordinates final
	{
		FAngelscriptStableModuleKey ModuleKey;
		FAngelscriptStableFunctionKey BaseCompute;
		FAngelscriptStableFunctionKey BaseStable;
		FAngelscriptStableFunctionKey DerivedCompute;
		FAngelscriptStableFunctionKey DerivedStable;
		FAngelscriptStableFunctionKey DerivedChanged;
		int32 BaseVmPropertyOffset = INDEX_NONE;
		int32 DerivedVmPropertyOffset = INDEX_NONE;
		int32 BaseReflectedPropertyOffset = INDEX_NONE;
		int32 DerivedReflectedPropertyOffset = INDEX_NONE;
	};

	static bool TryBuildMethodKey(
		const FAngelscriptStableModuleKey& ModuleKey,
		asCObjectType& Type,
		const char* Declaration,
		FAngelscriptStableFunctionKey& OutKey,
		FString& OutFailure)
	{
		asIScriptFunction* Method = Type.GetMethodByDecl(Declaration);
		if (Method == nullptr)
		{
			OutFailure = FString::Printf(
				TEXT("Missing method declaration %s on %s"),
				UTF8_TO_TCHAR(Declaration), UTF8_TO_TCHAR(Type.GetName()));
			return false;
		}
		return FAngelscriptCacheStableSymbolIdentity::TryBuildFunctionKey(
			ModuleKey,
			*static_cast<asCScriptFunction*>(Method),
			OutKey,
			&OutFailure);
	}

	static bool TryCaptureCoordinates(
		const TSharedRef<FAngelscriptModuleDesc>& Module,
		const FStringView BaseClassName,
		const FStringView DerivedClassName,
		FGraphCoordinates& OutCoordinates,
		FString& OutFailure)
	{
		OutCoordinates = {};
		if (Module->ScriptModule == nullptr || Module->Classes.Num() != 2)
		{
			OutFailure = TEXT("Expected one compiled module with two script classes");
			return false;
		}
		const TSharedRef<FAngelscriptClassDesc>* Base =
			FindClass(*Module, BaseClassName);
		const TSharedRef<FAngelscriptClassDesc>* Derived =
			FindClass(*Module, DerivedClassName);
		if (Base == nullptr || Derived == nullptr
			|| (*Base)->ScriptType == nullptr || (*Derived)->ScriptType == nullptr
			|| (*Base)->Class == nullptr || (*Derived)->Class == nullptr
			|| (*Derived)->Class->GetSuperClass() != (*Base)->Class)
		{
			OutFailure = TEXT("ClassGenerator did not materialize the expected base/derived UClass graph");
			return false;
		}

		asCObjectType& BaseType =
			*static_cast<asCObjectType*>((*Base)->ScriptType);
		asCObjectType& DerivedType =
			*static_cast<asCObjectType*>((*Derived)->ScriptType);
		if (BaseType.localProperties.GetLength() != 1
			|| DerivedType.localProperties.GetLength() != 1
			|| BaseType.localProperties[0] == nullptr
			|| DerivedType.localProperties[0] == nullptr
			|| DerivedType.derivedFrom != &BaseType)
		{
			OutFailure = TEXT("The staging VM class graph/property layout is incomplete");
			return false;
		}

		if (!FAngelscriptCacheStableSymbolIdentity::TryBuildModuleKey(
			*Module, OutCoordinates.ModuleKey, &OutFailure)
			|| !TryBuildMethodKey(
				OutCoordinates.ModuleKey, BaseType, "int Compute()",
				OutCoordinates.BaseCompute, OutFailure)
			|| !TryBuildMethodKey(
				OutCoordinates.ModuleKey, BaseType, "int BaseStable()",
				OutCoordinates.BaseStable, OutFailure)
			|| !TryBuildMethodKey(
				OutCoordinates.ModuleKey, DerivedType, "int Compute()",
				OutCoordinates.DerivedCompute, OutFailure)
			|| !TryBuildMethodKey(
				OutCoordinates.ModuleKey, DerivedType, "int ReadStable()",
				OutCoordinates.DerivedStable, OutFailure)
			|| !TryBuildMethodKey(
				OutCoordinates.ModuleKey, DerivedType, "int ReadChanged()",
				OutCoordinates.DerivedChanged, OutFailure))
		{
			return false;
		}

		const FIntProperty* BaseProperty = FindFProperty<FIntProperty>(
			(*Base)->Class, TEXT("BaseValue"));
		const FIntProperty* DerivedProperty = FindFProperty<FIntProperty>(
			(*Derived)->Class, TEXT("DerivedValue"));
		if (BaseProperty == nullptr || DerivedProperty == nullptr)
		{
			OutFailure = TEXT("Reflected base/derived properties are absent");
			return false;
		}
		OutCoordinates.BaseVmPropertyOffset =
			BaseType.localProperties[0]->byteOffset;
		OutCoordinates.DerivedVmPropertyOffset =
			DerivedType.localProperties[0]->byteOffset;
		OutCoordinates.BaseReflectedPropertyOffset =
			BaseProperty->GetOffset_ForInternal();
		OutCoordinates.DerivedReflectedPropertyOffset =
			DerivedProperty->GetOffset_ForInternal();
		return true;
	}

	struct FExecutionObservation final
	{
		int32 BaseDefault = 0;
		int32 DerivedDefault = 0;
		int32 StableValue = 0;
		int32 ChangedValue = 0;
	};

	static bool TryExecuteDerived(
		FAutomationTestBase& Runner,
		const TSharedRef<FAngelscriptModuleDesc>& Module,
		const FStringView DerivedClassName,
		FExecutionObservation& OutObservation,
		FString& OutFailure)
	{
		const TSharedRef<FAngelscriptClassDesc>* Derived =
			FindClass(*Module, DerivedClassName);
		if (Derived == nullptr || (*Derived)->Class == nullptr)
		{
			OutFailure = TEXT("The derived reflected class is absent");
			return false;
		}
		const FIntProperty* BaseProperty = FindFProperty<FIntProperty>(
			(*Derived)->Class, TEXT("BaseValue"));
		const FIntProperty* DerivedProperty = FindFProperty<FIntProperty>(
			(*Derived)->Class, TEXT("DerivedValue"));
		UObject* Instance = NewObject<UObject>(
			GetTransientPackage(), (*Derived)->Class);
		if (BaseProperty == nullptr || DerivedProperty == nullptr
			|| Instance == nullptr)
		{
			OutFailure = TEXT("The derived object or inherited/local properties are absent");
			return false;
		}
		OutObservation.BaseDefault =
			BaseProperty->GetPropertyValue_InContainer(Instance);
		OutObservation.DerivedDefault =
			DerivedProperty->GetPropertyValue_InContainer(Instance);
		BaseProperty->SetPropertyValue_InContainer(Instance, 7);
		DerivedProperty->SetPropertyValue_InContainer(Instance, 13);
		FFunctionInvoker StableInvoker(
			Runner, Instance, FName(TEXT("ReadStable")));
		FFunctionInvoker ChangedInvoker(
			Runner, Instance, FName(TEXT("ReadChanged")));
		if (!StableInvoker.IsValid() || !ChangedInvoker.IsValid())
		{
			OutFailure = TEXT("The derived reflected invocation surface is incomplete");
			return false;
		}
		OutObservation.StableValue =
			StableInvoker.CallAndReturn<int32>(INDEX_NONE);
		OutObservation.ChangedValue =
			ChangedInvoker.CallAndReturn<int32>(INDEX_NONE);
		return true;
	}

	static const FAngelscriptCacheDecisionEvent* FindFunctionEvent(
		const FAngelscriptCacheDecisionTraceSnapshot& Trace,
		const FAngelscriptStableFunctionKey& FunctionKey,
		const EAngelscriptCacheDecisionOutcome Outcome)
	{
		return Trace.Events.FindByPredicate(
			[&FunctionKey, Outcome](const FAngelscriptCacheDecisionEvent& Event)
			{
				return Event.Stage
						== EAngelscriptCacheDecisionStage::FunctionLookup
					&& Event.Outcome == Outcome
					&& Event.FunctionKey.IsSet()
					&& Event.FunctionKey->Hash == FunctionKey.Hash;
			});
	}

	static void DumpTrace(
		FAutomationTestBase& Runner,
		const TCHAR* Label,
		const FAngelscriptCacheDecisionTraceSnapshot& Trace)
	{
		for (const FAngelscriptCacheDecisionEvent& Event : Trace.Events)
		{
			Runner.AddInfo(FString::Printf(
				TEXT("%s: Stage=%u Outcome=%u Domain=%u Reason=%u Module=%s Function=%s Candidate=%s Detail=%s"),
				Label,
				static_cast<uint32>(Event.Stage),
				static_cast<uint32>(Event.Outcome),
				static_cast<uint32>(Event.ReasonDomain),
				Event.ReasonCode,
				Event.ModuleKeys.IsEmpty()
					? TEXT("none") : *Event.ModuleKeys[0].Hash.ToHexString(),
				Event.FunctionKey.IsSet()
					? *Event.FunctionKey->Hash.ToHexString() : TEXT("none"),
				Event.ExpectedCoordinate.IsSet()
					? *Event.ExpectedCoordinate->ToHexString() : TEXT("none"),
				*Event.Detail));
		}
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheProductionClassGraphCompilerReuseTests,
	"Angelscript.TestModule.Cache.ProductionClassGraphCompilerReuse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(BodyEditRestoresUnchangedInheritedGraphAcrossThreeEngines)
	{
		using namespace
			AngelscriptCacheProductionClassGraphCompilerReuseTests_Private;
		FScopedProjectRoot Project;
		const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const FString ColdTemplate = ASTEST_AS(R"AS(
			UCLASS()
			class UCacheProductionGraphBase%s : UObject
			{
				UPROPERTY()
				int BaseValue;

				default BaseValue = 7;

				UFUNCTION(BlueprintEvent)
				int Compute()
				{
					return BaseValue + 100;
				}

				int BaseStable()
				{
					return BaseValue + 1000;
				}
			}

			UCLASS()
			class UCacheProductionGraphDerived%s : UCacheProductionGraphBase%s
			{
				UPROPERTY()
				int DerivedValue;

				default DerivedValue = 13;

				UFUNCTION(BlueprintOverride)
				int Compute()
				{
					return Super::Compute() + DerivedValue + 10;
				}

				UFUNCTION()
				int ReadStable()
				{
					return BaseStable() + DerivedValue;
				}

				UFUNCTION()
				int ReadChanged()
				{
					return Compute() + 1;
				}
			}
			)AS");
		const FString WarmTemplate = ColdTemplate.Replace(
			TEXT("return Compute() + 1;"),
			TEXT("return Compute() + 2;"),
			ESearchCase::CaseSensitive);
		const FString ColdSource = ColdTemplate.Replace(
			TEXT("%s"), *Unique, ESearchCase::CaseSensitive);
		const FString WarmSource = WarmTemplate.Replace(
			TEXT("%s"), *Unique, ESearchCase::CaseSensitive);
		const FString BaseClassName =
			TEXT("UCacheProductionGraphBase") + Unique;
		const FString DerivedClassName =
			TEXT("UCacheProductionGraphDerived") + Unique;
		ASSERT_THAT(IsTrue(Project.WriteSource(
			TEXT("ClassGraphBodyEdit.as"), ColdSource)));

		FAngelscriptHash256 ColdGenerationId;
		FAngelscriptHash256 ColdSourceSnapshot;
		FGraphCoordinates ColdCoordinates;
		FExecutionObservation ColdObservation;
		{
			TUniquePtr<FAngelscriptEngine> Cold = CreateEngine(Project);
			ASSERT_THAT(IsNotNull(Cold.Get()));
			FAngelscriptEngineScope ColdScope(*Cold);
			Cold->InitialCompile();
			const TSharedPtr<FAngelscriptModuleDesc> Module =
				Cold->GetModuleByModuleName(TEXT("ClassGraphBodyEdit"));
			ASSERT_THAT(IsTrue(Module.IsValid()));
			FString Failure;
			ASSERT_THAT(IsTrue(TryCaptureCoordinates(
				Module.ToSharedRef(), BaseClassName, DerivedClassName,
				ColdCoordinates, Failure), *Failure));
			ASSERT_THAT(IsTrue(TryExecuteDerived(
				*TestRunner, Module.ToSharedRef(), DerivedClassName,
				ColdObservation, Failure), *Failure));
			ASSERT_THAT(AreEqual(1020, ColdObservation.StableValue));
			ASSERT_THAT(AreEqual(131, ColdObservation.ChangedValue));
			TestRunner->AddInfo(FString::Printf(
				TEXT("Class-graph cold defaults before explicit execution values: Base=%d Derived=%d"),
				ColdObservation.BaseDefault, ColdObservation.DerivedDefault));
			const FAngelscriptCacheLifecyclePublications Publications =
				Cold->GetCacheService()->GetLifecyclePublications();
			ASSERT_THAT(IsTrue(Publications.Current.IsValid()));
			ASSERT_THAT(AreEqual(1, Publications.Current->Modules.Num()));
			ColdSourceSnapshot = Publications.Current->SourceSnapshot;
			const FAngelscriptCacheFlushApiResult Flush =
				FlushAngelscriptCacheToStore(Cold.Get(), 5.0);
			ASSERT_THAT(IsTrue(Flush.IsSuccess(), *Flush.Detail));
			ASSERT_THAT(IsTrue(Flush.Flush.Current.IsSuccess()));
			ColdGenerationId = Flush.Flush.Current.GenerationId;
		}

		ASSERT_THAT(IsTrue(Project.WriteSource(
			TEXT("ClassGraphBodyEdit.as"), WarmSource)));
		FAngelscriptHash256 WarmGenerationId;
		FAngelscriptHash256 WarmSourceSnapshot;
		FGraphCoordinates WarmCoordinates;
		{
			TUniquePtr<FAngelscriptEngine> Warm = CreateEngine(Project);
			ASSERT_THAT(IsNotNull(Warm.Get()));
			FAngelscriptEngineScope WarmScope(*Warm);
			FAngelscriptCacheService* Service = Warm->GetCacheService();
			ASSERT_THAT(IsNotNull(Service));
			Service->ConfigureDecisionTrace(true, 512);
			Warm->InitialCompile();
			const TSharedPtr<FAngelscriptModuleDesc> Module =
				Warm->GetModuleByModuleName(TEXT("ClassGraphBodyEdit"));
			ASSERT_THAT(IsTrue(Module.IsValid()));
			FString Failure;
			ASSERT_THAT(IsTrue(TryCaptureCoordinates(
				Module.ToSharedRef(), BaseClassName, DerivedClassName,
				WarmCoordinates, Failure), *Failure));
			FExecutionObservation Observation;
			ASSERT_THAT(IsTrue(TryExecuteDerived(
				*TestRunner, Module.ToSharedRef(), DerivedClassName,
				Observation, Failure), *Failure));
			ASSERT_THAT(AreEqual(1020, Observation.StableValue));
			ASSERT_THAT(AreEqual(132, Observation.ChangedValue));
			ASSERT_THAT(IsTrue(
				ColdCoordinates.ModuleKey.Hash == WarmCoordinates.ModuleKey.Hash));
			ASSERT_THAT(AreEqual(
				ColdCoordinates.BaseVmPropertyOffset,
				WarmCoordinates.BaseVmPropertyOffset));
			ASSERT_THAT(AreEqual(
				ColdCoordinates.DerivedVmPropertyOffset,
				WarmCoordinates.DerivedVmPropertyOffset));
			ASSERT_THAT(AreEqual(
				ColdCoordinates.BaseReflectedPropertyOffset,
				WarmCoordinates.BaseReflectedPropertyOffset));
			ASSERT_THAT(AreEqual(
				ColdCoordinates.DerivedReflectedPropertyOffset,
				WarmCoordinates.DerivedReflectedPropertyOffset));

			const FAngelscriptCacheDecisionTraceSnapshot Trace =
				Service->CaptureDecisionTrace();
			DumpTrace(*TestRunner, TEXT("Class-graph warm trace"), Trace);
			const FAngelscriptCacheDecisionEvent* BaseComputeHit =
				FindFunctionEvent(Trace, WarmCoordinates.BaseCompute,
					EAngelscriptCacheDecisionOutcome::Restored);
			const FAngelscriptCacheDecisionEvent* BaseStableHit =
				FindFunctionEvent(Trace, WarmCoordinates.BaseStable,
					EAngelscriptCacheDecisionOutcome::Restored);
			const FAngelscriptCacheDecisionEvent* DerivedComputeHit =
				FindFunctionEvent(Trace, WarmCoordinates.DerivedCompute,
					EAngelscriptCacheDecisionOutcome::Restored);
			const FAngelscriptCacheDecisionEvent* DerivedStableHit =
				FindFunctionEvent(Trace, WarmCoordinates.DerivedStable,
					EAngelscriptCacheDecisionOutcome::Restored);
			const FAngelscriptCacheDecisionEvent* DerivedChangedCompile =
				FindFunctionEvent(Trace, WarmCoordinates.DerivedChanged,
					EAngelscriptCacheDecisionOutcome::Compiled);
			TestRunner->AddInfo(FString::Printf(
				TEXT("Class-graph warm observation: Candidate=%s Source=%s Module=%s Offsets=%d/%d/%d/%d Defaults=%d/%d Values=%d/%d Trace=%d Restored=%d%d%d%d ChangedCompiled=%d"),
				*ColdGenerationId.ToHexString(),
				*ColdSourceSnapshot.ToHexString(),
				*WarmCoordinates.ModuleKey.Hash.ToHexString(),
				WarmCoordinates.BaseVmPropertyOffset,
				WarmCoordinates.BaseReflectedPropertyOffset,
				WarmCoordinates.DerivedVmPropertyOffset,
				WarmCoordinates.DerivedReflectedPropertyOffset,
				Observation.BaseDefault, Observation.DerivedDefault,
				Observation.StableValue, Observation.ChangedValue,
				Trace.Events.Num(), BaseComputeHit != nullptr,
				BaseStableHit != nullptr, DerivedComputeHit != nullptr,
				DerivedStableHit != nullptr,
				DerivedChangedCompile != nullptr));
			ASSERT_THAT(IsNotNull(BaseComputeHit));
			ASSERT_THAT(IsNotNull(BaseStableHit));
			ASSERT_THAT(IsNotNull(DerivedComputeHit));
			ASSERT_THAT(IsNotNull(DerivedStableHit));
			ASSERT_THAT(IsNotNull(DerivedChangedCompile));
			ASSERT_THAT(IsNull(FindFunctionEvent(
				Trace, WarmCoordinates.DerivedChanged,
				EAngelscriptCacheDecisionOutcome::Restored)));
			const FAngelscriptCacheLifecyclePublications Publications =
				Service->GetLifecyclePublications();
			ASSERT_THAT(IsTrue(Publications.Current.IsValid()));
			ASSERT_THAT(AreEqual(1, Publications.Current->Modules.Num()));
			ASSERT_THAT(IsTrue(
				Publications.Current->SourceSnapshot != ColdSourceSnapshot));
			WarmSourceSnapshot = Publications.Current->SourceSnapshot;
			const FAngelscriptCacheFlushApiResult Flush =
				FlushAngelscriptCacheToStore(Warm.Get(), 5.0);
			ASSERT_THAT(IsTrue(Flush.IsSuccess(), *Flush.Detail));
			ASSERT_THAT(IsTrue(Flush.Flush.Current.IsSuccess()));
			WarmGenerationId = Flush.Flush.Current.GenerationId;
		}

		TUniquePtr<FAngelscriptEngine> Third = CreateEngine(Project);
		ASSERT_THAT(IsNotNull(Third.Get()));
		FAngelscriptEngineScope ThirdScope(*Third);
		FAngelscriptCacheService* ThirdService = Third->GetCacheService();
		ASSERT_THAT(IsNotNull(ThirdService));
		ThirdService->ConfigureDecisionTrace(true, 512);
		Third->InitialCompile();
		const TSharedPtr<FAngelscriptModuleDesc> ThirdModule =
			Third->GetModuleByModuleName(TEXT("ClassGraphBodyEdit"));
		ASSERT_THAT(IsTrue(ThirdModule.IsValid()));
		FGraphCoordinates ThirdCoordinates;
		FString ThirdFailure;
		ASSERT_THAT(IsTrue(TryCaptureCoordinates(
			ThirdModule.ToSharedRef(), BaseClassName, DerivedClassName,
			ThirdCoordinates, ThirdFailure), *ThirdFailure));
		FExecutionObservation ThirdObservation;
		ASSERT_THAT(IsTrue(TryExecuteDerived(
			*TestRunner, ThirdModule.ToSharedRef(), DerivedClassName,
			ThirdObservation, ThirdFailure), *ThirdFailure));
		ASSERT_THAT(AreEqual(1020, ThirdObservation.StableValue));
		ASSERT_THAT(AreEqual(132, ThirdObservation.ChangedValue));

		const FAngelscriptCacheDecisionTraceSnapshot ThirdTrace =
			ThirdService->CaptureDecisionTrace();
		DumpTrace(*TestRunner, TEXT("Class-graph third trace"), ThirdTrace);
		const FAngelscriptStableFunctionKey RequiredHits[] = {
			ThirdCoordinates.BaseCompute,
			ThirdCoordinates.BaseStable,
			ThirdCoordinates.DerivedCompute,
			ThirdCoordinates.DerivedStable,
			ThirdCoordinates.DerivedChanged,
		};
		for (const FAngelscriptStableFunctionKey& RequiredHit : RequiredHits)
		{
			ASSERT_THAT(IsNotNull(FindFunctionEvent(
				ThirdTrace, RequiredHit,
				EAngelscriptCacheDecisionOutcome::Restored)));
		}
		int32 UnexplainedFunctionKeyMisses = 0;
		for (const FAngelscriptCacheDecisionEvent& Event : ThirdTrace.Events)
		{
			if (Event.Stage == EAngelscriptCacheDecisionStage::FunctionLookup
				&& Event.Outcome == EAngelscriptCacheDecisionOutcome::Miss
				&& Event.ReasonCode == static_cast<uint32>(
					EAngelscriptCacheFunctionCandidateLookupStatus::FunctionKeyMiss))
			{
				++UnexplainedFunctionKeyMisses;
			}
		}
		const FAngelscriptCacheLifecyclePublications ThirdPublications =
			ThirdService->GetLifecyclePublications();
		TestRunner->AddInfo(FString::Printf(
			TEXT("Class-graph third observation: Candidate=%s Source=%s Module=%s Offsets=%d/%d/%d/%d Values=%d/%d Trace=%d UnknownMisses=%d Published=%d"),
			*WarmGenerationId.ToHexString(), *WarmSourceSnapshot.ToHexString(),
			*ThirdCoordinates.ModuleKey.Hash.ToHexString(),
			ThirdCoordinates.BaseVmPropertyOffset,
			ThirdCoordinates.BaseReflectedPropertyOffset,
			ThirdCoordinates.DerivedVmPropertyOffset,
			ThirdCoordinates.DerivedReflectedPropertyOffset,
			ThirdObservation.StableValue, ThirdObservation.ChangedValue,
			ThirdTrace.Events.Num(), UnexplainedFunctionKeyMisses,
			ThirdPublications.Current.IsValid()
				? ThirdPublications.Current->Modules.Num() : 0));
		ASSERT_THAT(AreEqual(0, UnexplainedFunctionKeyMisses));
		ASSERT_THAT(IsTrue(ThirdPublications.Current.IsValid()));
		ASSERT_THAT(AreEqual(1, ThirdPublications.Current->Modules.Num()));
		ASSERT_THAT(IsTrue(
			ThirdPublications.Current->SourceSnapshot == WarmSourceSnapshot));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
