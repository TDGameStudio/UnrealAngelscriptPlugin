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

namespace
{
	static const TCHAR* DefaultTexturePath = TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture");
	static const TCHAR* MissingTexturePath = TEXT("/Engine/EngineResources/DefinitelyMissingCoverageTexture.DefinitelyMissingCoverageTexture");
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageAssetLoadingTest,
	"Angelscript.TestModule.Coverage.AssetLoading",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
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

		FString Script = TEXT(R"AS(
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
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptModule& ScriptModule = Module.GetModule();
		ExpectGlobalInt(*TestRunner, Engine, ScriptModule, TEXT("int TryLoadKnownTexture()"),
			TEXT("FSoftObjectPath.TryLoad should synchronously load a known engine texture"), 1);
		ExpectGlobalInt(*TestRunner, Engine, ScriptModule, TEXT("int ResolveKnownTextureAfterLoad()"),
			TEXT("FSoftObjectPath.ResolveObject should return the same object after TryLoad"), 1);
		ExpectGlobalInt(*TestRunner, Engine, ScriptModule, TEXT("int MissingTextureReturnsNull()"),
			TEXT("FSoftObjectPath.TryLoad should return null for a missing path"), 1);
	}

	TEST_METHOD(SynchronousSoftClassPathLoad)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ActorClassPath = AActor::StaticClass()->GetPathName();
		FString Script = TEXT(R"AS(
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
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptModule& ScriptModule = Module.GetModule();
		ExpectGlobalInt(*TestRunner, Engine, ScriptModule, TEXT("int TryLoadKnownActorClass()"),
			TEXT("FSoftClassPath.TryLoadClass should synchronously load AActor"), 1);
		ExpectGlobalInt(*TestRunner, Engine, ScriptModule, TEXT("int ResolveKnownActorClassAfterLoad()"),
			TEXT("FSoftClassPath.ResolveClass should return the same class after TryLoadClass"), 1);
	}

	TEST_METHOD(GlobalLoadObject)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FString Script = TEXT(R"AS(
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
		if (!Module.IsValid())
		{
			return;
		}

		asIScriptModule& ScriptModule = Module.GetModule();
		ExpectGlobalInt(*TestRunner, Engine, ScriptModule, TEXT("int LoadObjectKnownTexture()"),
			TEXT("global LoadObject should synchronously load a known engine texture"), 1);
		ExpectGlobalInt(*TestRunner, Engine, ScriptModule, TEXT("int LoadObjectMissingTexture()"),
			TEXT("global LoadObject should return null for a missing path"), 1);
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
