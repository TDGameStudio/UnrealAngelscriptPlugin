#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "ClassGenerator/AngelscriptClassRedirects.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "ClassGenerator/ASClass.h"
#include "Components/ActorTestSpawner.h"
#include "Engine/Blueprint.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "UObject/GarbageCollection.h"
#include "UObject/CoreRedirects.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

// Test Layer: UE Functional
//
// Focus: what happens to a Blueprint that was created from an Angelscript class
// when that script class is *renamed* (e.g. `AOriginal` -> `ARenamed` inside the
// same .as file) and a hot reload runs.
//
// AS-generated classes use UE's official CoreRedirect path for class renames.
// A single removed AS UCLASS plus a single added AS UCLASS is treated as an
// unambiguous rename: the generator writes `[CoreRedirects] +ClassRedirects`,
// registers the redirect for the current session, and wires OldClass->NewerVersion
// so existing Blueprint hot-reload machinery can reparent children.
//
// Ambiguous reloads are intentionally not guessed; those still leave Blueprint
// children on the removed husk until a user-authored redirect or manual reparent
// supplies the missing intent.

namespace AngelscriptHotReloadClassRenameTest
{
	static const FName RenameModuleName(TEXT("HotReloadClassRename"));
	static const FString RenameFilename(TEXT("HotReloadClassRename.as"));
	static const FName OriginalParentClassName(TEXT("AHotReloadClassRenameOriginalParent"));
	static const FName RenamedParentClassName(TEXT("AHotReloadClassRenameRenamedParent"));
	static const FName FinalParentClassName(TEXT("AHotReloadClassRenameFinalParent"));
	static const FName ExtraRemovedParentClassName(TEXT("AHotReloadClassRenameExtraRemovedParent"));
	static const FName MissingParentClassName(TEXT("AHotReloadClassRenameMissingParent"));
	static const FString GeneratedRedirectSource(TEXT("Angelscript generated class rename redirect"));

	FString MakeScriptClassPath(const FName ClassName)
	{
		return FAngelscriptClassRedirects::MakeScriptClassPath(ClassName.ToString());
	}

	FCoreRedirect MakeClassRedirect(const FString& OldClassName, const FString& NewClassName)
	{
		return FCoreRedirect(ECoreRedirectFlags::Type_Class, OldClassName, NewClassName);
	}

	FCoreRedirect MakeClassRedirect(const FName OldClassName, const FName NewClassName)
	{
		return FCoreRedirect(ECoreRedirectFlags::Type_Class, OldClassName.ToString(), NewClassName.ToString());
	}

	static bool IsHandledReloadResult(const ECompileResult ReloadResult)
	{
		return ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled;
	}

	// V1: parent class is `AHotReloadClassRenameOriginalParent`.
	static FString MakeOriginalScript()
	{
		return TEXT(R"AS(
UCLASS()
class AHotReloadClassRenameOriginalParent : AActor
{
	UPROPERTY()
	int ExampleValue = 7;

	UFUNCTION()
	int GetValue()
	{
		return ExampleValue;
	}
}
)AS");
	}

	static FString MakeAmbiguousOriginalScript()
	{
		return TEXT(R"AS(
UCLASS()
class AHotReloadClassRenameOriginalParent : AActor
{
	UPROPERTY()
	int ExampleValue = 7;

	UFUNCTION()
	int GetValue()
	{
		return ExampleValue;
	}
}

UCLASS()
class AHotReloadClassRenameExtraRemovedParent : AActor
{
}
)AS");
	}

	// V2: same file, the class has been renamed to `AHotReloadClassRenameRenamedParent`.
	static FString MakeRenamedScript()
	{
		return TEXT(R"AS(
UCLASS()
class AHotReloadClassRenameRenamedParent : AActor
{
	UPROPERTY()
	int ExampleValue = 7;

	UFUNCTION()
	int GetValue()
	{
		return ExampleValue;
	}
}
)AS");
	}

	// V3: same file, the class has been renamed again to `AHotReloadClassRenameFinalParent`.
	static FString MakeFinalScript()
	{
		return TEXT(R"AS(
UCLASS()
class AHotReloadClassRenameFinalParent : AActor
{
	UPROPERTY()
	int ExampleValue = 7;

	UFUNCTION()
	int GetValue()
	{
		return ExampleValue;
	}
}
)AS");
	}

	class FScopedClassRedirectList
	{
	public:
		FScopedClassRedirectList(TArray<FCoreRedirect>&& InRedirects, const TCHAR* InSource)
			: Redirects(MoveTemp(InRedirects))
			, Source(InSource)
		{
			FCoreRedirects::AddRedirectList(MakeArrayView(Redirects), Source);
		}

		~FScopedClassRedirectList()
		{
			FCoreRedirects::RemoveRedirectList(MakeArrayView(Redirects), Source);
		}

		FScopedClassRedirectList(const FScopedClassRedirectList&) = delete;
		FScopedClassRedirectList& operator=(const FScopedClassRedirectList&) = delete;

	private:
		TArray<FCoreRedirect> Redirects;
		FString Source;
	};

	class FScopedIniClassRedirects
	{
	public:
		~FScopedIniClassRedirects()
		{
			if (bRegistered)
			{
				FCoreRedirects::RemoveRedirectList(MakeArrayView(Redirects), IniPath);
			}

			if (!IniPath.IsEmpty())
			{
				if (GConfig != nullptr)
				{
					GConfig->UnloadFile(IniPath);
				}

				IFileManager::Get().Delete(*IniPath, false, true);
			}
		}

		bool Load(FAutomationTestBase& Test, TArray<FCoreRedirect>&& InRedirects)
		{
			Redirects = MoveTemp(InRedirects);
			IniPath = FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("Automation"),
				TEXT("CoreRedirects"),
				FString::Printf(TEXT("AngelscriptHotReloadClassRename_%s.ini"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));

			const FString IniDirectory = FPaths::GetPath(IniPath);
			if (!IFileManager::Get().MakeDirectory(*IniDirectory, true))
			{
				Test.AddError(FString::Printf(TEXT("Failed to create CoreRedirect test directory: %s"), *IniDirectory));
				return false;
			}

			FString IniContents(TEXT("[CoreRedirects]\n"));
			for (const FCoreRedirect& Redirect : Redirects)
			{
				IniContents += FString::Printf(
					TEXT("ClassRedirects=(OldName=\"%s\",NewName=\"%s\")\n"),
					*Redirect.OldName.ToString(),
					*Redirect.NewName.ToString());
			}

			if (!FFileHelper::SaveStringToFile(IniContents, *IniPath))
			{
				Test.AddError(FString::Printf(TEXT("Failed to write CoreRedirect test ini: %s"), *IniPath));
				return false;
			}

			if (GConfig == nullptr)
			{
				Test.AddError(TEXT("CoreRedirect test requires GConfig"));
				return false;
			}

			GConfig->LoadFile(IniPath);
			bRegistered = FCoreRedirects::ReadRedirectsFromIni(IniPath);
			if (!bRegistered)
			{
				Test.AddError(FString::Printf(TEXT("Failed to read CoreRedirect test ini: %s"), *IniPath));
			}

			return bRegistered;
		}

		FScopedIniClassRedirects(const FScopedIniClassRedirects&) = delete;
		FScopedIniClassRedirects& operator=(const FScopedIniClassRedirects&) = delete;

		FScopedIniClassRedirects() = default;

	private:
		TArray<FCoreRedirect> Redirects;
		FString IniPath;
		bool bRegistered = false;
	};

	class FScopedGeneratedRedirectTarget
	{
	private:
		struct FExpectedRedirect
		{
			FString OldClassPath;
			FString NewClassPath;
		};

	public:
		~FScopedGeneratedRedirectTarget()
		{
			if (CleanupRedirects.Num() != 0)
			{
				FCoreRedirects::RemoveRedirectList(MakeArrayView(CleanupRedirects), GeneratedRedirectSource);
			}

			FAngelscriptClassRedirects::ResetCoreRedirectTargetIniOverrideForTesting();

			if (!IniPath.IsEmpty())
			{
				IFileManager::Get().Delete(*IniPath, false, true);
			}
		}

		bool Initialize(FAutomationTestBase& Test)
		{
			IniPath = FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("Automation"),
				TEXT("CoreRedirects"),
				FString::Printf(TEXT("AngelscriptGeneratedClassRename_%s.ini"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));

			const FString IniDirectory = FPaths::GetPath(IniPath);
			if (!IFileManager::Get().MakeDirectory(*IniDirectory, true))
			{
				Test.AddError(FString::Printf(TEXT("Failed to create generated CoreRedirect test directory: %s"), *IniDirectory));
				return false;
			}

			FAngelscriptClassRedirects::SetCoreRedirectTargetIniOverrideForTesting(IniPath);
			return true;
		}

		bool Initialize(FAutomationTestBase& Test, const FName OldClassName, const FName NewClassName)
		{
			if (!Initialize(Test))
			{
				return false;
			}

			AddExpectedRedirect(OldClassName, NewClassName);
			return true;
		}

		void AddExpectedRedirect(const FName OldClassName, const FName NewClassName)
		{
			ExpectedRedirects.Add({
				FAngelscriptClassRedirects::MakeScriptClassPath(OldClassName.ToString()),
				FAngelscriptClassRedirects::MakeScriptClassPath(NewClassName.ToString())
			});
		}

		void MarkRedirectRegistered(const FName OldClassName, const FName NewClassName)
		{
			CleanupRedirects.Add(FCoreRedirect(
				ECoreRedirectFlags::Type_Class,
				FAngelscriptClassRedirects::MakeScriptClassPath(OldClassName.ToString()),
				FAngelscriptClassRedirects::MakeScriptClassPath(NewClassName.ToString())));
		}

		const FString& GetIniPath() const
		{
			return IniPath;
		}

		bool ContainsExpectedRedirects(FAutomationTestBase& Test) const
		{
			FConfigFile ConfigFile;
			ConfigFile.Read(IniPath);

			const FConfigSection* RedirectSection = ConfigFile.FindSection(TEXT("CoreRedirects"));
			if (!Test.TestNotNull(TEXT("Generated redirect ini should contain [CoreRedirects]"), RedirectSection))
			{
				return false;
			}

			bool bAllRedirectsFound = true;
			for (const FExpectedRedirect& ExpectedRedirect : ExpectedRedirects)
			{
				bool bFoundRedirect = false;
				const auto CheckRedirectsForKey = [&](const FName RedirectsKey)
				{
					for (FConfigSection::TConstKeyIterator It(*RedirectSection, RedirectsKey); It; ++It)
					{
						FString OldName;
						FString NewName;
						if (FParse::Value(*It.Value().GetValue(), TEXT("OldName="), OldName)
							&& FParse::Value(*It.Value().GetValue(), TEXT("NewName="), NewName)
							&& OldName == ExpectedRedirect.OldClassPath
							&& NewName == ExpectedRedirect.NewClassPath)
						{
							bFoundRedirect = true;
							break;
						}
					}
				};
				CheckRedirectsForKey(FName(TEXT("ClassRedirects")));
				CheckRedirectsForKey(FName(TEXT("+ClassRedirects")));

				if (!Test.TestTrue(
					FString::Printf(
						TEXT("Generated CoreRedirect ini should contain ClassRedirects pair %s -> %s"),
						*ExpectedRedirect.OldClassPath,
						*ExpectedRedirect.NewClassPath),
					bFoundRedirect))
				{
					bAllRedirectsFound = false;
				}
			}
			return bAllRedirectsFound;
		}

		bool OmitsRedirect(FAutomationTestBase& Test, const FName OldClassName, const FName NewClassName) const
		{
			FConfigFile ConfigFile;
			ConfigFile.Read(IniPath);

			const FConfigSection* RedirectSection = ConfigFile.FindSection(TEXT("CoreRedirects"));
			if (!Test.TestNotNull(TEXT("Generated redirect ini should contain [CoreRedirects]"), RedirectSection))
			{
				return false;
			}

			const FString OldClassPath = MakeScriptClassPath(OldClassName);
			const FString NewClassPath = MakeScriptClassPath(NewClassName);
			bool bFoundRedirect = false;
			const auto CheckRedirectsForKey = [&](const FName RedirectsKey)
			{
				for (FConfigSection::TConstKeyIterator It(*RedirectSection, RedirectsKey); It; ++It)
				{
					FString OldName;
					FString NewName;
					if (FParse::Value(*It.Value().GetValue(), TEXT("OldName="), OldName)
						&& FParse::Value(*It.Value().GetValue(), TEXT("NewName="), NewName)
						&& OldName == OldClassPath
						&& NewName == NewClassPath)
					{
						bFoundRedirect = true;
						break;
					}
				}
			};
			CheckRedirectsForKey(FName(TEXT("ClassRedirects")));
			CheckRedirectsForKey(FName(TEXT("+ClassRedirects")));

			return Test.TestFalse(
				FString::Printf(
					TEXT("Generated CoreRedirect ini should not retain stale reverse ClassRedirects pair %s -> %s"),
					*OldClassPath,
					*NewClassPath),
				bFoundRedirect);
		}

		FScopedGeneratedRedirectTarget() = default;
		FScopedGeneratedRedirectTarget(const FScopedGeneratedRedirectTarget&) = delete;
		FScopedGeneratedRedirectTarget& operator=(const FScopedGeneratedRedirectTarget&) = delete;

	private:
		FString IniPath;
		TArray<FExpectedRedirect> ExpectedRedirects;
		TArray<FCoreRedirect> CleanupRedirects;
	};

	UBlueprint* CreateTransientBlueprintChild(FAutomationTestBase& Test, UClass* ParentClass)
	{
		if (!Test.TestNotNull(TEXT("HotReload rename test should have a script parent class"), ParentClass))
		{
			return nullptr;
		}

		const FString PackagePath = FString::Printf(
			TEXT("/Temp/AngelscriptHotReloadClassRename_%s"),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits));
		UPackage* BlueprintPackage = CreatePackage(*PackagePath);
		if (!Test.TestNotNull(TEXT("HotReload rename test should create a transient Blueprint package"), BlueprintPackage))
		{
			return nullptr;
		}

		BlueprintPackage->SetFlags(RF_Transient);
		const FName BlueprintName(*FPackageName::GetLongPackageAssetName(PackagePath));

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			ParentClass,
			BlueprintPackage,
			BlueprintName,
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			TEXT("AngelscriptHotReloadClassRenameTest"));
		if (!Test.TestNotNull(TEXT("HotReload rename test should create a transient Blueprint asset"), Blueprint))
		{
			return nullptr;
		}

		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		if (!Test.TestNotNull(TEXT("HotReload rename test should compile a generated Blueprint class"), Blueprint->GeneratedClass.Get()))
		{
			return nullptr;
		}

		return Blueprint;
	}

	void CleanupTransientBlueprint(UBlueprint*& Blueprint)
	{
		if (Blueprint == nullptr)
		{
			return;
		}

		if (UClass* BlueprintClass = Blueprint->GeneratedClass)
		{
			BlueprintClass->MarkAsGarbage();
		}

		if (UPackage* BlueprintPackage = Blueprint->GetOutermost())
		{
			BlueprintPackage->MarkAsGarbage();
		}

		Blueprint->MarkAsGarbage();
		CollectGarbage(RF_NoFlags, true);
		Blueprint = nullptr;
	}

	FScopedClassRedirectList MakeScopedClassRedirect(const FName OldClassName, const FName NewClassName)
	{
		TArray<FCoreRedirect> Redirects;
		Redirects.Add(MakeClassRedirect(OldClassName, NewClassName));
		return FScopedClassRedirectList(MoveTemp(Redirects), TEXT("Angelscript hot reload class rename test"));
	}

	bool FileContainsExpectedGeneratedRedirect(FAutomationTestBase& Test, const FScopedGeneratedRedirectTarget& RedirectTarget)
	{
		FString IniContents;
		if (!FFileHelper::LoadFileToString(IniContents, *RedirectTarget.GetIniPath()))
		{
			Test.AddError(FString::Printf(TEXT("Generated CoreRedirect ini was not written: %s"), *RedirectTarget.GetIniPath()));
			return false;
		}

		return RedirectTarget.ContainsExpectedRedirects(Test);
	}

	bool FileOmitsGeneratedRedirect(FAutomationTestBase& Test, const FScopedGeneratedRedirectTarget& RedirectTarget, const FName OldClassName, const FName NewClassName)
	{
		FString IniContents;
		if (!FFileHelper::LoadFileToString(IniContents, *RedirectTarget.GetIniPath()))
		{
			Test.AddError(FString::Printf(TEXT("Generated CoreRedirect ini was not written: %s"), *RedirectTarget.GetIniPath()));
			return false;
		}

		return RedirectTarget.OmitsRedirect(Test, OldClassName, NewClassName);
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadClassRenameTests,
	"Angelscript.TestModule.HotReload.ClassRename",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(AmbiguousRenameDoesNotInferRedirectAndOrphansBlueprintChild)
	{
		using namespace AngelscriptHotReloadClassRenameTest;

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope EngineScope(Engine);
		UBlueprint* Blueprint = nullptr;
		ON_SCOPE_EXIT
		{
			CleanupTransientBlueprint(Blueprint);
			Engine.DiscardModule(*RenameModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		// Compile V1 and build a Blueprint child from the original class.
		UClass* OriginalParentClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			*TestRunner,
			Engine,
			RenameModuleName,
			RenameFilename,
			MakeAmbiguousOriginalScript(),
			OriginalParentClassName);
		ASSERT_THAT(IsNotNull(OriginalParentClass));

		UASClass* OriginalParentAS = Cast<UASClass>(OriginalParentClass);
		ASSERT_THAT(IsNotNull(OriginalParentAS, TEXT("Original parent should be a UASClass before rename")));

		Blueprint = CreateTransientBlueprintChild(*TestRunner, OriginalParentClass);
		ASSERT_THAT(IsNotNull(Blueprint));

		UClass* BlueprintClass = Blueprint->GeneratedClass.Get();
		ASSERT_THAT(IsNotNull(BlueprintClass, TEXT("Rename test should expose the generated Blueprint class")));
		ASSERT_THAT(IsTrue(BlueprintClass->IsChildOf(OriginalParentClass), TEXT("Blueprint child should derive from the original script parent")));
		ASSERT_THAT(AreEqual(OriginalParentClass, Blueprint->ParentClass.Get(), TEXT("Blueprint ParentClass should be the original script class before rename")));

		// Keep the Blueprint (and its dead parent) alive across the reload's GC by
		// spawning a live actor of the generated class, mirroring the editor case
		// where an instance exists in a level.
		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* BlueprintActor = AngelscriptFunctionalTestUtils::SpawnScriptActor(*TestRunner, Spawner, BlueprintClass);
		ASSERT_THAT(IsNotNull(BlueprintActor, TEXT("Rename test should spawn a Blueprint child actor before reload")));
		AngelscriptFunctionalTestUtils::BeginPlayActor(Engine, *BlueprintActor);

		// Hot reload the same module/file, but the class is now renamed. Because
		// the old name disappears, the generator forces a full reload.
		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::FullReload, RenameModuleName, RenameFilename, MakeRenamedScript(), ReloadResult),
			TEXT("Rename test should compile the renamed-class update")));
		ASSERT_THAT(IsTrue(IsHandledReloadResult(ReloadResult), TEXT("Rename should resolve through a handled full-reload path")));

		// The renamed class now exists under its new name as a fresh class object.
		UClass* RenamedParentClass = FindGeneratedClass(&Engine, RenamedParentClassName);
		ASSERT_THAT(IsNotNull(RenamedParentClass, TEXT("Renamed class should be generated under the new name")));
		ASSERT_THAT(IsNotNull(Cast<UASClass>(RenamedParentClass), TEXT("Renamed class should be a live UASClass")));

		// The old name no longer resolves to a live script class -- it was moved
		// aside to `<Name>_REPLACED_<N>` during cleanup.
		UClass* OldNameLookup = FindGeneratedClass(&Engine, OriginalParentClassName);
		ASSERT_THAT(IsNull(OldNameLookup, TEXT("Original class name should no longer resolve to a live class after rename")));

		// The ambiguous reload does not generate a redirect, so the original parent
		// object still exists as a removed-class husk pinned by the Blueprint.
		ASSERT_THAT(IsTrue(OriginalParentAS->HasAnyClassFlags(CLASS_Hidden), TEXT("Orphaned original class should be hidden after rename")));
		ASSERT_THAT(IsTrue(OriginalParentAS->HasAnyClassFlags(CLASS_NotPlaceable), TEXT("Orphaned original class should be non-placeable after rename")));
		ASSERT_THAT(IsTrue(OriginalParentAS->GetName().Contains(TEXT("_REPLACED_")), TEXT("Orphaned original class should be renamed aside to a _REPLACED_ husk")));
		ASSERT_THAT(IsNull(OriginalParentAS->NewerVersion, TEXT("An ambiguous rename must not establish a NewerVersion link to the renamed class")));
		ASSERT_THAT(AreEqual((UClass*)OriginalParentAS, OriginalParentAS->GetMostUpToDateClass(), TEXT("Orphaned class cannot forward to the renamed class")));

		// The Blueprint was never migrated: its ParentClass still points at the
		// dead husk, not the renamed class. This is the broken state a rename
		// leaves behind.
		ASSERT_THAT(AreEqual((UClass*)OriginalParentAS, Blueprint->ParentClass.Get(), TEXT("Blueprint ParentClass should still point at the orphaned husk -- rename does not auto-migrate it")));
		ASSERT_THAT(IsFalse(BlueprintClass->IsChildOf(RenamedParentClass), TEXT("Blueprint child should NOT automatically become a child of the renamed class")));
		}
	}

	TEST_METHOD(ManualReparentRecoversBlueprintChildAfterAmbiguousRename)
	{
		using namespace AngelscriptHotReloadClassRenameTest;

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope EngineScope(Engine);
		UBlueprint* Blueprint = nullptr;
		ON_SCOPE_EXIT
		{
			CleanupTransientBlueprint(Blueprint);
			Engine.DiscardModule(*RenameModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* OriginalParentClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			*TestRunner,
			Engine,
			RenameModuleName,
			RenameFilename,
			MakeAmbiguousOriginalScript(),
			OriginalParentClassName);
		ASSERT_THAT(IsNotNull(OriginalParentClass));

		Blueprint = CreateTransientBlueprintChild(*TestRunner, OriginalParentClass);
		ASSERT_THAT(IsNotNull(Blueprint));

		UClass* BlueprintClass = Blueprint->GeneratedClass.Get();
		ASSERT_THAT(IsNotNull(BlueprintClass, TEXT("Recovery test should expose the generated Blueprint class")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* BlueprintActor = AngelscriptFunctionalTestUtils::SpawnScriptActor(*TestRunner, Spawner, BlueprintClass);
		ASSERT_THAT(IsNotNull(BlueprintActor, TEXT("Recovery test should spawn a Blueprint child actor before reload")));
		AngelscriptFunctionalTestUtils::BeginPlayActor(Engine, *BlueprintActor);

		// Trigger an ambiguous rename reload, leaving the Blueprint orphaned.
		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::FullReload, RenameModuleName, RenameFilename, MakeRenamedScript(), ReloadResult),
			TEXT("Recovery test should compile the renamed-class update")));
		ASSERT_THAT(IsTrue(IsHandledReloadResult(ReloadResult), TEXT("Recovery rename should resolve through a handled full-reload path")));

		UClass* RenamedParentClass = FindGeneratedClass(&Engine, RenamedParentClassName);
		ASSERT_THAT(IsNotNull(RenamedParentClass, TEXT("Recovery test should find the renamed class")));
		ASSERT_THAT(IsFalse(BlueprintClass->IsChildOf(RenamedParentClass), TEXT("Blueprint should be orphaned before the manual reparent")));

		// Manual recovery: point the Blueprint at the renamed class and recompile.
		Blueprint->ParentClass = RenamedParentClass;
		FBlueprintEditorUtils::RefreshAllNodes(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);

		UClass* RecoveredBlueprintClass = Blueprint->GeneratedClass.Get();
		ASSERT_THAT(IsNotNull(RecoveredBlueprintClass, TEXT("Recovery test should recompile a generated Blueprint class")));
		ASSERT_THAT(AreEqual(RenamedParentClass, Blueprint->ParentClass.Get(), TEXT("Blueprint ParentClass should now be the renamed class")));
		ASSERT_THAT(IsTrue(RecoveredBlueprintClass->IsChildOf(RenamedParentClass), TEXT("Blueprint child should derive from the renamed class after manual reparent")));
		ASSERT_THAT(AreEqual(
			Cast<UASClass>(RenamedParentClass),
			UASClass::GetFirstASClass(RecoveredBlueprintClass),
			TEXT("Recovered Blueprint child should resolve its first AS class through the renamed parent")));
		}
	}

	TEST_METHOD(ConfiguredRenameRedirectReparentsBlueprintChildAfterReload)
	{
		using namespace AngelscriptHotReloadClassRenameTest;

		FScopedGeneratedRedirectTarget RedirectTarget;
		ASSERT_THAT(IsTrue(RedirectTarget.Initialize(*TestRunner)));

		const FScopedClassRedirectList RenameRedirect = MakeScopedClassRedirect(OriginalParentClassName, RenamedParentClassName);

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope EngineScope(Engine);
		UBlueprint* Blueprint = nullptr;
		ON_SCOPE_EXIT
		{
			CleanupTransientBlueprint(Blueprint);
			Engine.DiscardModule(*RenameModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* OriginalParentClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			*TestRunner,
			Engine,
			RenameModuleName,
			RenameFilename,
			MakeOriginalScript(),
			OriginalParentClassName);
		ASSERT_THAT(IsNotNull(OriginalParentClass));

		UASClass* OriginalParentAS = Cast<UASClass>(OriginalParentClass);
		ASSERT_THAT(IsNotNull(OriginalParentAS, TEXT("Redirect test should start from a live AS parent class")));

		Blueprint = CreateTransientBlueprintChild(*TestRunner, OriginalParentClass);
		ASSERT_THAT(IsNotNull(Blueprint));

		UClass* BlueprintClass = Blueprint->GeneratedClass.Get();
		ASSERT_THAT(IsNotNull(BlueprintClass, TEXT("Redirect test should expose the generated Blueprint class")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* BlueprintActor = AngelscriptFunctionalTestUtils::SpawnScriptActor(*TestRunner, Spawner, BlueprintClass);
		ASSERT_THAT(IsNotNull(BlueprintActor, TEXT("Redirect test should spawn a Blueprint child actor before reload")));
		AngelscriptFunctionalTestUtils::BeginPlayActor(Engine, *BlueprintActor);

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::FullReload, RenameModuleName, RenameFilename, MakeRenamedScript(), ReloadResult),
			TEXT("Redirect test should compile the renamed-class update")));
		ASSERT_THAT(IsTrue(IsHandledReloadResult(ReloadResult), TEXT("Configured rename should resolve through a handled full-reload path")));

		UClass* RenamedParentClass = FindGeneratedClass(&Engine, RenamedParentClassName);
		ASSERT_THAT(IsNotNull(RenamedParentClass, TEXT("Redirect test should find the renamed class")));

		UClass* ReparentedBlueprintClass = Blueprint->GeneratedClass.Get();
		ASSERT_THAT(IsNotNull(ReparentedBlueprintClass, TEXT("Redirect test should retain a generated Blueprint class after reload")));
		ASSERT_THAT(AreEqual(RenamedParentClass, Blueprint->ParentClass.Get(), TEXT("Configured rename redirect should update Blueprint ParentClass to the renamed class")));
		ASSERT_THAT(IsTrue(ReparentedBlueprintClass->IsChildOf(RenamedParentClass), TEXT("Configured rename redirect should make the Blueprint child derive from the renamed class")));
		ASSERT_THAT(AreEqual((UClass*)RenamedParentClass, OriginalParentAS->NewerVersion, TEXT("Configured rename redirect should link the original AS class to the renamed class")));
		ASSERT_THAT(AreEqual(
			Cast<UASClass>(RenamedParentClass),
			UASClass::GetFirstASClass(ReparentedBlueprintClass),
			TEXT("Reparented Blueprint child should resolve its first AS class through the renamed parent")));
		}
	}

	TEST_METHOD(AutomaticRenameRedirectWritesProjectConfigAndReparentsBlueprintChildAfterReload)
	{
		using namespace AngelscriptHotReloadClassRenameTest;

		FScopedGeneratedRedirectTarget RedirectTarget;
		ASSERT_THAT(IsTrue(RedirectTarget.Initialize(*TestRunner, OriginalParentClassName, RenamedParentClassName)));

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope EngineScope(Engine);
		UBlueprint* Blueprint = nullptr;
		ON_SCOPE_EXIT
		{
			CleanupTransientBlueprint(Blueprint);
			Engine.DiscardModule(*RenameModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* OriginalParentClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			*TestRunner,
			Engine,
			RenameModuleName,
			RenameFilename,
			MakeOriginalScript(),
			OriginalParentClassName);
		ASSERT_THAT(IsNotNull(OriginalParentClass));

		UASClass* OriginalParentAS = Cast<UASClass>(OriginalParentClass);
		ASSERT_THAT(IsNotNull(OriginalParentAS, TEXT("Generated redirect test should start from a live AS parent class")));

		Blueprint = CreateTransientBlueprintChild(*TestRunner, OriginalParentClass);
		ASSERT_THAT(IsNotNull(Blueprint));

		UClass* BlueprintClass = Blueprint->GeneratedClass.Get();
		ASSERT_THAT(IsNotNull(BlueprintClass, TEXT("Generated redirect test should expose the generated Blueprint class")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* BlueprintActor = AngelscriptFunctionalTestUtils::SpawnScriptActor(*TestRunner, Spawner, BlueprintClass);
		ASSERT_THAT(IsNotNull(BlueprintActor, TEXT("Generated redirect test should spawn a Blueprint child actor before reload")));
		AngelscriptFunctionalTestUtils::BeginPlayActor(Engine, *BlueprintActor);

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::FullReload, RenameModuleName, RenameFilename, MakeRenamedScript(), ReloadResult),
			TEXT("Generated redirect test should compile the renamed-class update")));
		ASSERT_THAT(IsTrue(IsHandledReloadResult(ReloadResult), TEXT("Generated rename redirect should resolve through a handled full-reload path")));
		RedirectTarget.MarkRedirectRegistered(OriginalParentClassName, RenamedParentClassName);

		UClass* RenamedParentClass = FindGeneratedClass(&Engine, RenamedParentClassName);
		ASSERT_THAT(IsNotNull(RenamedParentClass, TEXT("Generated redirect test should find the renamed class")));

		UClass* ReparentedBlueprintClass = Blueprint->GeneratedClass.Get();
		ASSERT_THAT(IsNotNull(ReparentedBlueprintClass, TEXT("Generated redirect test should retain a generated Blueprint class after reload")));
		ASSERT_THAT(AreEqual(RenamedParentClass, Blueprint->ParentClass.Get(), TEXT("Generated rename redirect should update Blueprint ParentClass to the renamed class")));
		ASSERT_THAT(IsTrue(ReparentedBlueprintClass->IsChildOf(RenamedParentClass), TEXT("Generated rename redirect should make the Blueprint child derive from the renamed class")));
		ASSERT_THAT(AreEqual((UClass*)RenamedParentClass, OriginalParentAS->NewerVersion, TEXT("Generated rename redirect should link the original AS class to the renamed class")));
		ASSERT_THAT(IsTrue(FileContainsExpectedGeneratedRedirect(*TestRunner, RedirectTarget)));

		TArray<FCoreRedirectObjectName> PreviousClassNames;
		ASSERT_THAT(IsTrue(FCoreRedirects::FindPreviousNames(
			ECoreRedirectFlags::Type_Class,
			FCoreRedirectObjectName(MakeScriptClassPath(RenamedParentClassName)),
			PreviousClassNames),
			TEXT("Generated redirect should be registered in FCoreRedirects during the same session")));

		bool bFoundOriginalPath = false;
		for (const FCoreRedirectObjectName& PreviousClassName : PreviousClassNames)
		{
			if (PreviousClassName.ToString() == MakeScriptClassPath(OriginalParentClassName))
			{
				bFoundOriginalPath = true;
				break;
			}
		}
		ASSERT_THAT(IsTrue(bFoundOriginalPath, TEXT("Generated redirect previous-name lookup should include the original AS class path")));
		}
	}

	TEST_METHOD(IniFullPathRedirectWinsOverShortNameRedirectAfterReload)
	{
		using namespace AngelscriptHotReloadClassRenameTest;

		TArray<FCoreRedirect> Redirects;
		Redirects.Add(MakeClassRedirect(MakeScriptClassPath(OriginalParentClassName), MakeScriptClassPath(RenamedParentClassName)));
		Redirects.Add(MakeClassRedirect(MissingParentClassName, RenamedParentClassName));

		FScopedIniClassRedirects RenameRedirect;
		ASSERT_THAT(IsTrue(RenameRedirect.Load(*TestRunner, MoveTemp(Redirects))));

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope EngineScope(Engine);
		UBlueprint* Blueprint = nullptr;
		ON_SCOPE_EXIT
		{
			CleanupTransientBlueprint(Blueprint);
			Engine.DiscardModule(*RenameModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* OriginalParentClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			*TestRunner,
			Engine,
			RenameModuleName,
			RenameFilename,
			MakeAmbiguousOriginalScript(),
			OriginalParentClassName);
		ASSERT_THAT(IsNotNull(OriginalParentClass));

		UASClass* OriginalParentAS = Cast<UASClass>(OriginalParentClass);
		ASSERT_THAT(IsNotNull(OriginalParentAS, TEXT("Ini redirect test should start from a live AS parent class")));

		Blueprint = CreateTransientBlueprintChild(*TestRunner, OriginalParentClass);
		ASSERT_THAT(IsNotNull(Blueprint));

		UClass* BlueprintClass = Blueprint->GeneratedClass.Get();
		ASSERT_THAT(IsNotNull(BlueprintClass, TEXT("Ini redirect test should expose the generated Blueprint class")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* BlueprintActor = AngelscriptFunctionalTestUtils::SpawnScriptActor(*TestRunner, Spawner, BlueprintClass);
		ASSERT_THAT(IsNotNull(BlueprintActor, TEXT("Ini redirect test should spawn a Blueprint child actor before reload")));
		AngelscriptFunctionalTestUtils::BeginPlayActor(Engine, *BlueprintActor);

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::FullReload, RenameModuleName, RenameFilename, MakeRenamedScript(), ReloadResult),
			TEXT("Ini redirect test should compile the renamed-class update")));
		ASSERT_THAT(IsTrue(IsHandledReloadResult(ReloadResult), TEXT("Ini full-path rename should resolve through a handled full-reload path")));

		UClass* RenamedParentClass = FindGeneratedClass(&Engine, RenamedParentClassName);
		ASSERT_THAT(IsNotNull(RenamedParentClass, TEXT("Ini redirect test should find the renamed class")));

		UClass* ReparentedBlueprintClass = Blueprint->GeneratedClass.Get();
		ASSERT_THAT(IsNotNull(ReparentedBlueprintClass, TEXT("Ini redirect test should retain a generated Blueprint class after reload")));
		ASSERT_THAT(AreEqual(RenamedParentClass, Blueprint->ParentClass.Get(), TEXT("Ini full-path redirect should update Blueprint ParentClass to the renamed class")));
		ASSERT_THAT(IsTrue(ReparentedBlueprintClass->IsChildOf(RenamedParentClass), TEXT("Ini full-path redirect should make the Blueprint child derive from the renamed class")));
		ASSERT_THAT(AreEqual((UClass*)RenamedParentClass, OriginalParentAS->NewerVersion, TEXT("Ini full-path redirect should link the original AS class to the renamed class")));
		ASSERT_THAT(AreEqual(
			Cast<UASClass>(RenamedParentClass),
			UASClass::GetFirstASClass(ReparentedBlueprintClass),
			TEXT("Ini reparented Blueprint child should resolve its first AS class through the renamed parent")));
		}
	}

	TEST_METHOD(InvalidRenameRedirectDoesNotReplaceUnrelatedRemovedClass)
	{
		using namespace AngelscriptHotReloadClassRenameTest;

		const FScopedClassRedirectList RenameRedirect = MakeScopedClassRedirect(MissingParentClassName, RenamedParentClassName);

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope EngineScope(Engine);
		UBlueprint* Blueprint = nullptr;
		ON_SCOPE_EXIT
		{
			CleanupTransientBlueprint(Blueprint);
			Engine.DiscardModule(*RenameModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* OriginalParentClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			*TestRunner,
			Engine,
			RenameModuleName,
			RenameFilename,
			MakeAmbiguousOriginalScript(),
			OriginalParentClassName);
		ASSERT_THAT(IsNotNull(OriginalParentClass));

		UASClass* OriginalParentAS = Cast<UASClass>(OriginalParentClass);
		ASSERT_THAT(IsNotNull(OriginalParentAS, TEXT("Invalid redirect test should start from a live AS parent class")));

		Blueprint = CreateTransientBlueprintChild(*TestRunner, OriginalParentClass);
		ASSERT_THAT(IsNotNull(Blueprint));

		UClass* BlueprintClass = Blueprint->GeneratedClass.Get();
		ASSERT_THAT(IsNotNull(BlueprintClass, TEXT("Invalid redirect test should expose the generated Blueprint class")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* BlueprintActor = AngelscriptFunctionalTestUtils::SpawnScriptActor(*TestRunner, Spawner, BlueprintClass);
		ASSERT_THAT(IsNotNull(BlueprintActor, TEXT("Invalid redirect test should spawn a Blueprint child actor before reload")));
		AngelscriptFunctionalTestUtils::BeginPlayActor(Engine, *BlueprintActor);

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::FullReload, RenameModuleName, RenameFilename, MakeRenamedScript(), ReloadResult),
			TEXT("Invalid redirect test should compile the renamed-class update")));
		ASSERT_THAT(IsTrue(IsHandledReloadResult(ReloadResult), TEXT("Invalid redirect should still resolve through a handled full-reload path")));

		UClass* RenamedParentClass = FindGeneratedClass(&Engine, RenamedParentClassName);
		ASSERT_THAT(IsNotNull(RenamedParentClass, TEXT("Invalid redirect test should find the renamed class")));

		UClass* ReparentedBlueprintClass = Blueprint->GeneratedClass.Get();
		ASSERT_THAT(IsNotNull(ReparentedBlueprintClass, TEXT("Invalid redirect test should retain a generated Blueprint class after reload")));
		ASSERT_THAT(IsNull(OriginalParentAS->NewerVersion, TEXT("Redirect from a missing old class must not link the unrelated removed class")));
		ASSERT_THAT(AreEqual((UClass*)OriginalParentAS, Blueprint->ParentClass.Get(), TEXT("Invalid redirect must not update Blueprint ParentClass to the renamed class")));
		ASSERT_THAT(IsFalse(ReparentedBlueprintClass->IsChildOf(RenamedParentClass), TEXT("Invalid redirect must not make the Blueprint child derive from the renamed class")));
		}
	}

	TEST_METHOD(TwoStepRenameRedirectReachesFinalClass)
	{
		using namespace AngelscriptHotReloadClassRenameTest;

		FScopedGeneratedRedirectTarget RedirectTarget;
		ASSERT_THAT(IsTrue(RedirectTarget.Initialize(*TestRunner)));

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope EngineScope(Engine);
		UBlueprint* Blueprint = nullptr;
		ON_SCOPE_EXIT
		{
			CleanupTransientBlueprint(Blueprint);
			Engine.DiscardModule(*RenameModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* OriginalParentClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			*TestRunner,
			Engine,
			RenameModuleName,
			RenameFilename,
			MakeOriginalScript(),
			OriginalParentClassName);
		ASSERT_THAT(IsNotNull(OriginalParentClass));

		UASClass* OriginalParentAS = Cast<UASClass>(OriginalParentClass);
		ASSERT_THAT(IsNotNull(OriginalParentAS, TEXT("Two-step redirect test should start from a live AS parent class")));

		Blueprint = CreateTransientBlueprintChild(*TestRunner, OriginalParentClass);
		ASSERT_THAT(IsNotNull(Blueprint));

		UClass* BlueprintClass = Blueprint->GeneratedClass.Get();
		ASSERT_THAT(IsNotNull(BlueprintClass, TEXT("Two-step redirect test should expose the generated Blueprint class")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* BlueprintActor = AngelscriptFunctionalTestUtils::SpawnScriptActor(*TestRunner, Spawner, BlueprintClass);
		ASSERT_THAT(IsNotNull(BlueprintActor, TEXT("Two-step redirect test should spawn a Blueprint child actor before reload")));
		AngelscriptFunctionalTestUtils::BeginPlayActor(Engine, *BlueprintActor);

		{
			const FScopedClassRedirectList FirstRenameRedirect = MakeScopedClassRedirect(OriginalParentClassName, RenamedParentClassName);

			ECompileResult FirstReloadResult = ECompileResult::Error;
			ASSERT_THAT(IsTrue(
				CompileModuleWithResult(&Engine, ECompileType::FullReload, RenameModuleName, RenameFilename, MakeRenamedScript(), FirstReloadResult),
				TEXT("Two-step redirect test should compile the first rename")));
			ASSERT_THAT(IsTrue(IsHandledReloadResult(FirstReloadResult), TEXT("First configured rename should resolve through a handled full-reload path")));
		}

		UClass* RenamedParentClass = FindGeneratedClass(&Engine, RenamedParentClassName);
		ASSERT_THAT(IsNotNull(RenamedParentClass, TEXT("Two-step redirect test should find the intermediate renamed class")));

		UASClass* RenamedParentAS = Cast<UASClass>(RenamedParentClass);
		ASSERT_THAT(IsNotNull(RenamedParentAS, TEXT("Intermediate parent should be a live UASClass")));

		UClass* FirstReparentedBlueprintClass = Blueprint->GeneratedClass.Get();
		ASSERT_THAT(IsNotNull(FirstReparentedBlueprintClass, TEXT("Two-step redirect test should retain a generated Blueprint class after first reload")));
		ASSERT_THAT(AreEqual(RenamedParentClass, Blueprint->ParentClass.Get(), TEXT("First redirect should update Blueprint ParentClass to the intermediate class")));
		ASSERT_THAT(IsTrue(FirstReparentedBlueprintClass->IsChildOf(RenamedParentClass), TEXT("First redirect should make the Blueprint child derive from the intermediate class")));
		ASSERT_THAT(AreEqual((UClass*)RenamedParentAS, OriginalParentAS->NewerVersion, TEXT("First redirect should link original class to the intermediate class")));

		{
			const FScopedClassRedirectList SecondRenameRedirect = MakeScopedClassRedirect(RenamedParentClassName, FinalParentClassName);

			ECompileResult SecondReloadResult = ECompileResult::Error;
			ASSERT_THAT(IsTrue(
				CompileModuleWithResult(&Engine, ECompileType::FullReload, RenameModuleName, RenameFilename, MakeFinalScript(), SecondReloadResult),
				TEXT("Two-step redirect test should compile the second rename")));
			ASSERT_THAT(IsTrue(IsHandledReloadResult(SecondReloadResult), TEXT("Second configured rename should resolve through a handled full-reload path")));
		}

		UClass* FinalParentClass = FindGeneratedClass(&Engine, FinalParentClassName);
		ASSERT_THAT(IsNotNull(FinalParentClass, TEXT("Two-step redirect test should find the final renamed class")));

		UClass* SecondReparentedBlueprintClass = Blueprint->GeneratedClass.Get();
		ASSERT_THAT(IsNotNull(SecondReparentedBlueprintClass, TEXT("Two-step redirect test should retain a generated Blueprint class after second reload")));
		ASSERT_THAT(AreEqual(FinalParentClass, Blueprint->ParentClass.Get(), TEXT("Second redirect should update Blueprint ParentClass to the final class")));
		ASSERT_THAT(IsTrue(SecondReparentedBlueprintClass->IsChildOf(FinalParentClass), TEXT("Second redirect should make the Blueprint child derive from the final class")));
		ASSERT_THAT(AreEqual(
			Cast<UASClass>(FinalParentClass),
			UASClass::GetFirstASClass(SecondReparentedBlueprintClass),
			TEXT("Two-step reparented Blueprint child should resolve its first AS class through the final parent")));
		}
	}

	TEST_METHOD(AutomaticRenameBackReparentsBlueprintChildToFreshOriginalName)
	{
		using namespace AngelscriptHotReloadClassRenameTest;

		FScopedGeneratedRedirectTarget RedirectTarget;
		ASSERT_THAT(IsTrue(RedirectTarget.Initialize(*TestRunner)));
		RedirectTarget.AddExpectedRedirect(RenamedParentClassName, OriginalParentClassName);

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope EngineScope(Engine);
		UBlueprint* Blueprint = nullptr;
		ON_SCOPE_EXIT
		{
			CleanupTransientBlueprint(Blueprint);
			Engine.DiscardModule(*RenameModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* OriginalParentClass = AngelscriptFunctionalTestUtils::CompileScriptModule(
			*TestRunner,
			Engine,
			RenameModuleName,
			RenameFilename,
			MakeOriginalScript(),
			OriginalParentClassName);
		ASSERT_THAT(IsNotNull(OriginalParentClass));

		UASClass* FirstParentAS = Cast<UASClass>(OriginalParentClass);
		ASSERT_THAT(IsNotNull(FirstParentAS, TEXT("First parent should be a UASClass")));

		Blueprint = CreateTransientBlueprintChild(*TestRunner, OriginalParentClass);
		ASSERT_THAT(IsNotNull(Blueprint));

		UClass* BlueprintClass = Blueprint->GeneratedClass.Get();
		ASSERT_THAT(IsNotNull(BlueprintClass, TEXT("Rename-back test should expose the generated Blueprint class")));

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* BlueprintActor = AngelscriptFunctionalTestUtils::SpawnScriptActor(*TestRunner, Spawner, BlueprintClass);
		ASSERT_THAT(IsNotNull(BlueprintActor, TEXT("Rename-back test should spawn a Blueprint child actor before reload")));
		AngelscriptFunctionalTestUtils::BeginPlayActor(Engine, *BlueprintActor);

		// Rename A -> B.
		ECompileResult RenameResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::FullReload, RenameModuleName, RenameFilename, MakeRenamedScript(), RenameResult),
			TEXT("Rename-back test should compile the rename to the new name")));
		ASSERT_THAT(IsTrue(IsHandledReloadResult(RenameResult), TEXT("Rename A->B should resolve through a handled full-reload path")));
		RedirectTarget.MarkRedirectRegistered(OriginalParentClassName, RenamedParentClassName);

		UClass* RenamedParentClass = FindGeneratedClass(&Engine, RenamedParentClassName);
		ASSERT_THAT(IsNotNull(RenamedParentClass, TEXT("Rename-back test should find the renamed class")));
		UASClass* RenamedParentAS = Cast<UASClass>(RenamedParentClass);
		ASSERT_THAT(IsNotNull(RenamedParentAS, TEXT("Renamed parent should be a UASClass")));
		ASSERT_THAT(AreEqual(RenamedParentClass, Blueprint->ParentClass.Get(), TEXT("Automatic A->B redirect should reparent the Blueprint to the renamed class")));

		// Rename B -> A (back to the original name).
		ECompileResult RenameBackResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::FullReload, RenameModuleName, RenameFilename, MakeOriginalScript(), RenameBackResult),
			TEXT("Rename-back test should compile the rename back to the original name")));
		ASSERT_THAT(IsTrue(IsHandledReloadResult(RenameBackResult), TEXT("Rename B->A should resolve through a handled full-reload path")));
		RedirectTarget.MarkRedirectRegistered(RenamedParentClassName, OriginalParentClassName);

		// The original name resolves again, but to a NEW class object -- not the
		// first husk. The automatic B->A redirect reparents the Blueprint to this
		// fresh class object.
		UClass* RevivedOriginalClass = FindGeneratedClass(&Engine, OriginalParentClassName);
		ASSERT_THAT(IsNotNull(RevivedOriginalClass, TEXT("Original name should resolve again after renaming back")));
		ASSERT_THAT(AreNotEqual((UClass*)FirstParentAS, RevivedOriginalClass, TEXT("Renaming back should create a fresh class object, not revive the original husk")));

		UClass* ReparentedBlueprintClass = Blueprint->GeneratedClass.Get();
		ASSERT_THAT(IsNotNull(ReparentedBlueprintClass, TEXT("Rename-back test should retain a generated Blueprint class after reload")));
		ASSERT_THAT(AreEqual(RevivedOriginalClass, Blueprint->ParentClass.Get(), TEXT("Automatic B->A redirect should reparent the Blueprint to the fresh original-name class")));
		ASSERT_THAT(IsTrue(ReparentedBlueprintClass->IsChildOf(RevivedOriginalClass), TEXT("Blueprint child should derive from the fresh original-name class after automatic rename-back")));
		ASSERT_THAT(IsTrue(FileContainsExpectedGeneratedRedirect(*TestRunner, RedirectTarget)));
		ASSERT_THAT(IsTrue(FileOmitsGeneratedRedirect(*TestRunner, RedirectTarget, OriginalParentClassName, RenamedParentClassName)));
		}
	}
};
