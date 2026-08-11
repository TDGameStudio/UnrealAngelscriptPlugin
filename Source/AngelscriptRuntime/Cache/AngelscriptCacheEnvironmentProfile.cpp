#include "Cache/AngelscriptCacheEnvironmentProfile.h"

#include "Cache/AngelscriptCacheArchive.h"
#include "Cache/AngelscriptCacheManifestPack.h"
#include "Cache/AngelscriptCacheRemainingRecordTypes.h"
#include "Cache/AngelscriptCacheTypeSchema.h"
#include "Cache/AngelscriptFunctionArtifactCodec.h"
#include "Core/AngelscriptEngine.h"
#include "Core/AngelscriptInclude.h"
#include "Core/UnrealAngelscriptVersion.h"
#include "Preprocessor/AngelscriptPreprocessor.h"

#include "GenericPlatform/GenericPlatformProperties.h"

namespace AngelscriptCacheEnvironmentProfile_Private
{
	static FString BoolText(const bool bValue)
	{
		return bValue ? TEXT("true") : TEXT("false");
	}

	static void AddCompileOption(
		FAngelscriptCacheEnvironmentProfile& Profile,
		const EAngelscriptCacheDirectOptionKind Kind,
		const FStringView Key,
		const FStringView Value)
	{
		FAngelscriptCacheDirectOptionInput& Option =
			Profile.DiscoveryConfig.Options.AddDefaulted_GetRef();
		Option.Kind = Kind;
		Option.CanonicalKey = FString(Key);
		Option.CanonicalValue = FString(Value);
		Profile.CaptureOptions.CanonicalCompileOptions.Add(FString::Printf(
			TEXT("%u:%s=%s"), static_cast<uint32>(Kind),
			*Option.CanonicalKey, *Option.CanonicalValue));
	}

	static FString ArchitectureText()
	{
#if PLATFORM_CPU_X86_FAMILY
		return TEXT("x86-family");
#elif PLATFORM_CPU_ARM_FAMILY
		return TEXT("arm-family");
#else
		return TEXT("other");
#endif
	}

	static FString BuildConfigurationText()
	{
#if UE_BUILD_SHIPPING
		return TEXT("Shipping");
#elif UE_BUILD_TEST
		return TEXT("Test");
#elif UE_BUILD_DEBUG
		return TEXT("Debug");
#else
		return TEXT("Development");
#endif
	}

	static FAngelscriptCacheEnvironmentProfileResult Failure(
		const EAngelscriptCacheEnvironmentProfileError Error,
		FString Detail)
	{
		FAngelscriptCacheEnvironmentProfileResult Result;
		Result.Error = Error;
		Result.Detail = MoveTemp(Detail);
		return Result;
	}
}

FAngelscriptCacheEnvironmentProfileResult
BuildAngelscriptCacheEnvironmentProfile(
	const FAngelscriptEngine& Engine,
	const FAngelscriptPreprocessorContext& PreprocessorContext,
	const TConstArrayView<FAngelscriptSourceRoot> ScriptRoots,
	FAngelscriptCacheEnvironmentProfile& OutProfile)
{
	using namespace AngelscriptCacheEnvironmentProfile_Private;
	OutProfile = {};
	const asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
	if (ScriptEngine == nullptr)
	{
		return Failure(
			EAngelscriptCacheEnvironmentProfileError::InvalidEngine,
			TEXT("Cache environment profile requires an initialized AngelScript Engine"));
	}

	FAngelscriptCompatibilityDescriptor Compatibility;
	Compatibility.CanonicalInputs = {
		TEXT("CacheIdentitySchema=1"),
		FString::Printf(TEXT("CacheArchiveSchema=%u"),
			FAngelscriptCacheRecordArchive::ArchiveSchemaVersion),
		FString::Printf(TEXT("CachePackSchema=%u"),
			FAngelscriptCacheManifestPackArchive::PackSchemaVersion),
		FString::Printf(TEXT("CacheManifestSchema=%u"),
			FAngelscriptCacheManifestPackArchive::ManifestSchemaVersion),
		FString::Printf(TEXT("SourceIndexSchema=%u"),
			FAngelscriptCacheSemanticArchive::SourceIndexPayloadSchemaVersion),
		FString::Printf(TEXT("ModuleInterfaceSchema=%u"),
			FAngelscriptCacheSemanticArchive::ModuleInterfacePayloadSchemaVersion),
		FString::Printf(TEXT("TypeSchemaSchema=%u"),
			FAngelscriptCacheTypeSchemaArchive::TypeSchemaPayloadSchemaVersion),
		FString::Printf(TEXT("ModuleStateSchema=%u"),
			FAngelscriptCacheRemainingRecordArchive::ModuleStatePayloadSchemaVersion),
		FString::Printf(TEXT("FunctionBodySchema=%u"),
			FAngelscriptCacheRemainingRecordArchive::FunctionBodyPayloadSchemaVersion),
		FString::Printf(TEXT("DebugSidecarSchema=%u"),
			FAngelscriptCacheRemainingRecordArchive::DebugSidecarPayloadSchemaVersion),
		FString::Printf(TEXT("ModuleSnapshotSchema=%u"),
			FAngelscriptCacheRemainingRecordArchive::ModuleSnapshotPayloadSchemaVersion),
		FString::Printf(TEXT("VmExecutionCodec=%u"),
			FAngelscriptFunctionArtifactCodec::ExecutionCodecVersion),
		FString::Printf(TEXT("VmDebugCodec=%u"),
			FAngelscriptFunctionArtifactCodec::DebugCodecVersion),
		FString::Printf(TEXT("UnrealAngelscriptProduct=%s"),
			UTF8_TO_TCHAR(UNREAL_ANGELSCRIPT_VERSION_STRING)),
		TEXT("AngelScriptForkLineage=2.33-WIP-selective-2.38"),
		FString::Printf(TEXT("UE=%u.%u"),
			static_cast<uint32>(ENGINE_MAJOR_VERSION),
			static_cast<uint32>(ENGINE_MINOR_VERSION)),
		FString::Printf(TEXT("Platform=%s"),
			UTF8_TO_TCHAR(FPlatformProperties::PlatformName())),
		FString::Printf(TEXT("Architecture=%s"), *ArchitectureText()),
		FString::Printf(TEXT("PointerBits=%u"),
			static_cast<uint32>(sizeof(void*) * 8)),
		FString::Printf(TEXT("LittleEndian=%s"),
			*BoolText(PLATFORM_LITTLE_ENDIAN != 0)),
	};
	OutProfile.CaptureOptions.Compatibility =
		FAngelscriptArtifactIdentityBuilder::BuildCompatibilityKey(
			Compatibility);

	AddCompileOption(OutProfile,
		EAngelscriptCacheDirectOptionKind::Compiler,
		TEXT("AutomaticImports"),
		BoolText(PreprocessorContext.bUseAutomaticImportMethod));
	AddCompileOption(OutProfile,
		EAngelscriptCacheDirectOptionKind::Compiler,
		TEXT("WarnOnManualImportStatements"),
		BoolText(PreprocessorContext.bWarnOnManualImportStatements));
	AddCompileOption(OutProfile,
		EAngelscriptCacheDirectOptionKind::Compiler,
		TEXT("DefaultFunctionBlueprintCallable"),
		BoolText(PreprocessorContext.bDefaultFunctionBlueprintCallable));
	AddCompileOption(OutProfile,
		EAngelscriptCacheDirectOptionKind::Compiler,
		TEXT("DefaultPropertyEditSpecifier"),
		LexToString(static_cast<uint8>(
			PreprocessorContext.DefaultPropertyEditSpecifier)));
	AddCompileOption(OutProfile,
		EAngelscriptCacheDirectOptionKind::Compiler,
		TEXT("DefaultPropertyEditSpecifierForStructs"),
		LexToString(static_cast<uint8>(
			PreprocessorContext.DefaultPropertyEditSpecifierForStructs)));
	AddCompileOption(OutProfile,
		EAngelscriptCacheDirectOptionKind::Compiler,
		TEXT("DefaultPropertyBlueprintSpecifier"),
		LexToString(static_cast<uint8>(
			PreprocessorContext.DefaultPropertyBlueprintSpecifier)));
	AddCompileOption(OutProfile,
		EAngelscriptCacheDirectOptionKind::Compiler,
		TEXT("StaticClassDeprecation"),
		LexToString(static_cast<uint8>(
			PreprocessorContext.StaticClassDeprecation)));
	AddCompileOption(OutProfile,
		EAngelscriptCacheDirectOptionKind::Compiler,
		TEXT("ScriptFloatIsFloat64"),
		BoolText(PreprocessorContext.bScriptFloatIsFloat64));

	TArray<FString> FlagNames;
	PreprocessorContext.PreprocessorFlags.GetKeys(FlagNames);
	FlagNames.Sort([](const FString& Left, const FString& Right)
	{
		return FAngelscriptArtifactCanonicalWriter::
			CompareCanonicalUtf8Strings(Left, Right) < 0;
	});
	for (const FString& FlagName : FlagNames)
	{
		AddCompileOption(OutProfile,
			EAngelscriptCacheDirectOptionKind::Preprocessor,
			FlagName,
			BoolText(PreprocessorContext.PreprocessorFlags.FindRef(FlagName)));
	}
	OutProfile.CaptureOptions.CanonicalCompileOptions.Sort(
		[](const FString& Left, const FString& Right)
		{
			return FAngelscriptArtifactCanonicalWriter::
				CompareCanonicalUtf8Strings(Left, Right) < 0;
		});

	FAngelscriptContextDescriptor Context;
	Context.CanonicalInputs = OutProfile.CaptureOptions.CanonicalCompileOptions;
	Context.CanonicalInputs.Add(FString::Printf(TEXT("Target=%s"),
		WITH_EDITOR ? TEXT("Editor") : TEXT("Game")));
	Context.CanonicalInputs.Add(FString::Printf(TEXT("Configuration=%s"),
		*BuildConfigurationText()));
	Context.CanonicalInputs.Add(TEXT("SourceProviderAuthority=SourceIndex.v1"));
	for (int32 Property = 1;
		Property < static_cast<int32>(asEP_LAST_PROPERTY); ++Property)
	{
		Context.CanonicalInputs.Add(FString::Printf(
			TEXT("EngineProperty.%d=%llu"), Property,
			static_cast<uint64>(ScriptEngine->GetEngineProperty(
				static_cast<asEEngineProp>(Property)))));
	}
	for (const FAngelscriptSourceRoot& Root : ScriptRoots)
	{
		if (Root.SourceKind == EAngelscriptSourceKind::Unknown
			|| (Root.SourceKind == EAngelscriptSourceKind::Plugin
				&& Root.MountName.IsEmpty()))
		{
			OutProfile = {};
			return Failure(
				EAngelscriptCacheEnvironmentProfileError::InvalidSourceRoot,
				TEXT("Cache environment profile received an invalid logical source root"));
		}
		Context.CanonicalInputs.Add(FString::Printf(
			TEXT("SourceMount.%u=%s"),
			static_cast<uint32>(Root.SourceKind), *Root.MountName));
	}
	OutProfile.CaptureOptions.Context =
		FAngelscriptArtifactIdentityBuilder::BuildContextKey(Context);
	OutProfile.CaptureOptions.Profile =
		FAngelscriptArtifactIdentityBuilder::BuildArtifactProfileKey(
			OutProfile.CaptureOptions.Compatibility,
			OutProfile.CaptureOptions.Context);
	OutProfile.DiscoveryConfig.Profile =
		OutProfile.CaptureOptions.Profile;
	OutProfile.DiscoveryConfig.DiscoveryPolicyVersion = 1;
	OutProfile.DiscoveryConfig.bObserveLegacyGlobalPreprocessHooks = true;

	FAngelscriptCacheEnvironmentProfileResult Result;
	Result.Detail = FString::Printf(
		TEXT("Built production Cache V2 profile with %d compile options and %d logical roots"),
		OutProfile.CaptureOptions.CanonicalCompileOptions.Num(),
		ScriptRoots.Num());
	return Result;
}

FAngelscriptCacheCompileCapturePreparationResult
PrepareAngelscriptCacheCompileCaptureContext(
	const FAngelscriptEngine& Engine,
	const FAngelscriptPreprocessorContext& PreprocessorContext,
	IAngelscriptSourceProvider& SourceProvider,
	const TConstArrayView<FAngelscriptSourceRoot> ScriptRoots,
	const bool bSkipDevelopmentScripts,
	const bool bSkipEditorScripts,
	FAngelscriptCacheCompileCaptureContext& OutContext)
{
	OutContext.Reset();
	FAngelscriptCacheCompileCapturePreparationResult Result;
	const FAngelscriptCacheEnvironmentProfileResult EnvironmentResult =
		BuildAngelscriptCacheEnvironmentProfile(
			Engine, PreprocessorContext, ScriptRoots,
			OutContext.Environment);
	if (!EnvironmentResult.IsSuccess())
	{
		OutContext.Reset();
		Result.Error = EAngelscriptCacheCompileCapturePreparationError::
			EnvironmentProfileFailed;
		Result.EnvironmentError = EnvironmentResult.Error;
		Result.Detail = EnvironmentResult.Detail;
		return Result;
	}

	FAngelscriptCacheProductionSourceDiscoveryResult Discovery;
	const FAngelscriptCacheSourceDiscoveryStatus DiscoveryStatus =
		FAngelscriptCacheSourceDiscovery::DiscoverProductionSources(
			SourceProvider,
			ScriptRoots,
			bSkipDevelopmentScripts,
			bSkipEditorScripts,
			OutContext.Environment.DiscoveryConfig,
			FAngelscriptCacheDirectSourceLimits{},
			Discovery);
	if (!DiscoveryStatus.IsSuccess())
	{
		OutContext.Reset();
		Result.Error = EAngelscriptCacheCompileCapturePreparationError::
			SourceDiscoveryFailed;
		Result.SourceDiscoveryError = DiscoveryStatus.Error;
		Result.Validation = DiscoveryStatus.Validation;
		Result.Detail = DiscoveryStatus.Detail;
		return Result;
	}

	const FAngelscriptCacheValidationResult CandidateResult =
		FAngelscriptCacheSourcePlanner::BuildPersistedDependencyCandidate(
			Discovery.DirectPlan,
			OutContext.Environment.CaptureOptions.Profile,
			TConstArrayView<FAngelscriptCachedPreprocessorInput>(),
			TConstArrayView<FAngelscriptCachedSourceEdge>(),
			FAngelscriptCacheDependencyCandidateLimits{},
			OutContext.AuthoritativeSourceIndex);
	if (!CandidateResult.IsSuccess())
	{
		OutContext.Reset();
		Result.Error = EAngelscriptCacheCompileCapturePreparationError::
			SourceCandidateFailed;
		Result.Validation = CandidateResult;
		Result.Detail = FString::Printf(
			TEXT("Cache V2 source candidate failed with validation error %u"),
			static_cast<uint32>(CandidateResult.Error));
		return Result;
	}

	OutContext.DiscoveredSourceCount = Discovery.DiscoveredSourceCount;
	OutContext.LoadedSourceCount = Discovery.LoadedSourceCount;
	Result.Detail = FString::Printf(
		TEXT("Prepared Cache V2 compile capture with %d sources, %d modules and SourceSnapshot=%s"),
		Discovery.LoadedSourceCount,
		Discovery.Modules.Num(),
		*OutContext.AuthoritativeSourceIndex.SourceSnapshot.ToHexString());
	return Result;
}
