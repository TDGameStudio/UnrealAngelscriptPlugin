#include "../../AngelscriptRuntime/Core/AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Actor.h"
#include "Misc/FileHelper.h"
#include "CQTest.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/FileManager.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Package.h"
#include "AngelscriptTestUtilities.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTest_Core_AngelscriptGeneratedFunctionTableTests_Private
{
	int32 CountGeneratedBindingRegistrations(const FString& GeneratedDirectory)
	{
		TArray<FString> GeneratedFiles;
		IFileManager::Get().FindFilesRecursive(GeneratedFiles, *GeneratedDirectory, TEXT("AS_FunctionTable_*.cpp"), true, false);

		int32 RegistrationCount = 0;
		for (const FString& GeneratedFile : GeneratedFiles)
		{
			FString FileContents;
			if (!FFileHelper::LoadFileToString(FileContents, *GeneratedFile))
			{
				continue;
			}

			TArray<FString> Lines;
			FileContents.ParseIntoArrayLines(Lines);
			for (const FString& Line : Lines)
			{
				if (Line.Contains(TEXT("FAngelscriptBinds::AddFunctionEntry(")))
				{
					RegistrationCount++;
				}
			}
		}

		return RegistrationCount;
	}

	bool FindGeneratedBindingLine(const FString& GeneratedDirectory, const FString& FunctionName, FString& OutLine)
	{
		TArray<FString> GeneratedFiles;
		IFileManager::Get().FindFilesRecursive(GeneratedFiles, *GeneratedDirectory, TEXT("AS_FunctionTable_*.cpp"), true, false);

		for (const FString& GeneratedFile : GeneratedFiles)
		{
			FString FileContents;
			if (!FFileHelper::LoadFileToString(FileContents, *GeneratedFile))
			{
				continue;
			}

			TArray<FString> Lines;
			FileContents.ParseIntoArrayLines(Lines);
			for (const FString& Line : Lines)
			{
				if (Line.Contains(FunctionName))
				{
					OutLine = Line;
					return true;
				}
			}
		}

		return false;
	}

	TArray<FString> LoadNonEmptyFileLines(const FString& FilePath)
	{
		FString FileContents;
		if (!FFileHelper::LoadFileToString(FileContents, *FilePath))
		{
			return {};
		}

		TArray<FString> Lines;
		FileContents.ParseIntoArrayLines(Lines);
		Lines.RemoveAll([](const FString& Line)
		{
			return Line.TrimStartAndEnd().IsEmpty();
		});
		return Lines;
	}
}


TEST_CLASS_WITH_FLAGS(FAngelscriptGeneratedFunctionTableTests,
	"Angelscript.TestModule.Engine.GeneratedFunctionTable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(PopulatesClassFuncMaps)
	{
		using namespace AngelscriptTest_Core_AngelscriptGeneratedFunctionTableTests_Private;
		if (!FAngelscriptEngine::IsInitialized()) { TestRunner->AddInfo(TEXT("Production engine not initialized in headless mode, skipping")); return; }

		FAngelscriptEngine& Engine = FAngelscriptEngine::Get();
		(void)Engine;

		const TMap<UClass*, TMap<FString, FFuncEntry>>& ClassFuncMaps = FAngelscriptBinds::GetClassFuncMaps();
		int32 TotalFunctionEntryCount = 0;
		for (const TPair<UClass*, TMap<FString, FFuncEntry>>& ClassEntry : ClassFuncMaps)
		{
			TotalFunctionEntryCount += ClassEntry.Value.Num();
		}

		if (!TestRunner->TestTrue(TEXT("Generated function table startup pass should populate many ClassFuncMaps entries beyond the legacy handwritten baseline"), TotalFunctionEntryCount > 1000))
		{
			return;
		}

		const TMap<FString, FFuncEntry>* ActorFunctionMap = ClassFuncMaps.Find(AActor::StaticClass());
		if (!TestRunner->TestNotNull(TEXT("Generated function table startup pass should register an entry map for AActor"), ActorFunctionMap))
		{
			return;
		}

		const FFuncEntry* ActorTimeDilationEntry = ActorFunctionMap->Find(TEXT("GetActorTimeDilation"));
		if (!TestRunner->TestNotNull(TEXT("Generated function table startup pass should register the generated AActor::GetActorTimeDilation entry"), ActorTimeDilationEntry))
		{
			return;
		}

		bool bHasCallableActorEntry = false;
		for (const TPair<FString, FFuncEntry>& ActorEntry : *ActorFunctionMap)
		{
			FGenericFuncPtr ActorFuncPtr = ActorEntry.Value.FuncPtr;
			if (ActorFuncPtr.IsBound() || ActorEntry.Value.bReflectiveFallbackBound)
			{
				bHasCallableActorEntry = true;
				break;
			}
		}

		TestRunner->TestTrue(TEXT("Generated function table startup pass should leave at least one callable generated AActor entry in ClassFuncMaps"), bHasCallableActorEntry);
	}

	TEST_METHOD(EditorOutputsUseWithEditorGuard)
	{
		using namespace AngelscriptTest_Core_AngelscriptGeneratedFunctionTableTests_Private;
		const FString GeneratedDirectory = FPaths::Combine(
			FPaths::ProjectPluginsDir(),
			TEXT("Angelscript"),
			TEXT("Intermediate/Build/Win64/UnrealEditor/Inc/AngelscriptRuntime/UHT"));

		const FString EditorOutputPath = FPaths::Combine(GeneratedDirectory, TEXT("AS_FunctionTable_UMGEditor_000.cpp"));
		const FString RuntimeOutputPath = FPaths::Combine(GeneratedDirectory, TEXT("AS_FunctionTable_Engine_000.cpp"));

		FString EditorOutput;
		if (!TestRunner->TestTrue(TEXT("Generated strategy test should find the editor-only UHT output"), FFileHelper::LoadFileToString(EditorOutput, *EditorOutputPath)))
		{
			return;
		}

		FString RuntimeOutput;
		if (!TestRunner->TestTrue(TEXT("Generated strategy test should find the runtime UHT output"), FFileHelper::LoadFileToString(RuntimeOutput, *RuntimeOutputPath)))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("Generated strategy test should wrap editor-only outputs with #if WITH_EDITOR"), EditorOutput.StartsWith(TEXT("#if WITH_EDITOR")));
		TestRunner->TestFalse(TEXT("Generated strategy test should not wrap runtime outputs with #if WITH_EDITOR"), RuntimeOutput.StartsWith(TEXT("#if WITH_EDITOR")));
	}

	TEST_METHOD(OutputsShardTimingHooks)
	{
		using namespace AngelscriptTest_Core_AngelscriptGeneratedFunctionTableTests_Private;
		const FString GeneratedDirectory = FPaths::Combine(
			FPaths::ProjectPluginsDir(),
			TEXT("Angelscript"),
			TEXT("Intermediate/Build/Win64/UnrealEditor/Inc/AngelscriptRuntime/UHT"));
		const FString RuntimeOutputPath = FPaths::Combine(GeneratedDirectory, TEXT("AS_FunctionTable_Engine_000.cpp"));

		FString RuntimeOutput;
		if (!TestRunner->TestTrue(TEXT("Generated timing hook test should find the runtime UHT output"), FFileHelper::LoadFileToString(RuntimeOutput, *RuntimeOutputPath)))
		{
			return;
		}

		TestRunner->TestTrue(
			TEXT("Generated timing hook test should emit the generated shard timing recorder call"),
			RuntimeOutput.Contains(TEXT("FAngelscriptBinds::RecordGeneratedFunctionTableShardTiming(")));
	}

	TEST_METHOD(RepresentativeCoverage)
	{
		using namespace AngelscriptTest_Core_AngelscriptGeneratedFunctionTableTests_Private;
		if (!FAngelscriptEngine::IsInitialized()) { TestRunner->AddInfo(TEXT("Production engine not initialized in headless mode, skipping")); return; }

		const TMap<UClass*, TMap<FString, FFuncEntry>>& ClassFuncMaps = FAngelscriptBinds::GetClassFuncMaps();

		struct FRepresentativeClassExpectation
		{
			const TCHAR* ObjectPath;
			const TCHAR* DisplayName;
		};

		const FRepresentativeClassExpectation Expectations[] =
		{
			{ TEXT("/Script/Engine.Actor"), TEXT("AActor") },
			{ TEXT("/Script/Engine.World"), TEXT("UWorld") },
			{ TEXT("/Script/Engine.GameplayStatics"), TEXT("UGameplayStatics") },
			{ TEXT("/Script/Engine.PlayerController"), TEXT("APlayerController") },
			{ TEXT("/Script/Engine.ActorComponent"), TEXT("UActorComponent") },
			{ TEXT("/Script/Engine.SceneComponent"), TEXT("USceneComponent") },
			{ TEXT("/Script/Engine.KismetSystemLibrary"), TEXT("UKismetSystemLibrary") },
			{ TEXT("/Script/UMG.UserWidget"), TEXT("UUserWidget") },
			{ TEXT("/Script/AssetRegistry.AssetRegistryHelpers"), TEXT("UAssetRegistryHelpers") },
		};

		for (const FRepresentativeClassExpectation& Expectation : Expectations)
		{
			UClass* ExpectedClass = FindObject<UClass>(nullptr, Expectation.ObjectPath);
			if (!TestRunner->TestNotNull(FString::Printf(TEXT("Representative coverage test should resolve %s"), Expectation.DisplayName), ExpectedClass))
			{
				return;
			}

			const TMap<FString, FFuncEntry>* FunctionMap = ClassFuncMaps.Find(ExpectedClass);
			if (!TestRunner->TestNotNull(FString::Printf(TEXT("Representative coverage test should populate ClassFuncMaps for %s"), Expectation.DisplayName), FunctionMap))
			{
				return;
			}

			if (!TestRunner->TestTrue(FString::Printf(TEXT("Representative coverage test should add at least one generated function entry for %s"), Expectation.DisplayName), FunctionMap->Num() > 0))
			{
				return;
			}
		}
	}

	TEST_METHOD(MinimalApiFunctionLevelExports)
	{
		using namespace AngelscriptTest_Core_AngelscriptGeneratedFunctionTableTests_Private;
		if (!FAngelscriptEngine::IsInitialized()) { TestRunner->AddInfo(TEXT("Production engine not initialized in headless mode, skipping")); return; }

		const TMap<UClass*, TMap<FString, FFuncEntry>>& ClassFuncMaps = FAngelscriptBinds::GetClassFuncMaps();
		const TMap<FString, FFuncEntry>* PlayerCameraManagerEntries = ClassFuncMaps.Find(APlayerCameraManager::StaticClass());
		if (!TestRunner->TestNotNull(TEXT("MinimalAPI function export regression test should expose generated entries for APlayerCameraManager"), PlayerCameraManagerEntries))
		{
			return;
		}

		const TCHAR* ExpectedBoundFunctions[] =
		{
			TEXT("SetManualCameraFade"),
			TEXT("StartCameraFade"),
			TEXT("StopCameraFade"),
		};

		for (const TCHAR* ExpectedFunctionName : ExpectedBoundFunctions)
		{
			const FFuncEntry* Entry = PlayerCameraManagerEntries->Find(ExpectedFunctionName);
			if (!TestRunner->TestNotNull(FString::Printf(TEXT("MinimalAPI function export regression test should register %s"), ExpectedFunctionName), Entry))
			{
				return;
			}

			FGenericFuncPtr FunctionPointer = Entry->FuncPtr;
			if (!TestRunner->TestTrue(FString::Printf(TEXT("MinimalAPI function export regression test should recover a direct-call pointer for %s"), ExpectedFunctionName), FunctionPointer.IsBound()))
			{
				return;
			}
		}
	}

	TEST_METHOD(ReflectiveFallbackStats)
	{
		using namespace AngelscriptTest_Core_AngelscriptGeneratedFunctionTableTests_Private;
		if (!FAngelscriptEngine::IsInitialized()) { TestRunner->AddInfo(TEXT("Production engine not initialized in headless mode, skipping")); return; }

		const TMap<UClass*, TMap<FString, FFuncEntry>>& ClassFuncMaps = FAngelscriptBinds::GetClassFuncMaps();
		int32 DirectCount = 0;
		int32 ReflectiveCount = 0;
		int32 UnresolvedCount = 0;
		TMap<FString, int32> ReflectiveCountsByModule;

		for (const TPair<UClass*, TMap<FString, FFuncEntry>>& ClassEntry : ClassFuncMaps)
		{
			const FString PackageName = ClassEntry.Key != nullptr && ClassEntry.Key->GetOutermost() != nullptr
				? ClassEntry.Key->GetOutermost()->GetName()
				: FString();
			FString ModuleName = PackageName;
			ModuleName.RemoveFromStart(TEXT("/Script/"));

			for (const TPair<FString, FFuncEntry>& FunctionEntry : ClassEntry.Value)
			{
				FGenericFuncPtr FunctionPointer = FunctionEntry.Value.FuncPtr;
				if (FunctionPointer.IsBound())
				{
					++DirectCount;
					continue;
				}

				if (FunctionEntry.Value.bReflectiveFallbackBound)
				{
					++ReflectiveCount;
					ReflectiveCountsByModule.FindOrAdd(ModuleName) += 1;
					continue;
				}

				++UnresolvedCount;
			}
		}

		if (!TestRunner->TestTrue(TEXT("Generated function table stats should still report direct bindings after reflective fallback lands"), DirectCount > 0))
		{
			return;
		}

		if (!TestRunner->TestTrue(TEXT("Generated function table stats should report at least one reflective fallback binding"), ReflectiveCount > 0))
		{
			return;
		}

		if (!TestRunner->TestTrue(TEXT("Generated function table stats should continue to report unresolved entries after reflective fallback lands"), UnresolvedCount > 0))
		{
			return;
		}

		if (!TestRunner->TestTrue(TEXT("Generated function table stats should record reflective fallback coverage in AIModule"), ReflectiveCountsByModule.FindRef(TEXT("AIModule")) > 0))
		{
			return;
		}

		if (!TestRunner->TestTrue(TEXT("Generated function table stats should record reflective fallback coverage in UMG"), ReflectiveCountsByModule.FindRef(TEXT("UMG")) > 0))
		{
			return;
		}

		TestRunner->AddInfo(TEXT("Generated function table stats verified for main Angelscript modules; GAS handwritten entries are covered by AngelscriptGASTest."));
	}

	TEST_METHOD(SummaryOutput)
	{
		using namespace AngelscriptTest_Core_AngelscriptGeneratedFunctionTableTests_Private;
		const FString GeneratedDirectory = FPaths::Combine(
			FPaths::ProjectPluginsDir(),
			TEXT("Angelscript"),
			TEXT("Intermediate/Build/Win64/UnrealEditor/Inc/AngelscriptRuntime/UHT"));
		const FString SummaryPath = FPaths::Combine(GeneratedDirectory, TEXT("AS_FunctionTable_Summary.json"));

		FString SummaryJson;
		if (!TestRunner->TestTrue(TEXT("Generated function table summary test should find the UHT summary json output"), FFileHelper::LoadFileToString(SummaryJson, *SummaryPath)))
		{
			return;
		}

		TSharedPtr<FJsonObject> SummaryObject;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SummaryJson);
		if (!TestRunner->TestTrue(TEXT("Generated function table summary test should parse the summary json"), FJsonSerializer::Deserialize(Reader, SummaryObject) && SummaryObject.IsValid()))
		{
			return;
		}

		int32 TotalGeneratedEntries = 0;
		if (!TestRunner->TestTrue(TEXT("Generated function table summary test should expose totalGeneratedEntries"), SummaryObject->TryGetNumberField(TEXT("totalGeneratedEntries"), TotalGeneratedEntries)))
		{
			return;
		}

		int32 TotalDirectBindEntries = 0;
		if (!TestRunner->TestTrue(TEXT("Generated function table summary test should expose totalDirectBindEntries"), SummaryObject->TryGetNumberField(TEXT("totalDirectBindEntries"), TotalDirectBindEntries)))
		{
			return;
		}

		int32 TotalStubEntries = 0;
		if (!TestRunner->TestTrue(TEXT("Generated function table summary test should expose totalStubEntries"), SummaryObject->TryGetNumberField(TEXT("totalStubEntries"), TotalStubEntries)))
		{
			return;
		}

		int32 TotalCrossModuleEntries = 0;
		if (!TestRunner->TestTrue(TEXT("Generated function table summary test should expose totalCrossModuleEntries"), SummaryObject->TryGetNumberField(TEXT("totalCrossModuleEntries"), TotalCrossModuleEntries)))
		{
			return;
		}

		double DirectBindRate = 0.0;
		if (!TestRunner->TestTrue(TEXT("Generated function table summary test should expose directBindRate"), SummaryObject->TryGetNumberField(TEXT("directBindRate"), DirectBindRate)))
		{
			return;
		}

		double StubRate = 0.0;
		if (!TestRunner->TestTrue(TEXT("Generated function table summary test should expose stubRate"), SummaryObject->TryGetNumberField(TEXT("stubRate"), StubRate)))
		{
			return;
		}

		double CrossModuleRate = 0.0;
		if (!TestRunner->TestTrue(TEXT("Generated function table summary test should expose crossModuleRate"), SummaryObject->TryGetNumberField(TEXT("crossModuleRate"), CrossModuleRate)))
		{
			return;
		}

		const int32 CountedRegistrations = CountGeneratedBindingRegistrations(GeneratedDirectory);
		if (!TestRunner->TestTrue(TEXT("Generated function table summary test should count generated registration lines from UHT output"), CountedRegistrations > 0))
		{
			return;
		}

		if (!TestRunner->TestEqual(TEXT("Generated function table summary test should keep direct, stub, and cross-module totals aligned with totalGeneratedEntries"), TotalGeneratedEntries, TotalDirectBindEntries + TotalStubEntries + TotalCrossModuleEntries))
		{
			return;
		}

		const double ExpectedDirectBindRate = TotalGeneratedEntries > 0 ? static_cast<double>(TotalDirectBindEntries) / static_cast<double>(TotalGeneratedEntries) : 0.0;
		const double ExpectedStubRate = TotalGeneratedEntries > 0 ? static_cast<double>(TotalStubEntries) / static_cast<double>(TotalGeneratedEntries) : 0.0;
		const double ExpectedCrossModuleRate = TotalGeneratedEntries > 0 ? static_cast<double>(TotalCrossModuleEntries) / static_cast<double>(TotalGeneratedEntries) : 0.0;
		TestRunner->TestTrue(TEXT("Generated function table summary test should keep directBindRate aligned with entry counts"), FMath::Abs(DirectBindRate - ExpectedDirectBindRate) < 1.e-9);
		TestRunner->TestTrue(TEXT("Generated function table summary test should keep stubRate aligned with entry counts"), FMath::Abs(StubRate - ExpectedStubRate) < 1.e-9);
		TestRunner->TestTrue(TEXT("Generated function table summary test should keep crossModuleRate aligned with entry counts"), FMath::Abs(CrossModuleRate - ExpectedCrossModuleRate) < 1.e-9);
		TestRunner->TestTrue(TEXT("Generated function table summary test should keep directBindRate, stubRate, and crossModuleRate normalized"), FMath::Abs((DirectBindRate + StubRate + CrossModuleRate) - 1.0) < 1.e-9);

		const TArray<TSharedPtr<FJsonValue>>* Modules = nullptr;
		if (!TestRunner->TestTrue(TEXT("Generated function table summary test should expose per-module summaries"), SummaryObject->TryGetArrayField(TEXT("modules"), Modules) && Modules != nullptr))
		{
			return;
		}

		int32 SummedModuleEntries = 0;
		int32 RuntimeShardEntries = 0;
		for (const TSharedPtr<FJsonValue>& ModuleValue : *Modules)
		{
			const TSharedPtr<FJsonObject>* ModuleObject = nullptr;
			if (ModuleValue.IsValid() && ModuleValue->TryGetObject(ModuleObject) && ModuleObject != nullptr)
			{
				int32 ModuleEntries = 0;
				int32 ModuleDirectBindEntries = 0;
				int32 ModuleStubEntries = 0;
				int32 ModuleCrossModuleEntries = 0;
				double ModuleDirectBindRate = 0.0;
				double ModuleStubRate = 0.0;
				double ModuleCrossModuleRate = 0.0;
				if ((*ModuleObject)->TryGetNumberField(TEXT("totalEntries"), ModuleEntries))
				{
					SummedModuleEntries += ModuleEntries;
				}

				if (!TestRunner->TestTrue(TEXT("Generated function table summary test should expose per-module directBindEntries"), (*ModuleObject)->TryGetNumberField(TEXT("directBindEntries"), ModuleDirectBindEntries)))
				{
					return;
				}

				if (!TestRunner->TestTrue(TEXT("Generated function table summary test should expose per-module stubEntries"), (*ModuleObject)->TryGetNumberField(TEXT("stubEntries"), ModuleStubEntries)))
				{
					return;
				}

				if (!TestRunner->TestTrue(TEXT("Generated function table summary test should expose per-module crossModuleEntries"), (*ModuleObject)->TryGetNumberField(TEXT("crossModuleEntries"), ModuleCrossModuleEntries)))
				{
					return;
				}

				if (!TestRunner->TestTrue(TEXT("Generated function table summary test should expose per-module directBindRate"), (*ModuleObject)->TryGetNumberField(TEXT("directBindRate"), ModuleDirectBindRate)))
				{
					return;
				}

				if (!TestRunner->TestTrue(TEXT("Generated function table summary test should expose per-module stubRate"), (*ModuleObject)->TryGetNumberField(TEXT("stubRate"), ModuleStubRate)))
				{
					return;
				}

				if (!TestRunner->TestTrue(TEXT("Generated function table summary test should expose per-module crossModuleRate"), (*ModuleObject)->TryGetNumberField(TEXT("crossModuleRate"), ModuleCrossModuleRate)))
				{
					return;
				}

				if (!TestRunner->TestEqual(TEXT("Generated function table summary test should keep module totals aligned"), ModuleEntries, ModuleDirectBindEntries + ModuleStubEntries + ModuleCrossModuleEntries))
				{
					return;
				}
				if (ModuleDirectBindEntries > 0 || ModuleStubEntries > 0)
				{
					RuntimeShardEntries += ModuleEntries;
				}

				const double ExpectedModuleDirectRate = ModuleEntries > 0 ? static_cast<double>(ModuleDirectBindEntries) / static_cast<double>(ModuleEntries) : 0.0;
				const double ExpectedModuleStubRate = ModuleEntries > 0 ? static_cast<double>(ModuleStubEntries) / static_cast<double>(ModuleEntries) : 0.0;
				const double ExpectedModuleCrossModuleRate = ModuleEntries > 0 ? static_cast<double>(ModuleCrossModuleEntries) / static_cast<double>(ModuleEntries) : 0.0;
				if (!TestRunner->TestTrue(TEXT("Generated function table summary test should keep module directBindRate aligned with entry counts"), FMath::Abs(ModuleDirectBindRate - ExpectedModuleDirectRate) < 1.e-9))
				{
					return;
				}

				if (!TestRunner->TestTrue(TEXT("Generated function table summary test should keep module stubRate aligned with entry counts"), FMath::Abs(ModuleStubRate - ExpectedModuleStubRate) < 1.e-9))
				{
					return;
				}

				if (!TestRunner->TestTrue(TEXT("Generated function table summary test should keep module crossModuleRate aligned with entry counts"), FMath::Abs(ModuleCrossModuleRate - ExpectedModuleCrossModuleRate) < 1.e-9))
				{
					return;
				}
			}
		}

		TestRunner->TestEqual(TEXT("Generated function table summary test should match runtime shard registration count"), RuntimeShardEntries, CountedRegistrations);
		TestRunner->TestEqual(TEXT("Generated function table summary test should keep totalGeneratedEntries equal to the sum of module totals"), TotalGeneratedEntries, SummedModuleEntries);
	}

	TEST_METHOD(CsvOutput)
	{
		using namespace AngelscriptTest_Core_AngelscriptGeneratedFunctionTableTests_Private;
		const FString GeneratedDirectory = FPaths::Combine(
			FPaths::ProjectPluginsDir(),
			TEXT("Angelscript"),
			TEXT("Intermediate/Build/Win64/UnrealEditor/Inc/AngelscriptRuntime/UHT"));
		const FString SummaryPath = FPaths::Combine(GeneratedDirectory, TEXT("AS_FunctionTable_Summary.json"));
		const FString ModuleCsvPath = FPaths::Combine(GeneratedDirectory, TEXT("AS_FunctionTable_ModuleSummary.csv"));
		const FString EntryCsvPath = FPaths::Combine(GeneratedDirectory, TEXT("AS_FunctionTable_Entries.csv"));

		FString SummaryJson;
		if (!TestRunner->TestTrue(TEXT("Generated function table csv test should find the summary json output"), FFileHelper::LoadFileToString(SummaryJson, *SummaryPath)))
		{
			return;
		}

		TSharedPtr<FJsonObject> SummaryObject;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SummaryJson);
		if (!TestRunner->TestTrue(TEXT("Generated function table csv test should parse the summary json"), FJsonSerializer::Deserialize(Reader, SummaryObject) && SummaryObject.IsValid()))
		{
			return;
		}

		int32 TotalGeneratedEntries = 0;
		if (!TestRunner->TestTrue(TEXT("Generated function table csv test should expose totalGeneratedEntries"), SummaryObject->TryGetNumberField(TEXT("totalGeneratedEntries"), TotalGeneratedEntries)))
		{
			return;
		}

		int32 TotalDirectBindEntries = 0;
		if (!TestRunner->TestTrue(TEXT("Generated function table csv test should expose totalDirectBindEntries"), SummaryObject->TryGetNumberField(TEXT("totalDirectBindEntries"), TotalDirectBindEntries)))
		{
			return;
		}

		int32 TotalStubEntries = 0;
		if (!TestRunner->TestTrue(TEXT("Generated function table csv test should expose totalStubEntries"), SummaryObject->TryGetNumberField(TEXT("totalStubEntries"), TotalStubEntries)))
		{
			return;
		}

		int32 TotalCrossModuleEntries = 0;
		if (!TestRunner->TestTrue(TEXT("Generated function table csv test should expose totalCrossModuleEntries"), SummaryObject->TryGetNumberField(TEXT("totalCrossModuleEntries"), TotalCrossModuleEntries)))
		{
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>* Modules = nullptr;
		if (!TestRunner->TestTrue(TEXT("Generated function table csv test should expose per-module summaries"), SummaryObject->TryGetArrayField(TEXT("modules"), Modules) && Modules != nullptr))
		{
			return;
		}

		const TArray<FString> ModuleLines = LoadNonEmptyFileLines(ModuleCsvPath);
		if (!TestRunner->TestTrue(TEXT("Generated function table csv test should write the module summary csv"), ModuleLines.Num() > 0))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Generated function table csv test should keep one module csv row per module summary"), ModuleLines.Num() - 1, Modules->Num());
		TestRunner->TestEqual(TEXT("Generated function table csv test should write the expected module csv header"), ModuleLines[0], TEXT("ModuleName,EditorOnly,TotalEntries,DirectBindEntries,StubEntries,CrossModuleEntries,DirectBindRate,StubRate,CrossModuleRate,ShardCount"));

		const TArray<FString> EntryLines = LoadNonEmptyFileLines(EntryCsvPath);
		if (!TestRunner->TestTrue(TEXT("Generated function table csv test should write the entry detail csv"), EntryLines.Num() > 0))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Generated function table csv test should keep one entry csv row per generated binding entry"), EntryLines.Num() - 1, TotalGeneratedEntries);
		TestRunner->TestEqual(TEXT("Generated function table csv test should write the expected entry csv header"), EntryLines[0], TEXT("ModuleName,EditorOnly,ClassName,FunctionName,EntryKind,EraseMacro,ShardIndex,ThunkStyle"));

		int32 DirectCsvEntries = 0;
		int32 StubCsvEntries = 0;
		int32 CrossModuleCsvEntries = 0;
		for (int32 LineIndex = 1; LineIndex < EntryLines.Num(); ++LineIndex)
		{
			const FString& EntryLine = EntryLines[LineIndex];
			if (EntryLine.Contains(TEXT(",Direct,")))
			{
				DirectCsvEntries++;
			}
			else if (EntryLine.Contains(TEXT(",Stub,")))
			{
				StubCsvEntries++;
			}
			else if (EntryLine.Contains(TEXT(",CrossModule,")))
			{
				CrossModuleCsvEntries++;
			}
		}

		TestRunner->TestEqual(TEXT("Generated function table csv test should keep direct entry rows aligned with the summary"), DirectCsvEntries, TotalDirectBindEntries);
		TestRunner->TestEqual(TEXT("Generated function table csv test should keep stub entry rows aligned with the summary"), StubCsvEntries, TotalStubEntries);
		TestRunner->TestEqual(TEXT("Generated function table csv test should keep cross-module entry rows aligned with the summary"), CrossModuleCsvEntries, TotalCrossModuleEntries);

		FString RunBehaviorTreeCsvLine;
		if (!TestRunner->TestTrue(TEXT("Generated function table csv test should include RunBehaviorTree in the entry csv"), FindGeneratedBindingLine(GeneratedDirectory, TEXT("\"RunBehaviorTree\""), RunBehaviorTreeCsvLine)))
		{
			return;
		}

		bool bFoundRunBehaviorTreeCsv = false;
		for (const FString& EntryLine : EntryLines)
		{
			if (EntryLine.Contains(TEXT(",RunBehaviorTree,")))
			{
				bFoundRunBehaviorTreeCsv = true;
				TestRunner->TestTrue(TEXT("Generated function table csv test should classify RunBehaviorTree as a direct entry"), EntryLine.Contains(TEXT(",Direct,")));
				TestRunner->TestFalse(TEXT("Generated function table csv test should not emit ERASE_NO_FUNCTION for RunBehaviorTree in the csv"), EntryLine.Contains(TEXT("ERASE_NO_FUNCTION()")));
				break;
			}
		}

		TestRunner->TestTrue(TEXT("Generated function table csv test should locate RunBehaviorTree in the entry csv"), bFoundRunBehaviorTreeCsv);
	}

	TEST_METHOD(SkippedCsvOutput)
	{
		using namespace AngelscriptTest_Core_AngelscriptGeneratedFunctionTableTests_Private;
		const FString GeneratedDirectory = FPaths::Combine(
			FPaths::ProjectPluginsDir(),
			TEXT("Angelscript"),
			TEXT("Intermediate/Build/Win64/UnrealEditor/Inc/AngelscriptRuntime/UHT"));
		const FString SkippedCsvPath = FPaths::Combine(GeneratedDirectory, TEXT("AS_FunctionTable_SkippedEntries.csv"));

		const TArray<FString> SkippedLines = LoadNonEmptyFileLines(SkippedCsvPath);
		if (!TestRunner->TestTrue(TEXT("Generated function table skipped csv test should write the skipped entry csv"), SkippedLines.Num() > 0))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Generated function table skipped csv test should write the expected skipped csv header"), SkippedLines[0], TEXT("ModuleName,ClassName,FunctionName,FailureReason"));
		TestRunner->TestTrue(TEXT("Generated function table skipped csv test should contain at least one skipped function row"), SkippedLines.Num() > 1);

		bool bFoundFailureReason = false;
		for (int32 LineIndex = 1; LineIndex < SkippedLines.Num(); ++LineIndex)
		{
			TArray<FString> Columns;
			SkippedLines[LineIndex].ParseIntoArray(Columns, TEXT(","), false);
			if (!TestRunner->TestTrue(TEXT("Generated function table skipped csv rows should expose four columns"), Columns.Num() == 4))
			{
				return;
			}

			if (!Columns[3].IsEmpty())
			{
				bFoundFailureReason = true;
			}
		}

		TestRunner->TestTrue(TEXT("Generated function table skipped csv rows should include non-empty failure reasons"), bFoundFailureReason);
	}

	TEST_METHOD(SkippedReasonSummaryCsvOutput)
	{
		using namespace AngelscriptTest_Core_AngelscriptGeneratedFunctionTableTests_Private;
		const FString GeneratedDirectory = FPaths::Combine(
			FPaths::ProjectPluginsDir(),
			TEXT("Angelscript"),
			TEXT("Intermediate/Build/Win64/UnrealEditor/Inc/AngelscriptRuntime/UHT"));
		const FString SkippedCsvPath = FPaths::Combine(GeneratedDirectory, TEXT("AS_FunctionTable_SkippedEntries.csv"));
		const FString ReasonSummaryCsvPath = FPaths::Combine(GeneratedDirectory, TEXT("AS_FunctionTable_SkippedReasonSummary.csv"));

		const TArray<FString> SkippedLines = LoadNonEmptyFileLines(SkippedCsvPath);
		if (!TestRunner->TestTrue(TEXT("Generated function table skipped reason summary test should find the skipped entry csv"), SkippedLines.Num() > 0))
		{
			return;
		}

		const TArray<FString> SummaryLines = LoadNonEmptyFileLines(ReasonSummaryCsvPath);
		if (!TestRunner->TestTrue(TEXT("Generated function table skipped reason summary test should write the skipped reason summary csv"), SummaryLines.Num() > 0))
		{
			return;
		}

		TestRunner->TestEqual(TEXT("Generated function table skipped reason summary test should write the expected summary csv header"), SummaryLines[0], TEXT("FailureReason,SkippedCount"));
		TestRunner->TestTrue(TEXT("Generated function table skipped reason summary test should contain at least one reason row"), SummaryLines.Num() > 1);

		int32 SummedSkippedCount = 0;
		for (int32 LineIndex = 1; LineIndex < SummaryLines.Num(); ++LineIndex)
		{
			TArray<FString> Columns;
			SummaryLines[LineIndex].ParseIntoArray(Columns, TEXT(","), false);
			if (!TestRunner->TestTrue(TEXT("Generated function table skipped reason summary rows should expose two columns"), Columns.Num() == 2))
			{
				return;
			}

			if (!TestRunner->TestTrue(TEXT("Generated function table skipped reason summary rows should include a non-empty reason"), !Columns[0].IsEmpty()))
			{
				return;
			}

			SummedSkippedCount += FCString::Atoi(*Columns[1]);
		}

		TestRunner->TestEqual(TEXT("Generated function table skipped reason summary test should keep aggregate counts aligned with the skipped entry csv"), SummedSkippedCount, SkippedLines.Num() - 1);
	}

	TEST_METHOD(MacroQualifiedDirectBindings)
	{
		using namespace AngelscriptTest_Core_AngelscriptGeneratedFunctionTableTests_Private;
		const FString GeneratedDirectory = FPaths::Combine(
			FPaths::ProjectPluginsDir(),
			TEXT("Angelscript"),
			TEXT("Intermediate/Build/Win64/UnrealEditor/Inc/AngelscriptRuntime/UHT"));

		FString RunBehaviorTreeLine;
		if (!TestRunner->TestTrue(TEXT("Macro-qualified direct bindings test should find generated entry for RunBehaviorTree"), FindGeneratedBindingLine(GeneratedDirectory, TEXT("\"RunBehaviorTree\""), RunBehaviorTreeLine)))
		{
			return;
		}

		TestRunner->TestFalse(TEXT("Macro-qualified direct bindings test should not reduce RunBehaviorTree to ERASE_NO_FUNCTION"), RunBehaviorTreeLine.Contains(TEXT("ERASE_NO_FUNCTION()")));
		TestRunner->TestTrue(TEXT("Macro-qualified direct bindings test should keep RunBehaviorTree on a direct erase macro path"), RunBehaviorTreeLine.Contains(TEXT("ERASE_AUTO_METHOD_PTR")) || RunBehaviorTreeLine.Contains(TEXT("ERASE_METHOD_PTR")));

		FString ReportPerceptionEventLine;
		if (!TestRunner->TestTrue(TEXT("Macro-qualified direct bindings test should find generated entry for ReportPerceptionEvent"), FindGeneratedBindingLine(GeneratedDirectory, TEXT("\"ReportPerceptionEvent\""), ReportPerceptionEventLine)))
		{
			return;
		}

		TestRunner->TestFalse(TEXT("Macro-qualified direct bindings test should not reduce ReportPerceptionEvent to ERASE_NO_FUNCTION"), ReportPerceptionEventLine.Contains(TEXT("ERASE_NO_FUNCTION()")));
		TestRunner->TestTrue(TEXT("Macro-qualified direct bindings test should keep ReportPerceptionEvent on a direct erase macro path"), ReportPerceptionEventLine.Contains(TEXT("ERASE_AUTO_FUNCTION_PTR")) || ReportPerceptionEventLine.Contains(TEXT("ERASE_FUNCTION_PTR")));
	}
};

#endif
