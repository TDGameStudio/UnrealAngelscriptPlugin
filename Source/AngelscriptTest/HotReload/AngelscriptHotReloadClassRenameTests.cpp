#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"

#include "ClassGenerator/ASClass.h"
#include "Components/ActorTestSpawner.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
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
// The hot reloader matches old<->new script classes purely by AS class name
// (FAngelscriptClassGenerator::SetupModule). A rename is therefore not seen as a
// rename at all -- it is processed as "old class removed" + "new class added"
// with no migration link between them:
//   * The original generated UClass is moved aside to `<Name>_REPLACED_<N>`,
//     loses its ScriptTypePtr, is marked CLASS_Hidden / CLASS_NotPlaceable /
//     CLASS_HideDropDown and de-rooted (CleanupRemovedClass).
//   * It does NOT get a NewerVersion back-link (only the same-name replace path
//     sets that), so GetMostUpToDateClass() still returns the dead husk.
//   * The renamed class is registered as a brand-new class.
//
// A Blueprint child therefore keeps pointing its ParentClass at the orphaned
// husk and is never auto-migrated to the renamed class -- this is the concrete
// "renaming a script class can break derived Blueprints" hazard. The tests below
// pin that behaviour down and show the supported recovery (manual reparent).
#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptHotReloadClassRenameTest
{
	static const FName RenameModuleName(TEXT("HotReloadClassRename"));
	static const FString RenameFilename(TEXT("HotReloadClassRename.as"));
	static const FName OriginalParentClassName(TEXT("AHotReloadClassRenameOriginalParent"));
	static const FName RenamedParentClassName(TEXT("AHotReloadClassRenameRenamedParent"));

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

	FCoreRedirect MakeClassRedirect(const FName OldClassName, const FName NewClassName)
	{
		return FCoreRedirect(ECoreRedirectFlags::Type_Class, OldClassName.ToString(), NewClassName.ToString());
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadClassRenameTests,
	"Angelscript.TestModule.HotReload.ClassRename",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	// Renaming the script parent class orphans the Blueprint child: the BP keeps
	// pointing at the now-hidden husk, the renamed class is a separate object, and
	// nothing migrates the BP across automatically.
	TEST_METHOD(RenameParentClassOrphansBlueprintChild)
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
			MakeOriginalScript(),
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

		// The original parent object still exists (the Blueprint pins it alive),
		// but it has been orphaned: hidden, no script type, and -- crucially -- no
		// NewerVersion link to the renamed class. This is why GetMostUpToDateClass
		// cannot forward the Blueprint onto the renamed class.
		ASSERT_THAT(IsTrue(OriginalParentAS->HasAnyClassFlags(CLASS_Hidden), TEXT("Orphaned original class should be hidden after rename")));
		ASSERT_THAT(IsTrue(OriginalParentAS->HasAnyClassFlags(CLASS_NotPlaceable), TEXT("Orphaned original class should be non-placeable after rename")));
		ASSERT_THAT(IsTrue(OriginalParentAS->GetName().Contains(TEXT("_REPLACED_")), TEXT("Orphaned original class should be renamed aside to a _REPLACED_ husk")));
		ASSERT_THAT(IsNull(OriginalParentAS->NewerVersion, TEXT("A rename does NOT establish a NewerVersion link to the renamed class")));
		ASSERT_THAT(AreEqual((UClass*)OriginalParentAS, OriginalParentAS->GetMostUpToDateClass(), TEXT("Orphaned class cannot forward to the renamed class")));

		// The Blueprint was never migrated: its ParentClass still points at the
		// dead husk, not the renamed class. This is the broken state a rename
		// leaves behind.
		ASSERT_THAT(AreEqual((UClass*)OriginalParentAS, Blueprint->ParentClass.Get(), TEXT("Blueprint ParentClass should still point at the orphaned husk -- rename does not auto-migrate it")));
		ASSERT_THAT(IsFalse(BlueprintClass->IsChildOf(RenamedParentClass), TEXT("Blueprint child should NOT automatically become a child of the renamed class")));
		}
	}

	// Supported recovery from the orphaned state: manually reparent the Blueprint
	// onto the renamed class, refresh, and recompile. After that the child is a
	// proper descendant of the renamed class again.
	TEST_METHOD(ManualReparentRecoversBlueprintChildAfterRename)
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
			MakeOriginalScript(),
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

		// Trigger the rename reload, leaving the Blueprint orphaned.
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

		const FCoreRedirect RenameRedirect = MakeClassRedirect(OriginalParentClassName, RenamedParentClassName);
		FCoreRedirects::AddRedirectList(TArrayView<const FCoreRedirect>(&RenameRedirect, 1), TEXT("Angelscript hot reload class rename test"));
		ON_SCOPE_EXIT
		{
			FCoreRedirects::RemoveRedirectList(TArrayView<const FCoreRedirect>(&RenameRedirect, 1), TEXT("Angelscript hot reload class rename test"));
		};

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

	// Renaming the class back to its original name does NOT auto-heal the existing
	// Blueprint: identity is by object, and the "original" name is now a brand-new
	// class object, while the Blueprint still references the first husk.
	TEST_METHOD(RenameBackToOriginalNameDoesNotAutoHealBlueprintChild)
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

		// Rename B -> A (back to the original name).
		ECompileResult RenameBackResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::FullReload, RenameModuleName, RenameFilename, MakeOriginalScript(), RenameBackResult),
			TEXT("Rename-back test should compile the rename back to the original name")));
		ASSERT_THAT(IsTrue(IsHandledReloadResult(RenameBackResult), TEXT("Rename B->A should resolve through a handled full-reload path")));

		// The original name resolves again, but to a NEW class object -- not the
		// husk the Blueprint still references.
		UClass* RevivedOriginalClass = FindGeneratedClass(&Engine, OriginalParentClassName);
		ASSERT_THAT(IsNotNull(RevivedOriginalClass, TEXT("Original name should resolve again after renaming back")));
		ASSERT_THAT(AreNotEqual((UClass*)FirstParentAS, RevivedOriginalClass, TEXT("Renaming back should create a fresh class object, not revive the original husk")));

		// The Blueprint still points at the first husk and is still not migrated --
		// renaming back is not a substitute for a real reparent.
		ASSERT_THAT(AreEqual((UClass*)FirstParentAS, Blueprint->ParentClass.Get(), TEXT("Blueprint should still reference the original husk after renaming back")));
		ASSERT_THAT(IsFalse(BlueprintClass->IsChildOf(RevivedOriginalClass), TEXT("Blueprint child should NOT auto-attach to the freshly revived original-name class")));
		}
	}
};

#endif
