#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "ClassGenerator/AngelscriptClassGenerator.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;


#define TestTrue(...) Test.TestTrue(__VA_ARGS__)
#define TestFalse(...) Test.TestFalse(__VA_ARGS__)
#define TestEqual(...) Test.TestEqual(__VA_ARGS__)
#define TestNotEqual(...) Test.TestNotEqual(__VA_ARGS__)
#define TestNotNull(...) Test.TestNotNull(__VA_ARGS__)
#define TestNull(...) Test.TestNull(__VA_ARGS__)
#define AddExpectedError(...) Test.AddExpectedError(__VA_ARGS__)

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadLiteralAssetTests,
	"Angelscript.TestModule.HotReload.LiteralAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
inline static const FName LiteralAssetReloadModuleName = FName(TEXT("HotReloadLiteralAssetMod"));
inline static const FString LiteralAssetReloadFilename = FString(TEXT("HotReloadLiteralAssetMod.as"));
inline static const FName LiteralAssetClassName = FName(TEXT("ULiteralReloadAsset"));
inline static const FName LiteralAssetObjectName = FName(TEXT("ReloadExampleAsset"));

static bool IsHandledReloadResult(const ECompileResult ReloadResult)
{
	return ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled;
}

static UObject* FindLiteralAsset()
{
	return FindObject<UObject>(FAngelscriptEngine::Get().AssetsPackage, *LiteralAssetObjectName.ToString());
}

static bool LiteralAssetBroadcastsReloadedObjectReplacement(FAutomationTestBase& Test)
{
FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	AddExpectedError(TEXT("LogUObjectBase: Class pointer is invalid or CDO is invalid."), EAutomationExpectedErrorFlags::Contains, 2);

	int32 LiteralAssetReloadCount = 0;
	UObject* OldAssetSeenDuringReload = nullptr;
	UObject* NewAssetSeenDuringReload = nullptr;
	UClass* OldAssetClassSeenDuringReload = nullptr;
	UClass* NewAssetClassSeenDuringReload = nullptr;
	FString OldAssetNameSeenDuringReload;
	UObject* OldAssetOuterSeenDuringReload = nullptr;
	FDelegateHandle LiteralAssetReloadHandle;

	ON_SCOPE_EXIT
	{
		Engine.GetOnLiteralAssetReload().Remove(LiteralAssetReloadHandle);
		Engine.DiscardModule(*LiteralAssetReloadModuleName.ToString());
		ASTEST_RESET_ENGINE(Engine);
	};

	const FString ScriptV1 = TEXT(R"AS(
UCLASS()
class ULiteralReloadAsset : UObject
{
}

asset ReloadExampleAsset of ULiteralReloadAsset
{
}
)AS");

	const FString ScriptV2 = TEXT(R"AS(
UCLASS()
class ULiteralReloadAsset : UObject
{
	UPROPERTY()
	int ExtraValue = 2;
}

asset ReloadExampleAsset of ULiteralReloadAsset
{
}
)AS");

	if (!TestTrue(
		TEXT("Initial literal-asset module compile should succeed"),
		CompileAnnotatedModuleFromMemory(&Engine, LiteralAssetReloadModuleName, LiteralAssetReloadFilename, ScriptV1)))
	{
		return false;
	}

	UClass* OldAssetClass = FindGeneratedClass(&Engine, LiteralAssetClassName);
	if (!TestNotNull(TEXT("Literal-asset class should exist before full reload"), OldAssetClass))
	{
		return false;
	}

	UObject* AssetBeforeReload = FindLiteralAsset();
	if (!TestNotNull(TEXT("Literal asset should be created in the assets package after the initial compile"), AssetBeforeReload))
	{
		return false;
	}

	TestEqual(TEXT("Initial literal asset should use the initial generated class"), AssetBeforeReload->GetClass(), OldAssetClass);
	TestTrue(TEXT("Initial literal asset should keep the canonical asset name"), AssetBeforeReload->GetFName() == LiteralAssetObjectName);
	TestNull(TEXT("Initial literal-asset class should not expose the future ExtraValue property"), FindFProperty<FIntProperty>(OldAssetClass, TEXT("ExtraValue")));

	LiteralAssetReloadHandle = Engine.GetOnLiteralAssetReload().AddLambda(
		[&](UObject* OldObject, UObject* NewObject)
		{
			++LiteralAssetReloadCount;
			OldAssetSeenDuringReload = OldObject;
			NewAssetSeenDuringReload = NewObject;
			OldAssetClassSeenDuringReload = OldObject != nullptr ? OldObject->GetClass() : nullptr;
			NewAssetClassSeenDuringReload = NewObject != nullptr ? NewObject->GetClass() : nullptr;
			OldAssetNameSeenDuringReload = OldObject != nullptr ? OldObject->GetName() : FString();
			OldAssetOuterSeenDuringReload = OldObject != nullptr ? OldObject->GetOuter() : nullptr;
		});

	ECompileResult ReloadResult = ECompileResult::Error;
	if (!TestTrue(
		TEXT("Literal-asset full reload compile should succeed"),
		CompileModuleWithResult(&Engine, ECompileType::FullReload, LiteralAssetReloadModuleName, LiteralAssetReloadFilename, ScriptV2, ReloadResult)))
	{
		return false;
	}

	if (!TestTrue(
		TEXT("Literal-asset structural reload should stay on a handled full-reload path"),
		IsHandledReloadResult(ReloadResult)))
	{
		return false;
	}

	UClass* NewAssetClass = FindGeneratedClass(&Engine, LiteralAssetClassName);
	if (!TestNotNull(TEXT("Literal-asset class should still be queryable after full reload"), NewAssetClass))
	{
		return false;
	}

	UObject* AssetAfterReload = FindLiteralAsset();
	if (!TestNotNull(TEXT("Canonical literal asset should still be queryable after full reload"), AssetAfterReload))
	{
		return false;
	}

	FIntProperty* ExtraValueProperty = FindFProperty<FIntProperty>(NewAssetClass, TEXT("ExtraValue"));
	if (!TestNotNull(TEXT("Reloaded literal-asset class should expose the new ExtraValue property"), ExtraValueProperty))
	{
		return false;
	}

	TestTrue(TEXT("Full reload should replace the generated asset class object"), NewAssetClass != OldAssetClass);
	TestTrue(TEXT("Old literal-asset class should be marked as having a newer version"), OldAssetClass->HasAnyClassFlags(CLASS_NewerVersionExists));
	TestFalse(TEXT("Reloaded literal-asset class should remain the live canonical class"), NewAssetClass->HasAnyClassFlags(CLASS_NewerVersionExists));
	TestEqual(TEXT("Literal-asset full reload should broadcast exactly once"), LiteralAssetReloadCount, 1);
	TestEqual(TEXT("Literal-asset reload callback should expose the old asset object"), OldAssetSeenDuringReload, AssetBeforeReload);
	TestEqual(TEXT("Literal-asset reload callback should expose the new canonical asset object"), NewAssetSeenDuringReload, AssetAfterReload);
	TestNotEqual(TEXT("Literal-asset reload callback should expose distinct old/new asset objects"), OldAssetSeenDuringReload, NewAssetSeenDuringReload);
	TestEqual(TEXT("Literal-asset reload callback should capture the old asset class before replacement"), OldAssetClassSeenDuringReload, OldAssetClass);
	TestEqual(TEXT("Literal-asset reload callback should capture the new asset class before replacement"), NewAssetClassSeenDuringReload, NewAssetClass);
	TestEqual(TEXT("New canonical asset should use the reloaded generated class"), AssetAfterReload->GetClass(), NewAssetClass);
	TestTrue(TEXT("Old literal asset should lose the canonical asset name after replacement"), !OldAssetNameSeenDuringReload.IsEmpty() && OldAssetNameSeenDuringReload != LiteralAssetObjectName.ToString());
	TestTrue(TEXT("Old literal asset should be renamed to the REPLACED_ASSET_* pattern"), OldAssetNameSeenDuringReload.StartsWith(TEXT("REPLACED_ASSET_ReloadExampleAsset_")));
	TestTrue(TEXT("Old literal asset should move out of the canonical assets package"), OldAssetOuterSeenDuringReload != nullptr && OldAssetOuterSeenDuringReload != FAngelscriptEngine::Get().AssetsPackage);
	TestTrue(TEXT("Reloaded literal asset should keep the canonical asset name"), AssetAfterReload->GetFName() == LiteralAssetObjectName);
	TestNull(TEXT("Old generated asset class should keep its pre-reload reflected layout"), FindFProperty<FIntProperty>(OldAssetClass, TEXT("ExtraValue")));

	int32 NewAssetExtraValue = 0;
	if (!ReadPropertyValue<FIntProperty>(Test, AssetAfterReload, TEXT("ExtraValue"), NewAssetExtraValue))
	{
		return false;
	}

	TestEqual(TEXT("Reloaded literal asset should expose the new ExtraValue default"), NewAssetExtraValue, 2);

	}
	return true;
}

public:
	TEST_METHOD(BroadcastsReloadedObjectReplacement)
	{
		ASSERT_THAT(IsTrue(LiteralAssetBroadcastsReloadedObjectReplacement(*TestRunner)));
	}
};

#undef TestTrue
#undef TestFalse
#undef TestEqual
#undef TestNotEqual
#undef TestNotNull
#undef TestNull
#undef AddExpectedError

#endif
