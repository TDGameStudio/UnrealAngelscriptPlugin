// ============================================================================
// AngelscriptAssetRegistryBindingsTests.cpp
//
// AssetRegistry binding coverage �?CQTest refactor. Automation IDs:
//   Angelscript.TestModule.Bindings.AssetRegistry.FAngelscriptAssetRegistryBindingsTest.*
//
// Sections:
//   TopLevelPathAndNullParent �?FTopLevelAssetPath round-trip + null parent exception
//
// CQTest adaptation notes:
//   Two legacy automation tests merged into one TEST_CLASS.
//   TopLevelPathAndNullParent uses WorldCollisionExecuteFunctionExpectingException for negative path.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"
#include "Bindings/AngelscriptWorldCollisionBindingsTestHelpers.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "Templates/Function.h"

#if WITH_ANGELSCRIPT_UNITTESTS


// ----------------------------------------------------------------------------
// Helper utilities (retained from original)
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptAssetRegistryBindingsTest,
	"Angelscript.TestModule.Bindings.AssetRegistry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static IAssetRegistry& GetAssetRegistryChecked()
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		return AssetRegistryModule.Get();
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

	// ====================================================================
	// Section: TopLevelPathAndNullParent
	// ====================================================================

	TEST_METHOD(TopLevelPathAndNullParent)
	{
		TestRunner->AddExpectedError(TEXT("A null Class was passed to GetBlueprintCDOsByParentClass."), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("ASAssetRegistry_TopLevelPathAndNullParent"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("void TriggerNullParent(UObject[]&)"), EAutomationExpectedErrorFlags::Contains, 0, false);

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			int Entry()
			{
				FTopLevelAssetPath PathFromClass(AActor::StaticClass());
				if (!PathFromClass.IsValid())
				{
					return 10;
				}
				if (PathFromClass.IsNull())
				{
					return 20;
				}

				FString PathString = PathFromClass.ToString();
				if (PathString.IsEmpty())
				{
					return 30;
				}

				FTopLevelAssetPath PathFromString(PathString);
				if (!PathFromString.IsValid())
				{
					return 40;
				}
				if (!(PathFromString == PathFromClass))
				{
					return 50;
				}

				FTopLevelAssetPath AssignedPath;
				AssignedPath = PathString;
				if (!(AssignedPath == PathFromClass))
				{
					return 60;
				}

				TArray<FAssetData> Assets;
				if (!AssetRegistry::GetAssetsByClass(FTopLevelAssetPath(UBlueprint::StaticClass()), Assets))
				{
					return 70;
				}

				for (int Index = 0; Index < Assets.Num(); ++Index)
				{
					if (Assets[Index].GetObjectPathString().IsEmpty())
					{
						return 80;
					}
					if (Assets[Index].GetSoftObjectPath().ToString().IsEmpty())
					{
						return 90;
					}
				}

				return Assets.Num() + 1;
			}

			void TriggerNullParent(TArray<UObject>& OutAssets)
			{
				UClass NullClass;
				AssetRegistry::GetBlueprintCDOsByParentClass(NullClass, OutAssets);
			}
			)AS");

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASAssetRegistry_TopLevelPathAndNullParent"), ScriptSource);
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		// Compute native baseline
		TArray<FAssetData> NativeAssets;
		const bool bNativeQuerySucceeded = GetAssetRegistryChecked().GetAssetsByClass(FTopLevelAssetPath(UBlueprint::StaticClass()), NativeAssets);
		ASSERT_THAT(IsTrue(bNativeQuerySucceeded, TEXT("Native AssetRegistry GetAssetsByClass(UBlueprint) baseline should succeed")));

		// Validate Entry result matches native count
		FASGlobalFunctionInvoker EntryInvoker(*TestRunner, Engine, M, TEXT("int Entry()"));
		if (EntryInvoker.IsValid())
		{
			const int32 ScriptResult = EntryInvoker.CallAndReturn<int32>(INDEX_NONE);
			ASSERT_THAT(AreEqual(
				NativeAssets.Num() + 1,
				ScriptResult,
				TEXT("FTopLevelAssetPath round-trip and AssetRegistry::GetAssetsByClass should preserve native UBlueprint asset count")));
		}

		// Negative path: null parent exception
		TArray<UObject*> NativeBlueprintCDOs;
		if (WorldCollisionExecuteFunctionExpectingException(
			*TestRunner,
			Engine,
			M,
			TEXT("void TriggerNullParent(TArray<UObject>& OutAssets)"),
			[this, &NativeBlueprintCDOs](asIScriptContext& Context)
			{
				return WorldCollisionSetArgAddressChecked(*TestRunner, Context, 0, &NativeBlueprintCDOs, TEXT("TriggerNullParent"));
			},
			TEXT("TriggerNullParent"),
			TEXT("A null Class was passed to GetBlueprintCDOsByParentClass.")))
		{
			ASSERT_THAT(AreEqual(
				0,
				NativeBlueprintCDOs.Num(),
				TEXT("GetBlueprintCDOsByParentClass(null) should leave output array empty")));
		}
	}

};

#endif
