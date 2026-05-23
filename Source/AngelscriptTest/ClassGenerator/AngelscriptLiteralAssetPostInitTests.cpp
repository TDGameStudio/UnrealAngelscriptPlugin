#include "Shared/AngelscriptFunctionalTestUtils.h"
#include "Shared/AngelscriptTestEngineHelper.h"
#include "Shared/AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;
using namespace AngelscriptFunctionalTestUtils;

namespace LiteralAssetPostInitTest
{
	static const FName ModuleName(TEXT("ASLiteralAssetPostInit"));
	static const FString ScriptFilename(TEXT("ASLiteralAssetPostInit.as"));
	static const FName GeneratedClassName(TEXT("ULiteralPostInitAsset"));
	static const FName AssetName(TEXT("ExampleAsset"));
	static const FName WasPostInitPropertyName(TEXT("bWasPostInit"));
	static const FName PostInitCallsPropertyName(TEXT("PostInitCalls"));
	static const FName InitMarkerPropertyName(TEXT("InitMarker"));
	static constexpr int32 ExpectedInitMarker = 1337;

	struct FLiteralAssetSnapshot
	{
		bool bWasPostInit = false;
		int32 PostInitCalls = INDEX_NONE;
		int32 InitMarker = INDEX_NONE;
	};

	UObject* FindLiteralAsset()
	{
		return FindObject<UObject>(FAngelscriptEngine::Get().AssetsPackage, *AssetName.ToString());
	}

	UClass* CompileLiteralAssetCarrier(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		const FString ScriptSource = TEXT(R"AS(
UCLASS()
class ULiteralPostInitAsset : UObject
{
	UPROPERTY()
	bool bWasPostInit = false;

	UPROPERTY()
	int PostInitCalls = 0;

	UPROPERTY()
	int InitMarker = 0;
}

asset ExampleAsset of ULiteralPostInitAsset
{
	bWasPostInit = true;
	PostInitCalls += 1;
	InitMarker = 1337;
}

int TouchExampleAssetAgain()
{
	ULiteralPostInitAsset ExampleAsset = GetExampleAsset();
	if (ExampleAsset == null)
		return -1;

	return ExampleAsset.InitMarker;
}
)AS");

		return CompileScriptModule(
			Test,
			Engine,
			ModuleName,
			ScriptFilename,
			ScriptSource,
			GeneratedClassName);
	}

	bool ReadLiteralAssetSnapshot(
		FAutomationTestBase& Test,
		UObject* Object,
		FLiteralAssetSnapshot& OutSnapshot)
	{
		if (!ReadPropertyValue<FBoolProperty>(Test, Object, WasPostInitPropertyName, OutSnapshot.bWasPostInit))
		{
			return false;
		}

		if (!ReadPropertyValue<FIntProperty>(Test, Object, PostInitCallsPropertyName, OutSnapshot.PostInitCalls))
		{
			return false;
		}

		if (!ReadPropertyValue<FIntProperty>(Test, Object, InitMarkerPropertyName, OutSnapshot.InitMarker))
		{
			return false;
		}

		return true;
	}

	bool ExecuteModuleInt(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const FString& Declaration,
		const TCHAR* Context,
		int32& OutResult)
	{
		const bool bExecuted = ExecuteIntFunction(&Engine, ModuleName, Declaration, OutResult);
		return Test.TestTrue(Context, bExecuted);
	}
}

namespace LiteralAssetPostInitNameCollisionTest
{
	static const FName ModuleName(TEXT("ASLiteralAssetPostInitNameCollision"));
	static const FString ScriptFilename(TEXT("ASLiteralAssetPostInitNameCollision.as"));
	static const FName GeneratedClassName(TEXT("ULiteralPostInitCollisionAsset"));
	static const FName AssetName(TEXT("CollisionExampleAsset"));
	static const FName RightCallsPropertyName(TEXT("RightCalls"));
	static const FName WrongCallsPropertyName(TEXT("WrongCalls"));

	struct FLiteralAssetCollisionSnapshot
	{
		int32 RightCalls = INDEX_NONE;
		int32 WrongCalls = INDEX_NONE;
	};

	UObject* FindLiteralAsset()
	{
		return FindObject<UObject>(FAngelscriptEngine::Get().AssetsPackage, *AssetName.ToString());
	}

	UClass* CompileLiteralAssetCarrier(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		const FString ScriptSource = TEXT(R"AS(
UCLASS()
class ULiteralPostInitCollisionAsset : UObject
{
	UPROPERTY()
	int RightCalls = 0;

	UPROPERTY()
	int WrongCalls = 0;
}

namespace Shadow
{
	UObject GetCollisionExampleAsset()
	{
		ULiteralPostInitCollisionAsset ExampleAsset = Cast<ULiteralPostInitCollisionAsset>(__CreateLiteralAsset(ULiteralPostInitCollisionAsset, "CollisionExampleAsset"));
		if (ExampleAsset != null)
		{
			ExampleAsset.WrongCalls += 1;
			__PostLiteralAssetSetup(ExampleAsset, "CollisionExampleAsset");
		}
		return ExampleAsset;
	}
}

asset CollisionExampleAsset of ULiteralPostInitCollisionAsset
{
	RightCalls += 1;
}

int TouchExampleAssetAgain()
{
	ULiteralPostInitCollisionAsset ExampleAsset = GetCollisionExampleAsset();
	return ExampleAsset == null ? 0 : 1;
}
)AS");

		return CompileScriptModule(
			Test,
			Engine,
			ModuleName,
			ScriptFilename,
			ScriptSource,
			GeneratedClassName);
	}

	bool ReadLiteralAssetSnapshot(
		FAutomationTestBase& Test,
		UObject* Object,
		FLiteralAssetCollisionSnapshot& OutSnapshot)
	{
		if (!ReadPropertyValue<FIntProperty>(Test, Object, RightCallsPropertyName, OutSnapshot.RightCalls))
		{
			return false;
		}

		if (!ReadPropertyValue<FIntProperty>(Test, Object, WrongCallsPropertyName, OutSnapshot.WrongCalls))
		{
			return false;
		}

		return true;
	}

	bool ExecuteModuleInt(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const FString& Declaration,
		const TCHAR* Context,
		int32& OutResult)
	{
		const bool bExecuted = ExecuteIntFunction(&Engine, ModuleName, Declaration, OutResult);
		return Test.TestTrue(Context, bExecuted);
	}
}

namespace LiteralAssetMultipleCoexistTest
{
	static const FName ModuleName(TEXT("ASLiteralAssetMultipleCoexist"));
	static const FString ScriptFilename(TEXT("ASLiteralAssetMultipleCoexist.as"));
}

namespace LiteralAssetWithComponentTest
{
	static const FName ModuleName(TEXT("ASLiteralAssetWithComponent"));
	static const FString ScriptFilename(TEXT("ASLiteralAssetWithComponent.as"));
}

TEST_CLASS_WITH_FLAGS(FAngelscriptLiteralAssetPostInitTests,
	"Angelscript.TestModule.ClassGenerator.LiteralAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(PostInitMaterializesAssetOnce)
	{
		using namespace LiteralAssetPostInitTest;
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		TestRunner->AddExpectedError(TEXT("LogUObjectBase: Class pointer is invalid or CDO is invalid."), EAutomationExpectedErrorFlags::Contains, 1);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*LiteralAssetPostInitTest::ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* GeneratedClass = LiteralAssetPostInitTest::CompileLiteralAssetCarrier(*TestRunner, Engine);
		if (!TestRunner->TestNotNull(
				TEXT("Literal-asset post-init test case should compile the generated asset carrier class"),
				GeneratedClass))
		{
			return;
		}

		UObject* LiteralAssetBeforeTouch = LiteralAssetPostInitTest::FindLiteralAsset();
		if (!TestRunner->TestNotNull(
				TEXT("Literal-asset post-init test case should materialize the asset object before any explicit getter call"),
				LiteralAssetBeforeTouch))
		{
			return;
		}

		LiteralAssetPostInitTest::FLiteralAssetSnapshot SnapshotBeforeTouch;
		if (!LiteralAssetPostInitTest::ReadLiteralAssetSnapshot(*TestRunner, LiteralAssetBeforeTouch, SnapshotBeforeTouch))
		{
			return;
		}

		if (!TestRunner->TestEqual(
				TEXT("Literal-asset post-init test case should keep the generated literal asset on the expected script class"),
				LiteralAssetBeforeTouch->GetClass(),
				GeneratedClass)
			|| !TestRunner->TestEqual(
				TEXT("Literal-asset post-init test case should execute __Init_ExampleAsset exactly once during compile teardown"),
				SnapshotBeforeTouch.PostInitCalls,
				1)
			|| !TestRunner->TestTrue(
				TEXT("Literal-asset post-init test case should preserve the bool flag written by __Init_ExampleAsset"),
				SnapshotBeforeTouch.bWasPostInit)
			|| !TestRunner->TestEqual(
				TEXT("Literal-asset post-init test case should preserve the init marker written by __Init_ExampleAsset"),
				SnapshotBeforeTouch.InitMarker,
				LiteralAssetPostInitTest::ExpectedInitMarker))
		{
			return;
		}

		int32 TouchResult = INDEX_NONE;
		if (!LiteralAssetPostInitTest::ExecuteModuleInt(
				*TestRunner,
				Engine,
				TEXT("int TouchExampleAssetAgain()"),
				TEXT("Literal-asset post-init test should execute TouchExampleAssetAgain()"),
				TouchResult))
		{
			return;
		}

		UObject* LiteralAssetAfterTouch = LiteralAssetPostInitTest::FindLiteralAsset();
		if (!TestRunner->TestNotNull(
				TEXT("Literal-asset post-init test case should still expose the canonical asset after repeated getter access"),
				LiteralAssetAfterTouch))
		{
			return;
		}

		LiteralAssetPostInitTest::FLiteralAssetSnapshot SnapshotAfterTouch;
		if (!LiteralAssetPostInitTest::ReadLiteralAssetSnapshot(*TestRunner, LiteralAssetAfterTouch, SnapshotAfterTouch))
		{
			return;
		}

		TestRunner->TestEqual(
			TEXT("Literal-asset post-init test should return the initialized marker when the generated getter is touched again"),
			TouchResult,
			LiteralAssetPostInitTest::ExpectedInitMarker);
		TestRunner->TestTrue(
			TEXT("Literal-asset post-init test should keep returning the same materialized asset on repeated getter access"),
			LiteralAssetAfterTouch == LiteralAssetBeforeTouch);
		TestRunner->TestEqual(
			TEXT("Literal-asset post-init test should not rerun __Init_ExampleAsset when the generated getter is touched again"),
			SnapshotAfterTouch.PostInitCalls,
			1);
		TestRunner->TestTrue(
			TEXT("Literal-asset post-init test should preserve the bool flag after repeated getter access"),
			SnapshotAfterTouch.bWasPostInit);
		TestRunner->TestEqual(
			TEXT("Literal-asset post-init test should preserve the init marker after repeated getter access"),
			SnapshotAfterTouch.InitMarker,
			LiteralAssetPostInitTest::ExpectedInitMarker);

		}
	}

	// PostInitResolvesGeneratedGetterInsteadOfNameCollision:
	// Removed in 2026-05-22 alongside the autoaccessor refactor. The test relied on
	// the AS-side autoaccessor `obj.X ↔ GetX()` rewriting to prefer an asset-framework
	// generated `GetCollisionExampleAsset()` over a same-short-name namespaced user
	// function. With AS_PROPERTY_ACCESSOR_MODE forced to 0 (see openspec change
	// archive/2026-05-22-refactor-as-remove-autoaccessor), there is no longer any
	// auto-promotion, so the resolution behaviour the test was guarding no longer
	// exists in this fork. Asset getter wiring continues to be exercised by
	// PostInitMaterializesAssetOnce above.

	TEST_METHOD(MultipleAssetsInSameClassCoexist)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		TestRunner->AddExpectedError(TEXT("LogUObjectBase: Class pointer is invalid or CDO is invalid."), EAutomationExpectedErrorFlags::Contains, 1);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*LiteralAssetMultipleCoexistTest::ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			LiteralAssetMultipleCoexistTest::ModuleName,
			LiteralAssetMultipleCoexistTest::ScriptFilename,
			TEXT(R"AS(
UCLASS()
class UMultiAssetOwner : UObject
{
	UPROPERTY()
	int Marker = 0;
}

asset FirstAsset of UMultiAssetOwner
{
	Marker = 10;
}

asset SecondAsset of UMultiAssetOwner
{
	Marker = 20;
}
)AS"),
			CompileResult);

		if (!TestRunner->TestTrue(TEXT("Multiple assets in same class should compile"), bCompiled))
			return;

		UClass* GeneratedClass = FindGeneratedClass(&Engine, TEXT("UMultiAssetOwner"));
		if (!TestRunner->TestNotNull(TEXT("Class should be materialized"), GeneratedClass))
			return;

		UObject* FirstAsset = FindObject<UObject>(Engine.AssetsPackage, TEXT("FirstAsset"));
		UObject* SecondAsset = FindObject<UObject>(Engine.AssetsPackage, TEXT("SecondAsset"));
		TestRunner->TestNotNull(TEXT("FirstAsset should be materialized"), FirstAsset);
		TestRunner->TestNotNull(TEXT("SecondAsset should be materialized"), SecondAsset);
		if (FirstAsset && SecondAsset)
		{
			TestRunner->TestTrue(TEXT("Assets should be independent objects"), FirstAsset != SecondAsset);
		}

		}
	}

	TEST_METHOD(AssetWithDefaultComponentCoexist)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);
		TestRunner->AddExpectedError(TEXT("LogUObjectBase: Class pointer is invalid or CDO is invalid."), EAutomationExpectedErrorFlags::Contains, 1);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*LiteralAssetWithComponentTest::ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			LiteralAssetWithComponentTest::ModuleName,
			LiteralAssetWithComponentTest::ScriptFilename,
			TEXT(R"AS(
UCLASS()
class UAssetCarrier : UObject
{
	UPROPERTY()
	int CoexistMarker = 0;
}

UCLASS()
class AAssetAndComponentActor : AActor
{
	UPROPERTY(DefaultComponent, RootComponent)
	USceneComponent RootScene;
}

asset MyCoexistAsset of UAssetCarrier
{
	CoexistMarker = 99;
}
)AS"),
			CompileResult);

		if (!TestRunner->TestTrue(TEXT("Asset + DefaultComponent coexistence should compile"), bCompiled))
			return;

		UClass* ActorClass = FindGeneratedClass(&Engine, TEXT("AAssetAndComponentActor"));
		TestRunner->TestNotNull(TEXT("Actor class should be materialized"), ActorClass);

		UClass* CarrierClass = FindGeneratedClass(&Engine, TEXT("UAssetCarrier"));
		TestRunner->TestNotNull(TEXT("Carrier class should be materialized"), CarrierClass);

		UObject* AssetObj = FindObject<UObject>(Engine.AssetsPackage, TEXT("MyCoexistAsset"));
		TestRunner->TestNotNull(TEXT("Asset should coexist with component-bearing actor"), AssetObj);

		}
	}
};

#endif
