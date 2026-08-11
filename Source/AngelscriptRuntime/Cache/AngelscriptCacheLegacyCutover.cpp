#include "Cache/AngelscriptCacheLegacyCutover.h"

#include "Algo/Unique.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

namespace AngelscriptCacheLegacyCutover_Private
{
	struct FLegacyName
	{
		const TCHAR* Name;
		EAngelscriptLegacyCacheArtifactKind Kind;
	};

	static constexpr FLegacyName LegacyNames[] = {
		{TEXT("PrecompiledScript.Cache"),
			EAngelscriptLegacyCacheArtifactKind::PrecompiledScript},
		{TEXT("PrecompiledScript_Development.Cache"),
			EAngelscriptLegacyCacheArtifactKind::PrecompiledScriptDevelopment},
		{TEXT("PrecompiledScript_Test.Cache"),
			EAngelscriptLegacyCacheArtifactKind::PrecompiledScriptTest},
		{TEXT("PrecompiledScript_Shipping.Cache"),
			EAngelscriptLegacyCacheArtifactKind::PrecompiledScriptShipping},
	};

	static FString NormalizeCandidate(
		const FString& Root,
		const TCHAR* Name)
	{
		FString Candidate = FPaths::Combine(Root, Name);
		FPaths::NormalizeFilename(Candidate);
		return Candidate;
	}
}

FString FAngelscriptLegacyCacheInspection::FormatDiagnostic() const
{
	if (RejectedArtifacts.IsEmpty())
	{
		return AcceptedBindCachePaths.IsEmpty()
			? TEXT("No legacy script cache or Binds.Cache artifact was found.")
			: TEXT("No legacy script cache was found; Binds.Cache remains independently accepted.");
	}

	TArray<FString> Paths;
	Paths.Reserve(RejectedArtifacts.Num());
	for (const FAngelscriptRejectedLegacyCacheArtifact& Artifact :
		RejectedArtifacts)
	{
		Paths.Add(Artifact.AbsolutePath);
	}
	return FString::Printf(
		TEXT("Rejected legacy script cache artifact(s): %s. Cache V2 never reads or migrates these files; authoritative .as source is compiled instead. Binds.Cache remains independently supported."),
		*FString::Join(Paths, TEXT(", ")));
}

FAngelscriptLegacyCacheInspection InspectAngelscriptLegacyCacheArtifacts(
	const TConstArrayView<FString> ScriptRoots,
	const TFunctionRef<bool(const FString& AbsoluteCandidate)> FileExists)
{
	FAngelscriptLegacyCacheInspection Result;
	TArray<FString> SortedRoots(ScriptRoots);
	for (FString& Root : SortedRoots)
	{
		FPaths::NormalizeDirectoryName(Root);
	}
	SortedRoots.Sort();
	SortedRoots.SetNum(Algo::Unique(SortedRoots));

	for (const FString& Root : SortedRoots)
	{
		for (const AngelscriptCacheLegacyCutover_Private::FLegacyName& Legacy :
			AngelscriptCacheLegacyCutover_Private::LegacyNames)
		{
			const FString Candidate =
				AngelscriptCacheLegacyCutover_Private::NormalizeCandidate(
					Root, Legacy.Name);
			if (FileExists(Candidate))
			{
				Result.RejectedArtifacts.Add({Legacy.Kind, Candidate});
			}
		}

		const FString BindCache =
			AngelscriptCacheLegacyCutover_Private::NormalizeCandidate(
				Root, TEXT("Binds.Cache"));
		if (FileExists(BindCache))
		{
			Result.AcceptedBindCachePaths.Add(BindCache);
		}
	}

	return Result;
}

FAngelscriptLegacyCacheInspection
InspectAngelscriptLegacyCacheArtifactsFromDisk(
	const TConstArrayView<FString> ScriptRoots)
{
	return InspectAngelscriptLegacyCacheArtifacts(
		ScriptRoots,
		[](const FString& Candidate)
		{
			return IFileManager::Get().FileExists(*Candidate);
		});
}
