#include "Cache/AngelscriptCacheSourceDiscovery.h"

#include "Algo/Sort.h"
#include "Hash/Blake3.h"
#include "Internationalization/TextChar.h"
#include "Preprocessor/AngelscriptPreprocessor.h"

namespace AngelscriptCacheSourceDiscovery_Private
{
	struct FWorkingSource
	{
		FAngelscriptSource Source;
		TArray<uint8> RawBytes;
		FString LogicalMount;
		FAngelscriptStableModuleKey ModuleKey;
		FAngelscriptSourceProviderDescriptor Descriptor;
		int32 ProviderIndex = INDEX_NONE;
		TOptional<FAngelscriptHash256> GeneratedSourceKey;
		TOptional<FAngelscriptHash256> GeneratedConfigurationFingerprint;
	};

	struct FWorkingProvider
	{
		FAngelscriptSourceProviderDescriptor Descriptor;
		FAngelscriptCachedSourceProvider Cached;
		TArray<int32> SourceIndices;
	};

	struct FWorkingModule
	{
		FAngelscriptStableModuleKey ModuleKey;
		FString CanonicalModuleName;
		int32 SourceFileCount = 0;
	};

	static FAngelscriptCacheSourceDiscoveryStatus Failure(
		const EAngelscriptCacheSourceDiscoveryError Error,
		const FString& Detail,
		const FAngelscriptCacheValidationResult& Validation = {})
	{
		FAngelscriptCacheSourceDiscoveryStatus Status;
		Status.Error = Error;
		Status.Validation = Validation;
		Status.Detail = Detail;
		return Status;
	}

	static FAngelscriptCacheValidationResult ValidationFailure(
		const EAngelscriptCacheValidationError Error)
	{
		return FAngelscriptCacheValidationResult(
			Error, EAngelscriptCacheRecordKind::SourceIndex);
	}

	static FAngelscriptHash256 HashString(
		const FStringView Domain,
		const FStringView Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(Domain);
		Writer.WriteString(Value);
		return Writer.FinalizeHash();
	}

	static FAngelscriptHash256 HashRawBytes(
		const TConstArrayView<uint8> Bytes)
	{
		return FAngelscriptHash256{FBlake3::HashBuffer(
			Bytes.GetData(), static_cast<uint64>(Bytes.Num()))};
	}

	static bool ContainsEmbeddedNul(const FString& Value)
	{
		for (int32 Index = 0; Index < Value.Len(); ++Index)
		{
			if (Value[Index] == TEXT('\0'))
			{
				return true;
			}
		}
		return false;
	}

	static EAngelscriptCacheValidationError ValidateUtf8(
		const TConstArrayView<uint8> Bytes)
	{
		int32 Index = 0;
		while (Index < Bytes.Num())
		{
			const uint8 Lead = Bytes[Index];
			if (Lead <= 0x7f)
			{
				if (Lead == 0)
				{
					return EAngelscriptCacheValidationError::EmbeddedNul;
				}
				++Index;
				continue;
			}

			const auto HasContinuation = [&](const int32 Offset)
			{
				return Index + Offset < Bytes.Num()
					&& (Bytes[Index + Offset] & 0xc0) == 0x80;
			};
			if (Lead >= 0xc2 && Lead <= 0xdf)
			{
				if (!HasContinuation(1))
				{
					return EAngelscriptCacheValidationError::InvalidUtf8;
				}
				Index += 2;
				continue;
			}
			if (Lead >= 0xe0 && Lead <= 0xef)
			{
				if (!HasContinuation(1) || !HasContinuation(2))
				{
					return EAngelscriptCacheValidationError::InvalidUtf8;
				}
				const uint8 Second = Bytes[Index + 1];
				if ((Lead == 0xe0 && Second < 0xa0)
					|| (Lead == 0xed && Second > 0x9f))
				{
					return EAngelscriptCacheValidationError::InvalidUtf8;
				}
				Index += 3;
				continue;
			}
			if (Lead >= 0xf0 && Lead <= 0xf4)
			{
				if (!HasContinuation(1)
					|| !HasContinuation(2)
					|| !HasContinuation(3))
				{
					return EAngelscriptCacheValidationError::InvalidUtf8;
				}
				const uint8 Second = Bytes[Index + 1];
				if ((Lead == 0xf0 && Second < 0x90)
					|| (Lead == 0xf4 && Second > 0x8f))
				{
					return EAngelscriptCacheValidationError::InvalidUtf8;
				}
				Index += 4;
				continue;
			}
			return EAngelscriptCacheValidationError::InvalidUtf8;
		}
		return EAngelscriptCacheValidationError::None;
	}

	static int32 CompareUnicodeSimpleFold(
		const FString& A,
		const FString& B)
	{
		int32 AIndex = 0;
		int32 BIndex = 0;
		while (AIndex < A.Len() && BIndex < B.Len())
		{
			int32 ACount = 0;
			int32 BCount = 0;
			const UTF32CHAR AFolded = FTextChar::ToLower(
				FTextChar::GetCodepoint(*A + AIndex, &ACount));
			const UTF32CHAR BFolded = FTextChar::ToLower(
				FTextChar::GetCodepoint(*B + BIndex, &BCount));
			if (AFolded != BFolded)
			{
				return AFolded < BFolded ? -1 : 1;
			}
			AIndex += ACount;
			BIndex += BCount;
		}
		if (AIndex == A.Len() && BIndex == B.Len())
		{
			return 0;
		}
		return AIndex == A.Len() ? -1 : 1;
	}

	static bool TryGetLogicalMount(
		const FAngelscriptVirtualPath& VirtualPath,
		FString& OutLogicalMount)
	{
		switch (VirtualPath.GetSourceKind())
		{
		case EAngelscriptSourceKind::Game:
			OutLogicalMount = FAngelscriptVirtualPath::GameRoot;
			return true;
		case EAngelscriptSourceKind::Plugin:
			OutLogicalMount = FString(FAngelscriptVirtualPath::PluginRoot)
				/ VirtualPath.GetMountName();
			return !VirtualPath.GetMountName().IsEmpty();
		case EAngelscriptSourceKind::Memory:
			OutLogicalMount = FString(FAngelscriptVirtualPath::MemoryRoot)
				/ VirtualPath.GetMountName();
			return !VirtualPath.GetMountName().IsEmpty();
		default:
			OutLogicalMount.Reset();
			return false;
		}
	}

	static EAngelscriptCachedSourceKind ToCachedSourceKind(
		const EAngelscriptSourceKind Kind)
	{
		switch (Kind)
		{
		case EAngelscriptSourceKind::Game:
			return EAngelscriptCachedSourceKind::Game;
		case EAngelscriptSourceKind::Plugin:
			return EAngelscriptCachedSourceKind::Plugin;
		case EAngelscriptSourceKind::Memory:
			return EAngelscriptCachedSourceKind::Memory;
		default:
			return EAngelscriptCachedSourceKind::Invalid;
		}
	}

	static EAngelscriptCachedSourceProviderKind ToCachedProviderKind(
		const EAngelscriptSourceProviderDescriptorKind Kind)
	{
		switch (Kind)
		{
		case EAngelscriptSourceProviderDescriptorKind::BuiltInDisk:
			return EAngelscriptCachedSourceProviderKind::BuiltInDisk;
		case EAngelscriptSourceProviderDescriptorKind::Memory:
			return EAngelscriptCachedSourceProviderKind::Memory;
		case EAngelscriptSourceProviderDescriptorKind::Generated:
			return EAngelscriptCachedSourceProviderKind::Generated;
		case EAngelscriptSourceProviderDescriptorKind::External:
			return EAngelscriptCachedSourceProviderKind::External;
		default:
			return EAngelscriptCachedSourceProviderKind::Invalid;
		}
	}

	static FAngelscriptSourceProviderDescriptor MakeIntrinsicMemoryDescriptor(
		const FAngelscriptSource& Source)
	{
		FAngelscriptSourceProviderDescriptor Descriptor;
		Descriptor.Kind = EAngelscriptSourceProviderDescriptorKind::Memory;
		Descriptor.CanonicalImplementationIdentity =
			TEXT("Angelscript.InlineMemorySourceProvider.V1");
		Descriptor.StableInstanceIdentity =
			Source.VirtualPath.GetMountName();
		Descriptor.Version = FString(TEXT("1"));
		Descriptor.Configuration = FString(TEXT("canonical-utf8-inline-v1"));
		return Descriptor;
	}

	static FAngelscriptSourceProviderDescriptor MakeUnidentifiedDescriptor()
	{
		FAngelscriptSourceProviderDescriptor Descriptor;
		Descriptor.Kind = EAngelscriptSourceProviderDescriptorKind::External;
		Descriptor.CanonicalImplementationIdentity =
			TEXT("Angelscript.UnidentifiedExternalSourceProvider");
		return Descriptor;
	}

	static bool SameOptionalString(
		const TOptional<FString>& A,
		const TOptional<FString>& B)
	{
		return A.IsSet() == B.IsSet()
			&& (!A.IsSet() || A.GetValue() == B.GetValue());
	}

	static bool SameDescriptor(
		const FAngelscriptSourceProviderDescriptor& A,
		const FAngelscriptSourceProviderDescriptor& B)
	{
		return A.Kind == B.Kind
			&& A.CanonicalImplementationIdentity
				== B.CanonicalImplementationIdentity
			&& SameOptionalString(
				A.StableInstanceIdentity, B.StableInstanceIdentity)
			&& SameOptionalString(A.Version, B.Version)
			&& SameOptionalString(A.Configuration, B.Configuration)
			&& SameOptionalString(
				A.GeneratedSourceIdentity, B.GeneratedSourceIdentity)
			&& SameOptionalString(
				A.GeneratedSourceConfiguration,
				B.GeneratedSourceConfiguration);
	}

	static FAngelscriptCacheValidationResult ValidateDescriptor(
		const FAngelscriptSourceProviderDescriptor& Descriptor)
	{
		if (ToCachedProviderKind(Descriptor.Kind)
			== EAngelscriptCachedSourceProviderKind::Invalid)
		{
			return ValidationFailure(
				EAngelscriptCacheValidationError::UnknownEnumValue);
		}
		if (Descriptor.CanonicalImplementationIdentity.IsEmpty())
		{
			return ValidationFailure(
				EAngelscriptCacheValidationError::InvalidPresence);
		}
		const TOptional<FString>* OptionalStrings[] = {
			&Descriptor.StableInstanceIdentity,
			&Descriptor.Version,
			&Descriptor.Configuration,
			&Descriptor.GeneratedSourceIdentity,
			&Descriptor.GeneratedSourceConfiguration,
		};
		if (ContainsEmbeddedNul(Descriptor.CanonicalImplementationIdentity))
		{
			return ValidationFailure(
				EAngelscriptCacheValidationError::EmbeddedNul);
		}
		for (const TOptional<FString>* Value : OptionalStrings)
		{
			if (Value->IsSet()
				&& (Value->GetValue().IsEmpty()
					|| ContainsEmbeddedNul(Value->GetValue())))
			{
				return ValidationFailure(
					Value->GetValue().IsEmpty()
						? EAngelscriptCacheValidationError::InvalidPresence
						: EAngelscriptCacheValidationError::EmbeddedNul);
			}
		}

		const bool bGenerated = Descriptor.Kind
			== EAngelscriptSourceProviderDescriptorKind::Generated;
		if (bGenerated != Descriptor.GeneratedSourceIdentity.IsSet()
			|| bGenerated
				!= Descriptor.GeneratedSourceConfiguration.IsSet())
		{
			return ValidationFailure(
				EAngelscriptCacheValidationError::InvalidPresence);
		}
		return {};
	}

	static FAngelscriptCachedSourceProvider MakeCachedProvider(
		const FAngelscriptSourceProviderDescriptor& Descriptor)
	{
		FAngelscriptCachedSourceProvider Provider;
		Provider.ProviderKind = ToCachedProviderKind(Descriptor.Kind);
		Provider.CanonicalImplementationIdentity =
			Descriptor.CanonicalImplementationIdentity;
		if (Descriptor.StableInstanceIdentity.IsSet())
		{
			FAngelscriptArtifactCanonicalWriter Writer(
				TEXT("production-source-provider-identity-v1"));
			Writer.WriteString(Descriptor.CanonicalImplementationIdentity);
			Writer.WriteString(Descriptor.StableInstanceIdentity.GetValue());
			Provider.IdentityFingerprint = Writer.FinalizeHash();
			Provider.CapabilityFlags |= static_cast<uint32>(
				EAngelscriptCachedFingerprintCapabilityFlags::StableIdentity);
		}
		if (Descriptor.Version.IsSet())
		{
			Provider.VersionFingerprint = HashString(
				TEXT("production-source-provider-version-v1"),
				Descriptor.Version.GetValue());
			Provider.CapabilityFlags |= static_cast<uint32>(
				EAngelscriptCachedFingerprintCapabilityFlags::VersionFingerprint);
		}
		if (Descriptor.Configuration.IsSet())
		{
			Provider.ConfigurationFingerprint = HashString(
				TEXT("production-source-provider-configuration-v1"),
				Descriptor.Configuration.GetValue());
			Provider.CapabilityFlags |= static_cast<uint32>(
				EAngelscriptCachedFingerprintCapabilityFlags::ConfigurationFingerprint);
		}
		return Provider;
	}

	static FAngelscriptHash256 ComputeProviderContentFingerprint(
		const FWorkingProvider& Provider,
		const TArray<FWorkingSource>& Sources)
	{
		TArray<int32> Ordered = Provider.SourceIndices;
		Ordered.Sort([&](const int32 AIndex, const int32 BIndex)
		{
			const FWorkingSource& A = Sources[AIndex];
			const FWorkingSource& B = Sources[BIndex];
			return FAngelscriptArtifactCanonicalWriter::CompareCanonicalUtf8Strings(
				A.Source.VirtualPath.ToString(),
				B.Source.VirtualPath.ToString()) < 0;
		});
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("production-source-provider-content-v1"));
		Writer.WriteUInt32(static_cast<uint32>(Ordered.Num()));
		for (const int32 SourceIndex : Ordered)
		{
			const FWorkingSource& Source = Sources[SourceIndex];
			Writer.WriteString(Source.LogicalMount);
			Writer.WriteString(Source.Source.VirtualPath.GetRelativePath());
			Writer.WriteHash(HashRawBytes(Source.RawBytes));
			Writer.WriteBool(Source.GeneratedSourceKey.IsSet());
			if (Source.GeneratedSourceKey.IsSet())
			{
				Writer.WriteHash(Source.GeneratedSourceKey.GetValue());
				Writer.WriteHash(
					Source.GeneratedConfigurationFingerprint.GetValue());
			}
		}
		return Writer.FinalizeHash();
	}

	static FAngelscriptHash256 ComputeMountConfigurationFingerprint(
		const FWorkingSource& Source,
		const FAngelscriptCachedSourceProvider& Provider,
		const TConstArrayView<FAngelscriptSourceRoot> ScriptRoots,
		const uint32 FilterFlags)
	{
		TArray<FString> MatchingLogicalRoots;
		for (const FAngelscriptSourceRoot& Root : ScriptRoots)
		{
			bool bMatches = false;
			FString LogicalRoot;
			switch (Root.SourceKind)
			{
			case EAngelscriptSourceKind::Game:
				LogicalRoot = FAngelscriptVirtualPath::GameRoot;
				bMatches = Source.Source.SourceKind
					== EAngelscriptSourceKind::Game;
				break;
			case EAngelscriptSourceKind::Plugin:
				LogicalRoot = FString(FAngelscriptVirtualPath::PluginRoot)
					/ Root.MountName;
				bMatches = Source.Source.SourceKind
					== EAngelscriptSourceKind::Plugin
					&& Root.MountName
						== Source.Source.VirtualPath.GetMountName();
				break;
			default:
				break;
			}
			if (bMatches)
			{
				MatchingLogicalRoots.AddUnique(LogicalRoot);
			}
		}
		MatchingLogicalRoots.Sort([](const FString& A, const FString& B)
		{
			return FAngelscriptArtifactCanonicalWriter::CompareCanonicalUtf8Strings(
				A, B) < 0;
		});

		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("production-source-mount-configuration-v1"));
		Writer.WriteString(Source.LogicalMount);
		Writer.WriteUInt32(FilterFlags);
		Writer.WriteBool(Provider.ConfigurationFingerprint.IsSet());
		if (Provider.ConfigurationFingerprint.IsSet())
		{
			Writer.WriteHash(Provider.ConfigurationFingerprint.GetValue());
		}
		Writer.WriteUInt32(static_cast<uint32>(MatchingLogicalRoots.Num()));
		for (const FString& Root : MatchingLogicalRoots)
		{
			Writer.WriteString(Root);
		}
		return Writer.FinalizeHash();
	}

	static FAngelscriptCachedPreprocessHook MakeLegacyHook(
		const EAngelscriptCachedPreprocessHookPhase Phase,
		const FAngelscriptStableModuleKey& ModuleKey)
	{
		FAngelscriptCachedPreprocessHook Hook;
		Hook.Phase = Phase;
		Hook.CanonicalImplementationIdentity = FString::Printf(
			TEXT("Angelscript.UnfingerprintedLegacyGlobalPreprocessHook.%u"),
			static_cast<uint32>(Phase));
		Hook.AffectedScopeKind =
			EAngelscriptCachedFastPathScopeKind::Module;
		Hook.AffectedScopeStableKey = ModuleKey.Hash;
		check(FAngelscriptCacheSemanticArchive::TryBuildPreprocessHookKey(
			{Hook.Phase,
				Hook.CanonicalImplementationIdentity,
				Hook.AffectedScopeKind,
				Hook.AffectedScopeStableKey},
			Hook.HookKey).IsSuccess());
		return Hook;
	}

	static int32 FindSourceByKey(
		const TArray<FAngelscriptCachedSourceFile>& Files,
		const FAngelscriptHash256& Key,
		const bool bGenerated)
	{
		for (int32 Index = 0; Index < Files.Num(); ++Index)
		{
			if ((!bGenerated && Files[Index].SourceFileKey.Hash == Key)
				|| (bGenerated
					&& Files[Index].GeneratedSourceKey.IsSet()
					&& Files[Index].GeneratedSourceKey.GetValue() == Key))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	static TOptional<FAngelscriptHash256> ComputeModuleObservation(
		const FAngelscriptCachedSourceIndex& Current,
		const FAngelscriptHash256& ModuleKey)
	{
		TArray<const FAngelscriptCachedSourceFile*> Files;
		for (const FAngelscriptCachedSourceFile& File : Current.Files)
		{
			if (File.ModuleKey.Hash == ModuleKey)
			{
				Files.Add(&File);
			}
		}
		if (Files.IsEmpty())
		{
			return {};
		}
		Files.Sort([](
			const FAngelscriptCachedSourceFile& A,
			const FAngelscriptCachedSourceFile& B)
		{
			return A.SourceFileKey.Hash < B.SourceFileKey.Hash;
		});
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("production-source-module-observation-v1"));
		Writer.WriteHash(ModuleKey);
		Writer.WriteUInt32(static_cast<uint32>(Files.Num()));
		for (const FAngelscriptCachedSourceFile* File : Files)
		{
			Writer.WriteHash(File->SourceFileKey.Hash);
			Writer.WriteHash(File->RawContentHash);
		}
		return Writer.FinalizeHash();
	}

	static TOptional<FAngelscriptHash256> ResolveObservation(
		const FAngelscriptCachedSourceIndex& Current,
		const FAngelscriptCachedPreprocessorInput& Input)
	{
		if (Input.TargetKind ==
			EAngelscriptCachePreprocessorInputTargetKind::None)
		{
			if (Input.InputKind != EAngelscriptCachePreprocessorInputKind::Define
				&& Input.InputKind
					!= EAngelscriptCachePreprocessorInputKind::ConditionalSymbol)
			{
				return {};
			}
			const FString OptionKey = FString::Printf(
				TEXT("preprocessor:%s"), *Input.CanonicalName);
			for (const FAngelscriptCachedCanonicalOption& Option :
				Current.DiscoveryPolicy.Options)
			{
				if (Option.CanonicalKey == OptionKey)
				{
					return Option.ValueFingerprint;
				}
			}
			return {};
		}
		if (!Input.TargetStableKey.IsSet())
		{
			return {};
		}
		const FAngelscriptHash256& Target = Input.TargetStableKey.GetValue();
		switch (Input.TargetKind)
		{
		case EAngelscriptCachePreprocessorInputTargetKind::SourceFile:
		{
			const int32 Index = FindSourceByKey(Current.Files, Target, false);
			return Index == INDEX_NONE
				? TOptional<FAngelscriptHash256>()
				: TOptional<FAngelscriptHash256>(Current.Files[Index].RawContentHash);
		}
		case EAngelscriptCachePreprocessorInputTargetKind::GeneratedSource:
		{
			const int32 Index = FindSourceByKey(Current.Files, Target, true);
			return Index == INDEX_NONE
				? TOptional<FAngelscriptHash256>()
				: TOptional<FAngelscriptHash256>(Current.Files[Index].RawContentHash);
		}
		case EAngelscriptCachePreprocessorInputTargetKind::Provider:
			for (const FAngelscriptCachedSourceProvider& Provider : Current.Providers)
			{
				if (Provider.ProviderKey.Hash == Target)
				{
					return Provider.ContentFingerprint;
				}
			}
			return {};
		case EAngelscriptCachePreprocessorInputTargetKind::Hook:
			for (const FAngelscriptCachedPreprocessHook& Hook :
				Current.PreprocessHooks)
			{
				if (Hook.HookKey.Hash == Target)
				{
					return Hook.ContentFingerprint;
				}
			}
			return {};
		case EAngelscriptCachePreprocessorInputTargetKind::Module:
			return ComputeModuleObservation(Current, Target);
		default:
			return {};
		}
	}
}

FAngelscriptCacheSourceDiscoveryStatus
FAngelscriptCacheSourceDiscovery::DiscoverProductionSources(
	IAngelscriptSourceProvider& SourceProvider,
	const TConstArrayView<FAngelscriptSourceRoot> ScriptRoots,
	const bool bSkipDevelopmentScripts,
	const bool bSkipEditorScripts,
	const FAngelscriptCacheProductionSourceDiscoveryConfig& Config,
	const FAngelscriptCacheDirectSourceLimits& Limits,
	FAngelscriptCacheProductionSourceDiscoveryResult& OutResult)
{
	using namespace AngelscriptCacheSourceDiscovery_Private;

	OutResult.Reset();
	if (Config.Profile.Hash.IsZero()
		|| Config.DiscoveryPolicyVersion == 0)
	{
		return Failure(
			EAngelscriptCacheSourceDiscoveryError::InvalidRequest,
			TEXT("Production source discovery requires a nonzero profile and policy version"),
			ValidationFailure(EAngelscriptCacheValidationError::ZeroStableKey));
	}
	if (static_cast<uint64>(ScriptRoots.Num()) > Limits.MaxMounts)
	{
		return Failure(
			EAngelscriptCacheSourceDiscoveryError::DirectPlanRejected,
			TEXT("Configured source-root count exceeds the direct-source mount limit"),
			ValidationFailure(EAngelscriptCacheValidationError::BudgetExceeded));
	}

	TArray<FAngelscriptSource> DiscoveredSources;
	TArray<FAngelscriptSourceRoot> RootCopy(ScriptRoots);
	SourceProvider.FindSources(
		RootCopy,
		bSkipDevelopmentScripts,
		bSkipEditorScripts,
		DiscoveredSources);
	if (static_cast<uint64>(DiscoveredSources.Num()) > Limits.MaxFiles)
	{
		return Failure(
			EAngelscriptCacheSourceDiscoveryError::DirectPlanRejected,
			TEXT("Source provider returned more files than the configured direct-source limit"),
			ValidationFailure(EAngelscriptCacheValidationError::BudgetExceeded));
	}

	TArray<FWorkingSource> WorkingSources;
	WorkingSources.Reserve(DiscoveredSources.Num());
	uint64 TotalRawSourceBytes = 0;
	for (FAngelscriptSource& Source : DiscoveredSources)
	{
		FWorkingSource Working;
		Working.Source = MoveTemp(Source);
		if (static_cast<uint64>(Working.Source.ModuleName.Len())
			> Limits.MaxCanonicalStringCharacters)
		{
			return Failure(
				EAngelscriptCacheSourceDiscoveryError::DirectPlanRejected,
				FString::Printf(
					TEXT("Source module name exceeds the configured character budget for %s"),
					*Working.Source.VirtualPath.ToString()),
				ValidationFailure(
					EAngelscriptCacheValidationError::BudgetExceeded));
		}
		FAngelscriptVirtualPath ParsedPath;
		FString PathError;
		if (!Working.Source.VirtualPath.IsValid()
			|| !FAngelscriptVirtualPath::TryParse(
				Working.Source.VirtualPath.ToString(), ParsedPath, &PathError)
			|| ParsedPath.GetSourceKind() != Working.Source.SourceKind
			|| ParsedPath.GetMountName()
				!= Working.Source.VirtualPath.GetMountName()
			|| ParsedPath.GetRelativePath()
				!= Working.Source.VirtualPath.GetRelativePath()
			|| Working.Source.RelativeFilename
				!= Working.Source.VirtualPath.GetRelativePath()
			|| Working.Source.ModuleName.IsEmpty()
			|| Working.Source.ModuleName
				!= Working.Source.VirtualPath.ToModuleName()
			|| ContainsEmbeddedNul(Working.Source.ModuleName)
			|| !TryGetLogicalMount(
				Working.Source.VirtualPath, Working.LogicalMount))
		{
			return Failure(
				EAngelscriptCacheSourceDiscoveryError::InvalidSourceDescriptor,
				FString::Printf(
					TEXT("Invalid source descriptor for %s: %s"),
					*Working.Source.VirtualPath.ToString(),
					*PathError),
				ValidationFailure(
					EAngelscriptCacheValidationError::InvalidLogicalPath));
		}

		const TOptional<FAngelscriptStableModuleKey> ModuleKey =
			FAngelscriptArtifactIdentityBuilder::TryBuildModuleKey(
				Working.LogicalMount,
				Working.Source.VirtualPath.GetRelativePath(),
				Working.Source.ModuleName);
		if (!ModuleKey.IsSet())
		{
			return Failure(
				EAngelscriptCacheSourceDiscoveryError::InvalidSourceDescriptor,
				FString::Printf(TEXT("Source %s has no stable module coordinate"),
					*Working.Source.VirtualPath.ToString()),
				ValidationFailure(
					EAngelscriptCacheValidationError::InvalidLogicalPath));
		}
		Working.ModuleKey = ModuleKey.GetValue();

		if (!SourceProvider.LoadSourceBytes(
			Working.Source, Working.RawBytes))
		{
			return Failure(
				EAngelscriptCacheSourceDiscoveryError::SourceReadFailed,
				FString::Printf(TEXT("Failed to load source bytes for %s"),
					*Working.Source.VirtualPath.ToString()));
		}
		const uint64 RawSourceBytes =
			static_cast<uint64>(Working.RawBytes.Num());
		if (RawSourceBytes > Limits.MaxSingleRawSourceBytes
			|| TotalRawSourceBytes > Limits.MaxTotalRawSourceBytes
			|| RawSourceBytes
				> Limits.MaxTotalRawSourceBytes - TotalRawSourceBytes)
		{
			return Failure(
				EAngelscriptCacheSourceDiscoveryError::DirectPlanRejected,
				FString::Printf(
					TEXT("Source byte budget exceeded while loading %s"),
					*Working.Source.VirtualPath.ToString()),
				ValidationFailure(
					EAngelscriptCacheValidationError::BudgetExceeded));
		}
		TotalRawSourceBytes += RawSourceBytes;
		const EAngelscriptCacheValidationError EncodingError =
			ValidateUtf8(Working.RawBytes);
		if (EncodingError != EAngelscriptCacheValidationError::None)
		{
			return Failure(
				EAngelscriptCacheSourceDiscoveryError::InvalidSourceEncoding,
				FString::Printf(TEXT("Source %s is not strict NUL-free UTF-8"),
					*Working.Source.VirtualPath.ToString()),
				ValidationFailure(EncodingError));
		}

		if (!SourceProvider.QuerySourceDescriptor(
			Working.Source, Working.Descriptor))
		{
			Working.Descriptor = Working.Source.SourceKind
					== EAngelscriptSourceKind::Memory
				&& Working.Source.bHasSourceText
				? MakeIntrinsicMemoryDescriptor(Working.Source)
				: MakeUnidentifiedDescriptor();
		}
		const FAngelscriptCacheValidationResult DescriptorResult =
			ValidateDescriptor(Working.Descriptor);
		if (!DescriptorResult.IsSuccess())
		{
			return Failure(
				EAngelscriptCacheSourceDiscoveryError::InvalidSourceDescriptor,
				FString::Printf(TEXT("Invalid provider descriptor for %s"),
					*Working.Source.VirtualPath.ToString()),
				DescriptorResult);
		}
		if (Working.Descriptor.Kind
			== EAngelscriptSourceProviderDescriptorKind::Generated)
		{
			Working.GeneratedSourceKey = HashString(
				TEXT("production-generated-source-identity-v1"),
				Working.Descriptor.GeneratedSourceIdentity.GetValue());
			Working.GeneratedConfigurationFingerprint = HashString(
				TEXT("production-generated-source-configuration-v1"),
				Working.Descriptor.GeneratedSourceConfiguration.GetValue());
		}
		WorkingSources.Add(MoveTemp(Working));
	}

	TArray<int32> FoldedPathOrder;
	FoldedPathOrder.Reserve(WorkingSources.Num());
	for (int32 Index = 0; Index < WorkingSources.Num(); ++Index)
	{
		FoldedPathOrder.Add(Index);
	}
	FoldedPathOrder.Sort([&](const int32 AIndex, const int32 BIndex)
	{
		const FString& A =
			WorkingSources[AIndex].Source.VirtualPath.ToString();
		const FString& B =
			WorkingSources[BIndex].Source.VirtualPath.ToString();
		const int32 FoldCompare = CompareUnicodeSimpleFold(A, B);
		if (FoldCompare != 0)
		{
			return FoldCompare < 0;
		}
		return FAngelscriptArtifactCanonicalWriter::CompareCanonicalUtf8Strings(
			A, B) < 0;
	});
	for (int32 Index = 1; Index < FoldedPathOrder.Num(); ++Index)
	{
		const FString& A = WorkingSources[FoldedPathOrder[Index - 1]]
			.Source.VirtualPath.ToString();
		const FString& B = WorkingSources[FoldedPathOrder[Index]]
			.Source.VirtualPath.ToString();
		if (CompareUnicodeSimpleFold(A, B) == 0)
		{
			return Failure(
				EAngelscriptCacheSourceDiscoveryError::InvalidSourceDescriptor,
				FString::Printf(
					TEXT("Ambiguous logical source paths %s and %s"), *A, *B),
				ValidationFailure(
					EAngelscriptCacheValidationError::CaseCollision));
		}
	}

	TArray<FWorkingProvider> Providers;
	TMap<FString, int32> ProviderIndexByKey;
	for (int32 SourceIndex = 0;
		SourceIndex < WorkingSources.Num(); ++SourceIndex)
	{
		FWorkingSource& Source = WorkingSources[SourceIndex];
		FAngelscriptCachedSourceProvider Cached =
			MakeCachedProvider(Source.Descriptor);
		const FAngelscriptCacheValidationResult KeyResult =
			FAngelscriptCacheSemanticArchive::TryBuildSourceProviderKey(
				{Cached.ProviderKind,
					Cached.CanonicalImplementationIdentity,
					Cached.IdentityFingerprint},
				Cached.ProviderKey);
		if (!KeyResult.IsSuccess())
		{
			return Failure(
				EAngelscriptCacheSourceDiscoveryError::InvalidSourceDescriptor,
				TEXT("Provider key construction failed"), KeyResult);
		}
		const FString ProviderKey = Cached.ProviderKey.Hash.ToHexString();
		if (const int32* ExistingIndex =
			ProviderIndexByKey.Find(ProviderKey))
		{
			FWorkingProvider& Existing = Providers[*ExistingIndex];
			if (!SameDescriptor(Existing.Descriptor, Source.Descriptor))
			{
				return Failure(
					EAngelscriptCacheSourceDiscoveryError::InvalidSourceDescriptor,
					TEXT("One stable provider identity reported conflicting version/configuration descriptors"),
					ValidationFailure(
						EAngelscriptCacheValidationError::ConflictingKey));
			}
			Source.ProviderIndex = *ExistingIndex;
			Existing.SourceIndices.Add(SourceIndex);
		}
		else
		{
			FWorkingProvider& Added = Providers.AddDefaulted_GetRef();
			Added.Descriptor = Source.Descriptor;
			Added.Cached = MoveTemp(Cached);
			Added.SourceIndices.Add(SourceIndex);
			Source.ProviderIndex = Providers.Num() - 1;
			ProviderIndexByKey.Add(ProviderKey, Source.ProviderIndex);
		}
	}
	for (FWorkingProvider& Provider : Providers)
	{
		Provider.Cached.ContentFingerprint =
			ComputeProviderContentFingerprint(Provider, WorkingSources);
		Provider.Cached.CapabilityFlags |= static_cast<uint32>(
			EAngelscriptCachedFingerprintCapabilityFlags::ContentFingerprint);
	}

	FAngelscriptCacheDirectSourceInputs DirectInputs;
	DirectInputs.Profile = Config.Profile;
	DirectInputs.DiscoveryPolicyVersion = Config.DiscoveryPolicyVersion;
	DirectInputs.DiscoveryFilterFlags =
		(bSkipDevelopmentScripts
			? static_cast<uint32>(
				EAngelscriptCachedSourceDiscoveryFilterFlags::SkipDevelopment)
			: 0)
		| (bSkipEditorScripts
			? static_cast<uint32>(
				EAngelscriptCachedSourceDiscoveryFilterFlags::SkipEditor)
			: 0);
	DirectInputs.Options = Config.Options;
	DirectInputs.PreprocessHooks = Config.PreprocessHooks;
	for (const FWorkingProvider& Provider : Providers)
	{
		DirectInputs.Providers.Add(Provider.Cached);
	}

	TMap<FString, int32> MountIndexByIdentity;
	TArray<FWorkingModule> Modules;
	TMap<FString, int32> ModuleIndexByKey;
	for (const FWorkingSource& Source : WorkingSources)
	{
		const FAngelscriptCachedSourceProvider& Provider =
			DirectInputs.Providers[Source.ProviderIndex];
		FAngelscriptCachedSourceMount Mount;
		Mount.SourceKind = ToCachedSourceKind(Source.Source.SourceKind);
		Mount.LogicalMount = Source.LogicalMount;
		Mount.ProviderKey = Provider.ProviderKey;
		Mount.RootConfigurationFingerprint =
			ComputeMountConfigurationFingerprint(
				Source,
				Provider,
				ScriptRoots,
				DirectInputs.DiscoveryFilterFlags);
		const FAngelscriptCacheValidationResult MountKeyResult =
			FAngelscriptCacheSemanticArchive::TryBuildSourceMountKey(
				{Mount.SourceKind, Mount.LogicalMount, Mount.ProviderKey},
				Mount.MountKey);
		if (!MountKeyResult.IsSuccess())
		{
			return Failure(
				EAngelscriptCacheSourceDiscoveryError::InvalidSourceDescriptor,
				TEXT("Source mount key construction failed"),
				MountKeyResult);
		}
		const FString MountIdentity = Mount.MountKey.Hash.ToHexString();
		int32 MountIndex = INDEX_NONE;
		if (const int32* ExistingMount =
			MountIndexByIdentity.Find(MountIdentity))
		{
			MountIndex = *ExistingMount;
			if (DirectInputs.Mounts[MountIndex].RootConfigurationFingerprint
				!= Mount.RootConfigurationFingerprint)
			{
				return Failure(
					EAngelscriptCacheSourceDiscoveryError::InvalidSourceDescriptor,
					TEXT("One logical source mount reported conflicting configuration"),
					ValidationFailure(
						EAngelscriptCacheValidationError::ConflictingKey));
			}
		}
		else
		{
			MountIndex = DirectInputs.Mounts.Add(MoveTemp(Mount));
			MountIndexByIdentity.Add(MountIdentity, MountIndex);
		}

		FAngelscriptCacheDirectSourceFileInput File;
		File.MountIndex = MountIndex;
		File.RelativeLogicalPath =
			Source.Source.VirtualPath.GetRelativePath();
		File.RawSourceBytes = Source.RawBytes;
		File.GeneratedSourceKey = Source.GeneratedSourceKey;
		File.GeneratedConfigurationFingerprint =
			Source.GeneratedConfigurationFingerprint;
		File.ModuleKey = Source.ModuleKey;
		DirectInputs.Files.Add(MoveTemp(File));

		const FString ModuleIdentity = Source.ModuleKey.Hash.ToHexString();
		if (const int32* ExistingModule =
			ModuleIndexByKey.Find(ModuleIdentity))
		{
			FWorkingModule& Module = Modules[*ExistingModule];
			if (Module.CanonicalModuleName != Source.Source.ModuleName)
			{
				return Failure(
					EAngelscriptCacheSourceDiscoveryError::InvalidSourceDescriptor,
					TEXT("One stable ModuleKey resolved to conflicting module names"),
					ValidationFailure(
						EAngelscriptCacheValidationError::ConflictingKey));
			}
			++Module.SourceFileCount;
		}
		else
		{
			FWorkingModule& Module = Modules.AddDefaulted_GetRef();
			Module.ModuleKey = Source.ModuleKey;
			Module.CanonicalModuleName = Source.Source.ModuleName;
			Module.SourceFileCount = 1;
			ModuleIndexByKey.Add(ModuleIdentity, Modules.Num() - 1);
		}
	}

	if (Config.bObserveLegacyGlobalPreprocessHooks)
	{
		for (const FWorkingModule& Module : Modules)
		{
			if (FAngelscriptPreprocessor::OnProcessChunks.IsBound())
			{
				DirectInputs.PreprocessHooks.Add(MakeLegacyHook(
					EAngelscriptCachedPreprocessHookPhase::ProcessChunks,
					Module.ModuleKey));
			}
			if (FAngelscriptPreprocessor::OnPostProcessCode.IsBound())
			{
				DirectInputs.PreprocessHooks.Add(MakeLegacyHook(
					EAngelscriptCachedPreprocessHookPhase::PostProcessCode,
					Module.ModuleKey));
			}
		}
	}

	FAngelscriptCacheProductionSourceDiscoveryResult Candidate;
	const FAngelscriptCacheValidationResult PlanResult =
		FAngelscriptCacheSourcePlanner::BuildDirectSourcePlan(
			DirectInputs, Limits, Candidate.DirectPlan);
	if (!PlanResult.IsSuccess())
	{
		return Failure(
			EAngelscriptCacheSourceDiscoveryError::DirectPlanRejected,
			TEXT("Discovered production sources did not form a valid direct SourceIndex"),
			PlanResult);
	}
	// BuildDirectSourcePlan has reduced every raw payload to canonical hashes.
	// Release its input copies before transferring the sole retained byte arrays
	// into the transient current-source projection.
	DirectInputs = {};

	// Retain the current machine coordinates and already bounded raw bytes for
	// exact restore. SourceIndex intentionally carries neither absolute paths nor
	// raw payloads, so this projection must remain transient and must be joined
	// back to the canonical direct plan by its derived SourceFileKey.
	for (FWorkingSource& WorkingSource : WorkingSources)
	{
		const FAngelscriptCachedSourceFile* MatchingFile = nullptr;
		for (const FAngelscriptCachedSourceFile& File
			: Candidate.DirectPlan.DirectProjection.Files)
		{
			if (File.ModuleKey == WorkingSource.ModuleKey
				&& File.SourceKind
					== ToCachedSourceKind(WorkingSource.Source.SourceKind)
				&& File.RelativeLogicalPath.Equals(
					WorkingSource.Source.VirtualPath.GetRelativePath(),
					ESearchCase::CaseSensitive))
			{
				if (MatchingFile != nullptr
					|| File.RawContentHash
						!= HashRawBytes(WorkingSource.RawBytes))
				{
					return Failure(
						EAngelscriptCacheSourceDiscoveryError::InvalidSourceDescriptor,
						TEXT("Current source projection does not map uniquely to the canonical direct plan"),
						ValidationFailure(
							EAngelscriptCacheValidationError::ConflictingKey));
				}
				MatchingFile = &File;
			}
		}
		if (MatchingFile == nullptr)
		{
			return Failure(
				EAngelscriptCacheSourceDiscoveryError::InvalidSourceDescriptor,
				TEXT("Current source projection is absent from the canonical direct plan"),
				ValidationFailure(
					EAngelscriptCacheValidationError::MissingGraphTarget));
		}

		FAngelscriptCacheCurrentSourceProjection& Projection =
			Candidate.CurrentSources.AddDefaulted_GetRef();
		Projection.SourceFileKey = MatchingFile->SourceFileKey;
		Projection.ModuleKey = MatchingFile->ModuleKey;
		Projection.VirtualPath = WorkingSource.Source.VirtualPath.ToString();
		Projection.RelativeFilename =
			WorkingSource.Source.RelativeFilename;
		Projection.AbsoluteFilename =
			WorkingSource.Source.AbsoluteFilename;
		Projection.RawSourceBytes = MoveTemp(WorkingSource.RawBytes);
	}
	Candidate.CurrentSources.Sort([](
		const FAngelscriptCacheCurrentSourceProjection& A,
		const FAngelscriptCacheCurrentSourceProjection& B)
	{
		return A.SourceFileKey.Hash < B.SourceFileKey.Hash;
	});

	Modules.Sort([](const FWorkingModule& A, const FWorkingModule& B)
	{
		return A.ModuleKey.Hash < B.ModuleKey.Hash;
	});
	FAngelscriptCacheExactFastPathEligibilityBatch EligibilityBatch;
	if (!Modules.IsEmpty())
	{
		TArray<FAngelscriptStableModuleKey> ModuleKeys;
		ModuleKeys.Reserve(Modules.Num());
		for (const FWorkingModule& Module : Modules)
		{
			ModuleKeys.Add(Module.ModuleKey);
		}
		FAngelscriptCacheReadBudget EligibilityBudget;
		const FAngelscriptCacheValidationResult EligibilityResult =
			FAngelscriptCacheSemanticArchive::
				QueryCurrentExactFastPathEligibilityBatch(
					Candidate.DirectPlan.DirectProjection,
					ModuleKeys,
					{},
					EligibilityBudget,
					EligibilityBatch);
		if (!EligibilityResult.IsSuccess())
		{
			return Failure(
				EAngelscriptCacheSourceDiscoveryError::EligibilityPlanningFailed,
				TEXT("Failed to plan exact-fast-path eligibility batch"),
				EligibilityResult);
		}
		if (EligibilityBatch.Entries.Num() != Modules.Num())
		{
			return Failure(
				EAngelscriptCacheSourceDiscoveryError::EligibilityPlanningFailed,
				TEXT("Eligibility batch did not cover every discovered module"),
				ValidationFailure(
					EAngelscriptCacheValidationError::MissingCoverage));
		}
	}
	for (int32 ModuleIndex = 0; ModuleIndex < Modules.Num(); ++ModuleIndex)
	{
		const FWorkingModule& Module = Modules[ModuleIndex];
		FAngelscriptCacheExactFastPathEligibilityBatchEntry& EligibilityEntry =
			EligibilityBatch.Entries[ModuleIndex];
		if (EligibilityEntry.ModuleKey.Hash != Module.ModuleKey.Hash)
		{
			return Failure(
				EAngelscriptCacheSourceDiscoveryError::EligibilityPlanningFailed,
				TEXT("Eligibility batch module order did not match discovery order"),
				ValidationFailure(
					EAngelscriptCacheValidationError::SourceGraphMismatch));
		}
		FAngelscriptCacheModuleSourcePlan ModulePlan;
		ModulePlan.ModuleKey = Module.ModuleKey;
		ModulePlan.CanonicalModuleName = Module.CanonicalModuleName;
		ModulePlan.SourceFileCount = Module.SourceFileCount;
		ModulePlan.bExactFastPathEligible =
			EligibilityEntry.Eligibility.bExactFastPathEligible;
		ModulePlan.MatchingIneligibleScopes =
			MoveTemp(EligibilityEntry.Eligibility.MatchingScopes);
		Candidate.Modules.Add(MoveTemp(ModulePlan));
	}
	Candidate.DiscoveredSourceCount = DiscoveredSources.Num();
	Candidate.LoadedSourceCount = WorkingSources.Num();
	OutResult = MoveTemp(Candidate);
	return {};
}

FAngelscriptCacheValidationResult
FAngelscriptCacheSourceDiscovery::BuildCurrentDependencyObservations(
	const FAngelscriptCacheDirectSourcePlan& CurrentDirectPlan,
	const FAngelscriptCachedSourceIndex& PersistedCandidate,
	FAngelscriptCacheDependencyObservationPlan& OutPlan)
{
	using namespace AngelscriptCacheSourceDiscovery_Private;

	OutPlan.Reset();
	if (!CurrentDirectPlan.DirectProjection.PreprocessorInputs.IsEmpty()
		|| !CurrentDirectPlan.DirectProjection.Edges.IsEmpty())
	{
		return ValidationFailure(
			EAngelscriptCacheValidationError::UnexpectedRecord);
	}
	FAngelscriptHash256 CurrentSnapshot;
	FAngelscriptCacheValidationResult Validation =
		FAngelscriptCacheSemanticArchive::ComputeSourceSnapshot(
			CurrentDirectPlan.DirectProjection, CurrentSnapshot);
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	if (CurrentSnapshot
		!= CurrentDirectPlan.DirectProjection.SourceSnapshot)
	{
		return ValidationFailure(
			EAngelscriptCacheValidationError::SourceSnapshotMismatch);
	}
	FAngelscriptHash256 CandidateSnapshot;
	Validation = FAngelscriptCacheSemanticArchive::ComputeSourceSnapshot(
		PersistedCandidate, CandidateSnapshot);
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	if (CandidateSnapshot != PersistedCandidate.SourceSnapshot)
	{
		return ValidationFailure(
			EAngelscriptCacheValidationError::SourceSnapshotMismatch);
	}

	FAngelscriptCacheDependencyObservationPlan CandidatePlan;
	CandidatePlan.Observations.Reserve(
		PersistedCandidate.PreprocessorInputs.Num());
	CandidatePlan.UnavailableInputKeys.Reserve(
		PersistedCandidate.PreprocessorInputs.Num());
	for (const FAngelscriptCachedPreprocessorInput& Input :
		PersistedCandidate.PreprocessorInputs)
	{
		const TOptional<FAngelscriptHash256> Observation =
			ResolveObservation(CurrentDirectPlan.DirectProjection, Input);
		if (Observation.IsSet())
		{
			CandidatePlan.Observations.Add({
				Input.InputKey, Observation.GetValue()});
		}
		else
		{
			CandidatePlan.UnavailableInputKeys.Add(Input.InputKey);
		}
	}
	CandidatePlan.Observations.Sort([](
		const FAngelscriptCacheObservedDependencyInput& A,
		const FAngelscriptCacheObservedDependencyInput& B)
	{
		return A.InputKey.Hash < B.InputKey.Hash;
	});
	CandidatePlan.UnavailableInputKeys.Sort([](
		const FAngelscriptCachedPreprocessorInputKey& A,
		const FAngelscriptCachedPreprocessorInputKey& B)
	{
		return A.Hash < B.Hash;
	});
	OutPlan = MoveTemp(CandidatePlan);
	return {};
}
