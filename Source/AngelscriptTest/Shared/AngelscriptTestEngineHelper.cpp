#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestUtilities.h"

#include "ClassGenerator/AngelscriptClassGenerator.h"
#include "ClassGenerator/ASClass.h"
#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Preprocessor/AngelscriptPreprocessor.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "StaticJIT/PrecompiledData.h"
#include "StaticJIT/AngelscriptStaticJIT.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	static const TCHAR* EmptySourceDiagnostic = TEXT("Script file contains no code to compile.");

	TSharedRef<FAngelscriptModuleDesc> MakeModuleDesc(FName ModuleName, FString Filename, FString Script);

	void AddUniqueAbsoluteFilename(TArray<FString>& AbsoluteFilenames, const FString& AbsoluteFilename)
	{
		if (!AbsoluteFilename.IsEmpty())
		{
			AbsoluteFilenames.AddUnique(AbsoluteFilename);
		}
	}

	void CollectCompileTraceDiagnostics(const FAngelscriptEngine& Engine, const TArray<FString>& AbsoluteFilenames, TArray<FAngelscriptCompileTraceDiagnosticSummary>& OutDiagnostics)
	{
		for (const FString& AbsoluteFilename : AbsoluteFilenames)
		{
			if (const FAngelscriptEngine::FDiagnostics* Diagnostics = Engine.Diagnostics.Find(AbsoluteFilename))
			{
				for (const FAngelscriptEngine::FDiagnostic& Diagnostic : Diagnostics->Diagnostics)
				{
					FAngelscriptCompileTraceDiagnosticSummary& Summary = OutDiagnostics.AddDefaulted_GetRef();
					Summary.Section = AbsoluteFilename;
					Summary.Row = Diagnostic.Row;
					Summary.Column = Diagnostic.Column;
					Summary.bIsError = Diagnostic.bIsError;
					Summary.bIsInfo = Diagnostic.bIsInfo;
					Summary.Message = Diagnostic.Message;
				}
			}
		}
	}

	FString ResolveAutomationAbsoluteFilename(const FString& Filename)
	{
		if (FPaths::IsRelative(Filename))
		{
			return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), Filename);
		}

		return Filename;
	}

	bool IsEffectivelyEmptyScript(const FString& Script)
	{
		return Script.TrimStartAndEnd().IsEmpty();
	}

	void EmitCompileTraceError(FAngelscriptEngine& Engine, const FString& AbsoluteFilename, const FString& Message)
	{
		FAngelscriptEngine::FDiagnostic Diagnostic;
		Diagnostic.Message = Message;
		Diagnostic.Row = 1;
		Diagnostic.Column = 1;
		Diagnostic.bIsError = true;
		Diagnostic.bIsInfo = false;

		Engine.ScriptCompileError(AbsoluteFilename, Diagnostic);
	}

	bool BuildModulesForSummary(FName ModuleName, FString Filename, FString Script, bool bUsePreprocessor, TArray<TSharedRef<FAngelscriptModuleDesc>>& OutModules, TArray<FString>& OutAbsoluteFilenames)
	{
		if (bUsePreprocessor)
		{
			const FString AutomationDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"));
			const FString AbsoluteFilename = ResolveAutomationAbsoluteFilename(Filename);
			AddUniqueAbsoluteFilename(OutAbsoluteFilenames, AbsoluteFilename);
			IFileManager::Get().MakeDirectory(*AutomationDirectory, true);
			if (!FFileHelper::SaveStringToFile(Script, *AbsoluteFilename))
			{
				return false;
			}

			FAngelscriptPreprocessor Preprocessor;
			Preprocessor.AddFile(Filename, AbsoluteFilename);
			if (!Preprocessor.Preprocess())
			{
				return false;
			}

			OutModules = Preprocessor.GetModulesToCompile();
			for (const TSharedRef<FAngelscriptModuleDesc>& Module : OutModules)
			{
				for (const FAngelscriptModuleDesc::FCodeSection& Section : Module->Code)
				{
					AddUniqueAbsoluteFilename(OutAbsoluteFilenames, Section.AbsoluteFilename);
				}
			}
			return OutModules.Num() > 0;
		}

		TSharedRef<FAngelscriptModuleDesc> ModuleDesc = MakeModuleDesc(ModuleName, MoveTemp(Filename), MoveTemp(Script));
		for (const FAngelscriptModuleDesc::FCodeSection& Section : ModuleDesc->Code)
		{
			AddUniqueAbsoluteFilename(OutAbsoluteFilenames, Section.AbsoluteFilename);
		}
		OutModules.Add(ModuleDesc);
		return true;
	}

	bool CompilePreparedModules(FAngelscriptEngine* Engine, ECompileType CompileType, const TArray<TSharedRef<FAngelscriptModuleDesc>>& ModulesToCompile, ECompileResult& OutCompileResult, int32& OutCompiledModuleCount, bool bSuppressCompileErrorLogs)
	{
		if (Engine == nullptr)
		{
			OutCompileResult = ECompileResult::Error;
			OutCompiledModuleCount = 0;
			return false;
		}

		TArray<TSharedRef<FAngelscriptModuleDesc>> CompiledModules;
		TGuardValue<bool> AutomaticImportGuard(Engine->bUseAutomaticImportMethod, false);
		FScopedAutomaticImportsOverride AutomaticImportsOverride(Engine->GetScriptEngine());
		FAngelscriptEngineScope EngineScope(*Engine);
		if (bSuppressCompileErrorLogs)
		{
			UE_SET_LOG_VERBOSITY(Angelscript, Fatal);
		}
		OutCompileResult = Engine->CompileModules(CompileType, ModulesToCompile, CompiledModules);
		if (bSuppressCompileErrorLogs)
		{
			UE_SET_LOG_VERBOSITY(Angelscript, Log);
		}
		OutCompiledModuleCount = CompiledModules.Num();
		return OutCompileResult == ECompileResult::FullyHandled || OutCompileResult == ECompileResult::PartiallyHandled;
	}

	TSharedRef<FAngelscriptModuleDesc> MakeModuleDesc(FName ModuleName, FString Filename, FString Script)
	{
		const FString ModuleNameString = ModuleName.ToString();
		const FString RelativeFilename = FPaths::IsRelative(Filename)
			? FPaths::Combine(TEXT("Automation"), Filename)
			: FPaths::GetCleanFilename(Filename);
		const FString AbsoluteFilename = FPaths::IsRelative(Filename)
			? FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), Filename)
			: Filename;

		TSharedRef<FAngelscriptModuleDesc> ModuleDesc = MakeShared<FAngelscriptModuleDesc>();
		ModuleDesc->ModuleName = ModuleNameString;

		FAngelscriptModuleDesc::FCodeSection& Section = ModuleDesc->Code.AddDefaulted_GetRef();
		Section.RelativeFilename = RelativeFilename;
		Section.AbsoluteFilename = AbsoluteFilename;
		Section.Code = MoveTemp(Script);
		Section.CodeHash = static_cast<int64>(FCrc::StrCrc32(*Section.Code));
		ModuleDesc->CodeHash ^= Section.CodeHash;
		return ModuleDesc;
	}

	bool CompileModuleInternal(FAngelscriptEngine* Engine, ECompileType CompileType, FName ModuleName, FString Filename, FString Script, ECompileResult* OutCompileResult = nullptr)
	{
		if (Engine == nullptr)
		{
			if (OutCompileResult != nullptr)
			{
				*OutCompileResult = ECompileResult::Error;
			}
			return false;
		}

		TSharedRef<FAngelscriptModuleDesc> ModuleDesc = MakeModuleDesc(ModuleName, MoveTemp(Filename), MoveTemp(Script));

		TArray<TSharedRef<FAngelscriptModuleDesc>> ModulesToCompile;
		ModulesToCompile.Add(ModuleDesc);

		TArray<TSharedRef<FAngelscriptModuleDesc>> CompiledModules;
		TGuardValue<bool> AutomaticImportGuard(Engine->bUseAutomaticImportMethod, false);
		FScopedAutomaticImportsOverride AutomaticImportsOverride(Engine->GetScriptEngine());
		FAngelscriptEngineScope EngineScope(*Engine);
		const ECompileResult CompileResult = Engine->CompileModules(CompileType, ModulesToCompile, CompiledModules);
		if (OutCompileResult != nullptr)
		{
			*OutCompileResult = CompileResult;
		}
		return CompileResult == ECompileResult::FullyHandled || CompileResult == ECompileResult::PartiallyHandled;
	}

	bool PreprocessAndCompile(FAngelscriptEngine* Engine, ECompileType CompileType, FString Filename, FString Script, ECompileResult* OutCompileResult = nullptr)
	{
		if (Engine == nullptr)
		{
			if (OutCompileResult != nullptr)
			{
				*OutCompileResult = ECompileResult::Error;
			}
			return false;
		}

		FAngelscriptEngineScope EngineScope(*Engine);
		FString AbsoluteFilename;

		if (FPaths::IsRelative(Filename))
		{
			const FString TempDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"));
			AbsoluteFilename = FPaths::Combine(TempDir, Filename);
			IFileManager::Get().MakeDirectory(*TempDir, true);
			if (!FFileHelper::SaveStringToFile(Script, *AbsoluteFilename))
			{
				if (OutCompileResult != nullptr)
				{
					*OutCompileResult = ECompileResult::Error;
				}
				return false;
			}
		}
		else
		{
			AbsoluteFilename = Filename;
		}

		if (IsEffectivelyEmptyScript(Script))
		{
			EmitCompileTraceError(*Engine, AbsoluteFilename, EmptySourceDiagnostic);
			if (OutCompileResult != nullptr)
			{
				*OutCompileResult = ECompileResult::Error;
			}
			return false;
		}

		FAngelscriptPreprocessor Preprocessor;
		Preprocessor.AddFile(Filename, AbsoluteFilename);
		if (!Preprocessor.Preprocess())
		{
			if (OutCompileResult != nullptr)
			{
				*OutCompileResult = ECompileResult::Error;
			}
			return false;
		}

		TArray<TSharedRef<FAngelscriptModuleDesc>> ModulesToCompile = Preprocessor.GetModulesToCompile();
		if (ModulesToCompile.Num() == 0)
		{
			if (OutCompileResult != nullptr)
			{
				*OutCompileResult = ECompileResult::Error;
			}
			return false;
		}

		TArray<TSharedRef<FAngelscriptModuleDesc>> CompiledModules;
		TGuardValue<bool> AutomaticImportGuard(Engine->bUseAutomaticImportMethod, false);
		FScopedAutomaticImportsOverride AutomaticImportsOverride(Engine->GetScriptEngine());
		const ECompileResult CompileResult = Engine->CompileModules(CompileType, ModulesToCompile, CompiledModules);
		if (OutCompileResult != nullptr)
		{
			*OutCompileResult = CompileResult;
		}
		return CompileResult == ECompileResult::FullyHandled || CompileResult == ECompileResult::PartiallyHandled;
	}

	asIScriptFunction* FindFunctionByDecl(asIScriptModule& Module, const FString& Declaration)
	{
		FTCHARToUTF8 DeclarationUtf8(*Declaration);
		if (asIScriptFunction* Function = Module.GetFunctionByDecl(DeclarationUtf8.Get()))
		{
			return Function;
		}

		FString FunctionName;
		int32 OpenParenIndex = INDEX_NONE;
		if (Declaration.FindChar(TEXT('('), OpenParenIndex))
		{
			const FString Prefix = Declaration.Left(OpenParenIndex).TrimStartAndEnd();
			int32 NameSeparatorIndex = INDEX_NONE;
			if (Prefix.FindLastChar(TEXT(' '), NameSeparatorIndex))
			{
				FunctionName = Prefix.Mid(NameSeparatorIndex + 1).TrimStartAndEnd();
			}
		}

		if (FunctionName.IsEmpty())
		{
			return nullptr;
		}

		FTCHARToUTF8 FunctionNameUtf8(*FunctionName);
		if (asIScriptFunction* Function = Module.GetFunctionByName(FunctionNameUtf8.Get()))
		{
			return Function;
		}

		const asUINT FunctionCount = Module.GetFunctionCount();
		for (asUINT FunctionIndex = 0; FunctionIndex < FunctionCount; ++FunctionIndex)
		{
			asIScriptFunction* CandidateFunction = Module.GetFunctionByIndex(FunctionIndex);
			if (CandidateFunction != nullptr && FunctionName.Equals(UTF8_TO_TCHAR(CandidateFunction->GetName())))
			{
				return CandidateFunction;
			}
		}

		return nullptr;
	}
}

bool CompileModuleWithResult(FAngelscriptEngine* Engine, ECompileType CompileType, FName ModuleName, FString Filename, FString Script, ECompileResult& OutCompileResult)
{
	const bool bHasAnnotations = Script.Contains(TEXT("UCLASS(")) || Script.Contains(TEXT("USTRUCT(")) || Script.Contains(TEXT("UENUM("));
	if (bHasAnnotations)
	{
		return PreprocessAndCompile(Engine, CompileType, MoveTemp(Filename), MoveTemp(Script), &OutCompileResult);
	}
	return CompileModuleInternal(Engine, CompileType, ModuleName, MoveTemp(Filename), MoveTemp(Script), &OutCompileResult);
}

bool CompileModuleWithSummary(FAngelscriptEngine* Engine, ECompileType CompileType, FName ModuleName, FString Filename, FString Script, bool bUsePreprocessor, FAngelscriptCompileTraceSummary& OutSummary, bool bSuppressCompileErrorLogs)
{
	OutSummary = FAngelscriptCompileTraceSummary();
	OutSummary.CompileType = CompileType;
	OutSummary.bUsedPreprocessor = bUsePreprocessor;

	if (Engine == nullptr)
	{
		return false;
	}

	Engine->Diagnostics.Empty();
	Engine->LastEmittedDiagnostics.Empty();
	Engine->bDiagnosticsDirty = false;
	Engine->bIgnoreCompileErrorDiagnostics = false;

	if (bUsePreprocessor && IsEffectivelyEmptyScript(Script))
	{
		const FString AbsoluteFilename = ResolveAutomationAbsoluteFilename(Filename);
		AddUniqueAbsoluteFilename(OutSummary.AbsoluteFilenames, AbsoluteFilename);
		EmitCompileTraceError(*Engine, AbsoluteFilename, EmptySourceDiagnostic);
		OutSummary.CompileResult = ECompileResult::Error;
		CollectCompileTraceDiagnostics(*Engine, OutSummary.AbsoluteFilenames, OutSummary.Diagnostics);
		return false;
	}

	TArray<TSharedRef<FAngelscriptModuleDesc>> ModulesToCompile;
	if (!BuildModulesForSummary(ModuleName, Filename, Script, bUsePreprocessor, ModulesToCompile, OutSummary.AbsoluteFilenames))
	{
		OutSummary.CompileResult = ECompileResult::Error;
		CollectCompileTraceDiagnostics(*Engine, OutSummary.AbsoluteFilenames, OutSummary.Diagnostics);
		return false;
	}

	OutSummary.ModuleDescCount = ModulesToCompile.Num();
	for (const TSharedRef<FAngelscriptModuleDesc>& Module : ModulesToCompile)
	{
		OutSummary.ModuleNames.Add(Module->ModuleName);
	}

	OutSummary.bCompileSucceeded = CompilePreparedModules(Engine, CompileType, ModulesToCompile, OutSummary.CompileResult, OutSummary.CompiledModuleCount, bSuppressCompileErrorLogs);
	CollectCompileTraceDiagnostics(*Engine, OutSummary.AbsoluteFilenames, OutSummary.Diagnostics);
	return OutSummary.bCompileSucceeded;
}

bool AnalyzeReloadFromMemory(FAngelscriptEngine* Engine, FName ModuleName, FString Filename, FString Script, FAngelscriptClassGenerator::EReloadRequirement& OutReloadRequirement, bool& bOutWantsFullReload, bool& bOutNeedsFullReload)
{
	ECompileResult CompileResult = ECompileResult::Error;
	UE_SET_LOG_VERBOSITY(Angelscript, Fatal);
	const bool bCompiled = PreprocessAndCompile(Engine, ECompileType::SoftReloadOnly, MoveTemp(Filename), MoveTemp(Script), &CompileResult);
	UE_SET_LOG_VERBOSITY(Angelscript, Log);

	bOutWantsFullReload = false;
	bOutNeedsFullReload = false;

	switch (CompileResult)
	{
	case ECompileResult::FullyHandled:
		OutReloadRequirement = FAngelscriptClassGenerator::SoftReload;
		return bCompiled;

	case ECompileResult::PartiallyHandled:
		OutReloadRequirement = FAngelscriptClassGenerator::FullReloadSuggested;
		bOutWantsFullReload = true;
		return true;

	case ECompileResult::ErrorNeedFullReload:
		OutReloadRequirement = FAngelscriptClassGenerator::FullReloadRequired;
		bOutWantsFullReload = true;
		bOutNeedsFullReload = true;
		return true;

	case ECompileResult::Error:
	default:
		OutReloadRequirement = FAngelscriptClassGenerator::Error;
		return false;
	}
}

bool CompileModuleFromDiskPath(FAngelscriptEngine* Engine, FName ModuleName, const FString& AbsolutePath)
{
	if (Engine == nullptr || AbsolutePath.IsEmpty())
	{
		return false;
	}

	const FString NormalizedAbsolutePath = FPaths::ConvertRelativePathToFull(AbsolutePath);
	const FString RootPath = FPaths::GetPath(NormalizedAbsolutePath);
	const TArray<FString> PreviousRoots = Engine->AllRootPaths;
	ON_SCOPE_EXIT
	{
		Engine->AllRootPaths = PreviousRoots;
	};

	Engine->AllRootPaths = { RootPath };

	TArray<FAngelscriptEngine::FFilenamePair> DiskFiles;
	Engine->FindAllScriptFilenames(DiskFiles);

	const FAngelscriptEngine::FFilenamePair* TargetFile = DiskFiles.FindByPredicate(
		[&NormalizedAbsolutePath](const FAngelscriptEngine::FFilenamePair& FilenamePair)
		{
			return FilenamePair.AbsolutePath.Equals(NormalizedAbsolutePath, ESearchCase::IgnoreCase);
		});
	if (TargetFile == nullptr)
	{
		return false;
	}

	FAngelscriptPreprocessor Preprocessor;
	Preprocessor.AddFile(TargetFile->RelativePath, TargetFile->AbsolutePath);
	if (!Preprocessor.Preprocess())
	{
		return false;
	}

	TArray<TSharedRef<FAngelscriptModuleDesc>> ModulesToCompile = Preprocessor.GetModulesToCompile();
	if (ModulesToCompile.Num() == 0)
	{
		return false;
	}

	TArray<TSharedRef<FAngelscriptModuleDesc>> CompiledModules;
	TGuardValue<bool> AutomaticImportGuard(Engine->bUseAutomaticImportMethod, false);
	FScopedAutomaticImportsOverride AutomaticImportsOverride(Engine->GetScriptEngine());
	FAngelscriptEngineScope EngineScope(*Engine);
	const ECompileResult CompileResult = Engine->CompileModules(ECompileType::SoftReloadOnly, ModulesToCompile, CompiledModules);
	if (CompileResult != ECompileResult::FullyHandled && CompileResult != ECompileResult::PartiallyHandled)
	{
		return false;
	}

	return Engine->GetModuleByFilenameOrModuleName(TargetFile->AbsolutePath, ModuleName.ToString()).IsValid();
}

FScopedTempPrecompiledCacheFile::FScopedTempPrecompiledCacheFile(FString InLabel)
{
	if (InLabel.IsEmpty())
	{
		InLabel = TEXT("PrecompiledData");
	}

	const FString CacheDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("StaticJIT"));
	IFileManager::Get().MakeDirectory(*CacheDirectory, true);

	const FString UniqueSuffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Filename = FPaths::Combine(CacheDirectory, FString::Printf(TEXT("%s_%s.Cache"), *InLabel, *UniqueSuffix));
}

FScopedTempPrecompiledCacheFile::~FScopedTempPrecompiledCacheFile()
{
	if (!Filename.IsEmpty())
	{
		IFileManager::Get().Delete(*Filename, false, true, true);
	}
}

bool CompileModuleFromMemory(FAngelscriptEngine* Engine, FName ModuleName, FString Filename, FString Script)
{
	return CompileModuleInternal(Engine, ECompileType::SoftReloadOnly, ModuleName, MoveTemp(Filename), MoveTemp(Script));
}

bool CompileMemorySource(FAngelscriptEngine* Engine, const FString& VirtualPath, FString Script)
{
	if (Engine == nullptr)
	{
		return false;
	}

	FAngelscriptPreprocessor Preprocessor;
	Preprocessor.AddSource(FAngelscriptScriptSource::FromMemorySource(VirtualPath, MoveTemp(Script)));
	if (!Preprocessor.Preprocess())
	{
		return false;
	}

	TArray<TSharedRef<FAngelscriptModuleDesc>> ModulesToCompile = Preprocessor.GetModulesToCompile();
	if (ModulesToCompile.Num() == 0)
	{
		return false;
	}

	ECompileResult CompileResult = ECompileResult::Error;
	int32 CompiledModuleCount = 0;
	return CompilePreparedModules(
		Engine,
		ECompileType::SoftReloadOnly,
		ModulesToCompile,
		CompileResult,
		CompiledModuleCount,
		/*bSuppressCompileErrorLogs=*/ false);
}

bool CompileAnnotatedModuleFromMemory(FAngelscriptEngine* Engine, FName ModuleName, FString Filename, FString Script)
{
	if (Engine == nullptr)
	{
		return false;
	}
	return PreprocessAndCompile(Engine, ECompileType::FullReload, MoveTemp(Filename), MoveTemp(Script));
}

bool SaveAndReloadPrecompiledData(FAngelscriptEngine* Engine, FAngelscriptPrecompiledData& SourceData, const FString& Filename, TUniquePtr<FAngelscriptPrecompiledData>& OutLoadedData, FString* OutError)
{
	OutLoadedData.Reset();

	if (Engine == nullptr)
	{
		if (OutError != nullptr)
		{
			*OutError = TEXT("SaveAndReloadPrecompiledData failed: engine was null.");
		}
		return false;
	}

	if (Filename.IsEmpty())
	{
		if (OutError != nullptr)
		{
			*OutError = TEXT("SaveAndReloadPrecompiledData failed: filename was empty.");
		}
		return false;
	}

	asIScriptEngine* ScriptEngine = Engine->GetScriptEngine();
	if (ScriptEngine == nullptr)
	{
		if (OutError != nullptr)
		{
			*OutError = TEXT("SaveAndReloadPrecompiledData failed: script engine was null.");
		}
		return false;
	}

	SourceData.Save(Filename);
	if (!IFileManager::Get().FileExists(*Filename))
	{
		if (OutError != nullptr)
		{
			*OutError = FString::Printf(TEXT("SaveAndReloadPrecompiledData failed: cache file was not created at '%s'."), *Filename);
		}
		return false;
	}

	OutLoadedData = MakeUnique<FAngelscriptPrecompiledData>(ScriptEngine);
	OutLoadedData->Load(Filename);
	return true;
}

bool GenerateStaticJITSourceText(FAngelscriptEngine* Engine, FName ModuleName, FString& OutSourceText, bool bEmitDebugMetadata, FString* OutError)
{
	OutSourceText.Reset();
	if (Engine == nullptr)
	{
		if (OutError != nullptr)
		{
			*OutError = TEXT("GenerateStaticJITSourceText failed: engine was null.");
		}
		return false;
	}

#if WITH_DEV_AUTOMATION_TESTS && AS_CAN_GENERATE_JIT
	TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine->GetModuleByModuleName(ModuleName.ToString());
	if (!ModuleDesc.IsValid() || ModuleDesc->ScriptModule == nullptr)
	{
		if (OutError != nullptr)
		{
			*OutError = FString::Printf(TEXT("GenerateStaticJITSourceText failed: module '%s' was not compiled."), *ModuleName.ToString());
		}
		return false;
	}

	FAngelscriptEngineScope EngineScope(*Engine);
	return GenerateStaticJITSourceTextForDiagnostics(ModuleDesc->ScriptModule, OutSourceText, bEmitDebugMetadata, OutError);
#else
	if (OutError != nullptr)
	{
		*OutError = TEXT("GenerateStaticJITSourceText is unavailable when AS_CAN_GENERATE_JIT or WITH_DEV_AUTOMATION_TESTS is disabled.");
	}
	return false;
#endif
}

bool ExecuteIntFunction(FAngelscriptEngine* Engine, FName ModuleName, FString Decl, int32& OutResult)
{
	return ExecuteIntFunction(Engine, FString(), ModuleName, MoveTemp(Decl), OutResult);
}

bool ExecuteIntFunction(FAngelscriptEngine* Engine, FString Filename, FName ModuleName, FString Decl, int32& OutResult)
{
	if (Engine == nullptr)
	{
		UE_LOG(Angelscript, Warning, TEXT("ExecuteIntFunction failed: engine was null for module=%s decl=%s"), *ModuleName.ToString(), *Decl);
		return false;
	}

	TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Filename.IsEmpty()
		? Engine->GetModuleByModuleName(ModuleName.ToString())
		: Engine->GetModuleByFilenameOrModuleName(Filename, ModuleName.ToString());
	if (!ModuleDesc.IsValid() || ModuleDesc->ScriptModule == nullptr)
	{
		UE_LOG(Angelscript, Warning, TEXT("ExecuteIntFunction failed: module lookup failed for filename=%s module=%s decl=%s"), *Filename, *ModuleName.ToString(), *Decl);
		return false;
	}

	asIScriptModule* Module = ModuleDesc->ScriptModule;

	asIScriptFunction* Function = FindFunctionByDecl(*Module, Decl);
	if (Function == nullptr)
	{
		UE_LOG(Angelscript, Warning, TEXT("ExecuteIntFunction failed: function lookup failed for module=%s decl=%s"), *ModuleName.ToString(), *Decl);
		return false;
	}

	FAngelscriptEngineScope EngineScope(*Engine);

	asIScriptContext* Context = Engine->CreateContext();
	if (Context == nullptr)
	{
		UE_LOG(Angelscript, Warning, TEXT("ExecuteIntFunction failed: context creation failed for module=%s decl=%s"), *ModuleName.ToString(), *Decl);
		return false;
	}

	ON_SCOPE_EXIT
	{
		Context->Release();
	};

	const int PrepareResult = Context->Prepare(Function);
	if (PrepareResult != asSUCCESS)
	{
		UE_LOG(Angelscript, Warning, TEXT("ExecuteIntFunction failed: prepare returned %d for module=%s decl=%s"), PrepareResult, *ModuleName.ToString(), *Decl);
		return false;
	}

	const int ExecuteResult = Context->Execute();
	if (ExecuteResult != asEXECUTION_FINISHED)
	{
		UE_LOG(Angelscript, Warning, TEXT("ExecuteIntFunction failed: execute returned %d for module=%s decl=%s"), ExecuteResult, *ModuleName.ToString(), *Decl);
		if (Context->GetExceptionString() != nullptr)
		{
			UE_LOG(Angelscript, Warning, TEXT("ExecuteIntFunction exception: %s"), UTF8_TO_TCHAR(Context->GetExceptionString()));
		}
		return false;
	}

	OutResult = static_cast<int32>(Context->GetReturnDWord());
	return true;
}

bool ExecuteGeneratedIntEventOnGameThread(FAngelscriptEngine* Engine, UObject* Object, UFunction* Function, int32& OutResult)
{
	if (Object == nullptr || Function == nullptr)
	{
		return false;
	}

	auto Invoke = [Engine, Object, Function, &OutResult]()
	{
		if (Engine != nullptr)
		{
			FAngelscriptEngineScope EngineScope(*Engine, Object);
			if (UASFunction* ScriptFunction = Cast<UASFunction>(Function))
			{
				ScriptFunction->RuntimeCallEvent(Object, &OutResult);
			}
			else
			{
				Object->ProcessEvent(Function, &OutResult);
			}
		}
		else
		{
			FScopedTestWorldContextScope WorldContextScope(Object);
			if (UASFunction* ScriptFunction = Cast<UASFunction>(Function))
			{
				ScriptFunction->RuntimeCallEvent(Object, &OutResult);
			}
			else
			{
				Object->ProcessEvent(Function, &OutResult);
			}
		}
	};

	if (IsInGameThread())
	{
		Invoke();
		return true;
	}

	FEvent* CompletedEvent = FPlatformProcess::GetSynchEventFromPool(true);
	AsyncTask(ENamedThreads::GameThread, [Invoke, CompletedEvent]() mutable
	{
		Invoke();
		CompletedEvent->Trigger();
	});

	CompletedEvent->Wait();
	FPlatformProcess::ReturnSynchEventToPool(CompletedEvent);
	return true;
}

UClass* FindGeneratedClass(FAngelscriptEngine* Engine, FName ClassName)
{
	if (Engine == nullptr)
	{
		return nullptr;
	}

	UPackage* Package = Engine->GetPackageInstance();
	if (Package == nullptr)
	{
		return nullptr;
	}

	const FString ClassNameStr = ClassName.ToString();
	if (UClass* Found = FindObject<UClass>(Package, *ClassNameStr))
	{
		if (UASClass* ScriptClass = Cast<UASClass>(Found))
		{
			return ScriptClass->GetMostUpToDateClass();
		}
		return Found;
	}

	if (ClassNameStr.Len() >= 2 && (ClassNameStr[0] == TEXT('U') || ClassNameStr[0] == TEXT('A')) && FChar::IsUpper(ClassNameStr[1]))
	{
		if (UClass* Found = FindObject<UClass>(Package, *ClassNameStr.Mid(1)))
		{
			if (UASClass* ScriptClass = Cast<UASClass>(Found))
			{
				return ScriptClass->GetMostUpToDateClass();
			}
			return Found;
		}
	}

	return nullptr;
}

UFunction* FindGeneratedFunction(UClass* OwnerClass, FName FuncName)
{
	if (UASClass* ScriptClass = Cast<UASClass>(OwnerClass))
	{
		OwnerClass = ScriptClass->GetMostUpToDateClass();
	}
	return OwnerClass != nullptr ? OwnerClass->FindFunctionByName(FuncName) : nullptr;
}

