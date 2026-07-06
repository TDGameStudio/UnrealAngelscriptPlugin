#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

// Test Layer: Runtime Integration
#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptLiteralAssetPostInitTests,
	"Angelscript.TestModule.Generator.LiteralAsset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FPostInitCase
	{
	inline static const FName ModuleName = FName(TEXT("ASLiteralAssetPostInit"));
	inline static const FString ScriptFilename = FString(TEXT("ASLiteralAssetPostInit.as"));
	inline static const FName GeneratedClassName = FName(TEXT("ULiteralPostInitAsset"));
	inline static const FName AssetName = FName(TEXT("ExampleAsset"));
	inline static const FName WasPostInitPropertyName = FName(TEXT("bWasPostInit"));
	inline static const FName PostInitCallsPropertyName = FName(TEXT("PostInitCalls"));
	inline static const FName InitMarkerPropertyName = FName(TEXT("InitMarker"));
	static constexpr int32 ExpectedInitMarker = 1337;

	struct FLiteralAssetSnapshot
	{
		bool bWasPostInit = false;
		int32 PostInitCalls = INDEX_NONE;
		int32 InitMarker = INDEX_NONE;
	};

	static UObject* FindLiteralAsset()
	{
		return FindObject<UObject>(FAngelscriptEngine::Get().AssetsPackage, *AssetName.ToString());
	}

	static UClass* CompileLiteralAssetCarrier(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		const FString ScriptSource = ASTEST_AS(R"AS(
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

		return AngelscriptFunctionalTestUtils::CompileScriptModule(
			Test,
			Engine,
			ModuleName,
			ScriptFilename,
			ScriptSource,
			GeneratedClassName);
	}

	static bool ReadLiteralAssetSnapshot(
		FAutomationTestBase& Test,
		UObject* Object,
		FLiteralAssetSnapshot& OutSnapshot)
	{
		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FBoolProperty>(Test, Object, WasPostInitPropertyName, OutSnapshot.bWasPostInit))
		{
			return false;
		}

		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(Test, Object, PostInitCallsPropertyName, OutSnapshot.PostInitCalls))
		{
			return false;
		}

		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(Test, Object, InitMarkerPropertyName, OutSnapshot.InitMarker))
		{
			return false;
		}

		return true;
	}

	static bool ExecuteModuleInt(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const FString& Declaration,
		const TCHAR* Context,
		int32& OutResult)
	{
		const bool bExecuted = ::ExecuteIntFunction(&Engine, ModuleName, Declaration, OutResult);
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.IsTrue(bExecuted, Context);
	}
	};

	struct FNameCollisionCase
	{
	inline static const FName ModuleName = FName(TEXT("ASLiteralAssetPostInitNameCollision"));
	inline static const FString ScriptFilename = FString(TEXT("ASLiteralAssetPostInitNameCollision.as"));
	inline static const FName GeneratedClassName = FName(TEXT("ULiteralPostInitCollisionAsset"));
	inline static const FName AssetName = FName(TEXT("CollisionExampleAsset"));
	inline static const FName RightCallsPropertyName = FName(TEXT("RightCalls"));
	inline static const FName WrongCallsPropertyName = FName(TEXT("WrongCalls"));

	struct FLiteralAssetCollisionSnapshot
	{
		int32 RightCalls = INDEX_NONE;
		int32 WrongCalls = INDEX_NONE;
	};

	static UObject* FindLiteralAsset()
	{
		return FindObject<UObject>(FAngelscriptEngine::Get().AssetsPackage, *AssetName.ToString());
	}

	static UClass* CompileLiteralAssetCarrier(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine)
	{
		const FString ScriptSource = ASTEST_AS(R"AS(
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

		return AngelscriptFunctionalTestUtils::CompileScriptModule(
			Test,
			Engine,
			ModuleName,
			ScriptFilename,
			ScriptSource,
			GeneratedClassName);
	}

	static bool ReadLiteralAssetSnapshot(
		FAutomationTestBase& Test,
		UObject* Object,
		FLiteralAssetCollisionSnapshot& OutSnapshot)
	{
		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(Test, Object, RightCallsPropertyName, OutSnapshot.RightCalls))
		{
			return false;
		}

		if (!AngelscriptFunctionalTestUtils::ReadPropertyValue<FIntProperty>(Test, Object, WrongCallsPropertyName, OutSnapshot.WrongCalls))
		{
			return false;
		}

		return true;
	}

	static bool ExecuteModuleInt(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const FString& Declaration,
		const TCHAR* Context,
		int32& OutResult)
	{
		const bool bExecuted = ::ExecuteIntFunction(&Engine, ModuleName, Declaration, OutResult);
		FNoDiscardAsserter LocalAssert(Test);
		return LocalAssert.IsTrue(bExecuted, Context);
	}
	};

	struct FMultipleCoexistCase
	{
	inline static const FName ModuleName = FName(TEXT("ASLiteralAssetMultipleCoexist"));
	inline static const FString ScriptFilename = FString(TEXT("ASLiteralAssetMultipleCoexist.as"));
	};

	struct FWithComponentCase
	{
	inline static const FName ModuleName = FName(TEXT("ASLiteralAssetWithComponent"));
	inline static const FString ScriptFilename = FString(TEXT("ASLiteralAssetWithComponent.as"));
	};

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

	TEST_METHOD(PostInitMaterializesAssetOnce)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		TestRunner->AddExpectedError(TEXT("LogUObjectBase: Class pointer is invalid or CDO is invalid."), EAutomationExpectedErrorFlags::Contains, 1);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*FPostInitCase::ModuleName.ToString());
		};

		UClass* GeneratedClass = FPostInitCase::CompileLiteralAssetCarrier(*TestRunner, Engine);
		ASSERT_THAT(IsNotNull(GeneratedClass, TEXT("Literal-asset post-init test case should compile the generated asset carrier class")));
		if (GeneratedClass == nullptr)
		{
			return;
		}

		UObject* LiteralAssetBeforeTouch = FPostInitCase::FindLiteralAsset();
		ASSERT_THAT(IsNotNull(LiteralAssetBeforeTouch, TEXT("Literal-asset post-init test case should materialize the asset object before any explicit getter call")));
		if (LiteralAssetBeforeTouch == nullptr)
		{
			return;
		}

		FPostInitCase::FLiteralAssetSnapshot SnapshotBeforeTouch;
		if (!FPostInitCase::ReadLiteralAssetSnapshot(*TestRunner, LiteralAssetBeforeTouch, SnapshotBeforeTouch))
		{
			return;
		}

		if (!this->Assert.AreEqual(
				GeneratedClass,
				LiteralAssetBeforeTouch->GetClass(),
				TEXT("Literal-asset post-init test case should keep the generated literal asset on the expected script class"))
			|| !this->Assert.AreEqual(
				1,
				SnapshotBeforeTouch.PostInitCalls,
				TEXT("Literal-asset post-init test case should execute __Init_ExampleAsset exactly once during compile teardown"))
			|| !this->Assert.IsTrue(
				SnapshotBeforeTouch.bWasPostInit,
				TEXT("Literal-asset post-init test case should preserve the bool flag written by __Init_ExampleAsset"))
			|| !this->Assert.AreEqual(
				FPostInitCase::ExpectedInitMarker,
				SnapshotBeforeTouch.InitMarker,
				TEXT("Literal-asset post-init test case should preserve the init marker written by __Init_ExampleAsset")))
		{
			return;
		}

		int32 TouchResult = INDEX_NONE;
		if (!FPostInitCase::ExecuteModuleInt(
				*TestRunner,
				Engine,
				TEXT("int TouchExampleAssetAgain()"),
				TEXT("Literal-asset post-init test should execute TouchExampleAssetAgain()"),
				TouchResult))
		{
			return;
		}

		UObject* LiteralAssetAfterTouch = FPostInitCase::FindLiteralAsset();
		ASSERT_THAT(IsNotNull(LiteralAssetAfterTouch, TEXT("Literal-asset post-init test case should still expose the canonical asset after repeated getter access")));
		if (LiteralAssetAfterTouch == nullptr)
		{
			return;
		}

		FPostInitCase::FLiteralAssetSnapshot SnapshotAfterTouch;
		if (!FPostInitCase::ReadLiteralAssetSnapshot(*TestRunner, LiteralAssetAfterTouch, SnapshotAfterTouch))
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			FPostInitCase::ExpectedInitMarker,
			TouchResult,
			TEXT("Literal-asset post-init test should return the initialized marker when the generated getter is touched again")));
		ASSERT_THAT(IsTrue(
			LiteralAssetAfterTouch == LiteralAssetBeforeTouch,
			TEXT("Literal-asset post-init test should keep returning the same materialized asset on repeated getter access")));
		ASSERT_THAT(AreEqual(
			1,
			SnapshotAfterTouch.PostInitCalls,
			TEXT("Literal-asset post-init test should not rerun __Init_ExampleAsset when the generated getter is touched again")));
		ASSERT_THAT(IsTrue(
			SnapshotAfterTouch.bWasPostInit,
			TEXT("Literal-asset post-init test should preserve the bool flag after repeated getter access")));
		ASSERT_THAT(AreEqual(
			FPostInitCase::ExpectedInitMarker,
			SnapshotAfterTouch.InitMarker,
			TEXT("Literal-asset post-init test should preserve the init marker after repeated getter access")));
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
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		TestRunner->AddExpectedError(TEXT("LogUObjectBase: Class pointer is invalid or CDO is invalid."), EAutomationExpectedErrorFlags::Contains, 1);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*FMultipleCoexistCase::ModuleName.ToString());
		};

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			FMultipleCoexistCase::ModuleName,
			FMultipleCoexistCase::ScriptFilename,
			ASTEST_AS(R"AS(
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

		ASSERT_THAT(IsTrue(bCompiled, TEXT("Multiple assets in same class should compile")));
		if (!bCompiled)
			return;

		UClass* GeneratedClass = ::FindGeneratedClass(&Engine, TEXT("UMultiAssetOwner"));
		ASSERT_THAT(IsNotNull(GeneratedClass, TEXT("Class should be materialized")));
		if (GeneratedClass == nullptr)
			return;

		UObject* FirstAsset = FindObject<UObject>(Engine.AssetsPackage, TEXT("FirstAsset"));
		UObject* SecondAsset = FindObject<UObject>(Engine.AssetsPackage, TEXT("SecondAsset"));
		ASSERT_THAT(IsNotNull(FirstAsset, TEXT("FirstAsset should be materialized")));
		ASSERT_THAT(IsNotNull(SecondAsset, TEXT("SecondAsset should be materialized")));
		if (FirstAsset && SecondAsset)
		{
			ASSERT_THAT(IsTrue(FirstAsset != SecondAsset, TEXT("Assets should be independent objects")));
		}

	}

	TEST_METHOD(AssetWithDefaultComponentCoexist)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);
		TestRunner->AddExpectedError(TEXT("LogUObjectBase: Class pointer is invalid or CDO is invalid."), EAutomationExpectedErrorFlags::Contains, 1);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*FWithComponentCase::ModuleName.ToString());
		};

		ECompileResult CompileResult = ECompileResult::Error;
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::FullReload,
			FWithComponentCase::ModuleName,
			FWithComponentCase::ScriptFilename,
			ASTEST_AS(R"AS(
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

		ASSERT_THAT(IsTrue(bCompiled, TEXT("Asset + DefaultComponent coexistence should compile")));
		if (!bCompiled)
			return;

		UClass* ActorClass = ::FindGeneratedClass(&Engine, TEXT("AAssetAndComponentActor"));
		ASSERT_THAT(IsNotNull(ActorClass, TEXT("Actor class should be materialized")));

		UClass* CarrierClass = ::FindGeneratedClass(&Engine, TEXT("UAssetCarrier"));
		ASSERT_THAT(IsNotNull(CarrierClass, TEXT("Carrier class should be materialized")));

		UObject* AssetObj = FindObject<UObject>(Engine.AssetsPackage, TEXT("MyCoexistAsset"));
		ASSERT_THAT(IsNotNull(AssetObj, TEXT("Asset should coexist with component-bearing actor")));

	}
};

#endif
