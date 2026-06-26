#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "ClassGenerator/ASClass.h"
#include "Editor.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptActor.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "LevelEditorSubsystem.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/GarbageCollection.h"
#include "UObject/UnrealType.h"

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadLevelBlueprintTests,
	"Angelscript.TestModule.HotReload.LevelBlueprint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName CreateModuleName = FName(TEXT("HotReloadLevelBlueprintCreate"));
	inline static const FString CreateFilename = FString(TEXT("HotReloadLevelBlueprintCreate.as"));
	inline static const FName CreateParentClassName = FName(TEXT("AHotReloadLevelBlueprintCreateParent"));

	inline static const FName SoftReloadModuleName = FName(TEXT("HotReloadLevelBlueprintSoft"));
	inline static const FString SoftReloadFilename = FString(TEXT("HotReloadLevelBlueprintSoft.as"));
	inline static const FName SoftReloadParentClassName = FName(TEXT("AHotReloadLevelBlueprintSoftParent"));

	inline static const FName StructuralReloadModuleName = FName(TEXT("HotReloadLevelBlueprintStructural"));
	inline static const FString StructuralReloadFilename = FString(TEXT("HotReloadLevelBlueprintStructural.as"));
	inline static const FName StructuralReloadParentClassName = FName(TEXT("AHotReloadLevelBlueprintStructuralParent"));

	inline static const FString AutomationMapRootLongPackagePath = FString(TEXT("/Game/__Automation/HotReload/LevelBlueprint"));

	static bool IsHandledReloadResult(const ECompileResult ReloadResult)
	{
		return ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled;
	}

	static void DiscardModule(FAngelscriptEngine& Engine, const FName ModuleName)
	{
		Engine.DiscardModule(*ModuleName.ToString());
	}

	struct FScopedLevelScriptActorClassOverride
	{
		TSubclassOf<ALevelScriptActor> SavedClass;

		explicit FScopedLevelScriptActorClassOverride(UClass* NewClass)
		{
			if (GEngine != nullptr)
			{
				SavedClass = GEngine->LevelScriptActorClass;
				GEngine->LevelScriptActorClass = NewClass;
			}
		}

		~FScopedLevelScriptActorClassOverride()
		{
			if (GEngine != nullptr)
			{
				GEngine->LevelScriptActorClass = SavedClass;
			}
		}
	};

	struct FScopedTemporaryEditorLevel
	{
		FString MapLongPackageName;
		bool bCreated = false;

		~FScopedTemporaryEditorLevel()
		{
			Cleanup();
		}

		bool Create(FAutomationTestBase& Test, FStringView Suffix)
		{
			FNoDiscardAsserter LocalAssert(Test);
			if (!LocalAssert.IsNotNull(GEditor, TEXT("HotReload LevelBlueprint test should have GEditor")))
			{
				return false;
			}

			ULevelEditorSubsystem* LevelEditorSubsystem = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();
			if (!LocalAssert.IsNotNull(LevelEditorSubsystem, TEXT("HotReload LevelBlueprint test should resolve LevelEditorSubsystem")))
			{
				return false;
			}

			const FString MapAssetName = FString::Printf(
				TEXT("ASHotReloadLevelBlueprint_%.*s_%s"),
				Suffix.Len(),
				Suffix.GetData(),
				*FGuid::NewGuid().ToString(EGuidFormats::Digits));
			MapLongPackageName = AutomationMapRootLongPackagePath / MapAssetName;

			bCreated = LevelEditorSubsystem->NewLevel(MapLongPackageName);
			if (!LocalAssert.IsTrue(bCreated, TEXT("HotReload LevelBlueprint test should create and open a temporary editor level")))
			{
				return false;
			}

			FlushMapAssetRegistryEvents();

			UWorld* EditorWorld = GetEditorWorld();
			if (!LocalAssert.IsNotNull(EditorWorld, TEXT("HotReload LevelBlueprint test should expose the current editor world")))
			{
				return false;
			}

			return LocalAssert.AreEqual(
				MapLongPackageName,
				EditorWorld->GetOutermost()->GetName(),
				TEXT("HotReload LevelBlueprint test should open the newly created map"));
		}

		UWorld* GetEditorWorld() const
		{
			return GEditor != nullptr ? GEditor->GetEditorWorldContext().World() : nullptr;
		}

		FString GetDiskPackagePath() const
		{
			return FPackageName::LongPackageNameToFilename(MapLongPackageName, FPackageName::GetMapPackageExtension());
		}

		void FlushMapAssetRegistryEvents() const
		{
			if (MapLongPackageName.IsEmpty())
			{
				return;
			}

			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
			AssetRegistryModule.Get().ScanFilesSynchronous({ GetDiskPackagePath() }, true);
			AssetRegistryModule.Get().WaitForCompletion();
			FAssetRegistryModule::TickAssetRegistry(0.0f);
		}

		void Cleanup()
		{
			if (!bCreated)
			{
				return;
			}

			if (GEditor != nullptr)
			{
				FAutomationEditorCommonUtils::CreateNewMap();
			}

			const FString DiskPackagePath = GetDiskPackagePath();
			IFileManager::Get().Delete(*DiskPackagePath, false, true, true);

			const FString DiskDirectory = FPaths::ProjectContentDir() / TEXT("__Automation/HotReload/LevelBlueprint");
			IFileManager::Get().DeleteDirectory(*DiskDirectory, false, true);

			bCreated = false;
			CollectGarbage(RF_NoFlags, true);
		}
	};

	static ULevelScriptBlueprint* CreateAndCompileLevelBlueprint(FAutomationTestBase& Test, UWorld* EditorWorld)
	{
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(EditorWorld, TEXT("HotReload LevelBlueprint test should have an editor world")))
		{
			return nullptr;
		}

		ULevel* PersistentLevel = EditorWorld->PersistentLevel.Get();
		if (!LocalAssert.IsNotNull(PersistentLevel, TEXT("HotReload LevelBlueprint test should have a persistent level")))
		{
			return nullptr;
		}

		ULevelScriptBlueprint* LevelBlueprint = PersistentLevel->GetLevelScriptBlueprint();
		if (!LocalAssert.IsNotNull(LevelBlueprint, TEXT("HotReload LevelBlueprint test should create a LevelScriptBlueprint")))
		{
			return nullptr;
		}

		FKismetEditorUtilities::CompileBlueprint(LevelBlueprint);
		if (!LocalAssert.IsNotNull(LevelBlueprint->GeneratedClass.Get(), TEXT("HotReload LevelBlueprint test should compile a generated Level Blueprint class")))
		{
			return nullptr;
		}

		return LevelBlueprint;
	}

	static ALevelScriptActor* GetLevelScriptActor(FAutomationTestBase& Test, UWorld* EditorWorld)
	{
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(EditorWorld, TEXT("HotReload LevelBlueprint test should have an editor world for LevelScriptActor")))
		{
			return nullptr;
		}

		ULevel* PersistentLevel = EditorWorld->PersistentLevel.Get();
		if (!LocalAssert.IsNotNull(PersistentLevel, TEXT("HotReload LevelBlueprint test should have a persistent level for LevelScriptActor")))
		{
			return nullptr;
		}

		ALevelScriptActor* LevelScriptActor = PersistentLevel->GetLevelScriptActor();
		return LocalAssert.IsNotNull(LevelScriptActor, TEXT("HotReload LevelBlueprint test should create a LevelScriptActor for the open editor level"))
			? LevelScriptActor
			: nullptr;
	}

	static bool InvokeGetValue(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		UObject* Object,
		UClass* OwnerClass,
		const int32 ExpectedValue,
		const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(Object, *FString::Printf(TEXT("%s should have a target object"), Context)))
		{
			return false;
		}

		if (!LocalAssert.IsNotNull(OwnerClass, *FString::Printf(TEXT("%s should have an owner class"), Context)))
		{
			return false;
		}

		UFunction* GetValueFunction = FindGeneratedFunction(OwnerClass, TEXT("GetValue"));
		if (!LocalAssert.IsNotNull(GetValueFunction, *FString::Printf(TEXT("%s should expose GetValue"), Context)))
		{
			return false;
		}

		int32 ActualValue = 0;
		if (!LocalAssert.IsTrue(
				ExecuteGeneratedIntEventOnGameThread(&Engine, Object, GetValueFunction, ActualValue),
				*FString::Printf(TEXT("%s should execute GetValue"), Context)))
		{
			return false;
		}

		return LocalAssert.AreEqual(
			ExpectedValue,
			ActualValue,
			*FString::Printf(TEXT("%s should observe the expected GetValue result"), Context));
	}

	static bool LevelBlueprintParentChainResolvesTo(
		FAutomationTestBase& Test,
		UClass* LevelBlueprintClass,
		UClass* ExpectedMostUpToDateClass,
		const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		UASClass* ASParent = UASClass::GetFirstASClass(LevelBlueprintClass);
		if (!LocalAssert.IsNotNull(ASParent, *FString::Printf(TEXT("%s should resolve an AS parent through the Level Blueprint parent chain"), Context)))
		{
			return false;
		}

		return LocalAssert.AreEqual(
			ExpectedMostUpToDateClass,
			ASParent->GetMostUpToDateClass(),
			*FString::Printf(TEXT("%s should resolve the expected most up-to-date AS parent"), Context));
	}

	static bool ReadIntProperty(
		FAutomationTestBase& Test,
		UObject* Object,
		FName PropertyName,
		int32& OutValue,
		const TCHAR* Context)
	{
		FNoDiscardAsserter LocalAssert(Test);
		const bool bRead = AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(Test, Object, PropertyName, OutValue);
		return LocalAssert.IsTrue(bRead, *FString::Printf(TEXT("%s should read property '%s'"), Context, *PropertyName.ToString()));
	}

public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(CreateEditorLevelBlueprintWithAngelscriptParent)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			DiscardModule(Engine, CreateModuleName);
		};

		const FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS(Blueprintable, NotPlaceable)
			class AHotReloadLevelBlueprintCreateParent : ALevelScriptActor
			{
				UPROPERTY()
				int Value = 7;
			}
			)AS");

		UClass* ParentClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			*TestRunner,
			Engine,
			CreateModuleName,
			CreateFilename,
			ScriptSource,
			CreateParentClassName);
		ASSERT_THAT(IsNotNull(ParentClass, TEXT("Level Blueprint AS parent should compile")));
		ASSERT_THAT(IsTrue(ParentClass->IsChildOf(ALevelScriptActor::StaticClass()), TEXT("Level Blueprint AS parent should derive from ALevelScriptActor")));

		UASClass* ParentASClass = Cast<UASClass>(ParentClass);
		ASSERT_THAT(IsNotNull(ParentASClass, TEXT("Level Blueprint AS parent should be a UASClass")));

		FScopedLevelScriptActorClassOverride LevelScriptActorClassOverride(ParentClass);
		FScopedTemporaryEditorLevel EditorLevel;
		ASSERT_THAT(IsTrue(EditorLevel.Create(*TestRunner, TEXT("Create"))));

		UWorld* EditorWorld = EditorLevel.GetEditorWorld();
		ASSERT_THAT(IsNotNull(EditorWorld, TEXT("Level Blueprint create test should have an open editor world")));
		ASSERT_THAT(IsTrue(EditorWorld->IsEditorWorld(), TEXT("Level Blueprint create test should run against an editor world")));

		ULevelScriptBlueprint* LevelBlueprint = CreateAndCompileLevelBlueprint(*TestRunner, EditorWorld);
		ASSERT_THAT(IsNotNull(LevelBlueprint, TEXT("Created Level Blueprint should compile")));
		ASSERT_THAT(AreEqual(BPTYPE_LevelScript, LevelBlueprint->BlueprintType, TEXT("Created Blueprint should be a LevelScript Blueprint")));
		ASSERT_THAT(AreEqual(ParentClass, LevelBlueprint->ParentClass.Get(), TEXT("Created Level Blueprint should use the AS LevelScriptActor parent")));

		UClass* GeneratedClass = LevelBlueprint->GeneratedClass.Get();
		ASSERT_THAT(IsNotNull(GeneratedClass, TEXT("Created Level Blueprint should expose a generated class")));
		ASSERT_THAT(IsTrue(GeneratedClass->IsChildOf(ParentClass), TEXT("Created Level Blueprint class should inherit from the AS parent")));
		ASSERT_THAT(IsNull(Cast<UASClass>(GeneratedClass), TEXT("Created Level Blueprint generated class should not itself be a UASClass")));
		ASSERT_THAT(AreEqual(ParentASClass, UASClass::GetFirstASClass(GeneratedClass), TEXT("Created Level Blueprint should resolve the AS parent through its generated class")));

		ALevelScriptActor* LevelScriptActor = GetLevelScriptActor(*TestRunner, EditorWorld);
		ASSERT_THAT(IsNotNull(LevelScriptActor, TEXT("Created Level Blueprint should create a LevelScriptActor")));
		ASSERT_THAT(AreEqual(GeneratedClass, LevelScriptActor->GetClass(), TEXT("Opened editor level should instantiate the Level Blueprint generated class")));
	}

	TEST_METHOD(SoftReloadKeepsOpenEditorLevelBlueprintParentAndUpdatesBody)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			DiscardModule(Engine, SoftReloadModuleName);
		};

		const FString ScriptV1 = ASTEST_AS(R"AS(
			UCLASS(Blueprintable, NotPlaceable)
			class AHotReloadLevelBlueprintSoftParent : ALevelScriptActor
			{
				UFUNCTION()
				int GetValue()
				{
					return 11;
				}
			}
			)AS");

		const FString ScriptV2 = ASTEST_AS(R"AS(
			UCLASS(Blueprintable, NotPlaceable)
			class AHotReloadLevelBlueprintSoftParent : ALevelScriptActor
			{
				UFUNCTION()
				int GetValue()
				{
					return 22;
				}
			}
			)AS");

		UClass* ParentClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			*TestRunner,
			Engine,
			SoftReloadModuleName,
			SoftReloadFilename,
			ScriptV1,
			SoftReloadParentClassName);
		ASSERT_THAT(IsNotNull(ParentClass, TEXT("Soft Level Blueprint parent should compile")));

		UASClass* ParentASClass = Cast<UASClass>(ParentClass);
		ASSERT_THAT(IsNotNull(ParentASClass, TEXT("Soft Level Blueprint parent should be a UASClass")));

		FScopedLevelScriptActorClassOverride LevelScriptActorClassOverride(ParentClass);
		FScopedTemporaryEditorLevel EditorLevel;
		ASSERT_THAT(IsTrue(EditorLevel.Create(*TestRunner, TEXT("Soft"))));

		UWorld* EditorWorld = EditorLevel.GetEditorWorld();
		ULevelScriptBlueprint* LevelBlueprint = CreateAndCompileLevelBlueprint(*TestRunner, EditorWorld);
		ASSERT_THAT(IsNotNull(LevelBlueprint, TEXT("Soft Level Blueprint should compile")));

		UClass* InitialLevelBlueprintClass = LevelBlueprint->GeneratedClass.Get();
		ASSERT_THAT(IsNotNull(InitialLevelBlueprintClass, TEXT("Soft Level Blueprint should expose its initial generated class")));
		ASSERT_THAT(IsTrue(InitialLevelBlueprintClass->IsChildOf(ParentClass), TEXT("Soft Level Blueprint should inherit from the AS parent before reload")));
		ASSERT_THAT(AreEqual(ParentASClass, UASClass::GetFirstASClass(InitialLevelBlueprintClass), TEXT("Soft Level Blueprint should resolve AS parent before reload")));

		ALevelScriptActor* InitialLevelScriptActor = GetLevelScriptActor(*TestRunner, EditorWorld);
		ASSERT_THAT(IsNotNull(InitialLevelScriptActor, TEXT("Soft Level Blueprint should create the initial LevelScriptActor")));
		ASSERT_THAT(IsTrue(InvokeGetValue(*TestRunner, Engine, InitialLevelScriptActor, ParentClass, 11, TEXT("Soft Level Blueprint baseline"))));

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::SoftReloadOnly, SoftReloadModuleName, SoftReloadFilename, ScriptV2, ReloadResult),
			TEXT("Soft Level Blueprint should compile the body-only reload")));
		ASSERT_THAT(AreEqual(ECompileResult::FullyHandled, ReloadResult, TEXT("Soft Level Blueprint should stay on the pure soft reload path")));

		UClass* ReloadedParentClass = FindGeneratedClass(&Engine, SoftReloadParentClassName);
		ASSERT_THAT(IsNotNull(ReloadedParentClass, TEXT("Soft Level Blueprint should resolve the AS parent after reload")));
		ASSERT_THAT(AreEqual(ParentClass, ReloadedParentClass, TEXT("Soft Level Blueprint should keep parent UClass identity")));

		ASSERT_THAT(IsTrue(EditorLevel.GetEditorWorld() == EditorWorld, TEXT("Soft Level Blueprint should keep the same editor map open after reload")));
		FKismetEditorUtilities::CompileBlueprint(LevelBlueprint);

		UClass* ReloadedLevelBlueprintClass = LevelBlueprint->GeneratedClass.Get();
		ASSERT_THAT(IsNotNull(ReloadedLevelBlueprintClass, TEXT("Soft Level Blueprint should expose generated class after reload")));
		ASSERT_THAT(AreEqual(ParentASClass, UASClass::GetFirstASClass(ReloadedLevelBlueprintClass), TEXT("Soft Level Blueprint should keep the AS parent chain after reload")));

		ALevelScriptActor* ReloadedLevelScriptActor = GetLevelScriptActor(*TestRunner, EditorWorld);
		ASSERT_THAT(IsNotNull(ReloadedLevelScriptActor, TEXT("Soft Level Blueprint should keep a LevelScriptActor after reload")));
		ASSERT_THAT(IsTrue(InvokeGetValue(*TestRunner, Engine, ReloadedLevelScriptActor, ReloadedParentClass, 22, TEXT("Soft Level Blueprint after reload"))));
	}

	TEST_METHOD(FullReloadKeepsOpenEditorLevelBlueprintRecoverableAfterParentShapeChange)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		ON_SCOPE_EXIT
		{
			DiscardModule(Engine, StructuralReloadModuleName);
		};

		const FString ScriptV1 = ASTEST_AS(R"AS(
			UCLASS(Blueprintable, NotPlaceable)
			class AHotReloadLevelBlueprintStructuralParent : ALevelScriptActor
			{
				UPROPERTY()
				int ExistingValue = 12;

				UFUNCTION()
				int GetValue()
				{
					return ExistingValue;
				}
			}
			)AS");

		const FString ScriptV2 = ASTEST_AS(R"AS(
			UCLASS(Blueprintable, NotPlaceable)
			class AHotReloadLevelBlueprintStructuralParent : ALevelScriptActor
			{
				UPROPERTY()
				int ExistingValue = 12;

				UPROPERTY()
				int AddedValue = 33;

				UFUNCTION()
				int GetValue()
				{
					return ExistingValue + AddedValue;
				}
			}
			)AS");

		UClass* InitialParentClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			*TestRunner,
			Engine,
			StructuralReloadModuleName,
			StructuralReloadFilename,
			ScriptV1,
			StructuralReloadParentClassName);
		ASSERT_THAT(IsNotNull(InitialParentClass, TEXT("Structural Level Blueprint parent should compile")));

		UASClass* InitialASClass = Cast<UASClass>(InitialParentClass);
		ASSERT_THAT(IsNotNull(InitialASClass, TEXT("Structural Level Blueprint parent should be a UASClass")));

		FScopedLevelScriptActorClassOverride LevelScriptActorClassOverride(InitialParentClass);
		FScopedTemporaryEditorLevel EditorLevel;
		ASSERT_THAT(IsTrue(EditorLevel.Create(*TestRunner, TEXT("Structural"))));

		UWorld* EditorWorld = EditorLevel.GetEditorWorld();
		ULevelScriptBlueprint* LevelBlueprint = CreateAndCompileLevelBlueprint(*TestRunner, EditorWorld);
		ASSERT_THAT(IsNotNull(LevelBlueprint, TEXT("Structural Level Blueprint should compile")));

		UClass* InitialLevelBlueprintClass = LevelBlueprint->GeneratedClass.Get();
		ASSERT_THAT(IsNotNull(InitialLevelBlueprintClass, TEXT("Structural Level Blueprint should expose initial generated class")));
		ASSERT_THAT(IsTrue(InitialLevelBlueprintClass->IsChildOf(InitialParentClass), TEXT("Structural Level Blueprint should inherit from initial AS parent")));
		ASSERT_THAT(AreEqual(InitialASClass, UASClass::GetFirstASClass(InitialLevelBlueprintClass), TEXT("Structural Level Blueprint should resolve initial AS parent")));

		ALevelScriptActor* InitialLevelScriptActor = GetLevelScriptActor(*TestRunner, EditorWorld);
		ASSERT_THAT(IsNotNull(InitialLevelScriptActor, TEXT("Structural Level Blueprint should create the initial LevelScriptActor")));
		ASSERT_THAT(IsTrue(InvokeGetValue(*TestRunner, Engine, InitialLevelScriptActor, InitialParentClass, 12, TEXT("Structural Level Blueprint baseline"))));

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::FullReload, StructuralReloadModuleName, StructuralReloadFilename, ScriptV2, ReloadResult),
			TEXT("Structural Level Blueprint should compile the parent shape change")));
		ASSERT_THAT(IsTrue(IsHandledReloadResult(ReloadResult), TEXT("Structural Level Blueprint reload should finish on a handled path")));

		UClass* ReloadedParentClass = FindGeneratedClass(&Engine, StructuralReloadParentClassName);
		ASSERT_THAT(IsNotNull(ReloadedParentClass, TEXT("Structural Level Blueprint should resolve parent after full reload")));
		ASSERT_THAT(IsTrue(ReloadedParentClass != InitialParentClass, TEXT("Structural Level Blueprint should replace AS parent class after full reload")));
		ASSERT_THAT(AreEqual(ReloadedParentClass, InitialASClass->GetMostUpToDateClass(), TEXT("Structural Level Blueprint should chain initial parent to current parent")));

		ASSERT_THAT(IsTrue(LevelBlueprintParentChainResolvesTo(
			*TestRunner,
			LevelBlueprint->GeneratedClass.Get(),
			ReloadedParentClass,
			TEXT("Structural Level Blueprint before recompiling"))));

		FKismetEditorUtilities::CompileBlueprint(LevelBlueprint);

		UClass* ReloadedLevelBlueprintClass = LevelBlueprint->GeneratedClass.Get();
		ASSERT_THAT(IsNotNull(ReloadedLevelBlueprintClass, TEXT("Structural Level Blueprint should expose generated class after recompilation")));
		ASSERT_THAT(IsTrue(LevelBlueprintParentChainResolvesTo(
			*TestRunner,
			ReloadedLevelBlueprintClass,
			ReloadedParentClass,
			TEXT("Structural Level Blueprint after recompiling"))));

		ALevelScriptActor* ReloadedLevelScriptActor = GetLevelScriptActor(*TestRunner, EditorWorld);
		ASSERT_THAT(IsNotNull(ReloadedLevelScriptActor, TEXT("Structural Level Blueprint should keep a LevelScriptActor after reload")));

		int32 AddedValue = 0;
		ASSERT_THAT(IsTrue(ReadIntProperty(*TestRunner, ReloadedLevelScriptActor, TEXT("AddedValue"), AddedValue, TEXT("Structural Level Blueprint after reload"))));
		ASSERT_THAT(AreEqual(33, AddedValue, TEXT("Structural Level Blueprint should expose the added AS property on the level actor")));
		ASSERT_THAT(IsTrue(InvokeGetValue(*TestRunner, Engine, ReloadedLevelScriptActor, ReloadedParentClass, 45, TEXT("Structural Level Blueprint after reload"))));
	}
};
