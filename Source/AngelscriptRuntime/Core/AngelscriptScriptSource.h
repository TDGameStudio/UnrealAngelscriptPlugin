#pragma once

#include "CoreMinimal.h"

enum class EAngelscriptScriptSourceKind : uint8
{
	Unknown,
	Game,
	Plugin,
	Memory,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptScriptRoot
{
	FString AbsolutePath;
	EAngelscriptScriptSourceKind SourceKind = EAngelscriptScriptSourceKind::Unknown;
	FString MountName;

	static FAngelscriptScriptRoot FromGameRoot(const FString& AbsolutePath);
	static FAngelscriptScriptRoot FromPluginRoot(const FString& PluginName, const FString& AbsolutePath);
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptVirtualScriptPath
{
	static constexpr const TCHAR* Root = TEXT("/Angelscript");
	static constexpr const TCHAR* GameRoot = TEXT("/Angelscript/Game");
	static constexpr const TCHAR* PluginRoot = TEXT("/Angelscript/Plugin");
	static constexpr const TCHAR* MemoryRoot = TEXT("/Angelscript/Memory");

	static bool TryParse(const FString& InPath, FAngelscriptVirtualScriptPath& OutPath, FString* OutError = nullptr);
	static FAngelscriptVirtualScriptPath FromGameRelativePath(const FString& RelativePath);
	static FAngelscriptVirtualScriptPath FromPluginRelativePath(const FString& PluginName, const FString& RelativePath);
	static FAngelscriptVirtualScriptPath FromMemoryRelativePath(const FString& ProviderName, const FString& RelativePath);

	const FString& ToString() const;
	EAngelscriptScriptSourceKind GetSourceKind() const;
	const FString& GetMountName() const;
	const FString& GetRelativePath() const;
	FString ToModuleName() const;
	bool IsValid() const;

private:
	void SetParsed(
		const FString& InPath,
		EAngelscriptScriptSourceKind InSourceKind,
		const FString& InMountName,
		const FString& InRelativePath);

	FString Path;
	EAngelscriptScriptSourceKind SourceKind = EAngelscriptScriptSourceKind::Unknown;
	FString MountName;
	FString RelativePath;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptScriptSource
{
	FAngelscriptVirtualScriptPath VirtualPath;
	FString ModuleName;
	FString RelativeFilename;
	FString AbsoluteFilename;
	FString SourceText;
	EAngelscriptScriptSourceKind SourceKind = EAngelscriptScriptSourceKind::Unknown;
	bool bHasSourceText = false;

	static FAngelscriptScriptSource FromGameFile(const FString& RelativeFilename, const FString& AbsoluteFilename);
	static FAngelscriptScriptSource FromPluginFile(const FString& PluginName, const FString& RelativeFilename, const FString& AbsoluteFilename);
	static FAngelscriptScriptSource FromMemorySource(const FString& VirtualPath, const FString& SourceText);
	static bool TryFromMemorySource(const FString& VirtualPath, const FString& SourceText, FAngelscriptScriptSource& OutSource, FString* OutError = nullptr);
};
