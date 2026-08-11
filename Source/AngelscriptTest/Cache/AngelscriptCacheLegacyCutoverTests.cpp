#include "Cache/AngelscriptCacheLegacyCutover.h"
#include "Cache/AngelscriptCacheRestore.h"
#include "Cache/AngelscriptCacheService.h"

#include "AngelscriptEngine.h"
#include "CQTest.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Shared/AngelscriptTestEngineAcquisition.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheLegacyCutoverTests_Private
{
	template <typename T>
	static auto HasGeneratePrecompiledData(int) -> decltype(
		(void)std::declval<T&>().bGeneratePrecompiledData,
		std::true_type{});
	template <typename T>
	static std::false_type HasGeneratePrecompiledData(...);

	template <typename T>
	static auto HasIgnorePrecompiledData(int) -> decltype(
		(void)std::declval<T&>().bIgnorePrecompiledData,
		std::true_type{});
	template <typename T>
	static std::false_type HasIgnorePrecompiledData(...);

	template <typename T>
	static auto HasSkipStaticJITCodeGen(int) -> decltype(
		(void)std::declval<T&>().bSkipStaticJITCodeGen,
		std::true_type{});
	template <typename T>
	static std::false_type HasSkipStaticJITCodeGen(...);

	class FScopedProjectRoot final
	{
	public:
		FScopedProjectRoot()
		{
			Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("Automation/AngelscriptCacheLegacyCutover"),
				FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			FPaths::NormalizeDirectoryName(Root);
			check(Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheLegacyCutover/")));
			ScriptRoot = Root / TEXT("Script");
			CacheRoot = Root / TEXT("CacheV2");
			check(IFileManager::Get().MakeDirectory(*ScriptRoot, true));
		}

		~FScopedProjectRoot()
		{
			if (Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheLegacyCutover/")))
			{
				IFileManager::Get().DeleteDirectory(*Root, false, true);
			}
		}

		bool WriteSourceAndLegacyArtifact() const
		{
			const bool bSourceWritten = FFileHelper::SaveStringToFile(
				TEXT(R"AS(
enum ELegacyCutoverState
{
	Ready = 1,
}

int ReadLegacyCutoverSource()
{
	return 801;
}
)AS"),
				*(ScriptRoot / TEXT("LegacyCutover.as")));
			const TArray<uint8> InvalidLegacyPayload = {
				0xde, 0xad, 0xbe, 0xef, 0x42, 0x13, 0x37};
			const bool bLegacyWritten = FFileHelper::SaveArrayToFile(
				InvalidLegacyPayload,
				*(ScriptRoot / TEXT("PrecompiledScript.Cache")));
			return bSourceWritten && bLegacyWritten;
		}

		FString Root;
		FString ScriptRoot;
		FString CacheRoot;
	};

	static TUniquePtr<FAngelscriptEngine> CreateEngine(
		const FScopedProjectRoot& Project)
	{
		FAngelscriptEngineConfig Config;
		Config.bIsEditor = true;
		Config.bDevelopmentMode = true;
		Config.bSkipThreadedInitialize = true;
		Config.CacheV2RootOverride = Project.CacheRoot;

		FAngelscriptEngineDependencies Dependencies =
			FAngelscriptEngineDependencies::CreateDefault();
		Dependencies.GetProjectDir = [&Project]()
		{
			return Project.Root;
		};
		Dependencies.GetEnabledPluginScriptRoots = []()
		{
			return TArray<FString>();
		};
		Dependencies.GetEnabledPluginScriptRootDescriptors = []()
		{
			return TArray<FAngelscriptPluginScriptRoot>();
		};
		return CreateScriptScanFreeFullEngineForTesting(Config, Dependencies);
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheLegacyCutoverTests,
	"Angelscript.TestModule.Cache.LegacyCutover",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(LegacyProductionSelectionFlagsAreAbsent)
	{
		using namespace AngelscriptCacheLegacyCutoverTests_Private;
		using FConfig = FAngelscriptEngineConfig;
		ASSERT_THAT(IsFalse(decltype(
			HasGeneratePrecompiledData<FConfig>(0))::value));
		ASSERT_THAT(IsFalse(decltype(
			HasIgnorePrecompiledData<FConfig>(0))::value));
		ASSERT_THAT(IsFalse(decltype(
			HasSkipStaticJITCodeGen<FConfig>(0))::value));
	}

	TEST_METHOD(LegacyNamesAreRejectedByMetadataOnlyInspection)
	{
		const FString Root = TEXT("D:/Fixture/Script");
		TSet<FString> Existing = {
			Root / TEXT("PrecompiledScript.Cache"),
			Root / TEXT("PrecompiledScript_Development.Cache"),
			Root / TEXT("PrecompiledScript_Test.Cache"),
			Root / TEXT("PrecompiledScript_Shipping.Cache")};
		TArray<FString> Queries;
		const FAngelscriptLegacyCacheInspection Inspection =
			InspectAngelscriptLegacyCacheArtifacts(
				MakeArrayView(&Root, 1),
				[&Existing, &Queries](const FString& Candidate)
				{
					Queries.Add(Candidate);
					return Existing.Contains(Candidate);
				});

		ASSERT_THAT(IsTrue(Inspection.HasRejectedLegacyScriptCache()));
		ASSERT_THAT(AreEqual(4, Inspection.RejectedArtifacts.Num()));
		ASSERT_THAT(AreEqual(5, Queries.Num()));
		ASSERT_THAT(AreEqual(
			EAngelscriptLegacyCacheArtifactKind::PrecompiledScript,
			Inspection.RejectedArtifacts[0].Kind));
		ASSERT_THAT(IsTrue(Inspection.FormatDiagnostic().Contains(
			TEXT("Cache V2 never reads or migrates these files"))));
	}

	TEST_METHOD(BindsCacheRemainsAcceptedAndIndependent)
	{
		const FString Root = TEXT("D:/Fixture/Script");
		const FString BindsPath = Root / TEXT("Binds.Cache");
		const FAngelscriptLegacyCacheInspection Inspection =
			InspectAngelscriptLegacyCacheArtifacts(
				MakeArrayView(&Root, 1),
				[&BindsPath](const FString& Candidate)
				{
					return Candidate == BindsPath;
				});

		ASSERT_THAT(IsFalse(Inspection.HasRejectedLegacyScriptCache()));
		ASSERT_THAT(AreEqual(0, Inspection.RejectedArtifacts.Num()));
		ASSERT_THAT(AreEqual(1, Inspection.AcceptedBindCachePaths.Num()));
		ASSERT_THAT(AreEqual(BindsPath,
			Inspection.AcceptedBindCachePaths[0]));
	}

	TEST_METHOD(InvalidLegacyPayloadCannotReplaceAuthoritativeSource)
	{
		using namespace AngelscriptCacheLegacyCutoverTests_Private;
		FScopedProjectRoot Project;
		ASSERT_THAT(IsTrue(Project.WriteSourceAndLegacyArtifact()));

		const FAngelscriptLegacyCacheInspection Inspection =
			InspectAngelscriptLegacyCacheArtifactsFromDisk(
				MakeArrayView(&Project.ScriptRoot, 1));
		ASSERT_THAT(IsTrue(Inspection.HasRejectedLegacyScriptCache()));

		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(Project);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		FAngelscriptEngineScope Scope(*Engine);
		Engine->InitialCompile();

		const FAngelscriptCacheLifecyclePublications Publications =
			Engine->GetCacheService()->GetLifecyclePublications();
		ASSERT_THAT(IsTrue(Publications.Current.IsValid()));
		const TSharedPtr<const FAngelscriptCacheFunctionRouteSnapshot,
			ESPMode::ThreadSafe> Routes = Engine->GetFunctionRouteSnapshot();
		ASSERT_THAT(IsTrue(Routes.IsValid()));
		ASSERT_THAT(IsNotNull(Routes->FunctionRoutes.FindByPredicate(
			[](const FAngelscriptCacheLiveFunctionRoute& Route)
			{
				return Route.CanonicalDeclaration ==
					TEXT("int ReadLegacyCutoverSource()");
			})));
		TestRunner->AddInfo(FString::Printf(
			TEXT("[CacheV2][LegacyCutover] Rejected=%d CurrentTx=%llu Routes=%d Diagnostic=%s"),
			Inspection.RejectedArtifacts.Num(),
			Publications.Current->TransactionOrdinal,
			Routes->FunctionRoutes.Num(),
			*Inspection.FormatDiagnostic()));
	}
};

#endif
