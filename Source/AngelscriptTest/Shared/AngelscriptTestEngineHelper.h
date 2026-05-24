#pragma once

#include "ClassGenerator/AngelscriptClassGenerator.h"
#include "AngelscriptEngine.h"
#include "CoreMinimal.h"

// Note: prior to Phase 2 of OpenSpec change `refactor-as-test-shared-layout-and-naming`
// this header pulled in `Shared/AngelscriptTestUtilities.h` (the umbrella header).
// That edge was vestigial — none of the declarations below require any
// umbrella-exported symbol. It was removed when `AngelscriptTestExecute.h`
// gained a forward include on this header (for `FAngelscriptCompileTraceSummary` +
// `CompileModuleWithSummary`), which would otherwise close a Helper -> umbrella
// -> Execute -> Helper #pragma-once-skip cycle and leave the struct invisible
// to `ExpectBindingCompileFailure`'s inline body. Consumers that previously
// relied on the transitive umbrella include via Helper.h should `#include
// "Shared/AngelscriptTestUtilities.h"` (or the specific themed sub-header
// they actually need) directly.

struct FAngelscriptPrecompiledData;

namespace AngelscriptTestSupport
{
	struct FAngelscriptCompileTraceDiagnosticSummary
	{
		FString Section;
		int32 Row = 0;
		int32 Column = 0;
		bool bIsError = false;
		bool bIsInfo = false;
		FString Message;
	};

	struct FAngelscriptCompileTraceSummary
	{
		bool bCompileSucceeded = false;
		ECompileType CompileType = ECompileType::SoftReloadOnly;
		ECompileResult CompileResult = ECompileResult::Error;
		bool bUsedPreprocessor = false;
		int32 ModuleDescCount = 0;
		int32 CompiledModuleCount = 0;
		TArray<FString> ModuleNames;
		TArray<FString> AbsoluteFilenames;
		TArray<FAngelscriptCompileTraceDiagnosticSummary> Diagnostics;
	};

	struct FScopedTempPrecompiledCacheFile
	{
		explicit FScopedTempPrecompiledCacheFile(FString InLabel = TEXT("PrecompiledData"));
		~FScopedTempPrecompiledCacheFile();

		const FString& GetFilename() const
		{
			return Filename;
		}

	private:
		FString Filename;
	};

	ANGELSCRIPTTEST_API bool CompileModuleWithResult(FAngelscriptEngine* Engine, ECompileType CompileType, FName ModuleName, FString Filename, FString Script, ECompileResult& OutCompileResult);
	ANGELSCRIPTTEST_API bool CompileModuleWithSummary(FAngelscriptEngine* Engine, ECompileType CompileType, FName ModuleName, FString Filename, FString Script, bool bUsePreprocessor, FAngelscriptCompileTraceSummary& OutSummary, bool bSuppressCompileErrorLogs = false);
	ANGELSCRIPTTEST_API bool AnalyzeReloadFromMemory(FAngelscriptEngine* Engine, FName ModuleName, FString Filename, FString Script, FAngelscriptClassGenerator::EReloadRequirement& OutReloadRequirement, bool& bOutWantsFullReload, bool& bOutNeedsFullReload);
	ANGELSCRIPTTEST_API bool CompileModuleFromDiskPath(FAngelscriptEngine* Engine, FName ModuleName, const FString& AbsolutePath);
	ANGELSCRIPTTEST_API bool CompileModuleFromMemory(FAngelscriptEngine* Engine, FName ModuleName, FString Filename, FString Script);
	ANGELSCRIPTTEST_API bool CompileAnnotatedModuleFromMemory(FAngelscriptEngine* Engine, FName ModuleName, FString Filename, FString Script);
	ANGELSCRIPTTEST_API bool GenerateStaticJITSourceText(FAngelscriptEngine* Engine, FName ModuleName, FString& OutSourceText, bool bEmitDebugMetadata, FString* OutError = nullptr);
	ANGELSCRIPTTEST_API bool SaveAndReloadPrecompiledData(FAngelscriptEngine* Engine, FAngelscriptPrecompiledData& SourceData, const FString& Filename, TUniquePtr<FAngelscriptPrecompiledData>& OutLoadedData, FString* OutError = nullptr);
	ANGELSCRIPTTEST_API bool ExecuteIntFunction(FAngelscriptEngine* Engine, FString Filename, FName ModuleName, FString Decl, int32& OutResult);
	ANGELSCRIPTTEST_API bool ExecuteGeneratedIntEventOnGameThread(FAngelscriptEngine* Engine, UObject* Object, UFunction* Function, int32& OutResult);
	inline bool ExecuteGeneratedIntEventOnGameThread(UObject* Object, UFunction* Function, int32& OutResult)
	{
		return ExecuteGeneratedIntEventOnGameThread(FAngelscriptEngine::TryGetCurrentEngine(), Object, Function, OutResult);
	}
	ANGELSCRIPTTEST_API bool ExecuteIntFunction(FAngelscriptEngine* Engine, FName ModuleName, FString Decl, int32& OutResult);
	ANGELSCRIPTTEST_API UClass* FindGeneratedClass(FAngelscriptEngine* Engine, FName ClassName);
	ANGELSCRIPTTEST_API UFunction* FindGeneratedFunction(UClass* OwnerClass, FName FuncName);
}
