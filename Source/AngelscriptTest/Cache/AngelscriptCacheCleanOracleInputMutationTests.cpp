#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheDecodedRecord.h"
#include "Cache/AngelscriptCacheSourceDiscovery.h"

#include "CQTest.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Preprocessor/AngelscriptPreprocessor.h"
#include "Shared/AngelscriptTestFixture.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheCleanOracleInputMutationTests_Private
{
	static const TCHAR* RelativeSourcePath = TEXT("CleanOracleInput.as");

	class FScopedDiskRoot final
	{
	public:
		FScopedDiskRoot()
		{
			Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("Automation/AngelscriptCacheCleanOracleInput"),
				FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			FPaths::NormalizeDirectoryName(Root);
			check(Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheCleanOracleInput/")));
			check(IFileManager::Get().MakeDirectory(*Root, true));
		}

		~FScopedDiskRoot()
		{
			if (Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheCleanOracleInput/")))
			{
				IFileManager::Get().DeleteDirectory(*Root, false, true);
			}
		}

		FString Root;
	};

	struct FCaptureCase
	{
		TOptional<int32> ExternalHookValue;
		TOptional<bool> ConditionalOption;
	};

	struct FCleanObservation
	{
		FAngelscriptCacheRecordId SourceIndexRecordId;
		FAngelscriptCacheRecordId ModuleInterfaceRecordId;
		FAngelscriptCacheRecordId TypeSchemaRecordId;
		FAngelscriptCacheRecordId ModuleStateRecordId;
		FAngelscriptCacheRecordId FunctionBodyRecordId;
		FAngelscriptCacheRecordId DebugSidecarRecordId;
		FAngelscriptCacheRecordId ModuleSnapshotRecordId;
		FAngelscriptCachedSourceIndex SourceIndex;
		FAngelscriptCachedFunctionBody FunctionBody;
		FAngelscriptCachedDebugSidecar DebugSidecar;
	};

	static FString MakeHookSource()
	{
		return TEXT(R"AS(
class FCachePayload
{
	int Count;
}

const int GCacheAnswer = 41;

int GetCacheAnswer()
{
	return CACHE_EXTERNAL_VALUE;
}
)AS");
	}

	static FString MakeConditionalOptionSource()
	{
		return TEXT(R"AS(
class FCachePayload
{
	int Count;
}

const int GCacheAnswer = 41;

#if CACHE_OPTION_ON
int GetCacheAnswer()
{
	return 8;
}
#else
int GetCacheAnswer()
{
	return 7;
}
#endif
)AS");
	}

	static bool WriteSource(
		const FString& AbsoluteFilename,
		const FString& Source)
	{
		return FFileHelper::SaveStringToFile(
			Source,
			*AbsoluteFilename,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	static FAngelscriptHash256 HashString(
		const FStringView Domain,
		const FStringView Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(Domain);
		Writer.WriteString(Value);
		return Writer.FinalizeHash();
	}

	static FAngelscriptCacheCleanCaptureOptions MakeCaptureOptions(
		const FCaptureCase& Case)
	{
		FAngelscriptCacheCleanCaptureOptions Options;
		FAngelscriptCompatibilityDescriptor Compatibility;
		Compatibility.CanonicalInputs = {
			TEXT("CacheV2CleanOracleInputMutationTest"),
			TEXT("VmExecutionCodec=2"),
		};
		Options.Compatibility =
			FAngelscriptArtifactIdentityBuilder::BuildCompatibilityKey(
				Compatibility);

		FAngelscriptContextDescriptor Context;
		Context.CanonicalInputs = {
			TEXT("SourceMount=Game"),
			TEXT("DebugSidecar=Enabled"),
		};
		Options.Context =
			FAngelscriptArtifactIdentityBuilder::BuildContextKey(Context);
		Options.Profile =
			FAngelscriptArtifactIdentityBuilder::BuildArtifactProfileKey(
				Options.Compatibility, Options.Context);
		Options.CanonicalCompileOptions = {
			TEXT("AutomaticImports=false"),
		};
		if (Case.ConditionalOption.IsSet())
		{
			Options.CanonicalCompileOptions.Add(FString::Printf(
				TEXT("Preprocessor:CACHE_OPTION_ON=%s"),
				Case.ConditionalOption.GetValue()
					? TEXT("true") : TEXT("false")));
		}
		return Options;
	}

	static FAngelscriptCacheProductionSourceDiscoveryConfig
	MakeDiscoveryConfig(
		const FAngelscriptCacheCleanCaptureOptions& Options,
		const FCaptureCase& Case)
	{
		FAngelscriptCacheProductionSourceDiscoveryConfig Config;
		Config.Profile = Options.Profile;
		Config.DiscoveryPolicyVersion = 1;
		Config.bObserveLegacyGlobalPreprocessHooks = false;
		Config.Options.Add({
			EAngelscriptCacheDirectOptionKind::Compiler,
			TEXT("AutomaticImports"),
			TEXT("false")});
		if (Case.ConditionalOption.IsSet())
		{
			Config.Options.Add({
				EAngelscriptCacheDirectOptionKind::Preprocessor,
				TEXT("CACHE_OPTION_ON"),
				Case.ConditionalOption.GetValue()
					? TEXT("true") : TEXT("false")});
		}
		return Config;
	}

	static FAngelscriptCachedPreprocessHook MakeHook(
		const FAngelscriptStableModuleKey& ModuleKey,
		const int32 ExternalValue)
	{
		FAngelscriptCachedPreprocessHook Hook;
		Hook.Phase = EAngelscriptCachedPreprocessHookPhase::PostProcessCode;
		Hook.CanonicalImplementationIdentity =
			TEXT("Angelscript.CacheV2Test.ExternalValueHook.V1");
		Hook.AffectedScopeKind =
			EAngelscriptCachedFastPathScopeKind::Module;
		Hook.AffectedScopeStableKey = ModuleKey.Hash;
		Hook.IdentityFingerprint = HashString(
			TEXT("cache-v2-test-hook-identity"),
			Hook.CanonicalImplementationIdentity);
		Hook.VersionFingerprint = HashString(
			TEXT("cache-v2-test-hook-version"), TEXT("1"));
		Hook.ConfigurationFingerprint = HashString(
			TEXT("cache-v2-test-hook-external-value"),
			LexToString(ExternalValue));
		Hook.ContentFingerprint = HashString(
			TEXT("cache-v2-test-hook-content"),
			TEXT("replace-CACHE_EXTERNAL_VALUE-v1"));
		Hook.CapabilityFlags = static_cast<uint32>(
			EAngelscriptCachedFingerprintCapabilityFlags::KnownMask);
		check(FAngelscriptCacheSemanticArchive::TryBuildPreprocessHookKey(
			{Hook.Phase,
				Hook.CanonicalImplementationIdentity,
				Hook.AffectedScopeKind,
				Hook.AffectedScopeStableKey},
			Hook.HookKey).IsSuccess());
		return Hook;
	}

	static bool Discover(
		FAutomationTestBase& Test,
		const FString& SourceRoot,
		const FAngelscriptCacheProductionSourceDiscoveryConfig& Config,
		FAngelscriptCacheProductionSourceDiscoveryResult& OutDiscovery)
	{
		FAngelscriptDiskSourceProvider Provider;
		const TArray<FAngelscriptSourceRoot> Roots{
			FAngelscriptSourceRoot::FromGameRoot(SourceRoot)};
		const FAngelscriptCacheSourceDiscoveryStatus Status =
			FAngelscriptCacheSourceDiscovery::DiscoverProductionSources(
				Provider, Roots, false, false, Config, {}, OutDiscovery);
		Test.AddInfo(FString::Printf(
			TEXT("Clean-oracle input discovery: Success=%d Sources=%d Modules=%d Hooks=%d Options=%d Direct=%s Detail=%s"),
			Status.IsSuccess() ? 1 : 0,
			OutDiscovery.LoadedSourceCount,
			OutDiscovery.Modules.Num(),
			OutDiscovery.DirectPlan.DirectProjection.PreprocessHooks.Num(),
			OutDiscovery.DirectPlan.DirectProjection.DiscoveryPolicy.Options.Num(),
			*OutDiscovery.DirectPlan.DirectInputDigest.ToHexString(),
			*Status.Detail));
		return Status.IsSuccess();
	}

	static bool BuildAuthoritativeSourceIndex(
		FAutomationTestBase& Test,
		const FString& SourceRoot,
		const FAngelscriptCacheCleanCaptureOptions& Options,
		const FCaptureCase& Case,
		FAngelscriptCachedSourceIndex& OutSourceIndex)
	{
		FAngelscriptCacheProductionSourceDiscoveryConfig Config =
			MakeDiscoveryConfig(Options, Case);
		if (Case.ExternalHookValue.IsSet())
		{
			FAngelscriptCacheProductionSourceDiscoveryResult Bootstrap;
			if (!Discover(Test, SourceRoot, Config, Bootstrap)
				|| Bootstrap.Modules.Num() != 1)
			{
				Test.AddError(TEXT("Failed to bootstrap the input-hook module identity"));
				return false;
			}
			Config.PreprocessHooks.Add(MakeHook(
				Bootstrap.Modules[0].ModuleKey,
				Case.ExternalHookValue.GetValue()));
		}

		FAngelscriptCacheProductionSourceDiscoveryResult Discovery;
		if (!Discover(Test, SourceRoot, Config, Discovery)
			|| Discovery.Modules.Num() != 1
			|| Discovery.CurrentSources.Num() != 1)
		{
			Test.AddError(TEXT("Production discovery did not produce one input-oracle module"));
			return false;
		}
		const FAngelscriptCacheValidationResult CandidateResult =
			FAngelscriptCacheSourcePlanner::BuildPersistedDependencyCandidate(
				Discovery.DirectPlan,
				Options.Profile,
				{},
				{},
				{},
				OutSourceIndex);
		if (!CandidateResult.IsSuccess())
		{
			Test.AddError(FString::Printf(
				TEXT("Input-oracle SourceIndex candidate failed: Error=%u"),
				static_cast<uint32>(CandidateResult.Error)));
			return false;
		}
		return true;
	}

	static bool IsHandled(const ECompileResult Result)
	{
		return Result == ECompileResult::FullyHandled
			|| Result == ECompileResult::PartiallyHandled;
	}

	static TSharedPtr<FAngelscriptModuleDesc> CompileDiskModule(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const FString& AbsoluteFilename,
		const FCaptureCase& Case)
	{
		int32 ReplacementCount = 0;
		FDelegateHandle HookHandle;
		if (Case.ExternalHookValue.IsSet())
		{
			const FString Replacement =
				LexToString(Case.ExternalHookValue.GetValue());
			HookHandle = FAngelscriptPreprocessor::OnPostProcessCode.AddLambda(
				[Replacement, &ReplacementCount](
					FAngelscriptPreprocessor& Preprocessor)
				{
					for (FAngelscriptPreprocessor::FFile& File
						: Preprocessor.Files)
					{
						ReplacementCount += File.ProcessedCode.ReplaceInline(
							TEXT("CACHE_EXTERNAL_VALUE"),
							*Replacement,
							ESearchCase::CaseSensitive);
					}
				});
		}
		ON_SCOPE_EXIT
		{
			if (HookHandle.IsValid())
			{
				FAngelscriptPreprocessor::OnPostProcessCode.Remove(HookHandle);
			}
		};

		FAngelscriptPreprocessor Preprocessor;
		if (Case.ConditionalOption.IsSet())
		{
			Preprocessor.PreprocessorFlags.Add(
				TEXT("CACHE_OPTION_ON"),
				Case.ConditionalOption.GetValue());
		}
		Preprocessor.AddFile(RelativeSourcePath, AbsoluteFilename);
		if (!Preprocessor.Preprocess())
		{
			Test.AddError(TEXT("Input-oracle preprocessing failed"));
			return {};
		}
		if (Case.ExternalHookValue.IsSet() && ReplacementCount != 1)
		{
			Test.AddError(FString::Printf(
				TEXT("Input hook replaced %d tokens instead of exactly one"),
				ReplacementCount));
			return {};
		}

		TArray<TSharedRef<FAngelscriptModuleDesc>> Modules =
			Preprocessor.GetModulesToCompile();
		if (Modules.Num() != 1)
		{
			Test.AddError(FString::Printf(
				TEXT("Input-oracle expected one module, got %d"),
				Modules.Num()));
			return {};
		}
		TArray<TSharedRef<FAngelscriptModuleDesc>> CompiledModules;
		TGuardValue<bool> AutomaticImportGuard(
			Engine.bUseAutomaticImportMethod, false);
		FScopedAutomaticImportsOverride AutomaticImportsOverride(
			Engine.GetScriptEngine());
		const ECompileResult Result = Engine.CompileModules(
			ECompileType::Initial,
			Modules,
			CompiledModules,
			{});
		Test.AddInfo(FString::Printf(
			TEXT("Clean-oracle input compile: Result=%u Modules=%d HookValue=%s Conditional=%s"),
			static_cast<uint32>(Result),
			CompiledModules.Num(),
			Case.ExternalHookValue.IsSet()
				? *LexToString(Case.ExternalHookValue.GetValue()) : TEXT("none"),
			Case.ConditionalOption.IsSet()
				? (Case.ConditionalOption.GetValue() ? TEXT("true") : TEXT("false"))
				: TEXT("none")));
		if (!IsHandled(Result) || CompiledModules.Num() != 1)
		{
			return {};
		}
		return CompiledModules[0];
	}

	static const FAngelscriptPreparedRecord* FindRecord(
		const FAngelscriptCacheCleanModuleArtifacts& Artifacts,
		const EAngelscriptCacheRecordKind Kind)
	{
		return Artifacts.Records.FindByPredicate(
			[Kind](const FAngelscriptPreparedRecord& Record)
			{
				return Record.RecordId.Kind == Kind;
			});
	}

	static bool DecodeRecord(
		FAutomationTestBase& Test,
		FAngelscriptDecodedCacheRecordBatch& Batch,
		const FAngelscriptPreparedRecord* Prepared,
		TOptional<FAngelscriptDecodedCacheRecordHandle>& OutDecoded)
	{
		OutDecoded.Reset();
		if (Prepared == nullptr)
		{
			Test.AddError(TEXT("A required input-oracle record is absent"));
			return false;
		}
		const FAngelscriptCacheValidationResult Result = Batch.TryDecode(
			Prepared->RecordId, Prepared->CanonicalPayload, OutDecoded);
		if (!Result.IsSuccess() || !OutDecoded.IsSet())
		{
			Test.AddError(FString::Printf(
				TEXT("Input-oracle record decode failed: Kind=%u Error=%u Stage=%u Offset=%llu"),
				static_cast<uint32>(Prepared->RecordId.Kind),
				static_cast<uint32>(Result.Error),
				static_cast<uint32>(Result.Stage),
				Result.ByteOffset));
			return false;
		}
		return true;
	}

	static bool ObserveArtifacts(
		FAutomationTestBase& Test,
		const FAngelscriptCacheCleanModuleArtifacts& Artifacts,
		const FStringView TargetFunctionName,
		FCleanObservation& OutObservation)
	{
		const FAngelscriptPreparedRecord* Source = FindRecord(
			Artifacts, EAngelscriptCacheRecordKind::SourceIndex);
		const FAngelscriptPreparedRecord* Interface = FindRecord(
			Artifacts, EAngelscriptCacheRecordKind::ModuleInterface);
		const FAngelscriptPreparedRecord* Type = FindRecord(
			Artifacts, EAngelscriptCacheRecordKind::TypeSchema);
		const FAngelscriptPreparedRecord* State = FindRecord(
			Artifacts, EAngelscriptCacheRecordKind::ModuleState);
		const FAngelscriptPreparedRecord* Snapshot = FindRecord(
			Artifacts, EAngelscriptCacheRecordKind::ModuleSnapshot);
		if (Source == nullptr || Interface == nullptr || Type == nullptr
			|| State == nullptr || Snapshot == nullptr)
		{
			Test.AddError(TEXT("Input-oracle output is missing a required singleton record"));
			return false;
		}

		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		FAngelscriptDecodedCacheRecordBatch Batch(Budget, Limits);
		TOptional<FAngelscriptDecodedCacheRecordHandle> DecodedSource;
		TOptional<FAngelscriptDecodedCacheRecordHandle> DecodedInterface;
		if (!DecodeRecord(Test, Batch, Source, DecodedSource)
			|| !DecodeRecord(Test, Batch, Interface, DecodedInterface))
		{
			return false;
		}
		const FAngelscriptCachedSourceIndex* SourceValue =
			DecodedSource.GetValue()->TryGetSourceIndex();
		const FAngelscriptCachedModuleInterface* InterfaceValue =
			DecodedInterface.GetValue()->TryGetModuleInterface();
		if (SourceValue == nullptr || InterfaceValue == nullptr)
		{
			Test.AddError(TEXT("Input-oracle record decoded as the wrong kind"));
			return false;
		}

		const FAngelscriptCachedDeclaration* TargetDeclaration = nullptr;
		int32 TargetDeclarationCount = 0;
		for (const FAngelscriptCachedDeclaration& Declaration
			: InterfaceValue->Declarations)
		{
			if (Declaration.DeclarationKind
					== EAngelscriptCacheDeclarationKind::Function
				&& Declaration.CanonicalName == TargetFunctionName)
			{
				TargetDeclaration = &Declaration;
				++TargetDeclarationCount;
			}
		}
		if (TargetDeclarationCount != 1 || TargetDeclaration == nullptr)
		{
			Test.AddError(FString::Printf(
				TEXT("Input-oracle expected one target function declaration named %.*s, found %d"),
				TargetFunctionName.Len(), TargetFunctionName.GetData(),
				TargetDeclarationCount));
			return false;
		}

		int32 FunctionMatchCount = 0;
		int32 DebugMatchCount = 0;
		for (const FAngelscriptPreparedRecord& Record : Artifacts.Records)
		{
			if (Record.RecordId.Kind != EAngelscriptCacheRecordKind::FunctionBody
				&& Record.RecordId.Kind
					!= EAngelscriptCacheRecordKind::DebugSidecar)
			{
				continue;
			}
			TOptional<FAngelscriptDecodedCacheRecordHandle> Decoded;
			if (!DecodeRecord(Test, Batch, &Record, Decoded))
			{
				return false;
			}
			if (const FAngelscriptCachedFunctionBody* FunctionValue =
				Decoded.GetValue()->TryGetFunctionBody())
			{
				if (FunctionValue->Identity.FunctionKey.Hash
					== TargetDeclaration->StableKey)
				{
					++FunctionMatchCount;
					OutObservation.FunctionBodyRecordId = Record.RecordId;
					OutObservation.FunctionBody = *FunctionValue;
				}
			}
			else if (const FAngelscriptCachedDebugSidecar* DebugValue =
				Decoded.GetValue()->TryGetDebugSidecar())
			{
				if (DebugValue->FunctionKey.Hash
					== TargetDeclaration->StableKey)
				{
					++DebugMatchCount;
					OutObservation.DebugSidecarRecordId = Record.RecordId;
					OutObservation.DebugSidecar = *DebugValue;
				}
			}
		}
		if (FunctionMatchCount != 1 || DebugMatchCount != 1)
		{
			Test.AddError(FString::Printf(
				TEXT("Input-oracle target %.*s resolved to %d bodies and %d debug sidecars"),
				TargetFunctionName.Len(), TargetFunctionName.GetData(),
				FunctionMatchCount, DebugMatchCount));
			return false;
		}

		OutObservation.SourceIndexRecordId = Source->RecordId;
		OutObservation.ModuleInterfaceRecordId = Interface->RecordId;
		OutObservation.TypeSchemaRecordId = Type->RecordId;
		OutObservation.ModuleStateRecordId = State->RecordId;
		OutObservation.ModuleSnapshotRecordId = Snapshot->RecordId;
		OutObservation.SourceIndex = *SourceValue;
		return true;
	}

	static bool CaptureObservation(
		FAutomationTestBase& Test,
		const FString& SourceRoot,
		const FCaptureCase& Case,
		const FString& Label,
		FCleanObservation& OutObservation)
	{
		const FAngelscriptCacheCleanCaptureOptions Options =
			MakeCaptureOptions(Case);
		FAngelscriptCachedSourceIndex SourceIndex;
		if (!BuildAuthoritativeSourceIndex(
			Test, SourceRoot, Options, Case, SourceIndex))
		{
			return false;
		}

		FAngelscriptTestFixture Fixture(Test, ETestEngineMode::IsolatedFull);
		if (!Fixture.IsValid())
		{
			Test.AddError(TEXT("Failed to create the input-oracle isolated Engine"));
			return false;
		}
		const TSharedPtr<FAngelscriptModuleDesc> Module = CompileDiskModule(
			Test,
			Fixture.GetEngine(),
			SourceRoot / RelativeSourcePath,
			Case);
		if (!Module.IsValid())
		{
			return false;
		}

		FAngelscriptCacheCleanModuleArtifacts Artifacts;
		const FAngelscriptCacheCleanCaptureResult Capture =
			CaptureAngelscriptCleanCompiledModule(
				Module.ToSharedRef(), Options, SourceIndex, Artifacts);
		Test.AddInfo(FString::Printf(
			TEXT("Clean-oracle input capture: Label=%s Error=%u Records=%d Graph=%u SourceSnapshot=%s Detail=%s"),
			*Label,
			static_cast<uint32>(Capture.Error),
			Artifacts.Records.Num(),
			Capture.ValidatedGraphRecordCount,
			*Artifacts.SourceSnapshot.ToHexString(),
			*Capture.Detail));
		if (!Capture.IsSuccess()
			|| !ObserveArtifacts(
				Test, Artifacts, TEXT("GetCacheAnswer"), OutObservation))
		{
			return false;
		}
		Test.AddInfo(FString::Printf(
			TEXT("Clean-oracle input %s records: Source=%s Interface=%s Type=%s State=%s Function=%s Debug=%s Snapshot=%s"),
			*Label,
			*OutObservation.SourceIndexRecordId.ContentHash.ToHexString(),
			*OutObservation.ModuleInterfaceRecordId.ContentHash.ToHexString(),
			*OutObservation.TypeSchemaRecordId.ContentHash.ToHexString(),
			*OutObservation.ModuleStateRecordId.ContentHash.ToHexString(),
			*OutObservation.FunctionBodyRecordId.ContentHash.ToHexString(),
			*OutObservation.DebugSidecarRecordId.ContentHash.ToHexString(),
			*OutObservation.ModuleSnapshotRecordId.ContentHash.ToHexString()));
		Test.AddInfo(FString::Printf(
			TEXT("Clean-oracle input %s function: Key=%s Abi=%s Source=%s Input=%s Execution=%s Debug=%s"),
			*Label,
			*OutObservation.FunctionBody.Identity.FunctionKey.Hash.ToHexString(),
			*OutObservation.FunctionBody.ExpectedDeclarationAbi.ToHexString(),
			*OutObservation.FunctionBody.FunctionSourceDigest.Hash.ToHexString(),
			*OutObservation.FunctionBody.FunctionInputDigest.Hash.ToHexString(),
			*OutObservation.FunctionBody.Identity.Content.Execution.ToHexString(),
			*OutObservation.DebugSidecar.DebugHash.ToHexString()));
		return true;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheCleanOracleInputMutationTests,
	"Angelscript.TestModule.Cache.CleanOracleInputMutation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(FingerprintedPreprocessInputChangesCompiledContentWithOwnerSourceStable)
	{
		using namespace AngelscriptCacheCleanOracleInputMutationTests_Private;
		FScopedDiskRoot Disk;
		const FString AbsoluteFilename = Disk.Root / RelativeSourcePath;
		ASSERT_THAT(IsTrue(WriteSource(AbsoluteFilename, MakeHookSource())));

		FCleanObservation Baseline;
		FCleanObservation Current;
		ASSERT_THAT(IsTrue(CaptureObservation(
			*TestRunner,
			Disk.Root,
			FCaptureCase{7, {}},
			TEXT("hook-input-baseline"),
			Baseline)));
		ASSERT_THAT(IsTrue(CaptureObservation(
			*TestRunner,
			Disk.Root,
			FCaptureCase{8, {}},
			TEXT("hook-input-current"),
			Current)));

		ASSERT_THAT(AreEqual(1, Baseline.SourceIndex.Files.Num()));
		ASSERT_THAT(AreEqual(1, Current.SourceIndex.Files.Num()));
		ASSERT_THAT(AreEqual(1, Baseline.SourceIndex.PreprocessHooks.Num()));
		ASSERT_THAT(AreEqual(1, Current.SourceIndex.PreprocessHooks.Num()));
		ASSERT_THAT(IsTrue(Baseline.SourceIndex.Files[0].RawContentHash
			== Current.SourceIndex.Files[0].RawContentHash));
		ASSERT_THAT(IsTrue(Baseline.SourceIndex.PreprocessHooks[0].HookKey.Hash
			== Current.SourceIndex.PreprocessHooks[0].HookKey.Hash));
		ASSERT_THAT(IsFalse(
			Baseline.SourceIndex.PreprocessHooks[0].ConfigurationFingerprint
			== Current.SourceIndex.PreprocessHooks[0].ConfigurationFingerprint));
		ASSERT_THAT(IsTrue(
			Baseline.SourceIndex.PreprocessHooks[0].ContentFingerprint
			== Current.SourceIndex.PreprocessHooks[0].ContentFingerprint));

		ASSERT_THAT(IsFalse(Baseline.SourceIndexRecordId
			== Current.SourceIndexRecordId));
		ASSERT_THAT(IsTrue(Baseline.ModuleInterfaceRecordId
			== Current.ModuleInterfaceRecordId));
		ASSERT_THAT(IsTrue(Baseline.TypeSchemaRecordId
			== Current.TypeSchemaRecordId));
		ASSERT_THAT(IsTrue(Baseline.ModuleStateRecordId
			== Current.ModuleStateRecordId));
		ASSERT_THAT(IsFalse(Baseline.FunctionBodyRecordId
			== Current.FunctionBodyRecordId));
		ASSERT_THAT(IsTrue(Baseline.DebugSidecarRecordId
			== Current.DebugSidecarRecordId));
		ASSERT_THAT(IsFalse(Baseline.ModuleSnapshotRecordId
			== Current.ModuleSnapshotRecordId));
		ASSERT_THAT(IsTrue(Baseline.FunctionBody.Identity.FunctionKey
			== Current.FunctionBody.Identity.FunctionKey));
		ASSERT_THAT(IsTrue(Baseline.FunctionBody.ExpectedDeclarationAbi
			== Current.FunctionBody.ExpectedDeclarationAbi));
		ASSERT_THAT(IsFalse(Baseline.FunctionBody.FunctionSourceDigest.Hash
			== Current.FunctionBody.FunctionSourceDigest.Hash));
		ASSERT_THAT(IsFalse(Baseline.FunctionBody.FunctionInputDigest.Hash
			== Current.FunctionBody.FunctionInputDigest.Hash));
		ASSERT_THAT(IsFalse(Baseline.FunctionBody.Identity.Content.Execution
			== Current.FunctionBody.Identity.Content.Execution));
	}

	TEST_METHOD(CanonicalPreprocessorOptionChangesTheRealCompileAndItsConsumers)
	{
		using namespace AngelscriptCacheCleanOracleInputMutationTests_Private;
		FScopedDiskRoot Disk;
		const FString AbsoluteFilename = Disk.Root / RelativeSourcePath;
		ASSERT_THAT(IsTrue(WriteSource(
			AbsoluteFilename, MakeConditionalOptionSource())));

		FCleanObservation Baseline;
		FCleanObservation Current;
		ASSERT_THAT(IsTrue(CaptureObservation(
			*TestRunner,
			Disk.Root,
			FCaptureCase{{}, false},
			TEXT("compile-option-baseline"),
			Baseline)));
		ASSERT_THAT(IsTrue(CaptureObservation(
			*TestRunner,
			Disk.Root,
			FCaptureCase{{}, true},
			TEXT("compile-option-current"),
			Current)));

		ASSERT_THAT(AreEqual(1, Baseline.SourceIndex.Files.Num()));
		ASSERT_THAT(AreEqual(1, Current.SourceIndex.Files.Num()));
		ASSERT_THAT(AreEqual(2,
			Baseline.SourceIndex.DiscoveryPolicy.Options.Num()));
		ASSERT_THAT(AreEqual(2,
			Current.SourceIndex.DiscoveryPolicy.Options.Num()));
		ASSERT_THAT(IsTrue(Baseline.SourceIndex.Files[0].RawContentHash
			== Current.SourceIndex.Files[0].RawContentHash));
		ASSERT_THAT(IsFalse(
			Baseline.SourceIndex.DiscoveryPolicy.Options[1].ValueFingerprint
			== Current.SourceIndex.DiscoveryPolicy.Options[1].ValueFingerprint));

		ASSERT_THAT(IsFalse(Baseline.SourceIndexRecordId
			== Current.SourceIndexRecordId));
		ASSERT_THAT(IsTrue(Baseline.ModuleInterfaceRecordId
			== Current.ModuleInterfaceRecordId));
		ASSERT_THAT(IsTrue(Baseline.TypeSchemaRecordId
			== Current.TypeSchemaRecordId));
		ASSERT_THAT(IsTrue(Baseline.ModuleStateRecordId
			== Current.ModuleStateRecordId));
		ASSERT_THAT(IsFalse(Baseline.FunctionBodyRecordId
			== Current.FunctionBodyRecordId));
		ASSERT_THAT(IsFalse(Baseline.ModuleSnapshotRecordId
			== Current.ModuleSnapshotRecordId));
		ASSERT_THAT(IsTrue(Baseline.FunctionBody.Identity.FunctionKey
			== Current.FunctionBody.Identity.FunctionKey));
		ASSERT_THAT(IsTrue(Baseline.FunctionBody.ExpectedDeclarationAbi
			== Current.FunctionBody.ExpectedDeclarationAbi));
		ASSERT_THAT(IsFalse(Baseline.FunctionBody.FunctionSourceDigest.Hash
			== Current.FunctionBody.FunctionSourceDigest.Hash));
		ASSERT_THAT(IsFalse(Baseline.FunctionBody.FunctionInputDigest.Hash
			== Current.FunctionBody.FunctionInputDigest.Hash));
		ASSERT_THAT(IsFalse(Baseline.FunctionBody.Identity.Content.Execution
			== Current.FunctionBody.Identity.Content.Execution));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
