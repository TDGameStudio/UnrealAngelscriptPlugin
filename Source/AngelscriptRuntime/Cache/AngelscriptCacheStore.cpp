#include "Cache/AngelscriptCacheStore.h"

#include "Cache/AngelscriptCacheManifestPack.h"
#include "Cache/Private/AngelscriptCacheManifestPackValidation.h"

#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogAngelscriptCacheStore, Log, All);

namespace AngelscriptCacheStore_Private
{
	static bool ShouldStopAtFaultPoint(
		IAngelscriptCacheStoreFaultInjector* FaultInjector,
		const EAngelscriptCacheStoreFaultPoint Point)
	{
		return FaultInjector != nullptr && FaultInjector->ShouldStopAt(Point);
	}

	static FAngelscriptCacheStoreResult FaultInjected(
		const EAngelscriptCacheStoreStage Stage,
		const EAngelscriptCacheStorePathCategory PathCategory)
	{
		FAngelscriptCacheStoreResult Result = FAngelscriptCacheStoreResult::Failure(
			EAngelscriptCacheStoreError::FaultInjected, Stage, PathCategory);
		Result.CommitState = EAngelscriptCacheStoreCommitState::NotCommitted;
		return Result;
	}

	static FString JoinPath(const FStringView Parent, const FStringView Child)
	{
		FString Result(Parent);
		if (!Result.EndsWith(TEXT("/")))
		{
			Result.AppendChar(TEXT('/'));
		}
		Result.Append(Child);
		return Result;
	}

	static bool IsLowerFullHash(const FStringView Component)
	{
		if (Component.Len() != 64)
		{
			return false;
		}

		for (const TCHAR Character : Component)
		{
			if (!((Character >= TEXT('0') && Character <= TEXT('9'))
				|| (Character >= TEXT('a') && Character <= TEXT('f'))))
			{
				return false;
			}
		}
		return true;
	}

	static bool IsCanonicalAbsoluteRoot(const FString& Path)
	{
		if (Path.IsEmpty() || FPaths::IsRelative(Path) || Path.Contains(TEXT("\\")))
		{
			return false;
		}
		for (const TCHAR Character : Path)
		{
			if (Character == 0)
			{
				return false;
			}
		}

		TArray<FString> Components;
		Path.ParseIntoArray(Components, TEXT("/"), false);
		for (const FString& Component : Components)
		{
			if (Component == TEXT(".") || Component == TEXT(".."))
			{
				return false;
			}
		}
		return !Path.EndsWith(TEXT("/")) || FPaths::IsDrive(Path);
	}

	static bool IsDescendant(const FStringView Path, const FStringView Root)
	{
		if (Path.Len() <= Root.Len() || !Path.Left(Root.Len()).Equals(Root, ESearchCase::CaseSensitive))
		{
			return false;
		}
		return Root.EndsWith(TEXT("/")) || Path[Root.Len()] == TEXT('/');
	}

	static FString BuildTempPath(
		const FString& FinalPath,
		const FAngelscriptCacheWriterToken& WriterToken)
	{
		return FinalPath + TEXT(".tmp.") + WriterToken.ToString();
	}

	static bool HasCanonicalWriterTokenSuffix(
		const FString& FileName,
		const FStringView Prefix)
	{
		if (!FileName.StartsWith(Prefix, ESearchCase::CaseSensitive))
		{
			return false;
		}
		return FAngelscriptCacheWriterToken::TryParse(
			FileName.Mid(Prefix.Len())).IsSet();
	}

	static bool IsContentTempFileName(
		const FString& FileName,
		const FStringView TempMarker)
	{
		if (FileName.Len() <= 64
			|| !IsLowerFullHash(FileName.Left(64)))
		{
			return false;
		}
		return HasCanonicalWriterTokenSuffix(FileName.Mid(64), TempMarker);
	}

	static bool IsPointerTempFileName(const FString& FileName)
	{
		return HasCanonicalWriterTokenSuffix(
			FileName, TEXTVIEW("Current.ascurrent.tmp."))
			|| HasCanonicalWriterTokenSuffix(
				FileName, TEXTVIEW("Previous.ascurrent.tmp."))
			|| HasCanonicalWriterTokenSuffix(
				FileName, TEXTVIEW("PendingColdStart.ascurrent.tmp."));
	}

	class FStorePackSource final : public IAngelscriptCachePackSource
	{
	public:
		FStorePackSource(
			const FAngelscriptCacheStorePaths& InPaths,
			const FAngelscriptCacheReadLimits& InLimits,
			IAngelscriptCacheAtomicFileOps& InFileOps)
			: Paths(InPaths)
			, Limits(InLimits)
			, FileOps(InFileOps)
		{
		}

		virtual bool TryGetCompletePack(
			const FAngelscriptHash256& PackId,
			TConstArrayView<uint8>& OutBytes) override
		{
			OutBytes = TConstArrayView<uint8>();
			for (FLoadedPack& Loaded : LoadedPacks)
			{
				if (Loaded.PackId == PackId)
				{
					OutBytes = Loaded.Bytes;
					return true;
				}
			}

			FLoadedPack& Loaded = LoadedPacks.AddDefaulted_GetRef();
			Loaded.PackId = PackId;
			const FAngelscriptCacheStoreResult ReadResult = FileOps.ReopenReadAll(
				Paths.BuildPackPath(PackId), Limits.MaxPackBytes, Loaded.Bytes);
			if (!ReadResult.IsSuccess())
			{
				ReadFailure = ReadResult;
				LoadedPacks.Pop(EAllowShrinking::No);
				return false;
			}
			OutBytes = Loaded.Bytes;
			return true;
		}

		const TOptional<FAngelscriptCacheStoreResult>& GetReadFailure() const
		{
			return ReadFailure;
		}

	private:
		struct FLoadedPack
		{
			FAngelscriptHash256 PackId;
			TArray<uint8> Bytes;
		};

		const FAngelscriptCacheStorePaths& Paths;
		const FAngelscriptCacheReadLimits& Limits;
		IAngelscriptCacheAtomicFileOps& FileOps;
		TArray<FLoadedPack> LoadedPacks;
		TOptional<FAngelscriptCacheStoreResult> ReadFailure;
	};

	static bool EqualRecordId(
		const FAngelscriptCacheRecordId& A,
		const FAngelscriptCacheRecordId& B)
	{
		return A.Kind == B.Kind && A.ContentHash == B.ContentHash;
	}

	static bool EqualSemanticManifest(
		const FAngelscriptCacheGenerationManifest& A,
		const FAngelscriptCacheGenerationManifest& B)
	{
		if (A.Compatibility.Hash != B.Compatibility.Hash
			|| A.Context.Hash != B.Context.Hash
			|| A.Profile.Hash != B.Profile.Hash
			|| A.SourceSnapshot != B.SourceSnapshot
			|| !EqualRecordId(A.SourceIndexRecordId, B.SourceIndexRecordId)
			|| A.ModuleSnapshots.Num() != B.ModuleSnapshots.Num()
			|| A.Records.Num() != B.Records.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < A.ModuleSnapshots.Num(); ++Index)
		{
			const FAngelscriptCacheModuleSnapshotLink& ALink = A.ModuleSnapshots[Index];
			const FAngelscriptCacheModuleSnapshotLink& BLink = B.ModuleSnapshots[Index];
			if (ALink.ModuleKey != BLink.ModuleKey
				|| !EqualRecordId(ALink.RecordId, BLink.RecordId))
			{
				return false;
			}
		}

		for (int32 Index = 0; Index < A.Records.Num(); ++Index)
		{
			if (!EqualRecordId(A.Records[Index].RecordId, B.Records[Index].RecordId))
			{
				return false;
			}
		}
		return true;
	}
}

FAngelscriptCacheStoreResult FAngelscriptCacheStoreResult::Success()
{
	return FAngelscriptCacheStoreResult{};
}

FAngelscriptCacheStoreResult FAngelscriptCacheStoreResult::Failure(
	const EAngelscriptCacheStoreError Error,
	const EAngelscriptCacheStoreStage Stage,
	const EAngelscriptCacheStorePathCategory PathCategory)
{
	FAngelscriptCacheStoreResult Result;
	Result.Error = Error;
	Result.Stage = Stage;
	Result.CommitState = EAngelscriptCacheStoreCommitState::NotCommitted;
	Result.PathCategory = PathCategory;
	return Result;
}

bool FAngelscriptCacheStoreResult::IsSuccess() const
{
	return Error == EAngelscriptCacheStoreError::None;
}

FAngelscriptCacheReadSession::FAngelscriptCacheReadSession(
	const EAngelscriptCachePointerKind InPointerKind,
	const FAngelscriptHash256& InGenerationId,
	TUniquePtr<FAngelscriptCacheReadBudget>&& InBudget,
	FAngelscriptValidatedGeneration&& InGeneration,
	TArray<uint8>&& InManifestBytes,
	TUniquePtr<IAngelscriptCachePinnedFileHandle>&& InManifestHandle,
	TArray<TUniquePtr<IAngelscriptCachePinnedFileHandle>>&& InPackHandles)
	: PointerKind(InPointerKind)
	, GenerationId(InGenerationId)
	, Budget(MoveTemp(InBudget))
	, Generation(MoveTemp(InGeneration))
	, ManifestBytes(MoveTemp(InManifestBytes))
	, ManifestHandle(MoveTemp(InManifestHandle))
	, PackHandles(MoveTemp(InPackHandles))
{
	check(PointerKind != EAngelscriptCachePointerKind::Invalid);
	check(!GenerationId.IsZero());
	check(Budget.IsValid());
	check(ManifestHandle.IsValid());
}

FAngelscriptCacheReadSession::~FAngelscriptCacheReadSession() = default;

EAngelscriptCachePointerKind FAngelscriptCacheReadSession::GetPointerKind() const
{
	return PointerKind;
}

const FAngelscriptHash256& FAngelscriptCacheReadSession::GetGenerationId() const
{
	return GenerationId;
}

const FAngelscriptValidatedGeneration&
FAngelscriptCacheReadSession::GetGeneration() const
{
	return Generation;
}

const FAngelscriptCacheReadBudget& FAngelscriptCacheReadSession::GetBudget() const
{
	check(Budget.IsValid());
	return *Budget;
}

int32 FAngelscriptCacheReadSession::GetPinnedPackCount() const
{
	return PackHandles.Num();
}

struct FAngelscriptCacheReadSessionFactory
{
	static TUniquePtr<FAngelscriptCacheReadSession> Create(
		const EAngelscriptCachePointerKind PointerKind,
		const FAngelscriptHash256& GenerationId,
		TUniquePtr<FAngelscriptCacheReadBudget>&& Budget,
		FAngelscriptValidatedGeneration&& Generation,
		TArray<uint8>&& ManifestBytes,
		TUniquePtr<IAngelscriptCachePinnedFileHandle>&& ManifestHandle,
		TArray<TUniquePtr<IAngelscriptCachePinnedFileHandle>>&& PackHandles)
	{
		return TUniquePtr<FAngelscriptCacheReadSession>(
			new FAngelscriptCacheReadSession(
				PointerKind,
				GenerationId,
				MoveTemp(Budget),
				MoveTemp(Generation),
				MoveTemp(ManifestBytes),
				MoveTemp(ManifestHandle),
				MoveTemp(PackHandles)));
	}
};

FAngelscriptCacheWriterToken::FAngelscriptCacheWriterToken(FString&& InValue)
	: Value(MoveTemp(InValue))
{
}

TOptional<FAngelscriptCacheWriterToken> FAngelscriptCacheWriterToken::TryParse(
	const FStringView Token)
{
	const int32 Separator = Token.Find(TEXT("-"));
	if (Separator <= 0 || Separator != Token.Len() - 33
		|| Token.Find(TEXT("-"), Separator + 1) != INDEX_NONE)
	{
		return {};
	}

	if (Token[0] < TEXT('1') || Token[0] > TEXT('9'))
	{
		return {};
	}
	for (int32 Index = 1; Index < Separator; ++Index)
	{
		if (Token[Index] < TEXT('0') || Token[Index] > TEXT('9'))
		{
			return {};
		}
	}

	for (int32 Index = Separator + 1; Index < Token.Len(); ++Index)
	{
		const TCHAR Character = Token[Index];
		if (!((Character >= TEXT('0') && Character <= TEXT('9'))
			|| (Character >= TEXT('a') && Character <= TEXT('f'))))
		{
			return {};
		}
	}

	return FAngelscriptCacheWriterToken(FString(Token));
}

const FString& FAngelscriptCacheWriterToken::ToString() const
{
	return Value;
}

FString FAngelscriptCacheStorePaths::BuildPackPath(const FAngelscriptHash256& PackId) const
{
	return AngelscriptCacheStore_Private::JoinPath(
		PacksDirectory, PackId.ToHexString() + TEXT(".aspack"));
}

FString FAngelscriptCacheStorePaths::BuildManifestPath(
	const FAngelscriptHash256& GenerationId) const
{
	return AngelscriptCacheStore_Private::JoinPath(
		GenerationsDirectory, GenerationId.ToHexString() + TEXT(".asmanifest"));
}

FString FAngelscriptCacheStorePaths::BuildPackTempPath(
	const FAngelscriptHash256& PackId,
	const FAngelscriptCacheWriterToken& WriterToken) const
{
	return AngelscriptCacheStore_Private::BuildTempPath(BuildPackPath(PackId), WriterToken);
}

FString FAngelscriptCacheStorePaths::BuildManifestTempPath(
	const FAngelscriptHash256& GenerationId,
	const FAngelscriptCacheWriterToken& WriterToken) const
{
	return AngelscriptCacheStore_Private::BuildTempPath(
		BuildManifestPath(GenerationId), WriterToken);
}

FString FAngelscriptCacheStorePaths::BuildCurrentPointerTempPath(
	const FAngelscriptCacheWriterToken& WriterToken) const
{
	return AngelscriptCacheStore_Private::BuildTempPath(CurrentPointer, WriterToken);
}

FString FAngelscriptCacheStorePaths::BuildPreviousPointerTempPath(
	const FAngelscriptCacheWriterToken& WriterToken) const
{
	return AngelscriptCacheStore_Private::BuildTempPath(PreviousPointer, WriterToken);
}

FString FAngelscriptCacheStorePaths::BuildPendingColdStartPointerTempPath(
	const FAngelscriptCacheWriterToken& WriterToken) const
{
	return AngelscriptCacheStore_Private::BuildTempPath(PendingColdStartPointer, WriterToken);
}

FAngelscriptCacheStoreResult ResolveAngelscriptCacheRequestedBaseRoot(
	const FAngelscriptCacheRootSelectionInputs& Inputs,
	FString& OutRequestedBaseRoot)
{
	OutRequestedBaseRoot.Reset();
	if (Inputs.Override.IsSet())
	{
		const FString& Override = Inputs.Override.GetValue();
		if (Override.IsEmpty())
		{
			return FAngelscriptCacheStoreResult::Failure(
				EAngelscriptCacheStoreError::InvalidRoot,
				EAngelscriptCacheStoreStage::RootValidation,
				EAngelscriptCacheStorePathCategory::Root);
		}

		if (FPaths::IsRelative(Override))
		{
			if (Inputs.LaunchWorkingDirectory.IsEmpty()
				|| FPaths::IsRelative(Inputs.LaunchWorkingDirectory))
			{
				return FAngelscriptCacheStoreResult::Failure(
					EAngelscriptCacheStoreError::InvalidRoot,
					EAngelscriptCacheStoreStage::RootValidation,
					EAngelscriptCacheStorePathCategory::Root);
			}
			OutRequestedBaseRoot = AngelscriptCacheStore_Private::JoinPath(
				Inputs.LaunchWorkingDirectory, Override);
		}
		else
		{
			OutRequestedBaseRoot = Override;
		}
		return FAngelscriptCacheStoreResult::Success();
	}

	if (Inputs.ProjectSavedDirectory.IsEmpty())
	{
		return FAngelscriptCacheStoreResult::Failure(
			EAngelscriptCacheStoreError::InvalidRoot,
			EAngelscriptCacheStoreStage::RootValidation,
			EAngelscriptCacheStorePathCategory::Root);
	}

	OutRequestedBaseRoot = AngelscriptCacheStore_Private::JoinPath(
		AngelscriptCacheStore_Private::JoinPath(
			Inputs.ProjectSavedDirectory, TEXT("Angelscript")),
		TEXT("CacheV2"));
	return FAngelscriptCacheStoreResult::Success();
}

FAngelscriptCacheStoreResult BuildAngelscriptCacheStorePaths(
	const FString& RequestedBaseRoot,
	const FAngelscriptCacheCompatibilityKey& Compatibility,
	const FAngelscriptCacheContextKey& Context,
	IAngelscriptCacheAtomicFileOps& FileOps,
	FAngelscriptCacheStorePaths& OutPaths)
{
	OutPaths = FAngelscriptCacheStorePaths{};
	if (!FileOps.SupportsSharedAtomicCacheStore())
	{
		return FAngelscriptCacheStoreResult::Failure(
			EAngelscriptCacheStoreError::UnsupportedPlatformAtomicity,
			EAngelscriptCacheStoreStage::RootValidation,
			EAngelscriptCacheStorePathCategory::Root);
	}

	FAngelscriptCanonicalCacheRoot Root;
	FAngelscriptCacheStoreResult RootResult =
		FileOps.CanonicalizeAndValidateRoot(RequestedBaseRoot, Root);
	if (!RootResult.IsSuccess())
	{
		if (RootResult.Stage == EAngelscriptCacheStoreStage::None)
		{
			RootResult.Stage = EAngelscriptCacheStoreStage::RootValidation;
		}
		if (RootResult.PathCategory == EAngelscriptCacheStorePathCategory::None)
		{
			RootResult.PathCategory = EAngelscriptCacheStorePathCategory::Root;
		}
		return RootResult;
	}

	if (!AngelscriptCacheStore_Private::IsCanonicalAbsoluteRoot(Root.AbsolutePath)
		|| !AngelscriptCacheStore_Private::IsCanonicalAbsoluteRoot(Root.IdentityPath))
	{
		return FAngelscriptCacheStoreResult::Failure(
			EAngelscriptCacheStoreError::InvalidRoot,
			EAngelscriptCacheStoreStage::RootValidation,
			EAngelscriptCacheStorePathCategory::Root);
	}

	const FString CompatibilityHex = Compatibility.Hash.ToHexString();
	const FString ContextHex = Context.Hash.ToHexString();
	if (!AngelscriptCacheStore_Private::IsLowerFullHash(CompatibilityHex)
		|| !AngelscriptCacheStore_Private::IsLowerFullHash(ContextHex))
	{
		return FAngelscriptCacheStoreResult::Failure(
			EAngelscriptCacheStoreError::InvalidRoot,
			EAngelscriptCacheStoreStage::RootValidation,
			EAngelscriptCacheStorePathCategory::Root);
	}

	OutPaths.BaseRoot = Root.AbsolutePath;
	OutPaths.BaseRootIdentity = Root.IdentityPath;
	OutPaths.NamespaceRoot = AngelscriptCacheStore_Private::JoinPath(
		AngelscriptCacheStore_Private::JoinPath(Root.AbsolutePath, CompatibilityHex),
		ContextHex);
	OutPaths.NamespaceIdentity = AngelscriptCacheStore_Private::JoinPath(
		AngelscriptCacheStore_Private::JoinPath(Root.IdentityPath, CompatibilityHex),
		ContextHex);
	OutPaths.PacksDirectory = AngelscriptCacheStore_Private::JoinPath(
		OutPaths.NamespaceRoot, TEXT("Packs"));
	OutPaths.GenerationsDirectory = AngelscriptCacheStore_Private::JoinPath(
		OutPaths.NamespaceRoot, TEXT("Generations"));
	OutPaths.CurrentPointer = AngelscriptCacheStore_Private::JoinPath(
		OutPaths.NamespaceRoot, TEXT("Current.ascurrent"));
	OutPaths.PreviousPointer = AngelscriptCacheStore_Private::JoinPath(
		OutPaths.NamespaceRoot, TEXT("Previous.ascurrent"));
	OutPaths.PendingColdStartPointer = AngelscriptCacheStore_Private::JoinPath(
		OutPaths.NamespaceRoot, TEXT("PendingColdStart.ascurrent"));

	if (!AngelscriptCacheStore_Private::IsDescendant(
			OutPaths.NamespaceIdentity, OutPaths.BaseRootIdentity)
		|| !AngelscriptCacheStore_Private::IsDescendant(
			OutPaths.PacksDirectory, OutPaths.BaseRoot)
		|| !AngelscriptCacheStore_Private::IsDescendant(
			OutPaths.GenerationsDirectory, OutPaths.BaseRoot))
	{
		OutPaths = FAngelscriptCacheStorePaths{};
		return FAngelscriptCacheStoreResult::Failure(
			EAngelscriptCacheStoreError::PathEscapesRoot,
			EAngelscriptCacheStoreStage::RootValidation,
			EAngelscriptCacheStorePathCategory::Root);
	}

	return FAngelscriptCacheStoreResult::Success();
}

FAngelscriptCacheStoreResult EnsureAngelscriptCacheStoreDirectories(
	const FAngelscriptCacheStorePaths& Paths,
	IAngelscriptCacheAtomicFileOps& FileOps)
{
	auto DirectoryFailure = [](FAngelscriptCacheStoreResult Result)
	{
		if (Result.Stage == EAngelscriptCacheStoreStage::None)
		{
			Result.Stage = EAngelscriptCacheStoreStage::RootValidation;
		}
		if (Result.PathCategory == EAngelscriptCacheStorePathCategory::None)
		{
			Result.PathCategory = EAngelscriptCacheStorePathCategory::Root;
		}
		return Result;
	};
	if (!AngelscriptCacheStore_Private::IsCanonicalAbsoluteRoot(Paths.BaseRoot)
		|| !AngelscriptCacheStore_Private::IsCanonicalAbsoluteRoot(Paths.BaseRootIdentity)
		|| !AngelscriptCacheStore_Private::IsDescendant(
			Paths.NamespaceRoot, Paths.BaseRoot)
		|| !AngelscriptCacheStore_Private::IsDescendant(
			Paths.PacksDirectory, Paths.NamespaceRoot)
		|| !AngelscriptCacheStore_Private::IsDescendant(
			Paths.GenerationsDirectory, Paths.NamespaceRoot))
	{
		return FAngelscriptCacheStoreResult::Failure(
			EAngelscriptCacheStoreError::PathEscapesRoot,
			EAngelscriptCacheStoreStage::RootValidation,
			EAngelscriptCacheStorePathCategory::Root);
	}

	const FString* Directories[] = {
		&Paths.BaseRoot,
		&Paths.NamespaceRoot,
		&Paths.PacksDirectory,
		&Paths.GenerationsDirectory,
	};
	for (const FString* Directory : Directories)
	{
		FAngelscriptCacheStoreResult Result = FileOps.EnsureDirectoryTree(*Directory);
		if (!Result.IsSuccess())
		{
			return DirectoryFailure(MoveTemp(Result));
		}
	}

	const FString ExpectedIdentities[] = {
		Paths.NamespaceIdentity,
		AngelscriptCacheStore_Private::JoinPath(Paths.NamespaceIdentity, TEXT("packs")),
		AngelscriptCacheStore_Private::JoinPath(Paths.NamespaceIdentity, TEXT("generations")),
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ExpectedIdentities); ++Index)
	{
		FAngelscriptCanonicalCacheRoot Canonical;
		FAngelscriptCacheStoreResult Result = FileOps.CanonicalizeAndValidateRoot(
			*Directories[Index + 1], Canonical);
		if (!Result.IsSuccess())
		{
			return DirectoryFailure(MoveTemp(Result));
		}
		if (!Canonical.IdentityPath.Equals(
			ExpectedIdentities[Index], ESearchCase::CaseSensitive))
		{
			return FAngelscriptCacheStoreResult::Failure(
				EAngelscriptCacheStoreError::PathEscapesRoot,
				EAngelscriptCacheStoreStage::RootValidation,
				EAngelscriptCacheStorePathCategory::Root);
		}
	}

	return FAngelscriptCacheStoreResult::Success();
}

FAngelscriptCacheStoreResult CleanupAngelscriptCacheStaleTempsUnderLock(
	const FAngelscriptCacheStorePaths& Paths,
	IAngelscriptCacheNamespaceLockHandle& NamespaceLock,
	IAngelscriptCacheAtomicFileOps& FileOps)
{
	(void)NamespaceLock;
	struct FStaleTempCandidate
	{
		FString Path;
		EAngelscriptCacheStorePathCategory Category =
			EAngelscriptCacheStorePathCategory::None;
	};

	auto WithCleanupContext = [](
		FAngelscriptCacheStoreResult Result,
		const EAngelscriptCacheStorePathCategory Category)
	{
		Result.Stage = EAngelscriptCacheStoreStage::TempCleanup;
		if (Result.PathCategory == EAngelscriptCacheStorePathCategory::None)
		{
			Result.PathCategory = Category;
		}
		return Result;
	};

	TArray<FString> NamespaceNames;
	TArray<FString> PackNames;
	TArray<FString> ManifestNames;
	struct FDirectoryEnumeration
	{
		const FString* Directory = nullptr;
		TArray<FString>* Names = nullptr;
		EAngelscriptCacheStorePathCategory Category =
			EAngelscriptCacheStorePathCategory::None;
	};
	const FDirectoryEnumeration Enumerations[] = {
		{&Paths.NamespaceRoot, &NamespaceNames,
			EAngelscriptCacheStorePathCategory::Root},
		{&Paths.PacksDirectory, &PackNames,
			EAngelscriptCacheStorePathCategory::Pack},
		{&Paths.GenerationsDirectory, &ManifestNames,
			EAngelscriptCacheStorePathCategory::Manifest},
	};
	for (const FDirectoryEnumeration& Enumeration : Enumerations)
	{
		FAngelscriptCacheStoreResult Result = FileOps.EnumerateDirectFileNames(
			*Enumeration.Directory, *Enumeration.Names);
		if (!Result.IsSuccess())
		{
			return WithCleanupContext(MoveTemp(Result), Enumeration.Category);
		}
	}

	TArray<FStaleTempCandidate> Candidates;
	auto AddRecognized = [&Candidates](
		const FString& Directory,
		const TArray<FString>& Names,
		const EAngelscriptCacheStorePathCategory Category,
		TFunctionRef<bool(const FString&)> IsRecognized)
	{
		for (const FString& Name : Names)
		{
			if (Name.IsEmpty() || Name.Contains(TEXT("/"))
				|| Name.Contains(TEXT("\\")) || !IsRecognized(Name))
			{
				continue;
			}
			FStaleTempCandidate& Candidate = Candidates.AddDefaulted_GetRef();
			Candidate.Path = AngelscriptCacheStore_Private::JoinPath(Directory, Name);
			Candidate.Category = Category;
		}
	};
	AddRecognized(
		Paths.NamespaceRoot,
		NamespaceNames,
		EAngelscriptCacheStorePathCategory::Root,
		[](const FString& Name)
		{
			return AngelscriptCacheStore_Private::IsPointerTempFileName(Name);
		});
	AddRecognized(
		Paths.PacksDirectory,
		PackNames,
		EAngelscriptCacheStorePathCategory::Pack,
		[](const FString& Name)
		{
			return AngelscriptCacheStore_Private::IsContentTempFileName(
				Name, TEXTVIEW(".aspack.tmp."));
		});
	AddRecognized(
		Paths.GenerationsDirectory,
		ManifestNames,
		EAngelscriptCacheStorePathCategory::Manifest,
		[](const FString& Name)
		{
			return AngelscriptCacheStore_Private::IsContentTempFileName(
				Name, TEXTVIEW(".asmanifest.tmp."));
		});

	Candidates.Sort([](
		const FStaleTempCandidate& Left,
		const FStaleTempCandidate& Right)
	{
		return Left.Path < Right.Path;
	});
	TOptional<FAngelscriptCacheStoreResult> FirstFailure;
	FString PreviousPath;
	for (const FStaleTempCandidate& Candidate : Candidates)
	{
		if (Candidate.Path == PreviousPath)
		{
			continue;
		}
		PreviousPath = Candidate.Path;
		FAngelscriptCacheStoreResult Result = FileOps.RemoveOwnTemp(Candidate.Path);
		if (!Result.IsSuccess() && !FirstFailure.IsSet())
		{
			FirstFailure = WithCleanupContext(MoveTemp(Result), Candidate.Category);
		}
	}
	return FirstFailure.IsSet()
		? FirstFailure.GetValue()
		: FAngelscriptCacheStoreResult::Success();
}

FAngelscriptCacheStoreResult BuildAngelscriptCacheNamespaceLockName(
	const FAngelscriptCacheStorePaths& Paths,
	FString& OutLockName)
{
	OutLockName.Reset();
	if (!AngelscriptCacheStore_Private::IsCanonicalAbsoluteRoot(Paths.BaseRootIdentity)
		|| !AngelscriptCacheStore_Private::IsCanonicalAbsoluteRoot(Paths.NamespaceIdentity)
		|| !AngelscriptCacheStore_Private::IsDescendant(
			Paths.NamespaceIdentity, Paths.BaseRootIdentity))
	{
		return FAngelscriptCacheStoreResult::Failure(
			EAngelscriptCacheStoreError::InvalidRoot,
			EAngelscriptCacheStoreStage::RootValidation,
			EAngelscriptCacheStorePathCategory::Root);
	}

	FAngelscriptArtifactCanonicalWriter Writer(TEXT("cache-store-lock"));
	Writer.WriteString(Paths.NamespaceIdentity);
	OutLockName = TEXT("UEASCacheV2-") + Writer.FinalizeHash().ToHexString();
	return FAngelscriptCacheStoreResult::Success();
}

FAngelscriptCacheStoreResult AcquireAngelscriptCacheNamespaceLock(
	const FAngelscriptCacheStorePaths& Paths,
	const double DeadlineSeconds,
	TFunctionRef<bool()> IsCancellationRequested,
	IAngelscriptCacheNamespaceLockOps& LockOps,
	TUniquePtr<IAngelscriptCacheNamespaceLockHandle>& OutLock)
{
	OutLock.Reset();
	FString LockName;
	FAngelscriptCacheStoreResult NameResult =
		BuildAngelscriptCacheNamespaceLockName(Paths, LockName);
	if (!NameResult.IsSuccess())
	{
		return NameResult;
	}

	auto LockFailure = [](const EAngelscriptCacheStoreError Error)
	{
		return FAngelscriptCacheStoreResult::Failure(
			Error, EAngelscriptCacheStoreStage::LockAcquisition);
	};
	if (IsCancellationRequested())
	{
		return LockFailure(EAngelscriptCacheStoreError::Cancelled);
	}

	static constexpr double MaxWaitSliceSeconds = 0.1;
	for (;;)
	{
		const double NowSeconds = LockOps.MonotonicSeconds();
		if (!FMath::IsFinite(DeadlineSeconds)
			|| !FMath::IsFinite(NowSeconds)
			|| DeadlineSeconds <= NowSeconds)
		{
			return LockFailure(EAngelscriptCacheStoreError::LockTimeout);
		}

		const double WaitSeconds = FMath::Min(
			MaxWaitSliceSeconds, DeadlineSeconds - NowSeconds);
		const FTimespan WaitSlice = FTimespan::FromSeconds(WaitSeconds);
		if (WaitSlice <= FTimespan::Zero())
		{
			return LockFailure(EAngelscriptCacheStoreError::LockTimeout);
		}

		OutLock = LockOps.TryAcquire(LockName, WaitSlice);
		if (OutLock.IsValid())
		{
			return FAngelscriptCacheStoreResult::Success();
		}
		if (IsCancellationRequested())
		{
			return LockFailure(EAngelscriptCacheStoreError::Cancelled);
		}
	}
}

FAngelscriptCacheStoreResult EvaluateAngelscriptCacheRebase(
	const TOptional<FAngelscriptHash256>& ObservedBaseGenerationId,
	const TOptional<FAngelscriptHash256>& ActualGenerationId,
	const FAngelscriptCacheGenerationManifest* ActualManifest,
	const FAngelscriptCacheGenerationManifest& PreparedManifest,
	FAngelscriptCacheRebasePlan& OutPlan)
{
	OutPlan = FAngelscriptCacheRebasePlan{};
	auto RebaseFailure = [&](const EAngelscriptCacheStoreError Error)
	{
		FAngelscriptCacheStoreResult Result = FAngelscriptCacheStoreResult::Failure(
			Error, EAngelscriptCacheStoreStage::Rebase);
		Result.GenerationBefore = ActualGenerationId;
		return Result;
	};
	if (ActualGenerationId.IsSet() != (ActualManifest != nullptr))
	{
		return RebaseFailure(EAngelscriptCacheStoreError::PointerInvalid);
	}

	const bool bBaseUnchanged =
		ObservedBaseGenerationId.IsSet() == ActualGenerationId.IsSet()
		&& (!ActualGenerationId.IsSet()
			|| ObservedBaseGenerationId.GetValue() == ActualGenerationId.GetValue());
	if (bBaseUnchanged)
	{
		OutPlan.Action = EAngelscriptCacheRebaseAction::ProceedFromObservedBase;
		OutPlan.SelectedGenerationId = ActualGenerationId;
		return FAngelscriptCacheStoreResult::Success();
	}

	if (!ActualGenerationId.IsSet())
	{
		return RebaseFailure(EAngelscriptCacheStoreError::NeedsSourceRevalidation);
	}

	check(ActualManifest != nullptr);
	if (ActualManifest->Compatibility.Hash != PreparedManifest.Compatibility.Hash
		|| ActualManifest->Context.Hash != PreparedManifest.Context.Hash
		|| ActualManifest->Profile.Hash != PreparedManifest.Profile.Hash
		|| ActualManifest->SourceSnapshot != PreparedManifest.SourceSnapshot)
	{
		return RebaseFailure(EAngelscriptCacheStoreError::NeedsSourceRevalidation);
	}
	if (!AngelscriptCacheStore_Private::EqualSemanticManifest(
		*ActualManifest, PreparedManifest))
	{
		return RebaseFailure(EAngelscriptCacheStoreError::RebaseSemanticConflict);
	}

	OutPlan.Action = EAngelscriptCacheRebaseAction::AlreadySelected;
	OutPlan.SelectedGenerationId = ActualGenerationId;
	return FAngelscriptCacheStoreResult::Success();
}

FAngelscriptCacheStoreResult ReadAndValidateAngelscriptCacheGenerationUnderLock(
	const FAngelscriptCacheStorePaths& Paths,
	const FAngelscriptHash256& GenerationId,
	const FAngelscriptCacheReadLimits& Limits,
	FAngelscriptCacheReadBudget& Budget,
	IAngelscriptCacheStorageCodec& Codec,
	IAngelscriptCacheNamespaceLockHandle& NamespaceLock,
	IAngelscriptCacheAtomicFileOps& FileOps,
	TOptional<FAngelscriptValidatedGeneration>& OutGeneration)
{
	(void)NamespaceLock;
	OutGeneration.Reset();
	auto SetCandidateContext = [](
		FAngelscriptCacheStoreResult Result,
		const EAngelscriptCacheStorePathCategory PathCategory)
	{
		if (Result.Stage == EAngelscriptCacheStoreStage::None)
		{
			Result.Stage = EAngelscriptCacheStoreStage::CandidateValidation;
		}
		if (Result.PathCategory == EAngelscriptCacheStorePathCategory::None)
		{
			Result.PathCategory = PathCategory;
		}
		return Result;
	};

	TArray<uint8> ManifestBytes;
	FAngelscriptCacheStoreResult ManifestRead = FileOps.ReopenReadAll(
		Paths.BuildManifestPath(GenerationId),
		Limits.MaxManifestBytes,
		ManifestBytes);
	if (!ManifestRead.IsSuccess())
	{
		return SetCandidateContext(
			MoveTemp(ManifestRead),
			EAngelscriptCacheStorePathCategory::Manifest);
	}

	AngelscriptCacheStore_Private::FStorePackSource PackSource(
		Paths, Limits, FileOps);
	const FAngelscriptCacheValidationResult Validation =
		ValidateAngelscriptCacheGeneration(
			ManifestBytes,
			GenerationId,
			PackSource,
			Limits,
			Budget,
			Codec,
			OutGeneration);
	if (Validation.IsSuccess())
	{
		return FAngelscriptCacheStoreResult::Success();
	}

	OutGeneration.Reset();
	if (PackSource.GetReadFailure().IsSet())
	{
		return SetCandidateContext(
			PackSource.GetReadFailure().GetValue(),
			EAngelscriptCacheStorePathCategory::Pack);
	}

	const bool bManifestFailure =
		Validation.Stage == EAngelscriptCacheValidationStage::ManifestDecode
		|| Validation.Stage == EAngelscriptCacheValidationStage::ManifestGraph;
	FAngelscriptCacheStoreResult Result = FAngelscriptCacheStoreResult::Failure(
		EAngelscriptCacheStoreError::ContentValidationFailed,
		EAngelscriptCacheStoreStage::CandidateValidation,
		bManifestFailure
			? EAngelscriptCacheStorePathCategory::Manifest
			: EAngelscriptCacheStorePathCategory::Pack);
	Result.ContentValidation = Validation;
	return Result;
}

FAngelscriptCacheStoreResult OpenBestAngelscriptCacheReadSession(
	const FAngelscriptCacheStorePaths& Paths,
	const FAngelscriptCacheReadSelection& Selection,
	const FAngelscriptCacheReadLimits& Limits,
	const double LockDeadlineSeconds,
	TFunctionRef<bool()> IsCancellationRequested,
	IAngelscriptCacheStorageCodec& Codec,
	IAngelscriptCacheNamespaceLockOps& LockOps,
	IAngelscriptCacheAtomicFileOps& FileOps,
	TUniquePtr<FAngelscriptCacheReadSession>& OutSession)
{
	using AngelscriptCacheManifestPack_Private::CompleteGenerationValidation;
	using AngelscriptCacheManifestPack_Private::FPreparedGenerationValidation;
	using AngelscriptCacheManifestPack_Private::PrepareGenerationValidation;

	OutSession.Reset();
	TUniquePtr<FAngelscriptCacheReadBudget> Budget =
		MakeUnique<FAngelscriptCacheReadBudget>();

	auto Cancelled = []()
	{
		return FAngelscriptCacheStoreResult::Failure(
			EAngelscriptCacheStoreError::Cancelled,
			EAngelscriptCacheStoreStage::SessionPin);
	};
	auto ContentFailure = [](
		const FAngelscriptCacheValidationResult& Validation,
		const EAngelscriptCacheStorePathCategory PathCategory)
	{
		FAngelscriptCacheStoreResult Result =
			FAngelscriptCacheStoreResult::Failure(
				EAngelscriptCacheStoreError::ContentValidationFailed,
				EAngelscriptCacheStoreStage::SessionPin,
				PathCategory);
		Result.ContentValidation = Validation;
		return Result;
	};
	auto IsBudgetFailure = [](
		const FAngelscriptCacheValidationResult& Validation)
	{
		return Validation.Error
			== EAngelscriptCacheValidationError::BudgetExceeded;
	};
	auto MatchesSelection = [&Selection](
		const FAngelscriptCacheGenerationManifest& Manifest)
	{
		return Manifest.Compatibility.Hash == Selection.Compatibility.Hash
			&& Manifest.Context.Hash == Selection.Context.Hash
			&& Manifest.Profile.Hash == Selection.Profile.Hash
			&& (!Selection.bRequireSourceSnapshotMatch
				|| Manifest.SourceSnapshot == Selection.SourceSnapshot);
	};

	struct FPinnedPack
	{
		FAngelscriptHash256 PackId;
		TUniquePtr<IAngelscriptCachePinnedFileHandle> Handle;
		TArray<uint8> Bytes;
	};
	class FPinnedPackSource final : public IAngelscriptCachePackSource
	{
	public:
		explicit FPinnedPackSource(const TArray<FPinnedPack>& InPacks)
			: Packs(InPacks)
		{
		}

		virtual bool TryGetCompletePack(
			const FAngelscriptHash256& PackId,
			TConstArrayView<uint8>& OutBytes) override
		{
			for (const FPinnedPack& Pack : Packs)
			{
				if (Pack.PackId == PackId)
				{
					OutBytes = Pack.Bytes;
					return true;
				}
			}
			OutBytes = {};
			return false;
		}

	private:
		const TArray<FPinnedPack>& Packs;
	};

	const EAngelscriptCachePointerKind CandidateKinds[] = {
		EAngelscriptCachePointerKind::Current,
		EAngelscriptCachePointerKind::Previous,
		EAngelscriptCachePointerKind::PendingColdStart,
	};
	const int32 CandidateCount = Selection.bAllowPendingColdStart ? 3 : 2;
	for (int32 CandidateIndex = 0; CandidateIndex < CandidateCount; ++CandidateIndex)
	{
		if (IsCancellationRequested())
		{
			return Cancelled();
		}
		const EAngelscriptCachePointerKind Kind = CandidateKinds[CandidateIndex];
		TUniquePtr<IAngelscriptCacheNamespaceLockHandle> NamespaceLock;
		FAngelscriptCacheStoreResult Result = AcquireAngelscriptCacheNamespaceLock(
			Paths,
			LockDeadlineSeconds,
			IsCancellationRequested,
			LockOps,
			NamespaceLock);
		if (!Result.IsSuccess())
		{
			return Result;
		}
		check(NamespaceLock.IsValid());

		TOptional<FAngelscriptHash256> GenerationId;
		Result = ReadAngelscriptCachePointerSlot(
			Paths, Kind, FileOps, GenerationId);
		if (!Result.IsSuccess())
		{
			if (Result.Error == EAngelscriptCacheStoreError::PointerInvalid)
			{
				continue;
			}
			return Result;
		}
		if (!GenerationId.IsSet())
		{
			continue;
		}

		TUniquePtr<IAngelscriptCachePinnedFileHandle> ManifestHandle;
		Result = FileOps.OpenReadPinned(
			Paths.BuildManifestPath(GenerationId.GetValue()),
			Limits.MaxManifestBytes,
			ManifestHandle);
		if (!Result.IsSuccess())
		{
			if (Result.Error == EAngelscriptCacheStoreError::ManifestMissing
				|| Result.Error == EAngelscriptCacheStoreError::ReadFailed)
			{
				continue;
			}
			Result.Stage = EAngelscriptCacheStoreStage::SessionPin;
			Result.PathCategory = EAngelscriptCacheStorePathCategory::Manifest;
			return Result;
		}
		check(ManifestHandle.IsValid());
		TArray<uint8> ManifestBytes;
		Result = ManifestHandle->ReadAll(ManifestBytes);
		if (!Result.IsSuccess())
		{
			continue;
		}

		FPreparedGenerationValidation Prepared;
		const FAngelscriptCacheValidationResult PrepareResult =
			PrepareGenerationValidation(
				ManifestBytes,
				GenerationId.GetValue(),
				Limits,
				*Budget,
				Prepared);
		if (!PrepareResult.IsSuccess())
		{
			if (IsBudgetFailure(PrepareResult))
			{
				return ContentFailure(
					PrepareResult,
					EAngelscriptCacheStorePathCategory::Manifest);
			}
			continue;
		}
		if (!MatchesSelection(Prepared.GetManifest()))
		{
			continue;
		}

		const TConstArrayView<FAngelscriptHash256> PackIds =
			Prepared.GetDistinctPackIds();
		const uint64 HandleCount = static_cast<uint64>(PackIds.Num()) + 1;
		FAngelscriptCacheTemporaryResidentReservation HandleReservation;
		if (HandleCount > MAX_uint64
				/ static_cast<uint64>(
					sizeof(TUniquePtr<IAngelscriptCachePinnedFileHandle>))
			|| !Budget->TryReserveTemporaryDecoded(
				HandleCount
					* static_cast<uint64>(
						sizeof(TUniquePtr<IAngelscriptCachePinnedFileHandle>)),
				Limits,
				HandleReservation))
		{
			const FAngelscriptCacheValidationResult HandleBudget =
				FAngelscriptCacheValidationResult::AtStage(
					EAngelscriptCacheValidationError::BudgetExceeded,
					static_cast<EAngelscriptCacheRecordKind>(0),
					EAngelscriptCacheValidationStage::ManifestDecode);
			return ContentFailure(
				HandleBudget,
				EAngelscriptCacheStorePathCategory::Manifest);
		}

		TArray<FPinnedPack> PinnedPacks;
		PinnedPacks.Reserve(PackIds.Num());
		bool bCandidateUnavailable = false;
		for (const FAngelscriptHash256& PackId : PackIds)
		{
			FPinnedPack& Pack = PinnedPacks.AddDefaulted_GetRef();
			Pack.PackId = PackId;
			Result = FileOps.OpenReadPinned(
				Paths.BuildPackPath(PackId),
				Limits.MaxPackBytes,
				Pack.Handle);
			if (!Result.IsSuccess())
			{
				if (Result.Error == EAngelscriptCacheStoreError::PackMissing
					|| Result.Error == EAngelscriptCacheStoreError::ReadFailed)
				{
					bCandidateUnavailable = true;
					break;
				}
				Result.Stage = EAngelscriptCacheStoreStage::SessionPin;
				Result.PathCategory = EAngelscriptCacheStorePathCategory::Pack;
				return Result;
			}
		}
		if (bCandidateUnavailable)
		{
			continue;
		}

		// Every immutable handle is now pinned. Expensive Pack reads, record decode
		// and graph validation must not extend the namespace-lock critical section.
		NamespaceLock.Reset();
		if (IsCancellationRequested())
		{
			return Cancelled();
		}
		for (FPinnedPack& Pack : PinnedPacks)
		{
			Result = Pack.Handle->ReadAll(Pack.Bytes);
			if (!Result.IsSuccess())
			{
				bCandidateUnavailable = true;
				break;
			}
		}
		if (bCandidateUnavailable)
		{
			continue;
		}

		FPinnedPackSource PackSource(PinnedPacks);
		TOptional<FAngelscriptValidatedGeneration> Validated;
		const FAngelscriptCacheValidationResult Validation =
			CompleteGenerationValidation(
				MoveTemp(Prepared), PackSource, Codec, Validated);
		if (!Validation.IsSuccess())
		{
			if (IsBudgetFailure(Validation))
			{
				return ContentFailure(
					Validation,
					Validation.Stage
						== EAngelscriptCacheValidationStage::ManifestGraph
							? EAngelscriptCacheStorePathCategory::Manifest
							: EAngelscriptCacheStorePathCategory::Pack);
			}
			continue;
		}
		check(Validated.IsSet());
		if (!HandleReservation.PromoteToRetained())
		{
			const FAngelscriptCacheValidationResult HandlePromotion =
				FAngelscriptCacheValidationResult::AtStage(
					EAngelscriptCacheValidationError::Overflow,
					static_cast<EAngelscriptCacheRecordKind>(0),
					EAngelscriptCacheValidationStage::ManifestGraph);
			return ContentFailure(
				HandlePromotion,
				EAngelscriptCacheStorePathCategory::Manifest);
		}

		TArray<TUniquePtr<IAngelscriptCachePinnedFileHandle>> PackHandles;
		PackHandles.Reserve(PinnedPacks.Num());
		for (FPinnedPack& Pack : PinnedPacks)
		{
			PackHandles.Add(MoveTemp(Pack.Handle));
		}
		OutSession = FAngelscriptCacheReadSessionFactory::Create(
				Kind,
				GenerationId.GetValue(),
				MoveTemp(Budget),
				MoveTemp(Validated.GetValue()),
				MoveTemp(ManifestBytes),
				MoveTemp(ManifestHandle),
				MoveTemp(PackHandles));
		return FAngelscriptCacheStoreResult::Success();
	}

	return FAngelscriptCacheStoreResult::Success();
}

FAngelscriptCacheStoreResult PublishAngelscriptCacheGeneration(
	const FAngelscriptCacheStorePaths& Paths,
	const EAngelscriptCachePublicationDisposition Disposition,
	const TOptional<FAngelscriptHash256>& ObservedGenerationId,
	const TConstArrayView<FAngelscriptEncodedPack> NewPacks,
	const FAngelscriptCacheGenerationManifest& PreparedManifest,
	const FAngelscriptEncodedCacheGenerationManifest& EncodedManifest,
	const FAngelscriptCacheWriterToken& WriterToken,
	const FAngelscriptCacheReadLimits& Limits,
	FAngelscriptCacheReadBudget& Budget,
	const double LockDeadlineSeconds,
	TFunctionRef<bool()> IsCancellationRequested,
	IAngelscriptCacheStorageCodec& Codec,
	IAngelscriptCacheNamespaceLockOps& LockOps,
	IAngelscriptCacheAtomicFileOps& FileOps,
	IAngelscriptCacheStoreFaultInjector* FaultInjector)
{
	const bool bPublishCurrent =
		Disposition == EAngelscriptCachePublicationDisposition::Current;
	const bool bPublishPending =
		Disposition == EAngelscriptCachePublicationDisposition::PendingColdStart;
	const EAngelscriptCachePointerKind TargetKind = bPublishCurrent
		? EAngelscriptCachePointerKind::Current
		: EAngelscriptCachePointerKind::PendingColdStart;
	auto FailBeforeCommit = [&](FAngelscriptCacheStoreResult Result)
	{
		Result.CommitState = EAngelscriptCacheStoreCommitState::NotCommitted;
		Result.GenerationBefore.Reset();
		Result.GenerationAfter.Reset();
		return Result;
	};
	if ((!bPublishCurrent && !bPublishPending)
		|| EncodedManifest.ComputedGenerationId.IsZero())
	{
		return FailBeforeCommit(FAngelscriptCacheStoreResult::Failure(
			EAngelscriptCacheStoreError::PointerInvalid,
			EAngelscriptCacheStoreStage::Rebase));
	}

	FAngelscriptEncodedCacheGenerationManifest ReencodedManifest;
	const FAngelscriptCacheValidationResult EncodeResult =
		EncodeAngelscriptCacheGenerationManifest(
			PreparedManifest, ReencodedManifest);
	const bool bExactManifest = EncodeResult.IsSuccess()
		&& ReencodedManifest.ComputedGenerationId
			== EncodedManifest.ComputedGenerationId
		&& ReencodedManifest.CompleteBytes.Num()
			== EncodedManifest.CompleteBytes.Num()
		&& (ReencodedManifest.CompleteBytes.IsEmpty()
			|| FMemory::Memcmp(
				ReencodedManifest.CompleteBytes.GetData(),
				EncodedManifest.CompleteBytes.GetData(),
				ReencodedManifest.CompleteBytes.Num()) == 0);
	if (!bExactManifest)
	{
		FAngelscriptCacheStoreResult Result = FAngelscriptCacheStoreResult::Failure(
			EAngelscriptCacheStoreError::ContentValidationFailed,
			EAngelscriptCacheStoreStage::CandidateValidation,
			EAngelscriptCacheStorePathCategory::Manifest);
		if (!EncodeResult.IsSuccess())
		{
			Result.ContentValidation = EncodeResult;
		}
		return Result;
	}

	const FAngelscriptCacheValidationResult PublicationManifestValidation =
		ValidateAngelscriptCacheGenerationManifestValue(
			PreparedManifest, Limits);
	if (!PublicationManifestValidation.IsSuccess())
	{
		FAngelscriptCacheStoreResult ValidationFailure =
			FAngelscriptCacheStoreResult::Failure(
				EAngelscriptCacheStoreError::ContentValidationFailed,
				EAngelscriptCacheStoreStage::SessionPin,
				EAngelscriptCacheStorePathCategory::Manifest);
		ValidationFailure.ContentValidation = PublicationManifestValidation;
		return FailBeforeCommit(MoveTemp(ValidationFailure));
	}

	TArray<int32> PackOrdinals;
	PackOrdinals.Reserve(NewPacks.Num());
	for (int32 PackOrdinal = 0; PackOrdinal < NewPacks.Num(); ++PackOrdinal)
	{
		const FAngelscriptHash256& PackId = NewPacks[PackOrdinal].PackId;
		bool bReferenced = false;
		for (const FAngelscriptCacheRecordIndexEntry& Record : PreparedManifest.Records)
		{
			if (Record.Location.PackId == PackId)
			{
				bReferenced = true;
				break;
			}
		}
		if (!bReferenced)
		{
			return FailBeforeCommit(FAngelscriptCacheStoreResult::Failure(
				EAngelscriptCacheStoreError::ContentValidationFailed,
				EAngelscriptCacheStoreStage::PackTemp,
				EAngelscriptCacheStorePathCategory::Pack));
		}
		PackOrdinals.Add(PackOrdinal);
	}
	PackOrdinals.Sort([&](const int32 Left, const int32 Right)
	{
		return NewPacks[Left].PackId < NewPacks[Right].PackId;
	});
	for (int32 Ordinal = 1; Ordinal < PackOrdinals.Num(); ++Ordinal)
	{
		if (NewPacks[PackOrdinals[Ordinal - 1]].PackId
			== NewPacks[PackOrdinals[Ordinal]].PackId)
		{
			return FailBeforeCommit(FAngelscriptCacheStoreResult::Failure(
				EAngelscriptCacheStoreError::ContentValidationFailed,
				EAngelscriptCacheStoreStage::PackTemp,
				EAngelscriptCacheStorePathCategory::Pack));
		}
	}

	TUniquePtr<IAngelscriptCacheNamespaceLockHandle> NamespaceLock;
	FAngelscriptCacheStoreResult Result = AcquireAngelscriptCacheNamespaceLock(
		Paths,
		LockDeadlineSeconds,
		IsCancellationRequested,
		LockOps,
		NamespaceLock);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	check(NamespaceLock.IsValid());
	if (IsCancellationRequested())
	{
		return FailBeforeCommit(FAngelscriptCacheStoreResult::Failure(
			EAngelscriptCacheStoreError::Cancelled,
			EAngelscriptCacheStoreStage::Rebase));
	}

	Result = EnsureAngelscriptCacheStoreDirectories(Paths, FileOps);
	if (!Result.IsSuccess())
	{
		return FailBeforeCommit(MoveTemp(Result));
	}

	const FAngelscriptCacheStoreResult CleanupResult =
		CleanupAngelscriptCacheStaleTempsUnderLock(
			Paths, *NamespaceLock, FileOps);
	if (!CleanupResult.IsSuccess())
	{
		// Temp names are never cache inputs, so a stale cleanup failure is a
		// diagnostic rather than a publication failure. Do not log an absolute
		// cache path; the structured category and platform code are sufficient.
		UE_LOG(
			LogAngelscriptCacheStore,
			Warning,
			TEXT("Cache V2 stale-temp cleanup failed; publication will continue. Error=%u Stage=%u PathCategory=%u PlatformError=%lld"),
			static_cast<uint32>(CleanupResult.Error),
			static_cast<uint32>(CleanupResult.Stage),
			static_cast<uint32>(CleanupResult.PathCategory),
			static_cast<long long>(CleanupResult.PlatformErrorCode.IsSet()
				? CleanupResult.PlatformErrorCode.GetValue()
				: 0));
	}

	struct FLockedRoot
	{
		EAngelscriptCachePointerKind Kind = EAngelscriptCachePointerKind::Invalid;
		TOptional<FAngelscriptHash256> GenerationId;
		TOptional<FAngelscriptValidatedGeneration> Generation;
		TOptional<FAngelscriptCacheStoreResult> InvalidCandidate;
	};

	TArray<FLockedRoot> LockedRoots;
	LockedRoots.Reserve(3);
	for (const EAngelscriptCachePointerKind Kind : {
		EAngelscriptCachePointerKind::Current,
		EAngelscriptCachePointerKind::Previous,
		EAngelscriptCachePointerKind::PendingColdStart})
	{
		FLockedRoot& Root = LockedRoots.AddDefaulted_GetRef();
		Root.Kind = Kind;
	}

	auto IsRecoverableInvalidCandidate = [](
		const FAngelscriptCacheStoreResult& CandidateResult)
	{
		if (CandidateResult.Error == EAngelscriptCacheStoreError::ManifestMissing
			|| CandidateResult.Error == EAngelscriptCacheStoreError::PackMissing)
		{
			return true;
		}
		return CandidateResult.Error
				== EAngelscriptCacheStoreError::ContentValidationFailed
			&& CandidateResult.ContentValidation.IsSet()
			&& CandidateResult.ContentValidation->Error
				!= EAngelscriptCacheValidationError::BudgetExceeded;
	};

	for (int32 RootIndex = 0; RootIndex < LockedRoots.Num(); ++RootIndex)
	{
		FLockedRoot& Root = LockedRoots[RootIndex];
		Result = ReadAngelscriptCachePointerSlot(
			Paths, Root.Kind, FileOps, Root.GenerationId);
		if (!Result.IsSuccess())
		{
			return FailBeforeCommit(MoveTemp(Result));
		}
		if (!Root.GenerationId.IsSet())
		{
			continue;
		}

		bool bReusedEarlierRoot = false;
		for (int32 EarlierIndex = 0; EarlierIndex < RootIndex; ++EarlierIndex)
		{
			const FLockedRoot& EarlierRoot = LockedRoots[EarlierIndex];
			if (EarlierRoot.GenerationId.IsSet()
				&& EarlierRoot.GenerationId.GetValue()
					== Root.GenerationId.GetValue())
			{
				Root.Generation = EarlierRoot.Generation;
				Root.InvalidCandidate = EarlierRoot.InvalidCandidate;
				bReusedEarlierRoot = true;
				break;
			}
		}
		if (bReusedEarlierRoot)
		{
			continue;
		}

		Result = ReadAndValidateAngelscriptCacheGenerationUnderLock(
			Paths,
			Root.GenerationId.GetValue(),
			Limits,
			Budget,
			Codec,
			*NamespaceLock,
			FileOps,
			Root.Generation);
		if (!Result.IsSuccess())
		{
			if (!IsRecoverableInvalidCandidate(Result))
			{
				Result.GenerationBefore = Root.GenerationId;
				return Result;
			}
			Root.InvalidCandidate = Result;
		}
	}

	FLockedRoot* TargetRoot = LockedRoots.FindByPredicate(
		[TargetKind](const FLockedRoot& Root)
		{
			return Root.Kind == TargetKind;
		});
	check(TargetRoot != nullptr);
	TOptional<FAngelscriptHash256> ActualGenerationId = TargetRoot->GenerationId;
	TOptional<FAngelscriptValidatedGeneration> ActualGeneration = TargetRoot->Generation;
	bool bProceedWithInvalidObservedBase = false;
	if (TargetRoot->InvalidCandidate.IsSet())
	{
		Result = TargetRoot->InvalidCandidate.GetValue();
		const bool bStillObserved = ActualGenerationId.IsSet()
			&& ObservedGenerationId.IsSet()
			&& ObservedGenerationId.GetValue()
				== ActualGenerationId.GetValue();
		if (bStillObserved)
		{
			// The caller's selected base did not move, but it is no longer a
			// valid retention root. A source-valid prepared generation may
			// repair the slot; the invalid old Current must not rotate to
			// Previous.
			ActualGenerationId.Reset();
			ActualGeneration.Reset();
			bProceedWithInvalidObservedBase = true;
		}
		else
		{
			FAngelscriptCacheStoreResult Revalidation =
				FAngelscriptCacheStoreResult::Failure(
					EAngelscriptCacheStoreError::NeedsSourceRevalidation,
					EAngelscriptCacheStoreStage::Rebase);
			Revalidation.ContentValidation = Result.ContentValidation;
			Revalidation.GenerationBefore = ActualGenerationId;
			return Revalidation;
		}
	}

	auto NoOpResult = [&]()
	{
		FAngelscriptCacheStoreResult NoOp = FAngelscriptCacheStoreResult::Success();
		NoOp.CommitState = EAngelscriptCacheStoreCommitState::NotCommitted;
		NoOp.GenerationBefore = ActualGenerationId;
		NoOp.GenerationAfter = ActualGenerationId;
		return NoOp;
	};
	if (ActualGenerationId.IsSet()
		&& ActualGenerationId.GetValue()
			== EncodedManifest.ComputedGenerationId)
	{
		return NoOpResult();
	}

	FAngelscriptCacheRebasePlan RebasePlan;
	if (bProceedWithInvalidObservedBase)
	{
		RebasePlan.Action = EAngelscriptCacheRebaseAction::ProceedFromObservedBase;
	}
	else
	{
		Result = EvaluateAngelscriptCacheRebase(
			ObservedGenerationId,
			ActualGenerationId,
			ActualGeneration.IsSet() ? &ActualGeneration->Manifest : nullptr,
			PreparedManifest,
			RebasePlan);
		if (!Result.IsSuccess())
		{
			return Result;
		}
	}
	if (RebasePlan.Action == EAngelscriptCacheRebaseAction::AlreadySelected)
	{
		return NoOpResult();
	}
	if (RebasePlan.Action != EAngelscriptCacheRebaseAction::ProceedFromObservedBase)
	{
		return FailBeforeCommit(FAngelscriptCacheStoreResult::Failure(
			EAngelscriptCacheStoreError::RebaseSemanticConflict,
			EAngelscriptCacheStoreStage::Rebase));
	}

	for (const int32 PackOrdinal : PackOrdinals)
	{
		if (IsCancellationRequested())
		{
			return FailBeforeCommit(FAngelscriptCacheStoreResult::Failure(
				EAngelscriptCacheStoreError::Cancelled,
				EAngelscriptCacheStoreStage::PackTemp));
		}
		const FAngelscriptEncodedPack& Pack = NewPacks[PackOrdinal];
		Result = PutAngelscriptCachePackIfAbsent(
			Paths,
			Pack.PackId,
			Pack.Bytes,
			WriterToken,
			Limits,
			FileOps,
			FaultInjector);
		if (!Result.IsSuccess())
		{
			return FailBeforeCommit(MoveTemp(Result));
		}
	}
	if (IsCancellationRequested())
	{
		return FailBeforeCommit(FAngelscriptCacheStoreResult::Failure(
			EAngelscriptCacheStoreError::Cancelled,
			EAngelscriptCacheStoreStage::ManifestTemp));
	}

	Result = PutAngelscriptCacheManifestIfAbsent(
		Paths,
		EncodedManifest.ComputedGenerationId,
		EncodedManifest.CompleteBytes,
		WriterToken,
		Limits,
		Codec,
		FileOps,
		FaultInjector);
	if (!Result.IsSuccess())
	{
		return FailBeforeCommit(MoveTemp(Result));
	}

	return PublishAngelscriptCachePointers(
		Paths,
		Disposition,
		EncodedManifest.ComputedGenerationId,
		ActualGenerationId,
		WriterToken,
		IsCancellationRequested,
		*NamespaceLock,
		FileOps,
		FaultInjector);
}

FAngelscriptCacheStoreResult PutAngelscriptCachePackIfAbsent(
	const FAngelscriptCacheStorePaths& Paths,
	const FAngelscriptHash256& ExpectedPackId,
	const TConstArrayView<uint8> CompletePackBytes,
	const FAngelscriptCacheWriterToken& WriterToken,
	const FAngelscriptCacheReadLimits& Limits,
	IAngelscriptCacheAtomicFileOps& FileOps,
	IAngelscriptCacheStoreFaultInjector* FaultInjector)
{
	const FString FinalPath = Paths.BuildPackPath(ExpectedPackId);
	const FString TempPath = Paths.BuildPackTempPath(ExpectedPackId, WriterToken);
	auto SetPackContext = [](FAngelscriptCacheStoreResult Result,
		const EAngelscriptCacheStoreStage Stage)
	{
		if (Result.Stage == EAngelscriptCacheStoreStage::None)
		{
			Result.Stage = Stage;
		}
		if (Result.PathCategory == EAngelscriptCacheStorePathCategory::None)
		{
			Result.PathCategory = EAngelscriptCacheStorePathCategory::Pack;
		}
		return Result;
	};
	auto ValidateExactPack = [&](const TConstArrayView<uint8> ActualBytes,
		const EAngelscriptCacheStoreStage Stage,
		const EAngelscriptCacheStoreError InvalidError)
	{
		FAngelscriptCacheReadBudget Budget;
		TArray<FAngelscriptCachePackIndexEntry> Index;
		const FAngelscriptCacheValidationResult Validation = ValidateAngelscriptCachePack(
			ActualBytes, ExpectedPackId, Limits, Budget, Index);
		const bool bExactBytes = ActualBytes.Num() == CompletePackBytes.Num()
			&& (ActualBytes.IsEmpty() || FMemory::Memcmp(
				ActualBytes.GetData(), CompletePackBytes.GetData(), ActualBytes.Num()) == 0);
		if (Validation.IsSuccess() && bExactBytes)
		{
			return FAngelscriptCacheStoreResult::Success();
		}

		FAngelscriptCacheStoreResult Result = FAngelscriptCacheStoreResult::Failure(
			Validation.IsSuccess() ? InvalidError : EAngelscriptCacheStoreError::ContentValidationFailed,
			Stage,
			EAngelscriptCacheStorePathCategory::Pack);
		if (!Validation.IsSuccess())
		{
			Result.ContentValidation = Validation;
		}
		return Result;
	};
	auto ReturnAfterTempCleanup = [&](FAngelscriptCacheStoreResult PrimaryResult)
	{
		// Cleanup is best-effort and must never replace the primary Store failure.
		FileOps.RemoveOwnTemp(TempPath);
		return PrimaryResult;
	};

	TArray<uint8> ExistingBytes;
	FAngelscriptCacheStoreResult ReadResult = FileOps.ReopenReadAll(
		FinalPath, Limits.MaxPackBytes, ExistingBytes);
	if (ReadResult.IsSuccess())
	{
		FAngelscriptCacheStoreResult ExistingValidation = ValidateExactPack(
			ExistingBytes,
			EAngelscriptCacheStoreStage::PackFinal,
			EAngelscriptCacheStoreError::ImmutableObjectCollisionOrCorruption);
		if (!ExistingValidation.IsSuccess())
		{
			ExistingValidation.Error =
				EAngelscriptCacheStoreError::ImmutableObjectCollisionOrCorruption;
		}
		return ExistingValidation;
	}
	if (ReadResult.Error != EAngelscriptCacheStoreError::PackMissing)
	{
		return SetPackContext(MoveTemp(ReadResult), EAngelscriptCacheStoreStage::PackFinal);
	}
	if (AngelscriptCacheStore_Private::ShouldStopAtFaultPoint(
		FaultInjector,
		EAngelscriptCacheStoreFaultPoint::BeforePackTempWrite))
	{
		return AngelscriptCacheStore_Private::FaultInjected(
			EAngelscriptCacheStoreStage::PackTemp,
			EAngelscriptCacheStorePathCategory::Pack);
	}

	FAngelscriptCacheStoreResult WriteResult =
		FileOps.WriteFlushClose(TempPath, CompletePackBytes);
	if (!WriteResult.IsSuccess())
	{
		return ReturnAfterTempCleanup(SetPackContext(
			MoveTemp(WriteResult), EAngelscriptCacheStoreStage::PackTemp));
	}
	if (AngelscriptCacheStore_Private::ShouldStopAtFaultPoint(
		FaultInjector,
		EAngelscriptCacheStoreFaultPoint::AfterPackTempFlush))
	{
		return AngelscriptCacheStore_Private::FaultInjected(
			EAngelscriptCacheStoreStage::PackTemp,
			EAngelscriptCacheStorePathCategory::Pack);
	}

	TArray<uint8> TempBytes;
	FAngelscriptCacheStoreResult TempReadResult = FileOps.ReopenReadAll(
		TempPath, Limits.MaxPackBytes, TempBytes);
	if (!TempReadResult.IsSuccess())
	{
		return ReturnAfterTempCleanup(SetPackContext(
			MoveTemp(TempReadResult), EAngelscriptCacheStoreStage::PackTemp));
	}
	FAngelscriptCacheStoreResult TempValidation = ValidateExactPack(
		TempBytes,
		EAngelscriptCacheStoreStage::PackTemp,
		EAngelscriptCacheStoreError::ContentValidationFailed);
	if (!TempValidation.IsSuccess())
	{
		return ReturnAfterTempCleanup(MoveTemp(TempValidation));
	}

	FAngelscriptCacheStoreResult RenameResult =
		FileOps.RenameNewImmutable(TempPath, FinalPath);
	if (!RenameResult.IsSuccess())
	{
		return ReturnAfterTempCleanup(SetPackContext(
			MoveTemp(RenameResult), EAngelscriptCacheStoreStage::PackFinal));
	}

	FAngelscriptCacheStoreResult SyncResult = FileOps.SyncDirectory(Paths.PacksDirectory);
	if (!SyncResult.IsSuccess())
	{
		return SetPackContext(MoveTemp(SyncResult), EAngelscriptCacheStoreStage::PackFinal);
	}

	TArray<uint8> FinalBytes;
	FAngelscriptCacheStoreResult FinalReadResult = FileOps.ReopenReadAll(
		FinalPath, Limits.MaxPackBytes, FinalBytes);
	if (!FinalReadResult.IsSuccess())
	{
		return SetPackContext(MoveTemp(FinalReadResult), EAngelscriptCacheStoreStage::PackFinal);
	}
	FAngelscriptCacheStoreResult InstalledValidation = ValidateExactPack(
		FinalBytes,
		EAngelscriptCacheStoreStage::PackFinal,
		EAngelscriptCacheStoreError::ImmutableObjectCollisionOrCorruption);
	if (!InstalledValidation.IsSuccess())
	{
		return InstalledValidation;
	}
	if (AngelscriptCacheStore_Private::ShouldStopAtFaultPoint(
		FaultInjector,
		EAngelscriptCacheStoreFaultPoint::AfterPackRename))
	{
		return AngelscriptCacheStore_Private::FaultInjected(
			EAngelscriptCacheStoreStage::PackFinal,
			EAngelscriptCacheStorePathCategory::Pack);
	}
	return InstalledValidation;
}

FAngelscriptCacheStoreResult PutAngelscriptCacheManifestIfAbsent(
	const FAngelscriptCacheStorePaths& Paths,
	const FAngelscriptHash256& ExpectedGenerationId,
	const TConstArrayView<uint8> CompleteManifestBytes,
	const FAngelscriptCacheWriterToken& WriterToken,
	const FAngelscriptCacheReadLimits& Limits,
	IAngelscriptCacheStorageCodec& Codec,
	IAngelscriptCacheAtomicFileOps& FileOps,
	IAngelscriptCacheStoreFaultInjector* FaultInjector)
{
	const FString FinalPath = Paths.BuildManifestPath(ExpectedGenerationId);
	const FString TempPath = Paths.BuildManifestTempPath(ExpectedGenerationId, WriterToken);
	auto SetManifestContext = [](FAngelscriptCacheStoreResult Result,
		const EAngelscriptCacheStoreStage Stage)
	{
		if (Result.Stage == EAngelscriptCacheStoreStage::None)
		{
			Result.Stage = Stage;
		}
		if (Result.PathCategory == EAngelscriptCacheStorePathCategory::None)
		{
			Result.PathCategory = EAngelscriptCacheStorePathCategory::Manifest;
		}
		return Result;
	};
	auto ValidateExactManifest = [&](const TConstArrayView<uint8> ActualBytes,
		const EAngelscriptCacheStoreStage Stage,
		const EAngelscriptCacheStoreError InvalidError)
	{
		AngelscriptCacheStore_Private::FStorePackSource PackSource(Paths, Limits, FileOps);
		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptValidatedGeneration> Generation;
		const FAngelscriptCacheValidationResult Validation = ValidateAngelscriptCacheGeneration(
			ActualBytes, ExpectedGenerationId, PackSource, Limits, Budget, Codec, Generation);
		const bool bExactBytes = ActualBytes.Num() == CompleteManifestBytes.Num()
			&& (ActualBytes.IsEmpty() || FMemory::Memcmp(
				ActualBytes.GetData(), CompleteManifestBytes.GetData(), ActualBytes.Num()) == 0);
		if (Validation.IsSuccess() && bExactBytes)
		{
			return FAngelscriptCacheStoreResult::Success();
		}

		FAngelscriptCacheStoreResult Result = FAngelscriptCacheStoreResult::Failure(
			Validation.IsSuccess() ? InvalidError : EAngelscriptCacheStoreError::ContentValidationFailed,
			Stage,
			EAngelscriptCacheStorePathCategory::Manifest);
		if (!Validation.IsSuccess())
		{
			Result.ContentValidation = Validation;
		}
		return Result;
	};
	auto ReturnAfterTempCleanup = [&](FAngelscriptCacheStoreResult PrimaryResult)
	{
		FileOps.RemoveOwnTemp(TempPath);
		return PrimaryResult;
	};

	TArray<uint8> ExistingBytes;
	FAngelscriptCacheStoreResult ReadResult = FileOps.ReopenReadAll(
		FinalPath, Limits.MaxManifestBytes, ExistingBytes);
	if (ReadResult.IsSuccess())
	{
		FAngelscriptCacheStoreResult ExistingValidation = ValidateExactManifest(
			ExistingBytes,
			EAngelscriptCacheStoreStage::ManifestFinal,
			EAngelscriptCacheStoreError::ImmutableObjectCollisionOrCorruption);
		if (!ExistingValidation.IsSuccess())
		{
			ExistingValidation.Error =
				EAngelscriptCacheStoreError::ImmutableObjectCollisionOrCorruption;
		}
		return ExistingValidation;
	}
	if (ReadResult.Error != EAngelscriptCacheStoreError::ManifestMissing)
	{
		return SetManifestContext(MoveTemp(ReadResult), EAngelscriptCacheStoreStage::ManifestFinal);
	}

	FAngelscriptCacheStoreResult WriteResult =
		FileOps.WriteFlushClose(TempPath, CompleteManifestBytes);
	if (!WriteResult.IsSuccess())
	{
		return ReturnAfterTempCleanup(SetManifestContext(
			MoveTemp(WriteResult), EAngelscriptCacheStoreStage::ManifestTemp));
	}
	if (AngelscriptCacheStore_Private::ShouldStopAtFaultPoint(
		FaultInjector,
		EAngelscriptCacheStoreFaultPoint::AfterManifestTempFlush))
	{
		return AngelscriptCacheStore_Private::FaultInjected(
			EAngelscriptCacheStoreStage::ManifestTemp,
			EAngelscriptCacheStorePathCategory::Manifest);
	}

	TArray<uint8> TempBytes;
	FAngelscriptCacheStoreResult TempReadResult = FileOps.ReopenReadAll(
		TempPath, Limits.MaxManifestBytes, TempBytes);
	if (!TempReadResult.IsSuccess())
	{
		return ReturnAfterTempCleanup(SetManifestContext(
			MoveTemp(TempReadResult), EAngelscriptCacheStoreStage::ManifestTemp));
	}
	FAngelscriptCacheStoreResult TempValidation = ValidateExactManifest(
		TempBytes,
		EAngelscriptCacheStoreStage::ManifestTemp,
		EAngelscriptCacheStoreError::ContentValidationFailed);
	if (!TempValidation.IsSuccess())
	{
		return ReturnAfterTempCleanup(MoveTemp(TempValidation));
	}

	FAngelscriptCacheStoreResult RenameResult =
		FileOps.RenameNewImmutable(TempPath, FinalPath);
	if (!RenameResult.IsSuccess())
	{
		return ReturnAfterTempCleanup(SetManifestContext(
			MoveTemp(RenameResult), EAngelscriptCacheStoreStage::ManifestFinal));
	}

	FAngelscriptCacheStoreResult SyncResult = FileOps.SyncDirectory(Paths.GenerationsDirectory);
	if (!SyncResult.IsSuccess())
	{
		return SetManifestContext(MoveTemp(SyncResult), EAngelscriptCacheStoreStage::ManifestFinal);
	}

	TArray<uint8> FinalBytes;
	FAngelscriptCacheStoreResult FinalReadResult = FileOps.ReopenReadAll(
		FinalPath, Limits.MaxManifestBytes, FinalBytes);
	if (!FinalReadResult.IsSuccess())
	{
		return SetManifestContext(MoveTemp(FinalReadResult), EAngelscriptCacheStoreStage::ManifestFinal);
	}
	FAngelscriptCacheStoreResult InstalledValidation = ValidateExactManifest(
		FinalBytes,
		EAngelscriptCacheStoreStage::ManifestFinal,
		EAngelscriptCacheStoreError::ImmutableObjectCollisionOrCorruption);
	if (!InstalledValidation.IsSuccess())
	{
		return InstalledValidation;
	}
	if (AngelscriptCacheStore_Private::ShouldStopAtFaultPoint(
		FaultInjector,
		EAngelscriptCacheStoreFaultPoint::AfterManifestRename))
	{
		return AngelscriptCacheStore_Private::FaultInjected(
			EAngelscriptCacheStoreStage::ManifestFinal,
			EAngelscriptCacheStorePathCategory::Manifest);
	}
	return InstalledValidation;
}
