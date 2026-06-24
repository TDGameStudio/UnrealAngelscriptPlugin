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


TEST_CLASS_WITH_FLAGS(FAngelscriptGeneratedFunctionTableTests,
	"Angelscript.TestModule.Engine.GeneratedFunctionTable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
static int32 CountGeneratedBindingRegistrations(const FString& GeneratedDirectory)
{
	TArray<FString> GeneratedFiles;
	IFileManager::Get().FindFilesRecursive(GeneratedFiles, *GeneratedDirectory, TEXT("AS_FunctionTable_*.gen.cpp"), true, false);

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

static bool FindGeneratedBindingLine(const FString& GeneratedDirectory, const FString& FunctionName, FString& OutLine)
{
	TArray<FString> GeneratedFiles;
	IFileManager::Get().FindFilesRecursive(GeneratedFiles, *GeneratedDirectory, TEXT("AS_FunctionTable_*.gen.cpp"), true, false);

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

static TArray<FString> LoadNonEmptyFileLines(const FString& FilePath)
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

public:
	TEST_METHOD(PopulatesClassFuncMaps)
	{
if (!FAngelscriptEngine::IsInitialized()) { TestRunner->AddInfo(TEXT("Production engine not initialized in headless mode, skipping")); return; }

		FAngelscriptEngine& Engine = FAngelscriptEngine::Get();
		(void)Engine;

		const TMap<UClass*, TMap<FString, FFuncEntry>>& ClassFuncMaps = FAngelscriptBinds::GetClassFuncMaps();
		int32 TotalFunctionEntryCount = 0;
		for (const TPair<UClass*, TMap<FString, FFuncEntry>>& ClassEntry : ClassFuncMaps)
		{
			TotalFunctionEntryCount += ClassEntry.Value.Num();
		}

		if (!this->Assert.IsTrue(TotalFunctionEntryCount > 1000, TEXT("Generated function table startup pass should populate many ClassFuncMaps entries beyond the legacy handwritten baseline")))
		{
			return;
		}

		const TMap<FString, FFuncEntry>* ActorFunctionMap = ClassFuncMaps.Find(AActor::StaticClass());
		if (!this->Assert.IsNotNull(ActorFunctionMap, TEXT("Generated function table startup pass should register an entry map for AActor")))
		{
			return;
		}

		const FFuncEntry* ActorTimeDilationEntry = ActorFunctionMap->Find(TEXT("GetActorTimeDilation"));
		if (!this->Assert.IsNotNull(ActorTimeDilationEntry, TEXT("Generated function table startup pass should register the generated AActor::GetActorTimeDilation entry")))
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

		(void)this->Assert.IsTrue(bHasCallableActorEntry, TEXT("Generated function table startup pass should leave at least one callable generated AActor entry in ClassFuncMaps"));
	}

	TEST_METHOD(EditorOutputsUseWithEditorGuard)
	{
const FString GeneratedDirectory = FPaths::Combine(
			FPaths::ProjectPluginsDir(),
			TEXT("Angelscript"),
			TEXT("Intermediate/Build/Win64/UnrealEditor/Inc/AngelscriptRuntime/UHT"));

		const FString EditorOutputPath = FPaths::Combine(GeneratedDirectory, TEXT("AS_FunctionTable_UMGEditor_000.gen.cpp"));
		const FString RuntimeOutputPath = FPaths::Combine(GeneratedDirectory, TEXT("AS_FunctionTable_Engine_000.gen.cpp"));

		FString EditorOutput;
		if (!this->Assert.IsTrue(FFileHelper::LoadFileToString(EditorOutput, *EditorOutputPath), TEXT("Generated strategy test should find the editor-only UHT output")))
		{
			return;
		}

		FString RuntimeOutput;
		if (!this->Assert.IsTrue(FFileHelper::LoadFileToString(RuntimeOutput, *RuntimeOutputPath), TEXT("Generated strategy test should find the runtime UHT output")))
		{
			return;
		}

		bool bOk = true;
		bOk &= this->Assert.IsTrue(EditorOutput.StartsWith(TEXT("#if WITH_EDITOR")), TEXT("Generated strategy test should wrap editor-only outputs with #if WITH_EDITOR"));
		bOk &= this->Assert.IsFalse(RuntimeOutput.StartsWith(TEXT("#if WITH_EDITOR")), TEXT("Generated strategy test should not wrap runtime outputs with #if WITH_EDITOR"));
		(void)bOk;
	}

	TEST_METHOD(OutputsShardTimingHooks)
	{
const FString GeneratedDirectory = FPaths::Combine(
			FPaths::ProjectPluginsDir(),
			TEXT("Angelscript"),
			TEXT("Intermediate/Build/Win64/UnrealEditor/Inc/AngelscriptRuntime/UHT"));
		const FString RuntimeOutputPath = FPaths::Combine(GeneratedDirectory, TEXT("AS_FunctionTable_Engine_000.gen.cpp"));

		FString RuntimeOutput;
		if (!this->Assert.IsTrue(FFileHelper::LoadFileToString(RuntimeOutput, *RuntimeOutputPath), TEXT("Generated timing hook test should find the runtime UHT output")))
		{
			return;
		}

		(void)this->Assert.IsTrue(
			RuntimeOutput.Contains(TEXT("FAngelscriptBinds::RecordGeneratedFunctionTableShardTiming(")),
			TEXT("Generated timing hook test should emit the generated shard timing recorder call"));
	}

	TEST_METHOD(RepresentativeCoverage)
	{
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
			if (!this->Assert.IsNotNull(ExpectedClass, FString::Printf(TEXT("Representative coverage test should resolve %s"), Expectation.DisplayName)))
			{
				return;
			}

			const TMap<FString, FFuncEntry>* FunctionMap = ClassFuncMaps.Find(ExpectedClass);
			if (!this->Assert.IsNotNull(FunctionMap, FString::Printf(TEXT("Representative coverage test should populate ClassFuncMaps for %s"), Expectation.DisplayName)))
			{
				return;
			}

			if (!this->Assert.IsTrue(FunctionMap->Num() > 0, FString::Printf(TEXT("Representative coverage test should add at least one generated function entry for %s"), Expectation.DisplayName)))
			{
				return;
			}
		}
	}

	TEST_METHOD(MinimalApiFunctionLevelExports)
	{
if (!FAngelscriptEngine::IsInitialized()) { TestRunner->AddInfo(TEXT("Production engine not initialized in headless mode, skipping")); return; }

		const TMap<UClass*, TMap<FString, FFuncEntry>>& ClassFuncMaps = FAngelscriptBinds::GetClassFuncMaps();
		const TMap<FString, FFuncEntry>* PlayerCameraManagerEntries = ClassFuncMaps.Find(APlayerCameraManager::StaticClass());
		if (!this->Assert.IsNotNull(PlayerCameraManagerEntries, TEXT("MinimalAPI function export regression test should expose generated entries for APlayerCameraManager")))
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
			if (!this->Assert.IsNotNull(Entry, FString::Printf(TEXT("MinimalAPI function export regression test should register %s"), ExpectedFunctionName)))
			{
				return;
			}

			FGenericFuncPtr FunctionPointer = Entry->FuncPtr;
			if (!this->Assert.IsTrue(FunctionPointer.IsBound(), FString::Printf(TEXT("MinimalAPI function export regression test should recover a direct-call pointer for %s"), ExpectedFunctionName)))
			{
				return;
			}
		}
	}

	TEST_METHOD(ReflectiveFallbackStats)
	{
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

		if (!this->Assert.IsTrue(DirectCount > 0, TEXT("Generated function table stats should still report direct bindings after reflective fallback lands")))
		{
			return;
		}

		if (!this->Assert.IsTrue(ReflectiveCount > 0, TEXT("Generated function table stats should report at least one reflective fallback binding")))
		{
			return;
		}

		if (!this->Assert.IsTrue(UnresolvedCount > 0, TEXT("Generated function table stats should continue to report unresolved entries after reflective fallback lands")))
		{
			return;
		}

		if (!this->Assert.IsTrue(ReflectiveCountsByModule.FindRef(TEXT("AIModule")) > 0, TEXT("Generated function table stats should record reflective fallback coverage in AIModule")))
		{
			return;
		}

		if (!this->Assert.IsTrue(ReflectiveCountsByModule.FindRef(TEXT("UMG")) > 0, TEXT("Generated function table stats should record reflective fallback coverage in UMG")))
		{
			return;
		}

		TestRunner->AddInfo(TEXT("Generated function table stats verified for main Angelscript modules; GAS handwritten entries are covered by AngelscriptGASTest."));
	}

	TEST_METHOD(SummaryOutput)
	{
const FString GeneratedDirectory = FPaths::Combine(
			FPaths::ProjectPluginsDir(),
			TEXT("Angelscript"),
			TEXT("Intermediate/Build/Win64/UnrealEditor/Inc/AngelscriptRuntime/UHT"));
		const FString SummaryPath = FPaths::Combine(GeneratedDirectory, TEXT("AS_FunctionTable_Summary.json"));

		FString SummaryJson;
		if (!this->Assert.IsTrue(FFileHelper::LoadFileToString(SummaryJson, *SummaryPath), TEXT("Generated function table summary test should find the UHT summary json output")))
		{
			return;
		}

		TSharedPtr<FJsonObject> SummaryObject;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SummaryJson);
		if (!this->Assert.IsTrue(FJsonSerializer::Deserialize(Reader, SummaryObject) && SummaryObject.IsValid(), TEXT("Generated function table summary test should parse the summary json")))
		{
			return;
		}

		int32 TotalGeneratedEntries = 0;
		if (!this->Assert.IsTrue(SummaryObject->TryGetNumberField(TEXT("totalGeneratedEntries"), TotalGeneratedEntries), TEXT("Generated function table summary test should expose totalGeneratedEntries")))
		{
			return;
		}

		int32 TotalDirectBindEntries = 0;
		if (!this->Assert.IsTrue(SummaryObject->TryGetNumberField(TEXT("totalDirectBindEntries"), TotalDirectBindEntries), TEXT("Generated function table summary test should expose totalDirectBindEntries")))
		{
			return;
		}

		int32 TotalStubEntries = 0;
		if (!this->Assert.IsTrue(SummaryObject->TryGetNumberField(TEXT("totalStubEntries"), TotalStubEntries), TEXT("Generated function table summary test should expose totalStubEntries")))
		{
			return;
		}

		int32 TotalCrossModuleEntries = 0;
		if (!this->Assert.IsTrue(SummaryObject->TryGetNumberField(TEXT("totalCrossModuleEntries"), TotalCrossModuleEntries), TEXT("Generated function table summary test should expose totalCrossModuleEntries")))
		{
			return;
		}

		double DirectBindRate = 0.0;
		if (!this->Assert.IsTrue(SummaryObject->TryGetNumberField(TEXT("directBindRate"), DirectBindRate), TEXT("Generated function table summary test should expose directBindRate")))
		{
			return;
		}

		double StubRate = 0.0;
		if (!this->Assert.IsTrue(SummaryObject->TryGetNumberField(TEXT("stubRate"), StubRate), TEXT("Generated function table summary test should expose stubRate")))
		{
			return;
		}

		double CrossModuleRate = 0.0;
		if (!this->Assert.IsTrue(SummaryObject->TryGetNumberField(TEXT("crossModuleRate"), CrossModuleRate), TEXT("Generated function table summary test should expose crossModuleRate")))
		{
			return;
		}

		const int32 CountedRegistrations = CountGeneratedBindingRegistrations(GeneratedDirectory);
		if (!this->Assert.IsTrue(CountedRegistrations > 0, TEXT("Generated function table summary test should count generated registration lines from UHT output")))
		{
			return;
		}

		if (!this->Assert.AreEqual(TotalDirectBindEntries + TotalStubEntries + TotalCrossModuleEntries, TotalGeneratedEntries, TEXT("Generated function table summary test should keep direct, stub, and cross-module totals aligned with totalGeneratedEntries")))
		{
			return;
		}

		const double ExpectedDirectBindRate = TotalGeneratedEntries > 0 ? static_cast<double>(TotalDirectBindEntries) / static_cast<double>(TotalGeneratedEntries) : 0.0;
		const double ExpectedStubRate = TotalGeneratedEntries > 0 ? static_cast<double>(TotalStubEntries) / static_cast<double>(TotalGeneratedEntries) : 0.0;
		const double ExpectedCrossModuleRate = TotalGeneratedEntries > 0 ? static_cast<double>(TotalCrossModuleEntries) / static_cast<double>(TotalGeneratedEntries) : 0.0;
		bool bOk = true;
		bOk &= this->Assert.IsNear(ExpectedDirectBindRate, DirectBindRate, 1.e-9, TEXT("Generated function table summary test should keep directBindRate aligned with entry counts"));
		bOk &= this->Assert.IsNear(ExpectedStubRate, StubRate, 1.e-9, TEXT("Generated function table summary test should keep stubRate aligned with entry counts"));
		bOk &= this->Assert.IsNear(ExpectedCrossModuleRate, CrossModuleRate, 1.e-9, TEXT("Generated function table summary test should keep crossModuleRate aligned with entry counts"));
		bOk &= this->Assert.IsNear(1.0, DirectBindRate + StubRate + CrossModuleRate, 1.e-9, TEXT("Generated function table summary test should keep directBindRate, stubRate, and crossModuleRate normalized"));

		const TArray<TSharedPtr<FJsonValue>>* Modules = nullptr;
		if (!this->Assert.IsTrue(SummaryObject->TryGetArrayField(TEXT("modules"), Modules) && Modules != nullptr, TEXT("Generated function table summary test should expose per-module summaries")))
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

				if (!this->Assert.IsTrue((*ModuleObject)->TryGetNumberField(TEXT("directBindEntries"), ModuleDirectBindEntries), TEXT("Generated function table summary test should expose per-module directBindEntries")))
				{
					return;
				}

				if (!this->Assert.IsTrue((*ModuleObject)->TryGetNumberField(TEXT("stubEntries"), ModuleStubEntries), TEXT("Generated function table summary test should expose per-module stubEntries")))
				{
					return;
				}

				if (!this->Assert.IsTrue((*ModuleObject)->TryGetNumberField(TEXT("crossModuleEntries"), ModuleCrossModuleEntries), TEXT("Generated function table summary test should expose per-module crossModuleEntries")))
				{
					return;
				}

				if (!this->Assert.IsTrue((*ModuleObject)->TryGetNumberField(TEXT("directBindRate"), ModuleDirectBindRate), TEXT("Generated function table summary test should expose per-module directBindRate")))
				{
					return;
				}

				if (!this->Assert.IsTrue((*ModuleObject)->TryGetNumberField(TEXT("stubRate"), ModuleStubRate), TEXT("Generated function table summary test should expose per-module stubRate")))
				{
					return;
				}

				if (!this->Assert.IsTrue((*ModuleObject)->TryGetNumberField(TEXT("crossModuleRate"), ModuleCrossModuleRate), TEXT("Generated function table summary test should expose per-module crossModuleRate")))
				{
					return;
				}

				if (!this->Assert.AreEqual(ModuleDirectBindEntries + ModuleStubEntries + ModuleCrossModuleEntries, ModuleEntries, TEXT("Generated function table summary test should keep module totals aligned")))
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
				if (!this->Assert.IsNear(ExpectedModuleDirectRate, ModuleDirectBindRate, 1.e-9, TEXT("Generated function table summary test should keep module directBindRate aligned with entry counts")))
				{
					return;
				}

				if (!this->Assert.IsNear(ExpectedModuleStubRate, ModuleStubRate, 1.e-9, TEXT("Generated function table summary test should keep module stubRate aligned with entry counts")))
				{
					return;
				}

				if (!this->Assert.IsNear(ExpectedModuleCrossModuleRate, ModuleCrossModuleRate, 1.e-9, TEXT("Generated function table summary test should keep module crossModuleRate aligned with entry counts")))
				{
					return;
				}
			}
		}

		bOk &= this->Assert.AreEqual(CountedRegistrations, RuntimeShardEntries, TEXT("Generated function table summary test should match runtime shard registration count"));
		bOk &= this->Assert.AreEqual(SummedModuleEntries, TotalGeneratedEntries, TEXT("Generated function table summary test should keep totalGeneratedEntries equal to the sum of module totals"));
		(void)bOk;
	}

	TEST_METHOD(CsvOutput)
	{
const FString GeneratedDirectory = FPaths::Combine(
			FPaths::ProjectPluginsDir(),
			TEXT("Angelscript"),
			TEXT("Intermediate/Build/Win64/UnrealEditor/Inc/AngelscriptRuntime/UHT"));
		const FString SummaryPath = FPaths::Combine(GeneratedDirectory, TEXT("AS_FunctionTable_Summary.json"));
		const FString ModuleCsvPath = FPaths::Combine(GeneratedDirectory, TEXT("AS_FunctionTable_ModuleSummary.csv"));
		const FString EntryCsvPath = FPaths::Combine(GeneratedDirectory, TEXT("AS_FunctionTable_Entries.csv"));

		FString SummaryJson;
		if (!this->Assert.IsTrue(FFileHelper::LoadFileToString(SummaryJson, *SummaryPath), TEXT("Generated function table csv test should find the summary json output")))
		{
			return;
		}

		TSharedPtr<FJsonObject> SummaryObject;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SummaryJson);
		if (!this->Assert.IsTrue(FJsonSerializer::Deserialize(Reader, SummaryObject) && SummaryObject.IsValid(), TEXT("Generated function table csv test should parse the summary json")))
		{
			return;
		}

		int32 TotalGeneratedEntries = 0;
		if (!this->Assert.IsTrue(SummaryObject->TryGetNumberField(TEXT("totalGeneratedEntries"), TotalGeneratedEntries), TEXT("Generated function table csv test should expose totalGeneratedEntries")))
		{
			return;
		}

		int32 TotalDirectBindEntries = 0;
		if (!this->Assert.IsTrue(SummaryObject->TryGetNumberField(TEXT("totalDirectBindEntries"), TotalDirectBindEntries), TEXT("Generated function table csv test should expose totalDirectBindEntries")))
		{
			return;
		}

		int32 TotalStubEntries = 0;
		if (!this->Assert.IsTrue(SummaryObject->TryGetNumberField(TEXT("totalStubEntries"), TotalStubEntries), TEXT("Generated function table csv test should expose totalStubEntries")))
		{
			return;
		}

		int32 TotalCrossModuleEntries = 0;
		if (!this->Assert.IsTrue(SummaryObject->TryGetNumberField(TEXT("totalCrossModuleEntries"), TotalCrossModuleEntries), TEXT("Generated function table csv test should expose totalCrossModuleEntries")))
		{
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>* Modules = nullptr;
		if (!this->Assert.IsTrue(SummaryObject->TryGetArrayField(TEXT("modules"), Modules) && Modules != nullptr, TEXT("Generated function table csv test should expose per-module summaries")))
		{
			return;
		}

		const TArray<FString> ModuleLines = LoadNonEmptyFileLines(ModuleCsvPath);
		if (!this->Assert.IsTrue(ModuleLines.Num() > 0, TEXT("Generated function table csv test should write the module summary csv")))
		{
			return;
		}

		bool bOk = true;
		bOk &= this->Assert.AreEqual(Modules->Num(), ModuleLines.Num() - 1, TEXT("Generated function table csv test should keep one module csv row per module summary"));
		bOk &= this->Assert.AreEqual(FString(TEXT("ModuleName,EditorOnly,TotalEntries,DirectBindEntries,StubEntries,CrossModuleEntries,DirectBindRate,StubRate,CrossModuleRate,ShardCount")), ModuleLines[0], TEXT("Generated function table csv test should write the expected module csv header"));

		const TArray<FString> EntryLines = LoadNonEmptyFileLines(EntryCsvPath);
		if (!this->Assert.IsTrue(EntryLines.Num() > 0, TEXT("Generated function table csv test should write the entry detail csv")))
		{
			return;
		}

		bOk &= this->Assert.AreEqual(TotalGeneratedEntries, EntryLines.Num() - 1, TEXT("Generated function table csv test should keep one entry csv row per generated binding entry"));
		bOk &= this->Assert.AreEqual(FString(TEXT("ModuleName,EditorOnly,ClassName,FunctionName,EntryKind,EraseMacro,ShardIndex,ThunkStyle")), EntryLines[0], TEXT("Generated function table csv test should write the expected entry csv header"));

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

		bOk &= this->Assert.AreEqual(TotalDirectBindEntries, DirectCsvEntries, TEXT("Generated function table csv test should keep direct entry rows aligned with the summary"));
		bOk &= this->Assert.AreEqual(TotalStubEntries, StubCsvEntries, TEXT("Generated function table csv test should keep stub entry rows aligned with the summary"));
		bOk &= this->Assert.AreEqual(TotalCrossModuleEntries, CrossModuleCsvEntries, TEXT("Generated function table csv test should keep cross-module entry rows aligned with the summary"));

		FString RunBehaviorTreeCsvLine;
		if (!this->Assert.IsTrue(FindGeneratedBindingLine(GeneratedDirectory, TEXT("\"RunBehaviorTree\""), RunBehaviorTreeCsvLine), TEXT("Generated function table csv test should include RunBehaviorTree in the entry csv")))
		{
			return;
		}

		bool bFoundRunBehaviorTreeCsv = false;
		for (const FString& EntryLine : EntryLines)
		{
			if (EntryLine.Contains(TEXT(",RunBehaviorTree,")))
			{
				bFoundRunBehaviorTreeCsv = true;
				bOk &= this->Assert.IsTrue(EntryLine.Contains(TEXT(",Direct,")), TEXT("Generated function table csv test should classify RunBehaviorTree as a direct entry"));
				bOk &= this->Assert.IsFalse(EntryLine.Contains(TEXT("ERASE_NO_FUNCTION()")), TEXT("Generated function table csv test should not emit ERASE_NO_FUNCTION for RunBehaviorTree in the csv"));
				break;
			}
		}

		bOk &= this->Assert.IsTrue(bFoundRunBehaviorTreeCsv, TEXT("Generated function table csv test should locate RunBehaviorTree in the entry csv"));
		(void)bOk;
	}

	TEST_METHOD(SkippedCsvOutput)
	{
const FString GeneratedDirectory = FPaths::Combine(
			FPaths::ProjectPluginsDir(),
			TEXT("Angelscript"),
			TEXT("Intermediate/Build/Win64/UnrealEditor/Inc/AngelscriptRuntime/UHT"));
		const FString SkippedCsvPath = FPaths::Combine(GeneratedDirectory, TEXT("AS_FunctionTable_SkippedEntries.csv"));

		const TArray<FString> SkippedLines = LoadNonEmptyFileLines(SkippedCsvPath);
		if (!this->Assert.IsTrue(SkippedLines.Num() > 0, TEXT("Generated function table skipped csv test should write the skipped entry csv")))
		{
			return;
		}

		bool bOk = true;
		bOk &= this->Assert.AreEqual(FString(TEXT("ModuleName,ClassName,FunctionName,FailureReason")), SkippedLines[0], TEXT("Generated function table skipped csv test should write the expected skipped csv header"));
		bOk &= this->Assert.IsTrue(SkippedLines.Num() > 1, TEXT("Generated function table skipped csv test should contain at least one skipped function row"));

		bool bFoundFailureReason = false;
		for (int32 LineIndex = 1; LineIndex < SkippedLines.Num(); ++LineIndex)
		{
			TArray<FString> Columns;
			SkippedLines[LineIndex].ParseIntoArray(Columns, TEXT(","), false);
			if (!this->Assert.IsTrue(Columns.Num() == 4, TEXT("Generated function table skipped csv rows should expose four columns")))
			{
				return;
			}

			if (!Columns[3].IsEmpty())
			{
				bFoundFailureReason = true;
			}
		}

		bOk &= this->Assert.IsTrue(bFoundFailureReason, TEXT("Generated function table skipped csv rows should include non-empty failure reasons"));
		(void)bOk;
	}

	TEST_METHOD(SkippedReasonSummaryCsvOutput)
	{
const FString GeneratedDirectory = FPaths::Combine(
			FPaths::ProjectPluginsDir(),
			TEXT("Angelscript"),
			TEXT("Intermediate/Build/Win64/UnrealEditor/Inc/AngelscriptRuntime/UHT"));
		const FString SkippedCsvPath = FPaths::Combine(GeneratedDirectory, TEXT("AS_FunctionTable_SkippedEntries.csv"));
		const FString ReasonSummaryCsvPath = FPaths::Combine(GeneratedDirectory, TEXT("AS_FunctionTable_SkippedReasonSummary.csv"));

		const TArray<FString> SkippedLines = LoadNonEmptyFileLines(SkippedCsvPath);
		if (!this->Assert.IsTrue(SkippedLines.Num() > 0, TEXT("Generated function table skipped reason summary test should find the skipped entry csv")))
		{
			return;
		}

		const TArray<FString> SummaryLines = LoadNonEmptyFileLines(ReasonSummaryCsvPath);
		if (!this->Assert.IsTrue(SummaryLines.Num() > 0, TEXT("Generated function table skipped reason summary test should write the skipped reason summary csv")))
		{
			return;
		}

		bool bOk = true;
		bOk &= this->Assert.AreEqual(FString(TEXT("FailureReason,SkippedCount")), SummaryLines[0], TEXT("Generated function table skipped reason summary test should write the expected summary csv header"));
		bOk &= this->Assert.IsTrue(SummaryLines.Num() > 1, TEXT("Generated function table skipped reason summary test should contain at least one reason row"));

		int32 SummedSkippedCount = 0;
		for (int32 LineIndex = 1; LineIndex < SummaryLines.Num(); ++LineIndex)
		{
			TArray<FString> Columns;
			SummaryLines[LineIndex].ParseIntoArray(Columns, TEXT(","), false);
			if (!this->Assert.IsTrue(Columns.Num() == 2, TEXT("Generated function table skipped reason summary rows should expose two columns")))
			{
				return;
			}

			if (!this->Assert.IsTrue(!Columns[0].IsEmpty(), TEXT("Generated function table skipped reason summary rows should include a non-empty reason")))
			{
				return;
			}

			SummedSkippedCount += FCString::Atoi(*Columns[1]);
		}

		bOk &= this->Assert.AreEqual(SkippedLines.Num() - 1, SummedSkippedCount, TEXT("Generated function table skipped reason summary test should keep aggregate counts aligned with the skipped entry csv"));
		(void)bOk;
	}

	TEST_METHOD(MacroQualifiedDirectBindings)
	{
const FString GeneratedDirectory = FPaths::Combine(
			FPaths::ProjectPluginsDir(),
			TEXT("Angelscript"),
			TEXT("Intermediate/Build/Win64/UnrealEditor/Inc/AngelscriptRuntime/UHT"));

		FString RunBehaviorTreeLine;
		if (!this->Assert.IsTrue(FindGeneratedBindingLine(GeneratedDirectory, TEXT("\"RunBehaviorTree\""), RunBehaviorTreeLine), TEXT("Macro-qualified direct bindings test should find generated entry for RunBehaviorTree")))
		{
			return;
		}

		bool bOk = true;
		bOk &= this->Assert.IsFalse(RunBehaviorTreeLine.Contains(TEXT("ERASE_NO_FUNCTION()")), TEXT("Macro-qualified direct bindings test should not reduce RunBehaviorTree to ERASE_NO_FUNCTION"));
		bOk &= this->Assert.IsTrue(RunBehaviorTreeLine.Contains(TEXT("ERASE_AUTO_METHOD_PTR")) || RunBehaviorTreeLine.Contains(TEXT("ERASE_METHOD_PTR")), TEXT("Macro-qualified direct bindings test should keep RunBehaviorTree on a direct erase macro path"));

		FString ReportPerceptionEventLine;
		if (!this->Assert.IsTrue(FindGeneratedBindingLine(GeneratedDirectory, TEXT("\"ReportPerceptionEvent\""), ReportPerceptionEventLine), TEXT("Macro-qualified direct bindings test should find generated entry for ReportPerceptionEvent")))
		{
			return;
		}

		bOk &= this->Assert.IsFalse(ReportPerceptionEventLine.Contains(TEXT("ERASE_NO_FUNCTION()")), TEXT("Macro-qualified direct bindings test should not reduce ReportPerceptionEvent to ERASE_NO_FUNCTION"));
		bOk &= this->Assert.IsTrue(ReportPerceptionEventLine.Contains(TEXT("ERASE_AUTO_FUNCTION_PTR")) || ReportPerceptionEventLine.Contains(TEXT("ERASE_FUNCTION_PTR")), TEXT("Macro-qualified direct bindings test should keep ReportPerceptionEvent on a direct erase macro path"));
		(void)bOk;
	}
};

#endif
