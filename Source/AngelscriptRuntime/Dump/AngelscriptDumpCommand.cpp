#include "Dump/AngelscriptDumpCommand.h"

#include "AngelscriptEngine.h"
#include "Dump/AngelscriptStateDump.h"

#include "HAL/IConsoleManager.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogAngelscriptDumpCommand, Log, All);

namespace
{
	FString SanitizeAngelscriptDumpOutputDirArg(FString OutputDir)
	{
		OutputDir.TrimStartAndEndInline();
		while (OutputDir.EndsWith(TEXT(";")))
		{
			OutputDir.LeftChopInline(1, EAllowShrinking::No);
			OutputDir.TrimEndInline();
		}

		if (OutputDir.Len() >= 2 && OutputDir.StartsWith(TEXT("\"")) && OutputDir.EndsWith(TEXT("\"")))
		{
			OutputDir = OutputDir.Mid(1, OutputDir.Len() - 2);
		}

		return OutputDir;
	}

	void ExecuteAngelscriptDumpEngineState(const TArray<FString>& Args)
	{
		if (!FAngelscriptEngine::IsInitialized())
		{
			UE_LOG(LogAngelscriptDumpCommand, Error, TEXT("Cannot dump Angelscript engine state because no global engine is initialized."));
			return;
		}

		if (Args.Num() > 1)
		{
			TArray<FString> ExtraArgs;
			for (int32 ArgIndex = 1; ArgIndex < Args.Num(); ++ArgIndex)
			{
				ExtraArgs.Add(Args[ArgIndex]);
			}

			UE_LOG(LogAngelscriptDumpCommand, Warning, TEXT("Ignoring extra as.DumpEngineState arguments after the output directory: '%s'."), *FString::Join(ExtraArgs, TEXT(" ")));
		}

		const FString RequestedOutputDir = Args.IsEmpty() ? FString() : SanitizeAngelscriptDumpOutputDirArg(Args[0]);
		const FString ActualOutputDir = FAngelscriptStateDump::DumpAll(FAngelscriptEngine::Get(), RequestedOutputDir);
		if (ActualOutputDir.IsEmpty())
		{
			UE_LOG(LogAngelscriptDumpCommand, Error, TEXT("Angelscript engine state dump failed."));
			return;
		}

		UE_LOG(LogAngelscriptDumpCommand, Log, TEXT("Angelscript engine state dumped to '%s'."), *ActualOutputDir);
	}

	FAutoConsoleCommand GAngelscriptDumpEngineStateCommand(
		FAngelscriptDumpCommand::GetCommandName(),
		TEXT("Dump Angelscript engine state to CSV tables, including normalized snapshot tables. Optional: as.DumpEngineState [OutputDir]"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&ExecuteAngelscriptDumpEngineState));
}

const TCHAR* FAngelscriptDumpCommand::GetCommandName()
{
	return TEXT("as.DumpEngineState");
}
