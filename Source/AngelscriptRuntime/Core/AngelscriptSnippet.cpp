#include "AngelscriptSnippet.h"

#include "AngelscriptEngine.h"
#include "AngelscriptSource.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformAtomics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Misc/OutputDevice.h"
#include "Preprocessor/AngelscriptPreprocessor.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_context.h"
#include "source/as_module.h"
#include "EndAngelscriptHeaders.h"

namespace AngelscriptSnippet_Private
{
	static int32 GSnippetCounter = 0;
	static constexpr int32 StatementModeLineOffset = 2;

	struct FSnippetIdentity
	{
		int32 Id = 0;
		FString LeafName;
		FString EntryPointName;
		FString EntryPointDeclaration;
	};

	FSnippetIdentity MakeSnippetIdentity(const FString& Label)
	{
		const int32 SnippetId = FPlatformAtomics::InterlockedIncrement(&GSnippetCounter);
		FString SanitizedLabel = Label;
		if (!SanitizedLabel.IsEmpty())
		{
			for (TCHAR& Character : SanitizedLabel)
			{
				if (!FChar::IsAlnum(Character) && Character != TEXT('_'))
				{
					Character = TEXT('_');
				}
			}
			SanitizedLabel = SanitizedLabel.TrimStartAndEnd();
		}

		FSnippetIdentity Identity;
		Identity.Id = SnippetId;
		Identity.EntryPointName = FString::Printf(TEXT("__SnippetMain_%03d"), SnippetId);
		Identity.EntryPointDeclaration = FString::Printf(TEXT("void %s()"), *Identity.EntryPointName);
		if (SanitizedLabel.IsEmpty())
		{
			Identity.LeafName = FString::Printf(TEXT("Snippet_%03d.as"), SnippetId);
			return Identity;
		}

		Identity.LeafName = FString::Printf(TEXT("Snippet_%03d_%s.as"), SnippetId, *SanitizedLabel);
		return Identity;
	}

	FString MakeVirtualPath(const FSnippetIdentity& Identity)
	{
		const FAngelscriptVirtualPath VirtualPath = FAngelscriptVirtualPath::FromMemoryRelativePath(
			TEXT("Immediate"),
			Identity.LeafName);
		return VirtualPath.ToString();
	}

	FString MakeSourceText(const FAngelscriptSnippetRequest& Request, const FSnippetIdentity& Identity)
	{
		if (Request.SourceMode == EAngelscriptSnippetSourceMode::FullSource)
		{
			return Request.SourceText;
		}

		return FString::Printf(TEXT("void %s()\n{\n%s\n}\n"), *Identity.EntryPointName, *Request.SourceText);
	}

	int32 ToUserRow(EAngelscriptSnippetSourceMode SourceMode, int32 Row)
	{
		if (Row <= 0)
		{
			return Row;
		}

		if (SourceMode == EAngelscriptSnippetSourceMode::Statements)
		{
			return FMath::Max(1, Row - StatementModeLineOffset);
		}

		return Row;
	}

	void CollectDiagnostics(
		const FAngelscriptEngine& Engine,
		const FString& VirtualPath,
		EAngelscriptSnippetSourceMode SourceMode,
		TArray<FAngelscriptSnippetDiagnostic>& OutDiagnostics)
	{
		const FAngelscriptEngine::FDiagnostics* Diagnostics = Engine.Diagnostics.Find(VirtualPath);
		if (Diagnostics == nullptr)
		{
			Diagnostics = Engine.Diagnostics.Find(FString());
		}
		if (Diagnostics == nullptr)
		{
			return;
		}

		for (const FAngelscriptEngine::FDiagnostic& Diagnostic : Diagnostics->Diagnostics)
		{
			FAngelscriptSnippetDiagnostic& OutDiagnostic = OutDiagnostics.AddDefaulted_GetRef();
			OutDiagnostic.Section = VirtualPath;
			OutDiagnostic.Row = Diagnostic.Row;
			OutDiagnostic.Column = Diagnostic.Column;
			OutDiagnostic.bIsError = Diagnostic.bIsError;
			OutDiagnostic.bIsInfo = Diagnostic.bIsInfo;
			OutDiagnostic.Message = Diagnostic.Message;
			OutDiagnostic.UserRow = ToUserRow(SourceMode, Diagnostic.Row);
		}
	}

	void DiscardModuleIfRequested(FAngelscriptEngine& Engine, const FAngelscriptSnippetRequest& Request, const FString& ModuleName)
	{
		if (!ModuleName.IsEmpty() && Request.bDiscardModuleAfterExecute && !Request.bKeepModuleForDebugging)
		{
			Engine.DiscardModule(*ModuleName);
		}
	}

	void RemoveSnippetDiagnostics(FAngelscriptEngine& Engine, const FString& VirtualPath)
	{
		Engine.Diagnostics.Remove(VirtualPath);
		Engine.Diagnostics.Remove(FString());
	}

	void FillExceptionResult(
		asIScriptContext& Context,
		EAngelscriptSnippetSourceMode SourceMode,
		FAngelscriptSnippetResult& Result)
	{
		Result.ResultCode = EAngelscriptSnippetResultCode::ExecutionException;
		Result.ExceptionMessage = Context.GetExceptionString() != nullptr
			? UTF8_TO_TCHAR(Context.GetExceptionString())
			: TEXT("");

		const char* SectionName = nullptr;
		const int32 ExceptionLine = Context.GetExceptionLineNumber(nullptr, &SectionName);
		Result.ExceptionSection = SectionName != nullptr ? UTF8_TO_TCHAR(SectionName) : TEXT("");
		Result.ExceptionLine = ToUserRow(SourceMode, ExceptionLine);
		Result.ErrorMessage = Result.ExceptionMessage;
	}

	FString ResolveConsoleSnippetPath(const FString& InputPath)
	{
		const FString TrimmedPath = InputPath.TrimQuotes();
		if (FPaths::IsRelative(TrimmedPath))
		{
			return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), TrimmedPath);
		}

		return FPaths::ConvertRelativePathToFull(TrimmedPath);
	}

	void WriteSnippetResultToConsole(const FAngelscriptSnippetResult& Result, FOutputDevice& OutputDevice)
	{
		if (Result.bSucceeded)
		{
			OutputDevice.Logf(
				TEXT("Angelscript snippet succeeded: %s (%s)"),
				*Result.VirtualPath,
				*Result.ModuleName);
			return;
		}

		OutputDevice.Logf(
			TEXT("Angelscript snippet failed: %s (%s)"),
			*Result.ErrorMessage,
			*Result.VirtualPath);
		for (const FAngelscriptSnippetDiagnostic& Diagnostic : Result.Diagnostics)
		{
			OutputDevice.Logf(
				TEXT("%s(%d:%d): %s"),
				*Diagnostic.Section,
				Diagnostic.UserRow,
				Diagnostic.Column,
				*Diagnostic.Message);
		}
	}

	void ExecuteSnippetFileConsoleCommand(const TArray<FString>& Args, FOutputDevice& OutputDevice)
	{
		if (Args.Num() == 0 || Args[0].TrimStartAndEnd().IsEmpty())
		{
			OutputDevice.Logf(TEXT("as.Snippet.ExecuteFile failed: missing file path."));
			return;
		}

		const FString Filename = ResolveConsoleSnippetPath(Args[0]);
		FString SourceText;
		if (!FFileHelper::LoadFileToString(SourceText, *Filename))
		{
			OutputDevice.Logf(TEXT("as.Snippet.ExecuteFile failed to read: %s"), *Filename);
			return;
		}

		FAngelscriptEngine* Engine = FAngelscriptEngine::TryGetCurrentEngine();
		if (Engine == nullptr && FAngelscriptEngine::IsInitialized())
		{
			Engine = &FAngelscriptEngine::Get();
		}
		if (Engine == nullptr)
		{
			OutputDevice.Logf(TEXT("as.Snippet.ExecuteFile failed: Angelscript engine is not initialized."));
			return;
		}

		FAngelscriptSnippetRequest Request;
		Request.SourceText = MoveTemp(SourceText);
		Request.SourceMode = EAngelscriptSnippetSourceMode::Statements;
		Request.Label = FPaths::GetBaseFilename(Filename);
		const FAngelscriptSnippetResult Result = FAngelscriptSnippetRunner::Execute(*Engine, Request);
		WriteSnippetResultToConsole(Result, OutputDevice);
	}

#if !UE_BUILD_SHIPPING
	FAutoConsoleCommand GExecuteSnippetFileCommand(
		TEXT("as.Snippet.ExecuteFile"),
		TEXT("Execute an Angelscript statement snippet from a file. Usage: as.Snippet.ExecuteFile <path>"),
		FConsoleCommandWithArgsAndOutputDeviceDelegate::CreateStatic(&ExecuteSnippetFileConsoleCommand));
#endif
}

FAngelscriptSnippetResult FAngelscriptSnippetRunner::Execute(FAngelscriptEngine& Engine, const FAngelscriptSnippetRequest& Request)
{
	using namespace AngelscriptSnippet_Private;

	FAngelscriptSnippetResult Result;

#if UE_BUILD_SHIPPING
	Result.ResultCode = EAngelscriptSnippetResultCode::DisabledInShipping;
	Result.ErrorMessage = TEXT("Angelscript snippet execution is disabled in shipping builds.");
	return Result;
#else
	if (Request.SourceText.TrimStartAndEnd().IsEmpty())
	{
		Result.ResultCode = EAngelscriptSnippetResultCode::InvalidRequest;
		Result.ErrorMessage = TEXT("Snippet source text is empty.");
		return Result;
	}

	const FSnippetIdentity Identity = MakeSnippetIdentity(Request.Label);
	Result.VirtualPath = MakeVirtualPath(Identity);
	Result.EntryPointDeclaration = Request.SourceMode == EAngelscriptSnippetSourceMode::Statements
		? Identity.EntryPointDeclaration
		: FString(TEXT("void Main()"));
	FAngelscriptSource Source;
	FString SourceError;
	if (!FAngelscriptSource::TryFromMemorySource(Result.VirtualPath, MakeSourceText(Request, Identity), Source, &SourceError))
	{
		Result.ResultCode = EAngelscriptSnippetResultCode::InvalidRequest;
		Result.ErrorMessage = SourceError;
		return Result;
	}
	Result.ModuleName = Source.ModuleName;
	Engine.Diagnostics.Remove(Result.VirtualPath);
	Engine.Diagnostics.Remove(FString());
	FAngelscriptEngine::FDiagnostics& SnippetDiagnostics = Engine.Diagnostics.FindOrAdd(Result.VirtualPath);
	SnippetDiagnostics.Filename = Result.VirtualPath;
	SnippetDiagnostics.Diagnostics.Reset();
	SnippetDiagnostics.bHasEmittedAny = false;
	SnippetDiagnostics.bIsCompiling = true;

	FAngelscriptPreprocessor Preprocessor;
	Preprocessor.AddSource(Source);
	if (!Preprocessor.Preprocess())
	{
		Result.ResultCode = EAngelscriptSnippetResultCode::PreprocessFailed;
		Result.ErrorMessage = TEXT("Snippet preprocessing failed.");
		CollectDiagnostics(Engine, Result.VirtualPath, Request.SourceMode, Result.Diagnostics);
		DiscardModuleIfRequested(Engine, Request, Result.ModuleName);
		RemoveSnippetDiagnostics(Engine, Result.VirtualPath);
		return Result;
	}

	TArray<TSharedRef<FAngelscriptModuleDesc>> ModulesToCompile = Preprocessor.GetModulesToCompile();
	if (ModulesToCompile.Num() == 0)
	{
		Result.ResultCode = EAngelscriptSnippetResultCode::PreprocessFailed;
		Result.ErrorMessage = TEXT("Snippet preprocessing produced no module.");
		CollectDiagnostics(Engine, Result.VirtualPath, Request.SourceMode, Result.Diagnostics);
		DiscardModuleIfRequested(Engine, Request, Result.ModuleName);
		RemoveSnippetDiagnostics(Engine, Result.VirtualPath);
		return Result;
	}

	TArray<TSharedRef<FAngelscriptModuleDesc>> CompiledModules;
	{
		FAngelscriptEngineScope EngineScope(Engine, Request.WorldContextObject);
		const ECompileResult CompileResult = Engine.CompileModules(ECompileType::SoftReloadOnly, ModulesToCompile, CompiledModules);
		if (CompileResult != ECompileResult::FullyHandled && CompileResult != ECompileResult::PartiallyHandled)
		{
			Result.ResultCode = EAngelscriptSnippetResultCode::CompileFailed;
			Result.ErrorMessage = TEXT("Snippet compilation failed.");
			CollectDiagnostics(Engine, Result.VirtualPath, Request.SourceMode, Result.Diagnostics);
			DiscardModuleIfRequested(Engine, Request, Result.ModuleName);
			RemoveSnippetDiagnostics(Engine, Result.VirtualPath);
			return Result;
		}
	}

	TSharedPtr<FAngelscriptModuleDesc> Module = Engine.GetModule(Result.ModuleName);
	if (!Module.IsValid() || Module->ScriptModule == nullptr)
	{
		Result.ResultCode = EAngelscriptSnippetResultCode::CompileFailed;
		Result.ErrorMessage = TEXT("Snippet module was not published after compilation.");
		CollectDiagnostics(Engine, Result.VirtualPath, Request.SourceMode, Result.Diagnostics);
		DiscardModuleIfRequested(Engine, Request, Result.ModuleName);
		RemoveSnippetDiagnostics(Engine, Result.VirtualPath);
		return Result;
	}

	const FTCHARToUTF8 EntryPointDeclarationUtf8(*Result.EntryPointDeclaration);
	asIScriptFunction* EntryFunction = Module->ScriptModule->GetFunctionByDecl(EntryPointDeclarationUtf8.Get());
	if (EntryFunction == nullptr)
	{
		Result.ResultCode = EAngelscriptSnippetResultCode::EntryPointMissing;
		Result.ErrorMessage = FString::Printf(TEXT("Snippet module does not define %s."), *Result.EntryPointDeclaration);
		DiscardModuleIfRequested(Engine, Request, Result.ModuleName);
		RemoveSnippetDiagnostics(Engine, Result.VirtualPath);
		return Result;
	}

	FAngelscriptContext Context(Request.WorldContextObject, Engine.GetScriptEngine());
	if (!PrepareAngelscriptContextWithLog(Context, EntryFunction, TEXT("FAngelscriptSnippetRunner::Execute")))
	{
		Result.ResultCode = EAngelscriptSnippetResultCode::ExecutionException;
		Result.ErrorMessage = TEXT("Failed to prepare snippet entry point.");
		DiscardModuleIfRequested(Engine, Request, Result.ModuleName);
		RemoveSnippetDiagnostics(Engine, Result.VirtualPath);
		return Result;
	}

	const int32 ExecuteResult = Context->Execute();
	if (ExecuteResult == asEXECUTION_EXCEPTION)
	{
		FillExceptionResult(*Context, Request.SourceMode, Result);
		DiscardModuleIfRequested(Engine, Request, Result.ModuleName);
		RemoveSnippetDiagnostics(Engine, Result.VirtualPath);
		return Result;
	}
	if (ExecuteResult != asEXECUTION_FINISHED)
	{
		Result.ResultCode = EAngelscriptSnippetResultCode::ExecutionException;
		Result.ErrorMessage = FString::Printf(TEXT("Snippet execution ended with result %d."), ExecuteResult);
		DiscardModuleIfRequested(Engine, Request, Result.ModuleName);
		RemoveSnippetDiagnostics(Engine, Result.VirtualPath);
		return Result;
	}

	Result.bSucceeded = true;
	Result.ResultCode = EAngelscriptSnippetResultCode::Succeeded;
	DiscardModuleIfRequested(Engine, Request, Result.ModuleName);
	RemoveSnippetDiagnostics(Engine, Result.VirtualPath);
	return Result;
#endif
}
