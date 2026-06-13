#include "AngelscriptScriptSource.h"

namespace AngelscriptScriptSource_Private
{
	FString NormalizeVirtualPath(FString Path)
	{
		Path.RemoveFromEnd(TEXT("/"));
		return Path;
	}

	FString NormalizeRelativePath(FString Path)
	{
		Path.ReplaceInline(TEXT("\\"), TEXT("/"));
		while (Path.StartsWith(TEXT("/")))
		{
			Path.RightChopInline(1, EAllowShrinking::No);
		}
		while (Path.Contains(TEXT("//")))
		{
			Path.ReplaceInline(TEXT("//"), TEXT("/"));
		}
		return Path;
	}

	bool HasInvalidSegments(const FString& Path)
	{
		TArray<FString> Segments;
		Path.ParseIntoArray(Segments, TEXT("/"), false);
		if (Segments.Num() == 0)
		{
			return true;
		}
		for (const FString& Segment : Segments)
		{
			if (Segment.IsEmpty() || Segment == TEXT(".") || Segment == TEXT(".."))
			{
				return true;
			}
		}
		return false;
	}

	bool IsInvalidSegment(const FString& Segment)
	{
		return Segment.IsEmpty() || Segment == TEXT(".") || Segment == TEXT("..");
	}

	void SetError(FString* OutError, const TCHAR* Message)
	{
		if (OutError != nullptr)
		{
			*OutError = Message;
		}
	}

	FString RelativePathToModuleName(FString RelativePath)
	{
		RelativePath.ReplaceInline(TEXT("\\"), TEXT("/"));
		RelativePath.RemoveFromEnd(TEXT(".as"));
		RelativePath.ReplaceInline(TEXT("/"), TEXT("."));
		return RelativePath;
	}

	bool ValidateParts(
		const EAngelscriptScriptSourceKind SourceKind,
		const FString& MountName,
		const FString& RelativePath,
		FString* OutError)
	{
		if (RelativePath.IsEmpty())
		{
			SetError(OutError, TEXT("Virtual script path is missing a relative source path."));
			return false;
		}

		if (!RelativePath.EndsWith(TEXT(".as")))
		{
			SetError(OutError, TEXT("Virtual script path must end with .as."));
			return false;
		}

		if (HasInvalidSegments(RelativePath))
		{
			SetError(OutError, TEXT("Virtual script path contains an invalid segment."));
			return false;
		}

		if ((SourceKind == EAngelscriptScriptSourceKind::Plugin || SourceKind == EAngelscriptScriptSourceKind::Memory)
			&& IsInvalidSegment(MountName))
		{
			SetError(OutError, TEXT("Virtual script path is missing a mount name."));
			return false;
		}

		return true;
	}
}

FAngelscriptScriptRoot FAngelscriptScriptRoot::FromGameRoot(const FString& AbsolutePath)
{
	FAngelscriptScriptRoot Root;
	Root.AbsolutePath = AbsolutePath;
	Root.SourceKind = EAngelscriptScriptSourceKind::Game;
	return Root;
}

FAngelscriptScriptRoot FAngelscriptScriptRoot::FromPluginRoot(const FString& PluginName, const FString& AbsolutePath)
{
	FAngelscriptScriptRoot Root;
	Root.AbsolutePath = AbsolutePath;
	Root.SourceKind = EAngelscriptScriptSourceKind::Plugin;
	Root.MountName = PluginName;
	return Root;
}

bool FAngelscriptVirtualScriptPath::TryParse(const FString& InPath, FAngelscriptVirtualScriptPath& OutPath, FString* OutError)
{
	using namespace AngelscriptScriptSource_Private;

	OutPath = FAngelscriptVirtualScriptPath();
	if (OutError != nullptr)
	{
		OutError->Reset();
	}

	if (InPath.Contains(TEXT("://")))
	{
		SetError(OutError, TEXT("Virtual script paths use /Angelscript roots, not URI schemes."));
		return false;
	}

	if (InPath.Contains(TEXT("\\")))
	{
		SetError(OutError, TEXT("Virtual script paths must use forward slashes."));
		return false;
	}

	if (InPath.EndsWith(TEXT("/")))
	{
		SetError(OutError, TEXT("Virtual script paths cannot contain empty segments."));
		return false;
	}

	const FString NormalizedPath = NormalizeVirtualPath(InPath);
	if (NormalizedPath.Contains(TEXT("//")))
	{
		SetError(OutError, TEXT("Virtual script paths cannot contain empty segments."));
		return false;
	}

	if (!NormalizedPath.StartsWith(Root, ESearchCase::CaseSensitive))
	{
		SetError(OutError, TEXT("Virtual script path must start with /Angelscript."));
		return false;
	}

	if (NormalizedPath.StartsWith(FString(GameRoot) / TEXT(""), ESearchCase::CaseSensitive))
	{
		const FString Relative = NormalizedPath.RightChop(FCString::Strlen(GameRoot) + 1);
		if (!ValidateParts(EAngelscriptScriptSourceKind::Game, FString(), Relative, OutError))
		{
			return false;
		}
		OutPath.SetParsed(NormalizedPath, EAngelscriptScriptSourceKind::Game, FString(), Relative);
		return true;
	}

	if (NormalizedPath.StartsWith(FString(PluginRoot) / TEXT(""), ESearchCase::CaseSensitive))
	{
		const FString Remainder = NormalizedPath.RightChop(FCString::Strlen(PluginRoot) + 1);
		FString PluginName;
		FString Relative;
		if (!Remainder.Split(TEXT("/"), &PluginName, &Relative) || PluginName.IsEmpty())
		{
			SetError(OutError, TEXT("Plugin virtual script path is missing a plugin name."));
			return false;
		}
		if (!ValidateParts(EAngelscriptScriptSourceKind::Plugin, PluginName, Relative, OutError))
		{
			return false;
		}
		OutPath.SetParsed(NormalizedPath, EAngelscriptScriptSourceKind::Plugin, PluginName, Relative);
		return true;
	}

	if (NormalizedPath.StartsWith(FString(MemoryRoot) / TEXT(""), ESearchCase::CaseSensitive))
	{
		const FString Remainder = NormalizedPath.RightChop(FCString::Strlen(MemoryRoot) + 1);
		FString ProviderName;
		FString Relative;
		if (!Remainder.Split(TEXT("/"), &ProviderName, &Relative) || ProviderName.IsEmpty())
		{
			SetError(OutError, TEXT("Memory virtual script path is missing a provider name."));
			return false;
		}
		if (!ValidateParts(EAngelscriptScriptSourceKind::Memory, ProviderName, Relative, OutError))
		{
			return false;
		}
		OutPath.SetParsed(NormalizedPath, EAngelscriptScriptSourceKind::Memory, ProviderName, Relative);
		return true;
	}

	SetError(OutError, TEXT("Virtual script path uses an unsupported /Angelscript mount."));
	return false;
}

FAngelscriptVirtualScriptPath FAngelscriptVirtualScriptPath::FromGameRelativePath(const FString& RelativePath)
{
	FAngelscriptVirtualScriptPath VirtualPath;
	const FString Path = FString(GameRoot) / AngelscriptScriptSource_Private::NormalizeRelativePath(RelativePath);
	ensure(TryParse(Path, VirtualPath));
	return VirtualPath;
}

FAngelscriptVirtualScriptPath FAngelscriptVirtualScriptPath::FromPluginRelativePath(const FString& PluginName, const FString& RelativePath)
{
	FAngelscriptVirtualScriptPath VirtualPath;
	const FString Path = FString(PluginRoot) / PluginName / AngelscriptScriptSource_Private::NormalizeRelativePath(RelativePath);
	ensure(TryParse(Path, VirtualPath));
	return VirtualPath;
}

FAngelscriptVirtualScriptPath FAngelscriptVirtualScriptPath::FromMemoryRelativePath(const FString& ProviderName, const FString& RelativePath)
{
	FAngelscriptVirtualScriptPath VirtualPath;
	const FString Path = FString(MemoryRoot) / ProviderName / AngelscriptScriptSource_Private::NormalizeRelativePath(RelativePath);
	ensure(TryParse(Path, VirtualPath));
	return VirtualPath;
}

const FString& FAngelscriptVirtualScriptPath::ToString() const
{
	return Path;
}

EAngelscriptScriptSourceKind FAngelscriptVirtualScriptPath::GetSourceKind() const
{
	return SourceKind;
}

const FString& FAngelscriptVirtualScriptPath::GetMountName() const
{
	return MountName;
}

const FString& FAngelscriptVirtualScriptPath::GetRelativePath() const
{
	return RelativePath;
}

FString FAngelscriptVirtualScriptPath::ToModuleName() const
{
	using namespace AngelscriptScriptSource_Private;

	if (SourceKind == EAngelscriptScriptSourceKind::Memory)
	{
		FString ModuleName = TEXT("Angelscript.Memory");
		if (!MountName.IsEmpty())
		{
			ModuleName += TEXT(".");
			ModuleName += MountName;
		}
		const FString RelativeModuleName = RelativePathToModuleName(RelativePath);
		if (!RelativeModuleName.IsEmpty())
		{
			ModuleName += TEXT(".");
			ModuleName += RelativeModuleName;
		}
		return ModuleName;
	}

	return RelativePathToModuleName(RelativePath);
}

bool FAngelscriptVirtualScriptPath::IsValid() const
{
	return SourceKind != EAngelscriptScriptSourceKind::Unknown && !Path.IsEmpty();
}

void FAngelscriptVirtualScriptPath::SetParsed(
	const FString& InPath,
	const EAngelscriptScriptSourceKind InSourceKind,
	const FString& InMountName,
	const FString& InRelativePath)
{
	Path = InPath;
	SourceKind = InSourceKind;
	MountName = InMountName;
	RelativePath = InRelativePath;
}

FAngelscriptScriptSource FAngelscriptScriptSource::FromGameFile(const FString& RelativeFilename, const FString& AbsoluteFilename)
{
	FAngelscriptScriptSource Source;
	Source.VirtualPath = FAngelscriptVirtualScriptPath::FromGameRelativePath(RelativeFilename);
	Source.ModuleName = Source.VirtualPath.ToModuleName();
	Source.RelativeFilename = Source.VirtualPath.GetRelativePath();
	Source.AbsoluteFilename = AbsoluteFilename;
	Source.SourceKind = EAngelscriptScriptSourceKind::Game;
	return Source;
}

FAngelscriptScriptSource FAngelscriptScriptSource::FromPluginFile(const FString& PluginName, const FString& RelativeFilename, const FString& AbsoluteFilename)
{
	FAngelscriptScriptSource Source;
	Source.VirtualPath = FAngelscriptVirtualScriptPath::FromPluginRelativePath(PluginName, RelativeFilename);
	Source.ModuleName = Source.VirtualPath.ToModuleName();
	Source.RelativeFilename = Source.VirtualPath.GetRelativePath();
	Source.AbsoluteFilename = AbsoluteFilename;
	Source.SourceKind = EAngelscriptScriptSourceKind::Plugin;
	return Source;
}

FAngelscriptScriptSource FAngelscriptScriptSource::FromMemorySource(const FString& InVirtualPath, const FString& InSourceText)
{
	FAngelscriptScriptSource Source;
	TryFromMemorySource(InVirtualPath, InSourceText, Source);
	return Source;
}

bool FAngelscriptScriptSource::TryFromMemorySource(
	const FString& InVirtualPath,
	const FString& InSourceText,
	FAngelscriptScriptSource& OutSource,
	FString* OutError)
{
	OutSource = FAngelscriptScriptSource();
	if (!FAngelscriptVirtualScriptPath::TryParse(InVirtualPath, OutSource.VirtualPath, OutError))
	{
		return false;
	}

	if (OutSource.VirtualPath.GetSourceKind() != EAngelscriptScriptSourceKind::Memory)
	{
		OutSource = FAngelscriptScriptSource();
		AngelscriptScriptSource_Private::SetError(OutError, TEXT("Memory-backed script sources must use /Angelscript/Memory virtual paths."));
		return false;
	}

	OutSource.ModuleName = OutSource.VirtualPath.ToModuleName();
	OutSource.RelativeFilename = OutSource.VirtualPath.GetRelativePath();
	OutSource.SourceKind = OutSource.VirtualPath.GetSourceKind();
	OutSource.SourceText = InSourceText;
	OutSource.bHasSourceText = true;
	return true;
}
