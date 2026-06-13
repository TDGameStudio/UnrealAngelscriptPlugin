#pragma once

#include "CoreMinimal.h"

enum class EAngelscriptSourceKind : uint8
{
	Unknown,
	Game,
	Plugin,
	Memory,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptSourceRoot
{
	FString AbsolutePath;
	EAngelscriptSourceKind SourceKind = EAngelscriptSourceKind::Unknown;
	FString MountName;

	static FAngelscriptSourceRoot FromGameRoot(const FString& AbsolutePath);
	static FAngelscriptSourceRoot FromPluginRoot(const FString& PluginName, const FString& AbsolutePath);
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptVirtualPath
{
	static constexpr const TCHAR* Root = TEXT("/Angelscript");
	static constexpr const TCHAR* GameRoot = TEXT("/Angelscript/Game");
	static constexpr const TCHAR* PluginRoot = TEXT("/Angelscript/Plugin");
	static constexpr const TCHAR* MemoryRoot = TEXT("/Angelscript/Memory");

	static bool TryParse(const FString& InPath, FAngelscriptVirtualPath& OutPath, FString* OutError = nullptr);
	static FAngelscriptVirtualPath FromGameRelativePath(const FString& RelativePath);
	static FAngelscriptVirtualPath FromPluginRelativePath(const FString& PluginName, const FString& RelativePath);
	static FAngelscriptVirtualPath FromMemoryRelativePath(const FString& ProviderName, const FString& RelativePath);

	const FString& ToString() const;
	EAngelscriptSourceKind GetSourceKind() const;
	const FString& GetMountName() const;
	const FString& GetRelativePath() const;
	FString ToModuleName() const;
	bool IsValid() const;

private:
	void SetParsed(
		const FString& InPath,
		EAngelscriptSourceKind InSourceKind,
		const FString& InMountName,
		const FString& InRelativePath);

	FString Path;
	EAngelscriptSourceKind SourceKind = EAngelscriptSourceKind::Unknown;
	FString MountName;
	FString RelativePath;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptSource
{
	FAngelscriptVirtualPath VirtualPath;
	FString ModuleName;
	FString RelativeFilename;
	FString AbsoluteFilename;
	FString SourceText;
	EAngelscriptSourceKind SourceKind = EAngelscriptSourceKind::Unknown;
	bool bHasSourceText = false;

	static FAngelscriptSource FromGameFile(const FString& RelativeFilename, const FString& AbsoluteFilename);
	static FAngelscriptSource FromPluginFile(const FString& PluginName, const FString& RelativeFilename, const FString& AbsoluteFilename);
	static FAngelscriptSource FromMemorySource(const FString& VirtualPath, const FString& SourceText);
	static bool TryFromMemorySource(const FString& VirtualPath, const FString& SourceText, FAngelscriptSource& OutSource, FString* OutError = nullptr);
};
