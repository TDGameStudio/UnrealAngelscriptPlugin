#include "CQTest.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "Modules/ModuleManager.h"
#include "UObject/SoftObjectPath.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageAssetLoadingTests
// -----------------------------------------------------------------------------
// Coverage for the high-priority asset-loading and soft-reference slices from:
//
//   OpenSpec: test-coverage/coverage-matrix.md
//
// Axes covered here:
//   * FSoftObjectPath.TryLoad for a known engine asset
//   * FSoftClassPath.TryLoadClass for a known native class
//   * global LoadObject for a known engine asset
//   * missing-path boundaries for sync load helpers
//   * soft path string/metadata identity
//   * TSoftObjectPtr/TSoftClassPtr path construction, pending, and reset paths
// -----------------------------------------------------------------------------

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageAssetLoadingTest,
	"Angelscript.TestModule.Coverage.AssetLoading",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static constexpr const TCHAR* DefaultTexturePath = TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture");
	static constexpr const TCHAR* MissingTexturePath = TEXT("/Engine/EngineResources/DefinitelyMissingCoverageTexture.DefinitelyMissingCoverageTexture");
	static constexpr const TCHAR* MissingActorClassPath = TEXT("/Game/Coverage/DefinitelyMissingCoverageActor.DefinitelyMissingCoverageActor_C");
	static constexpr const TCHAR* CrossLevelActorPath = TEXT("/Game/Coverage/OtherMap.OtherMap:PersistentLevel.OtherActor");

	static IAssetRegistry& GetAssetRegistryChecked()
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		return AssetRegistryModule.Get();
	}

	static bool ContainsAssetObjectPath(const TArray<FAssetData>& Assets, const FString& ObjectPath)
	{
		return Assets.ContainsByPredicate([&ObjectPath](const FAssetData& AssetData)
		{
			return AssetData.GetObjectPathString() == ObjectPath;
		});
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

	TEST_METHOD(SynchronousSoftObjectPathLoad)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FString Script = ASTEST_AS(R"AS(
			int TryLoadKnownTexture()
			{
				FSoftObjectPath TexturePath("__DEFAULT_TEXTURE_PATH__");
				UObject Loaded = TexturePath.TryLoad();
				return Cast<UTexture2D>(Loaded) != null ? 1 : 0;
			}

			int ResolveKnownTextureAfterLoad()
			{
				FSoftObjectPath TexturePath("__DEFAULT_TEXTURE_PATH__");
				UObject Loaded = TexturePath.TryLoad();
				UObject Resolved = TexturePath.ResolveObject();
				return Loaded != null && Resolved == Loaded ? 1 : 0;
			}

			int MissingTextureReturnsNull()
			{
				FSoftObjectPath TexturePath("__MISSING_TEXTURE_PATH__");
				return TexturePath.TryLoad() == null ? 1 : 0;
			}
			)AS");
		Script.ReplaceInline(TEXT("__DEFAULT_TEXTURE_PATH__"), DefaultTexturePath, ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__MISSING_TEXTURE_PATH__"), MissingTexturePath, ESearchCase::CaseSensitive);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASCoverageAssetLoading_SoftObjectPath"), Script);
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("soft object path module should compile")));
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptModule& ScriptModule = Module.GetModule();
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int TryLoadKnownTexture()"),
			TEXT("FSoftObjectPath.TryLoad should synchronously load a known engine texture"), 1)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int ResolveKnownTextureAfterLoad()"),
			TEXT("FSoftObjectPath.ResolveObject should return the same object after TryLoad"), 1)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int MissingTextureReturnsNull()"),
			TEXT("FSoftObjectPath.TryLoad should return null for a missing path"), 1)));
	}

	TEST_METHOD(SynchronousSoftClassPathLoad)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ActorClassPath = AActor::StaticClass()->GetPathName();
		FString Script = ASTEST_AS(R"AS(
			int TryLoadKnownActorClass()
			{
				FSoftClassPath ClassPath("__ACTOR_CLASS_PATH__");
				return ClassPath.TryLoadClass() == AActor::StaticClass() ? 1 : 0;
			}

			int ResolveKnownActorClassAfterLoad()
			{
				FSoftClassPath ClassPath("__ACTOR_CLASS_PATH__");
				UClass LoadedClass = ClassPath.TryLoadClass();
				UClass ResolvedClass = ClassPath.ResolveClass();
				return LoadedClass != null && ResolvedClass == LoadedClass ? 1 : 0;
			}
			)AS");
		Script.ReplaceInline(TEXT("__ACTOR_CLASS_PATH__"), *ActorClassPath.ReplaceCharWithEscapedChar(), ESearchCase::CaseSensitive);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASCoverageAssetLoading_SoftClassPath"), Script);
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("soft class path module should compile")));
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptModule& ScriptModule = Module.GetModule();
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int TryLoadKnownActorClass()"),
			TEXT("FSoftClassPath.TryLoadClass should synchronously load AActor"), 1)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int ResolveKnownActorClassAfterLoad()"),
			TEXT("FSoftClassPath.ResolveClass should return the same class after TryLoadClass"), 1)));
	}

	TEST_METHOD(SoftPathStringIdentityAndMissingClassBoundaries)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FSoftObjectPath DefaultTextureSoftPath(DefaultTexturePath);
		const FSoftClassPath ActorSoftClassPath(AActor::StaticClass());
		FString Script = ASTEST_AS(R"AS(
			int SoftObjectPathStringIdentity()
			{
				FSoftObjectPath TexturePath("__DEFAULT_TEXTURE_PATH__");
				if (!TexturePath.IsValid() || TexturePath.IsNull())
					return 0;

				if (!TexturePath.IsAsset() || TexturePath.IsSubobject())
					return 0;

				if (TexturePath.ToString() != "__DEFAULT_TEXTURE_PATH__")
					return 0;

				if (TexturePath.GetLongPackageName() != "__DEFAULT_TEXTURE_PACKAGE__")
					return 0;

				return TexturePath.GetAssetName() == "__DEFAULT_TEXTURE_ASSET__" ? 1 : 0;
			}

			int SoftClassPathStringIdentity()
			{
				FSoftClassPath ClassPath("__ACTOR_CLASS_PATH__");
				if (!ClassPath.IsValid() || ClassPath.IsNull())
					return 0;

				if (!ClassPath.IsAsset() || ClassPath.IsSubobject())
					return 0;

				if (ClassPath.ToString() != "__ACTOR_CLASS_PATH__")
					return 0;

				if (ClassPath.GetLongPackageName() != "__ACTOR_CLASS_PACKAGE__")
					return 0;

				return ClassPath.GetAssetName() == "__ACTOR_CLASS_ASSET__" ? 1 : 0;
			}

			int MissingSoftClassPathResolveBoundaries()
			{
				FSoftClassPath ClassPath("__MISSING_CLASS_PATH__");
				return ClassPath.IsValid()
					&& ClassPath.ResolveClass() == null
					&& ClassPath.TryLoadClass() == null ? 1 : 0;
			}
			)AS");
		Script.ReplaceInline(TEXT("__DEFAULT_TEXTURE_PATH__"), *DefaultTextureSoftPath.ToString().ReplaceCharWithEscapedChar(), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__DEFAULT_TEXTURE_PACKAGE__"), *DefaultTextureSoftPath.GetLongPackageName().ReplaceCharWithEscapedChar(), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__DEFAULT_TEXTURE_ASSET__"), *DefaultTextureSoftPath.GetAssetName().ReplaceCharWithEscapedChar(), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__ACTOR_CLASS_PATH__"), *ActorSoftClassPath.ToString().ReplaceCharWithEscapedChar(), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__ACTOR_CLASS_PACKAGE__"), *ActorSoftClassPath.GetLongPackageName().ReplaceCharWithEscapedChar(), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__ACTOR_CLASS_ASSET__"), *ActorSoftClassPath.GetAssetName().ReplaceCharWithEscapedChar(), ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__MISSING_CLASS_PATH__"), MissingActorClassPath, ESearchCase::CaseSensitive);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASCoverageAssetLoading_SoftPathStringIdentity"), Script);
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("soft path string identity module should compile")));
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptModule& ScriptModule = Module.GetModule();
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int SoftObjectPathStringIdentity()"),
			TEXT("FSoftObjectPath string and metadata identity should match the native path"), 1)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int SoftClassPathStringIdentity()"),
			TEXT("FSoftClassPath string and metadata identity should match the native class path"), 1)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int MissingSoftClassPathResolveBoundaries()"),
			TEXT("FSoftClassPath missing-class resolve and load boundaries should return null"), 1)));
	}

	TEST_METHOD(GlobalLoadObject)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FString Script = ASTEST_AS(R"AS(
			int LoadObjectKnownTexture()
			{
				UObject Outer = nullptr;
				UObject Loaded = LoadObject(Outer, "__DEFAULT_TEXTURE_PATH__");
				return Cast<UTexture2D>(Loaded) != null ? 1 : 0;
			}

			int LoadObjectMissingTexture()
			{
				UObject Outer = nullptr;
				UObject Loaded = LoadObject(Outer, "__MISSING_TEXTURE_PATH__");
				return Loaded == null ? 1 : 0;
			}
			)AS");
		Script.ReplaceInline(TEXT("__DEFAULT_TEXTURE_PATH__"), DefaultTexturePath, ESearchCase::CaseSensitive);
		Script.ReplaceInline(TEXT("__MISSING_TEXTURE_PATH__"), MissingTexturePath, ESearchCase::CaseSensitive);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASCoverageAssetLoading_LoadObject"), Script);
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("global LoadObject module should compile")));
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptModule& ScriptModule = Module.GetModule();
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int LoadObjectKnownTexture()"),
			TEXT("global LoadObject should synchronously load a known engine texture"), 1)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int LoadObjectMissingTexture()"),
			TEXT("global LoadObject should return null for a missing path"), 1)));
	}

	TEST_METHOD(AssetRegistryLiveQueryParity)
	{
		static const FName EngineMaterialsPath(TEXT("/Engine/EngineMaterials"));
		static const FString TargetObjectPath(TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));

		IAssetRegistry& AssetRegistry = GetAssetRegistryChecked();
		const bool bNativeHasAssets = AssetRegistry.HasAssets(EngineMaterialsPath, false);

		TArray<FAssetData> NativeAssetsByPath;
		const bool bNativeGetAssetsByPath = AssetRegistry.GetAssetsByPath(EngineMaterialsPath, NativeAssetsByPath, false, false);

		const FAssetData NativeAssetByObjectPath = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(TargetObjectPath), false);
		const FString NativeObjectPathString = NativeAssetByObjectPath.GetObjectPathString();
		const FString NativeSoftObjectPathString = NativeAssetByObjectPath.GetSoftObjectPath().ToString();

		TArray<FAssetData> NativeAllAssets;
		const bool bNativeGetAllAssets = AssetRegistry.GetAllAssets(NativeAllAssets, false);
		const bool bNativeAllAssetsContainTarget = ContainsAssetObjectPath(NativeAllAssets, TargetObjectPath);

		ASSERT_THAT(IsTrue(bNativeHasAssets, TEXT("Native HasAssets baseline")));
		ASSERT_THAT(IsTrue(bNativeGetAssetsByPath, TEXT("Native GetAssetsByPath baseline")));
		ASSERT_THAT(IsTrue(NativeObjectPathString == TargetObjectPath, TEXT("Native GetAssetByObjectPath baseline")));
		ASSERT_THAT(IsTrue(bNativeGetAllAssets, TEXT("Native GetAllAssets baseline")));
		ASSERT_THAT(IsTrue(bNativeAllAssetsContainTarget, TEXT("Native GetAllAssets contains target")));

		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FString ScriptSource = ASTEST_AS(R"AS(
			int HasAssetsParity()
			{
				const bool bExpectedHasAssets = __EXPECTED_HAS_ASSETS__;
				const bool bActualHasAssets = AssetRegistry::HasAssets(n"__ENGINE_MATERIALS_PATH__", false);
				return bActualHasAssets == bExpectedHasAssets ? 1 : 0;
			}

			int GetAssetsByPathParity()
			{
				const bool bExpectedGetAssetsByPath = __EXPECTED_GET_ASSETS_BY_PATH__;
				const int ExpectedAssetsByPathCount = __EXPECTED_ASSETS_BY_PATH_COUNT__;
				const FString ExpectedObjectPathString = "__EXPECTED_OBJECT_PATH_STRING__";
				const FString ExpectedSoftObjectPathString = "__EXPECTED_SOFT_OBJECT_PATH_STRING__";

				TArray<FAssetData> AssetsByPath;
				const bool bActualGetAssetsByPath = AssetRegistry::GetAssetsByPath(n"__ENGINE_MATERIALS_PATH__", AssetsByPath, false, false);
				if (bActualGetAssetsByPath != bExpectedGetAssetsByPath)
				{
					return 0;
				}
				if (AssetsByPath.Num() != ExpectedAssetsByPathCount)
				{
					return 0;
				}

				for (int Index = 0; Index < AssetsByPath.Num(); ++Index)
				{
					if (AssetsByPath[Index].GetObjectPathString() == ExpectedObjectPathString)
					{
						return AssetsByPath[Index].GetSoftObjectPath().ToString() == ExpectedSoftObjectPathString ? 1 : 0;
					}
				}
				return 0;
			}

			int GetAssetByObjectPathParity()
			{
				const FString TargetObjectPath = "__TARGET_OBJECT_PATH__";
				const FString ExpectedObjectPathString = "__EXPECTED_OBJECT_PATH_STRING__";
				const FString ExpectedSoftObjectPathString = "__EXPECTED_SOFT_OBJECT_PATH_STRING__";

				FAssetData AssetByObjectPath = AssetRegistry::GetAssetByObjectPath(FSoftObjectPath(TargetObjectPath), false);
				return AssetByObjectPath.GetObjectPathString() == ExpectedObjectPathString
					&& AssetByObjectPath.GetSoftObjectPath().ToString() == ExpectedSoftObjectPathString ? 1 : 0;
			}

			int GetAllAssetsParity()
			{
				const bool bExpectedGetAllAssets = __EXPECTED_GET_ALL_ASSETS__;
				const bool bExpectedAllAssetsContainTarget = __EXPECTED_ALL_ASSETS_CONTAIN_TARGET__;
				const int ExpectedAllAssetsCount = __EXPECTED_ALL_ASSETS_COUNT__;
				const FString ExpectedObjectPathString = "__EXPECTED_OBJECT_PATH_STRING__";

				TArray<FAssetData> AllAssets;
				const bool bActualGetAllAssets = AssetRegistry::GetAllAssets(AllAssets, false);
				if (bActualGetAllAssets != bExpectedGetAllAssets)
				{
					return 0;
				}
				if (AllAssets.Num() != ExpectedAllAssetsCount)
				{
					return 0;
				}

				bool bFoundTargetInAllAssets = false;
				for (int Index = 0; Index < AllAssets.Num(); ++Index)
				{
					if (AllAssets[Index].GetObjectPathString() == ExpectedObjectPathString)
					{
						bFoundTargetInAllAssets = true;
						break;
					}
				}
				return bFoundTargetInAllAssets == bExpectedAllAssetsContainTarget ? 1 : 0;
			}
			)AS");
		ScriptSource.ReplaceInline(TEXT("__TARGET_OBJECT_PATH__"), *TargetObjectPath, ESearchCase::CaseSensitive);
		ScriptSource.ReplaceInline(TEXT("__EXPECTED_HAS_ASSETS__"), bNativeHasAssets ? TEXT("true") : TEXT("false"), ESearchCase::CaseSensitive);
		ScriptSource.ReplaceInline(TEXT("__EXPECTED_GET_ASSETS_BY_PATH__"), bNativeGetAssetsByPath ? TEXT("true") : TEXT("false"), ESearchCase::CaseSensitive);
		ScriptSource.ReplaceInline(TEXT("__EXPECTED_GET_ALL_ASSETS__"), bNativeGetAllAssets ? TEXT("true") : TEXT("false"), ESearchCase::CaseSensitive);
		ScriptSource.ReplaceInline(TEXT("__EXPECTED_ALL_ASSETS_CONTAIN_TARGET__"), bNativeAllAssetsContainTarget ? TEXT("true") : TEXT("false"), ESearchCase::CaseSensitive);
		ScriptSource.ReplaceInline(TEXT("__EXPECTED_ASSETS_BY_PATH_COUNT__"), *FString::FromInt(NativeAssetsByPath.Num()), ESearchCase::CaseSensitive);
		ScriptSource.ReplaceInline(TEXT("__EXPECTED_ALL_ASSETS_COUNT__"), *FString::FromInt(NativeAllAssets.Num()), ESearchCase::CaseSensitive);
		ScriptSource.ReplaceInline(TEXT("__EXPECTED_OBJECT_PATH_STRING__"), *NativeObjectPathString, ESearchCase::CaseSensitive);
		ScriptSource.ReplaceInline(TEXT("__EXPECTED_SOFT_OBJECT_PATH_STRING__"), *NativeSoftObjectPathString, ESearchCase::CaseSensitive);
		ScriptSource.ReplaceInline(TEXT("__ENGINE_MATERIALS_PATH__"), *EngineMaterialsPath.ToString(), ESearchCase::CaseSensitive);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASCoverageAssetLoading_AssetRegistryLiveQueryParity"), ScriptSource);
		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("AssetRegistry live query parity module should compile")));
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptModule& ScriptModule = Module.GetModule();
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int HasAssetsParity()"),
			TEXT("AssetRegistry::HasAssets should match the native baseline"), 1)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int GetAssetsByPathParity()"),
			TEXT("AssetRegistry::GetAssetsByPath should match the native baseline"), 1)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int GetAssetByObjectPathParity()"),
			TEXT("AssetRegistry::GetAssetByObjectPath should match the native baseline"), 1)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int GetAllAssetsParity()"),
			TEXT("AssetRegistry::GetAllAssets should match the native baseline"), 1)));
	}

	TEST_METHOD(SoftReferencePathConstructionAndPending)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FSoftClassPath ActorSoftClassPath(AActor::StaticClass());
		FString ScriptSource = ASTEST_AS(R"AS(
			int SoftObjectPtrConstructedFromPathKeepsIdentity()
			{
				FSoftObjectPath TexturePath("__DEFAULT_TEXTURE_PATH__");
				TSoftObjectPtr<UTexture2D> TextureRef(TexturePath);
				return !TextureRef.IsNull()
					&& TextureRef.ToSoftObjectPath() == TexturePath
					&& TextureRef.ToString() == TexturePath.ToString()
					&& TextureRef.GetLongPackageName() == TexturePath.GetLongPackageName()
					&& TextureRef.GetAssetName() == TexturePath.GetAssetName() ? 1 : 0;
			}

			int MissingSoftObjectPtrReportsPending()
			{
				TSoftObjectPtr<UTexture2D> MissingRef(FSoftObjectPath("__MISSING_TEXTURE_PATH__"));
				return !MissingRef.IsNull()
					&& !MissingRef.IsValid()
					&& MissingRef.IsPending()
					&& MissingRef.Get() == null ? 1 : 0;
			}

			int CrossLevelSoftObjectPtrStaysPathOnly()
			{
				TSoftObjectPtr<AActor> ActorRef(FSoftObjectPath("__CROSS_LEVEL_ACTOR_PATH__"));
				return !ActorRef.IsNull()
					&& !ActorRef.IsValid()
					&& ActorRef.IsPending()
					&& ActorRef.ToString().Contains("PersistentLevel.OtherActor") ? 1 : 0;
			}

			int SoftClassPtrConstructedFromPathResolvesActor()
			{
				FSoftObjectPath ClassObjectPath("__ACTOR_CLASS_PATH__");
				TSoftClassPtr<AActor> ActorClassRef(ClassObjectPath);
				TSubclassOf<AActor> LoadedClass = ActorClassRef.Get();
				return !ActorClassRef.IsNull()
					&& ActorClassRef.IsValid()
					&& ActorClassRef.ToSoftObjectPath() == ClassObjectPath
					&& LoadedClass.IsValid()
					&& LoadedClass.IsChildOf(AActor::StaticClass()) ? 1 : 0;
			}

			int MissingSoftClassPtrReportsPending()
			{
				TSoftClassPtr<AActor> MissingClassRef(FSoftObjectPath("__MISSING_CLASS_PATH__"));
				TSubclassOf<AActor> MissingClass = MissingClassRef.Get();
				return !MissingClassRef.IsNull()
					&& !MissingClassRef.IsValid()
					&& MissingClassRef.IsPending()
					&& !MissingClass.IsValid() ? 1 : 0;
			}

			int ResetSoftReferencesClearPaths()
			{
				TSoftObjectPtr<UTexture2D> TextureRef(FSoftObjectPath("__DEFAULT_TEXTURE_PATH__"));
				TSoftClassPtr<AActor> ActorClassRef(FSoftObjectPath("__ACTOR_CLASS_PATH__"));
				TextureRef.Reset();
				ActorClassRef.Reset();
				return TextureRef.IsNull()
					&& ActorClassRef.IsNull()
					&& !TextureRef.IsPending()
					&& !ActorClassRef.IsPending()
					&& TextureRef.ToString().IsEmpty()
					&& ActorClassRef.ToString().IsEmpty() ? 1 : 0;
			}
			)AS");
		ScriptSource.ReplaceInline(TEXT("__DEFAULT_TEXTURE_PATH__"), DefaultTexturePath, ESearchCase::CaseSensitive);
		ScriptSource.ReplaceInline(TEXT("__MISSING_TEXTURE_PATH__"), MissingTexturePath, ESearchCase::CaseSensitive);
		ScriptSource.ReplaceInline(TEXT("__CROSS_LEVEL_ACTOR_PATH__"), CrossLevelActorPath, ESearchCase::CaseSensitive);
		ScriptSource.ReplaceInline(TEXT("__ACTOR_CLASS_PATH__"), *ActorSoftClassPath.ToString().ReplaceCharWithEscapedChar(), ESearchCase::CaseSensitive);
		ScriptSource.ReplaceInline(TEXT("__MISSING_CLASS_PATH__"), MissingActorClassPath, ESearchCase::CaseSensitive);

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASCoverageAssetLoading_SoftReferencePathConstruction"), ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("soft-reference path construction module should compile")));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		asIScriptModule& ScriptModule = ModuleScope.GetModule();
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int SoftObjectPtrConstructedFromPathKeepsIdentity()"),
			TEXT("TSoftObjectPtr should preserve FSoftObjectPath identity and metadata"), 1)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int MissingSoftObjectPtrReportsPending()"),
			TEXT("TSoftObjectPtr should report pending for path-only missing assets"), 1)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int CrossLevelSoftObjectPtrStaysPathOnly()"),
			TEXT("TSoftObjectPtr should preserve cross-level actor object paths without loading"), 1)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int SoftClassPtrConstructedFromPathResolvesActor()"),
			TEXT("TSoftClassPtr should construct from a class path and resolve the loaded actor class"), 1)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int MissingSoftClassPtrReportsPending()"),
			TEXT("TSoftClassPtr should report pending for path-only missing classes"), 1)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int ResetSoftReferencesClearPaths()"),
			TEXT("soft reference Reset should clear stored object and class paths"), 1)));
	}

	TEST_METHOD(SoftReferenceAsyncBoundaries)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ActorClassPath = AActor::StaticClass()->GetPathName();
		FString ScriptSource = ASTEST_AS(R"AS(
			UCLASS()
			class UCoverageSoftReferenceAsyncReceiver : UObject
			{
				UPROPERTY()
				int ObjectCallbackCount = 0;

				UPROPERTY()
				bool bObjectPayloadWasTexture = false;

				UPROPERTY()
				int MissingObjectCallbackCount = 0;

				UPROPERTY()
				bool bMissingObjectPayloadWasNull = false;

				UPROPERTY()
				int ClassCallbackCount = 0;

				UPROPERTY()
				bool bClassPayloadWasActorClass = false;

				UFUNCTION()
				void HandleObjectLoaded(UObject Loaded)
				{
					ObjectCallbackCount += 1;
					bObjectPayloadWasTexture = Cast<UTexture2D>(Loaded) != nullptr;
				}

				UFUNCTION()
				void HandleMissingObject(UObject Loaded)
				{
					MissingObjectCallbackCount += 1;
					bMissingObjectPayloadWasNull = Loaded == nullptr;
				}

				UFUNCTION()
				void HandleClassLoaded(UClass Loaded)
				{
					ClassCallbackCount += 1;
					bClassPayloadWasActorClass = Loaded != nullptr && Loaded.IsChildOf(AActor::StaticClass());
				}
			}

			int AsyncObjectLoadedPathInvokesCallback()
			{
				UCoverageSoftReferenceAsyncReceiver Receiver = Cast<UCoverageSoftReferenceAsyncReceiver>(
					NewObject(GetTransientPackage(), UCoverageSoftReferenceAsyncReceiver::StaticClass(), n"CoverageSoftReferenceObjectReceiver", true));
				if (Receiver == nullptr)
					return 0;

				FOnSoftObjectLoaded Delegate;
				Delegate.BindUFunction(Receiver, n"HandleObjectLoaded");

				FSoftObjectPath TexturePath("__DEFAULT_TEXTURE_PATH__");
				UObject LoadedTexture = TexturePath.TryLoad();
				if (LoadedTexture == nullptr)
				{
					return 0;
				}
				TSoftObjectPtr<UTexture2D> TextureRef(TexturePath);
				TextureRef.LoadAsync(Delegate);
				return Receiver.ObjectCallbackCount == 1 && Receiver.bObjectPayloadWasTexture ? 1 : 0;
			}

			int AsyncObjectMissingPathReportsNull()
			{
				UCoverageSoftReferenceAsyncReceiver Receiver = Cast<UCoverageSoftReferenceAsyncReceiver>(
					NewObject(GetTransientPackage(), UCoverageSoftReferenceAsyncReceiver::StaticClass(), n"CoverageSoftReferenceMissingReceiver", true));
				if (Receiver == nullptr)
					return 0;

				FOnSoftObjectLoaded Delegate;
				Delegate.BindUFunction(Receiver, n"HandleMissingObject");

				TSoftObjectPtr<UTexture2D> MissingRef(FSoftObjectPath("__MISSING_TEXTURE_PATH__"));
				MissingRef.LoadAsync(Delegate);
				return Receiver.MissingObjectCallbackCount == 1 && Receiver.bMissingObjectPayloadWasNull ? 1 : 0;
			}

			int AsyncClassLoadedPathInvokesCallback()
			{
				UCoverageSoftReferenceAsyncReceiver Receiver = Cast<UCoverageSoftReferenceAsyncReceiver>(
					NewObject(GetTransientPackage(), UCoverageSoftReferenceAsyncReceiver::StaticClass(), n"CoverageSoftReferenceClassReceiver", true));
				if (Receiver == nullptr)
					return 0;

				FOnSoftClassLoaded Delegate;
				Delegate.BindUFunction(Receiver, n"HandleClassLoaded");

				FSoftClassPath ClassPath("__ACTOR_CLASS_PATH__");
				UClass LoadedClass = ClassPath.TryLoadClass();
				if (LoadedClass == nullptr)
				{
					return 0;
				}
				TSoftClassPtr<AActor> ActorClassRef(FSoftObjectPath("__ACTOR_CLASS_PATH__"));
				ActorClassRef.LoadAsync(Delegate);
				return Receiver.ClassCallbackCount == 1 && Receiver.bClassPayloadWasActorClass ? 1 : 0;
			}
			)AS");
		ScriptSource.ReplaceInline(TEXT("__DEFAULT_TEXTURE_PATH__"), DefaultTexturePath, ESearchCase::CaseSensitive);
		ScriptSource.ReplaceInline(TEXT("__MISSING_TEXTURE_PATH__"), MissingTexturePath, ESearchCase::CaseSensitive);
		ScriptSource.ReplaceInline(TEXT("__ACTOR_CLASS_PATH__"), *ActorClassPath.ReplaceCharWithEscapedChar(), ESearchCase::CaseSensitive);

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASCoverageAssetLoading_SoftReferenceAsyncBoundaries"), ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("soft-reference async boundary module should compile")));
		if (!ModuleScope.IsValid())
		{
			return;
		}

		asIScriptModule& ScriptModule = ModuleScope.GetModule();
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int AsyncObjectLoadedPathInvokesCallback()"),
			TEXT("TSoftObjectPtr.LoadAsync should synchronously invoke callback for an already loaded texture"), 1)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int AsyncObjectMissingPathReportsNull()"),
			TEXT("TSoftObjectPtr.LoadAsync should report null for a missing package path"), 1)));
		ASSERT_THAT(IsTrue(ExecuteAndExpectInt(*TestRunner, Engine, ScriptModule, TEXT("int AsyncClassLoadedPathInvokesCallback()"),
			TEXT("TSoftClassPtr.LoadAsync should synchronously invoke callback for an already loaded native class"), 1)));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
