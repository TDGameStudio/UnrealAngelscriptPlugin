#include "CQTest.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestMacros.h"

#include "ClassGenerator/AngelscriptClassGenerator.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

// Test Layer: Runtime Integration
#if WITH_ANGELSCRIPT_UNITTESTS

using namespace AngelscriptFunctionalTestUtils;

TEST_CLASS_WITH_FLAGS(FAngelscriptHotReloadLiteralAssetTests,
	"Angelscript.TestModule.HotReload.LiteralAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	inline static const FName LiteralAssetReloadModuleName = FName(TEXT("HotReloadLiteralAssetMod"));
	inline static const FString LiteralAssetReloadFilename = FString(TEXT("HotReloadLiteralAssetMod.as"));
	inline static const FName LiteralAssetClassName = FName(TEXT("ULiteralReloadAsset"));
	inline static const FName LiteralAssetObjectName = FName(TEXT("ReloadExampleAsset"));

	struct FLiteralAssetReloadObservation
	{
		int32 ReloadCount = 0;
		UObject* OldAsset = nullptr;
		UObject* NewAsset = nullptr;
		UClass* OldAssetClass = nullptr;
		UClass* NewAssetClass = nullptr;
		FString OldAssetName;
		UObject* OldAssetOuter = nullptr;
	};

	static bool IsHandledReloadResult(const ECompileResult ReloadResult)
	{
		return ReloadResult == ECompileResult::FullyHandled || ReloadResult == ECompileResult::PartiallyHandled;
	}

	static UObject* FindLiteralAsset()
	{
		return FindObject<UObject>(FAngelscriptEngine::Get().AssetsPackage, *LiteralAssetObjectName.ToString());
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

	TEST_METHOD(BroadcastsReloadedObjectReplacement)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		TestRunner->AddExpectedError(TEXT("LogUObjectBase: Class pointer is invalid or CDO is invalid."), EAutomationExpectedErrorFlags::Contains, 2);

		FLiteralAssetReloadObservation Observation;
		FDelegateHandle LiteralAssetReloadHandle;

		ON_SCOPE_EXIT
		{
			Engine.GetOnLiteralAssetReload().Remove(LiteralAssetReloadHandle);
			Engine.DiscardModule(*LiteralAssetReloadModuleName.ToString());
		};

		const FString LiteralAssetReloadV1Source = ASTEST_AS(R"AS(
			UCLASS()
			class ULiteralReloadAsset : UObject
			{
			}

			asset ReloadExampleAsset of ULiteralReloadAsset
			{
			}
			)AS");

		ASSERT_THAT(IsTrue(
			CompileAnnotatedModuleFromMemory(&Engine, LiteralAssetReloadModuleName, LiteralAssetReloadFilename, LiteralAssetReloadV1Source),
			TEXT("Initial literal-asset module compile should succeed")));

		UClass* OldAssetClass = FindGeneratedClass(&Engine, LiteralAssetClassName);
		ASSERT_THAT(IsNotNull(OldAssetClass, TEXT("Literal-asset class should exist before full reload")));

		UObject* AssetBeforeReload = FindLiteralAsset();
		ASSERT_THAT(IsNotNull(AssetBeforeReload, TEXT("Literal asset should be created in the assets package after the initial compile")));

		ASSERT_THAT(AreEqual(OldAssetClass, AssetBeforeReload->GetClass(), TEXT("Initial literal asset should use the initial generated class")));
		ASSERT_THAT(AreEqual(LiteralAssetObjectName, AssetBeforeReload->GetFName(), TEXT("Initial literal asset should keep the canonical asset name")));
		ASSERT_THAT(IsNull(FindFProperty<FIntProperty>(OldAssetClass, TEXT("ExtraValue")), TEXT("Initial literal-asset class should not expose the future ExtraValue property")));

		LiteralAssetReloadHandle = Engine.GetOnLiteralAssetReload().AddLambda(
			[&Observation](UObject* OldObject, UObject* NewObject)
			{
				++Observation.ReloadCount;
				Observation.OldAsset = OldObject;
				Observation.NewAsset = NewObject;
				Observation.OldAssetClass = OldObject != nullptr ? OldObject->GetClass() : nullptr;
				Observation.NewAssetClass = NewObject != nullptr ? NewObject->GetClass() : nullptr;
				Observation.OldAssetName = OldObject != nullptr ? OldObject->GetName() : FString();
				Observation.OldAssetOuter = OldObject != nullptr ? OldObject->GetOuter() : nullptr;
			});

		const FString LiteralAssetReloadV2Source = ASTEST_AS(R"AS(
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

		ECompileResult ReloadResult = ECompileResult::Error;
		ASSERT_THAT(IsTrue(
			CompileModuleWithResult(&Engine, ECompileType::FullReload, LiteralAssetReloadModuleName, LiteralAssetReloadFilename, LiteralAssetReloadV2Source, ReloadResult),
			TEXT("Literal-asset full reload compile should succeed")));

		ASSERT_THAT(IsTrue(
			IsHandledReloadResult(ReloadResult),
			TEXT("Literal-asset structural reload should stay on a handled full-reload path")));

		UClass* NewAssetClass = FindGeneratedClass(&Engine, LiteralAssetClassName);
		ASSERT_THAT(IsNotNull(NewAssetClass, TEXT("Literal-asset class should still be queryable after full reload")));

		UObject* AssetAfterReload = FindLiteralAsset();
		ASSERT_THAT(IsNotNull(AssetAfterReload, TEXT("Canonical literal asset should still be queryable after full reload")));

		FIntProperty* ExtraValueProperty = FindFProperty<FIntProperty>(NewAssetClass, TEXT("ExtraValue"));
		ASSERT_THAT(IsNotNull(ExtraValueProperty, TEXT("Reloaded literal-asset class should expose the new ExtraValue property")));

		ASSERT_THAT(AreNotEqual(OldAssetClass, NewAssetClass, TEXT("Full reload should replace the generated asset class object")));
		ASSERT_THAT(IsTrue(OldAssetClass->HasAnyClassFlags(CLASS_NewerVersionExists), TEXT("Old literal-asset class should be marked as having a newer version")));
		ASSERT_THAT(IsFalse(NewAssetClass->HasAnyClassFlags(CLASS_NewerVersionExists), TEXT("Reloaded literal-asset class should remain the live canonical class")));
		ASSERT_THAT(AreEqual(1, Observation.ReloadCount, TEXT("Literal-asset full reload should broadcast exactly once")));
		ASSERT_THAT(AreEqual(AssetBeforeReload, Observation.OldAsset, TEXT("Literal-asset reload callback should expose the old asset object")));
		ASSERT_THAT(AreEqual(AssetAfterReload, Observation.NewAsset, TEXT("Literal-asset reload callback should expose the new canonical asset object")));
		ASSERT_THAT(AreNotEqual(Observation.OldAsset, Observation.NewAsset, TEXT("Literal-asset reload callback should expose distinct old/new asset objects")));
		ASSERT_THAT(AreEqual(OldAssetClass, Observation.OldAssetClass, TEXT("Literal-asset reload callback should capture the old asset class before replacement")));
		ASSERT_THAT(AreEqual(NewAssetClass, Observation.NewAssetClass, TEXT("Literal-asset reload callback should capture the new asset class before replacement")));
		ASSERT_THAT(AreEqual(NewAssetClass, AssetAfterReload->GetClass(), TEXT("New canonical asset should use the reloaded generated class")));
		ASSERT_THAT(IsTrue(!Observation.OldAssetName.IsEmpty() && Observation.OldAssetName != LiteralAssetObjectName.ToString(), TEXT("Old literal asset should lose the canonical asset name after replacement")));
		ASSERT_THAT(IsTrue(Observation.OldAssetName.StartsWith(TEXT("REPLACED_ASSET_ReloadExampleAsset_")), TEXT("Old literal asset should be renamed to the REPLACED_ASSET_* pattern")));
		ASSERT_THAT(IsTrue(Observation.OldAssetOuter != nullptr && Observation.OldAssetOuter != FAngelscriptEngine::Get().AssetsPackage, TEXT("Old literal asset should move out of the canonical assets package")));
		ASSERT_THAT(AreEqual(LiteralAssetObjectName, AssetAfterReload->GetFName(), TEXT("Reloaded literal asset should keep the canonical asset name")));
		ASSERT_THAT(IsNull(FindFProperty<FIntProperty>(OldAssetClass, TEXT("ExtraValue")), TEXT("Old generated asset class should keep its pre-reload reflected layout")));

		int32 NewAssetExtraValue = 0;
		ASSERT_THAT(IsTrue(ReadPropertyValue<FIntProperty>(*TestRunner, AssetAfterReload, TEXT("ExtraValue"), NewAssetExtraValue)));
		ASSERT_THAT(AreEqual(2, NewAssetExtraValue, TEXT("Reloaded literal asset should expose the new ExtraValue default")));
	}
};

#endif
