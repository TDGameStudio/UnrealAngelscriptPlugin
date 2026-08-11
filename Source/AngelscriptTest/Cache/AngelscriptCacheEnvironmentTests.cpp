#include "Cache/AngelscriptCacheEnvironmentProfile.h"

#include "CQTest.h"
#include "Preprocessor/AngelscriptPreprocessor.h"
#include "Shared/AngelscriptTestFixture.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheEnvironmentTests_Private
{
	static FAngelscriptCacheEnvironmentProfile BuildIdentity(
		FAutomationTestBase& Test,
		const FAngelscriptEngine& Engine,
		const FAngelscriptPreprocessorContext& PreprocessorContext,
		const TArray<FAngelscriptSourceRoot>& Roots)
	{
		FAngelscriptCacheEnvironmentProfile Identity;
		const FAngelscriptCacheEnvironmentProfileResult Result =
			BuildAngelscriptCacheEnvironmentProfile(
				Engine, PreprocessorContext, Roots, Identity);
		Test.AddInfo(FString::Printf(
			TEXT("V6.2 environment identity: Success=%d Compatibility=%s Context=%s Profile=%s CompileOptions=%d DiscoveryOptions=%d Detail=%s"),
			Result.IsSuccess() ? 1 : 0,
			*Identity.CaptureOptions.Compatibility.Hash.ToHexString(),
			*Identity.CaptureOptions.Context.Hash.ToHexString(),
			*Identity.CaptureOptions.Profile.Hash.ToHexString(),
			Identity.CaptureOptions.CanonicalCompileOptions.Num(),
			Identity.DiscoveryConfig.Options.Num(), *Result.Detail));
		if (!Result.IsSuccess())
		{
			Test.AddError(Result.Detail);
		}
		return Identity;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheEnvironmentTests,
	"Angelscript.TestModule.Cache.EnvironmentIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(IsStableAcrossEnginesAndHostRootRelocation)
	{
		using namespace AngelscriptCacheEnvironmentTests_Private;
		FAngelscriptTestFixture FirstFixture(
			*TestRunner, ETestEngineMode::IsolatedFull);
		FAngelscriptTestFixture SecondFixture(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(FirstFixture.IsValid()));
		ASSERT_THAT(IsTrue(SecondFixture.IsValid()));

		FAngelscriptPreprocessorContext Context =
			FAngelscriptPreprocessorContext::CreateFromCurrentEngineContext();
		const TArray<FAngelscriptSourceRoot> FirstRoots{
			FAngelscriptSourceRoot::FromGameRoot(
				TEXT("D:/Workspace/FirstProject/Script")),
			FAngelscriptSourceRoot::FromPluginRoot(
				TEXT("Inventory"),
				TEXT("D:/Workspace/FirstProject/Plugins/Inventory/Script")),
		};
		const TArray<FAngelscriptSourceRoot> RelocatedRoots{
			FAngelscriptSourceRoot::FromGameRoot(
				TEXT("E:/Relocated/SecondProject/Script")),
			FAngelscriptSourceRoot::FromPluginRoot(
				TEXT("Inventory"),
				TEXT("E:/Relocated/SecondProject/Plugins/Inventory/Script")),
		};

		const FAngelscriptCacheEnvironmentProfile First = BuildIdentity(
			*TestRunner, FirstFixture.GetEngine(), Context, FirstRoots);
		const FAngelscriptCacheEnvironmentProfile Second = BuildIdentity(
			*TestRunner, SecondFixture.GetEngine(), Context, RelocatedRoots);

		ASSERT_THAT(IsFalse(First.CaptureOptions.Profile.Hash.IsZero()));
		ASSERT_THAT(IsTrue(First.CaptureOptions.Compatibility.Hash
			== Second.CaptureOptions.Compatibility.Hash));
		ASSERT_THAT(IsTrue(First.CaptureOptions.Context.Hash
			== Second.CaptureOptions.Context.Hash));
		ASSERT_THAT(IsTrue(First.CaptureOptions.Profile.Hash
			== Second.CaptureOptions.Profile.Hash));
		ASSERT_THAT(AreEqual(
			First.CaptureOptions.CanonicalCompileOptions,
			Second.CaptureOptions.CanonicalCompileOptions));
		ASSERT_THAT(AreEqual(
			First.DiscoveryConfig.Options.Num(),
			Second.DiscoveryConfig.Options.Num()));
	}

	TEST_METHOD(ContextTracksEffectiveOptionsAndLogicalMountsOnly)
	{
		using namespace AngelscriptCacheEnvironmentTests_Private;
		FAngelscriptTestFixture Fixture(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Fixture.IsValid()));

		FAngelscriptPreprocessorContext BaseContext =
			FAngelscriptPreprocessorContext::CreateFromCurrentEngineContext();
		const TArray<FAngelscriptSourceRoot> BaseRoots{
			FAngelscriptSourceRoot::FromGameRoot(
				TEXT("D:/Workspace/IdentityProject/Script")),
		};
		const FAngelscriptCacheEnvironmentProfile Base = BuildIdentity(
			*TestRunner, Fixture.GetEngine(), BaseContext, BaseRoots);

		FAngelscriptPreprocessorContext ChangedOption = BaseContext;
		ChangedOption.bUseAutomaticImportMethod =
			!BaseContext.bUseAutomaticImportMethod;
		const FAngelscriptCacheEnvironmentProfile OptionVariant = BuildIdentity(
			*TestRunner, Fixture.GetEngine(), ChangedOption, BaseRoots);

		const TArray<FAngelscriptSourceRoot> ChangedMountRoots{
			FAngelscriptSourceRoot::FromGameRoot(
				TEXT("D:/Workspace/IdentityProject/Script")),
			FAngelscriptSourceRoot::FromPluginRoot(
				TEXT("NewLogicalPlugin"),
				TEXT("D:/Workspace/IdentityProject/Plugins/NewLogicalPlugin/Script")),
		};
		const FAngelscriptCacheEnvironmentProfile MountVariant = BuildIdentity(
			*TestRunner, Fixture.GetEngine(), BaseContext, ChangedMountRoots);

		ASSERT_THAT(IsTrue(Base.CaptureOptions.Compatibility.Hash
			== OptionVariant.CaptureOptions.Compatibility.Hash));
		ASSERT_THAT(IsTrue(Base.CaptureOptions.Compatibility.Hash
			== MountVariant.CaptureOptions.Compatibility.Hash));
		ASSERT_THAT(IsFalse(Base.CaptureOptions.Context.Hash
			== OptionVariant.CaptureOptions.Context.Hash));
		ASSERT_THAT(IsFalse(Base.CaptureOptions.Profile.Hash
			== OptionVariant.CaptureOptions.Profile.Hash));
		ASSERT_THAT(IsFalse(Base.CaptureOptions.Context.Hash
			== MountVariant.CaptureOptions.Context.Hash));
		ASSERT_THAT(IsFalse(Base.CaptureOptions.Profile.Hash
			== MountVariant.CaptureOptions.Profile.Hash));
	}
};

#endif
