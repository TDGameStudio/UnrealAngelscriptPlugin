#include "CQTest.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"

#include "Engine/Texture2D.h"
#include "GameFramework/Actor.h"
#include "UObject/SoftObjectPath.h"

// -----------------------------------------------------------------------------
// AngelscriptCoverageAssetLoadingTests
// -----------------------------------------------------------------------------
// Coverage for the high-priority synchronous asset-loading slice from:
//
//   Documents/Coverage/Coverage_AssetLoading.md
//
// Axes covered here:
//   * FSoftObjectPath.TryLoad for a known engine asset
//   * FSoftClassPath.TryLoadClass for a known native class
//   * global LoadObject for a known engine asset
//   * missing-path boundaries for sync load helpers
// -----------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageAssetLoadingTest,
	"Angelscript.TestModule.Coverage.AssetLoading",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static constexpr const TCHAR* DefaultTexturePath = TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture");
	static constexpr const TCHAR* MissingTexturePath = TEXT("/Engine/EngineResources/DefinitelyMissingCoverageTexture.DefinitelyMissingCoverageTexture");

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

	TEST_METHOD(GlobalLoadObject)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FString Script = ASTEST_AS(R"AS(
			int LoadObjectKnownTexture()
			{
				UObject Loaded = LoadObject(null, "__DEFAULT_TEXTURE_PATH__");
				return Cast<UTexture2D>(Loaded) != null ? 1 : 0;
			}

			int LoadObjectMissingTexture()
			{
				UObject Loaded = LoadObject(null, "__MISSING_TEXTURE_PATH__");
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
				TexturePath.TryLoad();
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
				ClassPath.TryLoadClass();
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

#endif // WITH_DEV_AUTOMATION_TESTS
