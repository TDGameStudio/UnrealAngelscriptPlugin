#include "../Support/AngelscriptNativeCaseTestSupport.h"

#include "AngelscriptTestMacros.h"
#include "Core/UnrealAngelscriptVersion.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FEngineVersionTests,
	"Angelscript.TestModule.AngelScriptSDK.Engine.Version",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ReportsOwnedProductIdentity)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("ENG-PRODUCT-VERSION-CONTRACT",
			ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Lifecycle);

		ASSERT_THAT(AreEqual(1u, static_cast<uint32>(UNREAL_ANGELSCRIPT_VERSION_MAJOR),
			TEXT("Product version should expose major version 1")));
		ASSERT_THAT(AreEqual(0u, static_cast<uint32>(UNREAL_ANGELSCRIPT_VERSION_MINOR),
			TEXT("Product version should expose minor version 0")));
		ASSERT_THAT(AreEqual(0u, static_cast<uint32>(UNREAL_ANGELSCRIPT_VERSION_PATCH),
			TEXT("Product version should expose patch version 0")));
		ASSERT_THAT(AreEqual(10000u, static_cast<uint32>(UNREAL_ANGELSCRIPT_VERSION),
			TEXT("Product version should encode Unreal AngelScript 1.0.0 as 10000")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("Unreal AngelScript")),
			FString(ANSI_TO_TCHAR(UNREAL_ANGELSCRIPT_PRODUCT_NAME)),
			TEXT("Product name should use the owned Unreal AngelScript identity")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("1.0.0")),
			FString(ANSI_TO_TCHAR(UNREAL_ANGELSCRIPT_VERSION_STRING)),
			TEXT("Product version string should expose semantic version 1.0.0")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("Unreal AngelScript 1.0.0")),
			FString(ANSI_TO_TCHAR(UNREAL_ANGELSCRIPT_PRODUCT_VERSION_STRING)),
			TEXT("Product identity string should combine the owned name and semantic version")));
		ASSERT_THAT(AreEqual(
			static_cast<uint32>(UNREAL_ANGELSCRIPT_VERSION),
			static_cast<uint32>(ANGELSCRIPT_VERSION),
			TEXT("Legacy AngelScript version macro should alias the owned product version")));
		ASSERT_THAT(AreEqual(
			FString(ANSI_TO_TCHAR(UNREAL_ANGELSCRIPT_PRODUCT_VERSION_STRING)),
			FString(ANSI_TO_TCHAR(ANGELSCRIPT_VERSION_STRING)),
			TEXT("Legacy AngelScript version string should alias the owned product identity")));

		const ANSICHAR* const RawLibraryVersion = asGetLibraryVersion();
		ASSERT_THAT(IsNotNull(RawLibraryVersion, TEXT("Runtime version query should return a product identity")));

#ifdef _DEBUG
		const FString ExpectedLibraryVersion(TEXT("Unreal AngelScript 1.0.0 DEBUG"));
#else
		const FString ExpectedLibraryVersion(TEXT("Unreal AngelScript 1.0.0"));
#endif

		ASSERT_THAT(AreEqual(
			ExpectedLibraryVersion,
			FString(ANSI_TO_TCHAR(RawLibraryVersion)),
			TEXT("Runtime version query should return the owned product identity")));
	}

	TEST_METHOD(ReportsUpstreamLineageSeparately)
	{
		AS_NATIVE_PRODUCT_PART("ENG-PRODUCT-VERSION-CONTRACT", "upstream_lineage");

		const ANSICHAR* const RawUpstreamVersion = asGetLibraryUpstreamVersion();
		ASSERT_THAT(IsNotNull(RawUpstreamVersion, TEXT("Upstream version query should return source provenance")));

		const FString UpstreamVersion = ANSI_TO_TCHAR(RawUpstreamVersion);
		ASSERT_THAT(AreEqual(
			FString(TEXT("AngelScript 2.33.0 WIP lineage + selective 2.38 backports")),
			UpstreamVersion,
			TEXT("Upstream version query should preserve the exact fork lineage")));
		ASSERT_THAT(AreNotEqual(
			FString(ANSI_TO_TCHAR(UNREAL_ANGELSCRIPT_PRODUCT_VERSION_STRING)),
			UpstreamVersion,
			TEXT("Upstream lineage should remain separate from the product identity")));
	}

	TEST_METHOD(CreatesEngineForCurrentProductVersion)
	{
		AS_NATIVE_PRODUCT_PART("ENG-PRODUCT-VERSION-CONTRACT", "current_version_creation");

		asIScriptEngine* const ScriptEngine = asCreateScriptEngine(UNREAL_ANGELSCRIPT_VERSION);
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Current product header should create the current runtime")));

		if (ScriptEngine != nullptr)
		{
			ScriptEngine->ShutDownAndRelease();
		}
	}

	TEST_METHOD(RejectsLegacyUpstreamVersion)
	{
		AS_NATIVE_PRODUCT_PART("ENG-PRODUCT-VERSION-CONTRACT", "legacy_upstream_rejection");

		asIScriptEngine* const ScriptEngine = asCreateScriptEngine(UNREAL_ANGELSCRIPT_UPSTREAM_BASE_VERSION);
		ON_SCOPE_EXIT
		{
			if (ScriptEngine != nullptr)
			{
				ScriptEngine->ShutDownAndRelease();
			}
		};

		ASSERT_THAT(IsNull(
			ScriptEngine,
			TEXT("Legacy AngelScript 2.33 headers should not create an Unreal AngelScript runtime")));
	}

	TEST_METHOD(RejectsNewerHeaderAgainstCurrentRuntime)
	{
		AS_NATIVE_PRODUCT_PART("ENG-PRODUCT-VERSION-CONTRACT", "newer_header_rejection");

		asIScriptEngine* const PatchEngine = asCreateScriptEngine(UnrealAngelscriptVersion::Encode(1, 0, 1));
		ON_SCOPE_EXIT
		{
			if (PatchEngine != nullptr)
			{
				PatchEngine->ShutDownAndRelease();
			}
		};

		ASSERT_THAT(IsNull(
			PatchEngine,
			TEXT("A 1.0.1 header should not create the older 1.0.0 runtime")));

		asIScriptEngine* const MinorEngine = asCreateScriptEngine(UnrealAngelscriptVersion::Encode(1, 1, 0));
		ON_SCOPE_EXIT
		{
			if (MinorEngine != nullptr)
			{
				MinorEngine->ShutDownAndRelease();
			}
		};

		ASSERT_THAT(IsNull(
			MinorEngine,
			TEXT("A 1.1.0 header should not create the older 1.0.0 runtime")));
	}

	TEST_METHOD(RejectsDifferentMajorAndZeroVersions)
	{
		AS_NATIVE_PRODUCT_PART("ENG-PRODUCT-VERSION-CONTRACT", "major_and_zero_rejection");

		asIScriptEngine* const MajorEngine = asCreateScriptEngine(UnrealAngelscriptVersion::Encode(2, 0, 0));
		ON_SCOPE_EXIT
		{
			if (MajorEngine != nullptr)
			{
				MajorEngine->ShutDownAndRelease();
			}
		};

		ASSERT_THAT(IsNull(
			MajorEngine,
			TEXT("A different product major should not create the 1.0.0 runtime")));

		asIScriptEngine* const ZeroEngine = asCreateScriptEngine(0);
		ON_SCOPE_EXIT
		{
			if (ZeroEngine != nullptr)
			{
				ZeroEngine->ShutDownAndRelease();
			}
		};

		ASSERT_THAT(IsNull(
			ZeroEngine,
			TEXT("A zero requested version should not create the product runtime")));
	}

	TEST_METHOD(AppliesSemanticVersionCompatibilityRules)
	{
		AS_NATIVE_PRODUCT_PART("ENG-PRODUCT-VERSION-CONTRACT", "semantic_compatibility");

		ASSERT_THAT(IsTrue(
			UnrealAngelscriptVersion::IsCompatible(
				UnrealAngelscriptVersion::Encode(1, 0, 0),
				UnrealAngelscriptVersion::Encode(1, 1, 0)),
			TEXT("An older 1.0.0 header should be compatible with a later 1.1.0 runtime")));
		ASSERT_THAT(IsTrue(
			UnrealAngelscriptVersion::IsCompatible(
				UnrealAngelscriptVersion::Encode(1, 0, 1),
				UnrealAngelscriptVersion::Encode(1, 1, 0)),
			TEXT("An older 1.0.1 header should be compatible with a later 1.1.0 runtime")));
		ASSERT_THAT(IsFalse(
			UnrealAngelscriptVersion::IsCompatible(
				UnrealAngelscriptVersion::Encode(1, 1, 0),
				UnrealAngelscriptVersion::Encode(1, 0, 0)),
			TEXT("A newer 1.1.0 header should be incompatible with an older 1.0.0 runtime")));
		ASSERT_THAT(IsFalse(
			UnrealAngelscriptVersion::IsCompatible(
				UnrealAngelscriptVersion::Encode(2, 0, 0),
				UnrealAngelscriptVersion::Encode(1, 9, 9)),
			TEXT("Different product majors should remain incompatible")));
		ASSERT_THAT(IsFalse(
			UnrealAngelscriptVersion::IsCompatible(
				0,
				UnrealAngelscriptVersion::Encode(1, 0, 0)),
			TEXT("A zero requested version should remain incompatible")));
	}

	TEST_METHOD(EncodesSemanticVersionComponents)
	{
		AS_NATIVE_PRODUCT_PART("ENG-PRODUCT-VERSION-CONTRACT", "version_encoding");

		ASSERT_THAT(AreEqual(
			10000u,
			static_cast<uint32>(UnrealAngelscriptVersion::Encode(1, 0, 0)),
			TEXT("Semantic version encoding should encode 1.0.0 as 10000")));
		ASSERT_THAT(AreEqual(
			10203u,
			static_cast<uint32>(UnrealAngelscriptVersion::Encode(1, 2, 3)),
			TEXT("Semantic version encoding should preserve two decimal digits per minor and patch component")));
		ASSERT_THAT(AreEqual(
			static_cast<uint32>(UNREAL_ANGELSCRIPT_VERSION),
			static_cast<uint32>(UnrealAngelscriptVersion::Encode(
				UNREAL_ANGELSCRIPT_VERSION_MAJOR,
				UNREAL_ANGELSCRIPT_VERSION_MINOR,
				UNREAL_ANGELSCRIPT_VERSION_PATCH)),
			TEXT("Published product version should match the canonical semantic encoding")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
