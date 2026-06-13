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
		const EAngelscriptSourceKind SourceKind,
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

		if ((SourceKind == EAngelscriptSourceKind::Plugin || SourceKind == EAngelscriptSourceKind::Memory)
			&& IsInvalidSegment(MountName))
		{
			SetError(OutError, TEXT("Virtual script path is missing a mount name."));
			return false;
		}

		return true;
	}
}

FAngelscriptSourceRoot FAngelscriptSourceRoot::FromGameRoot(const FString& AbsolutePath)
{
	FAngelscriptSourceRoot Root;
	Root.AbsolutePath = AbsolutePath;
	Root.SourceKind = EAngelscriptSourceKind::Game;
	return Root;
}

FAngelscriptSourceRoot FAngelscriptSourceRoot::FromPluginRoot(const FString& PluginName, const FString& AbsolutePath)
{
	FAngelscriptSourceRoot Root;
	Root.AbsolutePath = AbsolutePath;
	Root.SourceKind = EAngelscriptSourceKind::Plugin;
	Root.MountName = PluginName;
	return Root;
}

bool FAngelscriptVirtualPath::TryParse(const FString& InPath, FAngelscriptVirtualPath& OutPath, FString* OutError)
{
	using namespace AngelscriptScriptSource_Private;

	OutPath = FAngelscriptVirtualPath();
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
		if (!ValidateParts(EAngelscriptSourceKind::Game, FString(), Relative, OutError))
		{
			return false;
		}
		OutPath.SetParsed(NormalizedPath, EAngelscriptSourceKind::Game, FString(), Relative);
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
		if (!ValidateParts(EAngelscriptSourceKind::Plugin, PluginName, Relative, OutError))
		{
			return false;
		}
		OutPath.SetParsed(NormalizedPath, EAngelscriptSourceKind::Plugin, PluginName, Relative);
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
		if (!ValidateParts(EAngelscriptSourceKind::Memory, ProviderName, Relative, OutError))
		{
			return false;
		}
		OutPath.SetParsed(NormalizedPath, EAngelscriptSourceKind::Memory, ProviderName, Relative);
		return true;
	}

	SetError(OutError, TEXT("Virtual script path uses an unsupported /Angelscript mount."));
	return false;
}

FAngelscriptVirtualPath FAngelscriptVirtualPath::FromGameRelativePath(const FString& RelativePath)
{
	FAngelscriptVirtualPath VirtualPath;
	const FString Path = FString(GameRoot) / AngelscriptScriptSource_Private::NormalizeRelativePath(RelativePath);
	ensure(TryParse(Path, VirtualPath));
	return VirtualPath;
}

FAngelscriptVirtualPath FAngelscriptVirtualPath::FromPluginRelativePath(const FString& PluginName, const FString& RelativePath)
{
	FAngelscriptVirtualPath VirtualPath;
	const FString Path = FString(PluginRoot) / PluginName / AngelscriptScriptSource_Private::NormalizeRelativePath(RelativePath);
	ensure(TryParse(Path, VirtualPath));
	return VirtualPath;
}

FAngelscriptVirtualPath FAngelscriptVirtualPath::FromMemoryRelativePath(const FString& ProviderName, const FString& RelativePath)
{
	FAngelscriptVirtualPath VirtualPath;
	const FString Path = FString(MemoryRoot) / ProviderName / AngelscriptScriptSource_Private::NormalizeRelativePath(RelativePath);
	ensure(TryParse(Path, VirtualPath));
	return VirtualPath;
}

const FString& FAngelscriptVirtualPath::ToString() const
{
	return Path;
}

EAngelscriptSourceKind FAngelscriptVirtualPath::GetSourceKind() const
{
	return SourceKind;
}

const FString& FAngelscriptVirtualPath::GetMountName() const
{
	return MountName;
}

const FString& FAngelscriptVirtualPath::GetRelativePath() const
{
	return RelativePath;
}

FString FAngelscriptVirtualPath::ToModuleName() const
{
	using namespace AngelscriptScriptSource_Private;

	if (SourceKind == EAngelscriptSourceKind::Memory)
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

bool FAngelscriptVirtualPath::IsValid() const
{
	return SourceKind != EAngelscriptSourceKind::Unknown && !Path.IsEmpty();
}

void FAngelscriptVirtualPath::SetParsed(
	const FString& InPath,
	const EAngelscriptSourceKind InSourceKind,
	const FString& InMountName,
	const FString& InRelativePath)
{
	Path = InPath;
	SourceKind = InSourceKind;
	MountName = InMountName;
	RelativePath = InRelativePath;
}

FAngelscriptSource FAngelscriptSource::FromGameFile(const FString& RelativeFilename, const FString& AbsoluteFilename)
{
	FAngelscriptSource Source;
	Source.VirtualPath = FAngelscriptVirtualPath::FromGameRelativePath(RelativeFilename);
	Source.ModuleName = Source.VirtualPath.ToModuleName();
	Source.RelativeFilename = Source.VirtualPath.GetRelativePath();
	Source.AbsoluteFilename = AbsoluteFilename;
	Source.SourceKind = EAngelscriptSourceKind::Game;
	return Source;
}

FAngelscriptSource FAngelscriptSource::FromPluginFile(const FString& PluginName, const FString& RelativeFilename, const FString& AbsoluteFilename)
{
	FAngelscriptSource Source;
	Source.VirtualPath = FAngelscriptVirtualPath::FromPluginRelativePath(PluginName, RelativeFilename);
	Source.ModuleName = Source.VirtualPath.ToModuleName();
	Source.RelativeFilename = Source.VirtualPath.GetRelativePath();
	Source.AbsoluteFilename = AbsoluteFilename;
	Source.SourceKind = EAngelscriptSourceKind::Plugin;
	return Source;
}

FAngelscriptSource FAngelscriptSource::FromMemorySource(const FString& InVirtualPath, const FString& InSourceText)
{
	FAngelscriptSource Source;
	TryFromMemorySource(InVirtualPath, InSourceText, Source);
	return Source;
}

bool FAngelscriptSource::TryFromMemorySource(
	const FString& InVirtualPath,
	const FString& InSourceText,
	FAngelscriptSource& OutSource,
	FString* OutError)
{
	OutSource = FAngelscriptSource();
	if (!FAngelscriptVirtualPath::TryParse(InVirtualPath, OutSource.VirtualPath, OutError))
	{
		return false;
	}

	if (OutSource.VirtualPath.GetSourceKind() != EAngelscriptSourceKind::Memory)
	{
		OutSource = FAngelscriptSource();
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
