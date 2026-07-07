#include "Dump/AngelscriptStateDump.h"

#include "Dump/AngelscriptCSVWriter.h"
#include "Dump/AngelscriptStateDiff.h"
#include "Dump/AngelscriptStateSnapshot.h"

#include "Core/AngelscriptBinds.h"
#include "Core/AngelscriptDocs.h"
#include "Core/AngelscriptEngine.h"
#include "Core/AngelscriptPerformanceStats.h"
#include "Core/AngelscriptSettings.h"
#include "Core/AngelscriptType.h"

#include "Extension/CodeCoverage/AngelscriptCodeCoverage.h"

#include "Debugging/AngelscriptDebugServer.h"

#include "StaticJIT/AngelscriptStaticJIT.h"
#include "StaticJIT/PrecompiledData.h"

#include "HAL/FileManager.h"
#include "Logging/LogMacros.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "UObject/UnrealType.h"

namespace
{
	FString BoolToString(const bool bValue)
	{
		return bValue ? TEXT("true") : TEXT("false");
	}

	FString JoinStrings(const TArray<FString>& Values)
	{
		return FString::Join(Values, TEXT(";"));
	}

	FString JoinNames(const TSet<FName>& Values)
	{
		TArray<FString> AsStrings;
		AsStrings.Reserve(Values.Num());
		for (const FName Value : Values)
		{
			AsStrings.Add(Value.ToString());
		}
		AsStrings.Sort();
		return JoinStrings(AsStrings);
	}

	FAngelscriptStateDump::FTableResult SaveTable(const FString& OutputDir, const FString& TableName, const FCSVWriter& Writer)
	{
		FAngelscriptStateDump::FTableResult Result;
		Result.TableName = TableName;
		Result.RowCount = Writer.GetRowCount();

		FString ErrorMessage;
		if (!Writer.SaveToFile(FPaths::Combine(OutputDir, TableName), &ErrorMessage))
		{
			Result.Status = TEXT("Error");
			Result.ErrorMessage = MoveTemp(ErrorMessage);
		}

		return Result;
	}

	bool SnapshotRowMatchesCategoryTable(const FAngelscriptStateSnapshotRow& Row, const FString& TableName)
	{
		if (TableName == TEXT("EngineMemberState.csv"))
		{
			return Row.Category == TEXT("EngineMember");
		}
		if (TableName == TEXT("EngineCollections.csv"))
		{
			return Row.Category == TEXT("EngineCollection");
		}
		if (TableName == TEXT("AsEngineInternalState.csv"))
		{
			return Row.Category == TEXT("AsEngineInternal");
		}
		if (TableName == TEXT("AsModuleInternalState.csv"))
		{
			return Row.Category == TEXT("AsModuleInternal");
		}
		if (TableName == TEXT("AsTypeInternalState.csv"))
		{
			return Row.Category == TEXT("AsTypeInternal");
		}
		if (TableName == TEXT("AsFunctionInternalState.csv"))
		{
			return Row.Category == TEXT("AsFunctionInternal");
		}
		return false;
	}

	void AddSnapshotHeader(FCSVWriter& Writer)
	{
		Writer.AddHeader({
			TEXT("Category"),
			TEXT("Identity"),
			TEXT("Field"),
			TEXT("Value"),
			TEXT("ValueKind"),
			TEXT("Source")
		});
	}

	void AddSnapshotRow(FCSVWriter& Writer, const FAngelscriptStateSnapshotRow& Row)
	{
		Writer.AddRow({
			Row.Category,
			Row.Identity,
			Row.Field,
			Row.Value,
			Row.ValueKind,
			Row.Source
		});
	}

	FString GetReplicationConditionString(const ELifetimeCondition Condition)
	{
		const UEnum* ConditionEnum = StaticEnum<ELifetimeCondition>();
		const FString ConditionName = ConditionEnum != nullptr
			? ConditionEnum->GetNameStringByValue(static_cast<int64>(Condition))
			: FString();

		if (ConditionName.IsEmpty())
		{
			return LexToString(static_cast<int32>(Condition));
		}

		return FString::Printf(TEXT("%d (%s)"), static_cast<int32>(Condition), *ConditionName);
	}

	FString GetTypeUsageString(const FAngelscriptTypeUsage& TypeUsage)
	{
		return TypeUsage.IsValid() ? TypeUsage.GetAngelscriptDeclaration() : FString();
	}

	FString FormatArguments(const TArray<FAngelscriptArgumentDesc>& Arguments)
	{
		TArray<FString> ArgumentParts;
		ArgumentParts.Reserve(Arguments.Num());
		for (const FAngelscriptArgumentDesc& Argument : Arguments)
		{
			const FString TypeName = GetTypeUsageString(Argument.Type);
			if (Argument.ArgumentName.IsEmpty())
			{
				ArgumentParts.Add(TypeName);
			}
			else
			{
				ArgumentParts.Add(FString::Printf(TEXT("%s %s"), *TypeName, *Argument.ArgumentName));
			}
		}

		return FString::Join(ArgumentParts, TEXT(", "));
	}

	FString FormatEnumValues(const FAngelscriptEnumDesc& EnumDesc)
	{
		TArray<FString> ValuePairs;
		const int32 ValueCount = FMath::Min(EnumDesc.ValueNames.Num(), EnumDesc.EnumValues.Num());
		ValuePairs.Reserve(ValueCount);
		for (int32 ValueIndex = 0; ValueIndex < ValueCount; ++ValueIndex)
		{
			ValuePairs.Add(FString::Printf(TEXT("%s=%d"), *EnumDesc.ValueNames[ValueIndex].ToString(), EnumDesc.EnumValues[ValueIndex]));
		}

		return FString::Join(ValuePairs, TEXT(";"));
	}

	FString ExportPropertyValue(const FProperty& Property, const void* Container)
	{
		FString Value;
		const void* ValuePtr = Property.ContainerPtrToValuePtr<void>(Container);
		Property.ExportTextItem_Direct(Value, ValuePtr, nullptr, nullptr, PPF_None);
		return Value;
	}

	FString GetFilenamePairPath(const FAngelscriptEngine::FFilenamePair& FilenamePair)
	{
		return !FilenamePair.AbsolutePath.IsEmpty() ? FilenamePair.AbsolutePath : FilenamePair.RelativePath;
	}
}

DEFINE_LOG_CATEGORY_STATIC(LogAngelscriptStateDump, Log, All);

FAngelscriptStateDump::FDumpExtensionsDelegate FAngelscriptStateDump::OnDumpExtensions;

FString FAngelscriptStateDump::DumpAll(FAngelscriptEngine& Engine, const FString& OutputDir)
{
	AS_PERF_SCOPE_DUMP_ALL();

	const FString ResolvedOutputDir = ResolveOutputDir(OutputDir);
	if (!EnsureOutputDir(ResolvedOutputDir))
	{
		UE_LOG(LogAngelscriptStateDump, Error, TEXT("Failed to create dump output directory '%s'."), *ResolvedOutputDir);
		return FString();
	}

	TArray<FTableResult> TableResults;
	TableResults.Reserve(18);
	TableResults.Add(DumpEngineOverview(Engine, ResolvedOutputDir));
	TableResults.Add(DumpRuntimeConfig(Engine, ResolvedOutputDir));
	TableResults.Add(DumpModules(Engine, ResolvedOutputDir));
	TableResults.Add(DumpClasses(Engine, ResolvedOutputDir));
	TableResults.Add(DumpProperties(Engine, ResolvedOutputDir));
	TableResults.Add(DumpFunctions(Engine, ResolvedOutputDir));
	TableResults.Add(DumpEnums(Engine, ResolvedOutputDir));
	TableResults.Add(DumpDelegates(Engine, ResolvedOutputDir));
	TableResults.Add(DumpRegisteredTypes(Engine, ResolvedOutputDir));
	TableResults.Add(DumpDiagnostics(Engine, ResolvedOutputDir));
	TableResults.Add(DumpScriptEngineState(Engine, ResolvedOutputDir));
	TableResults.Add(DumpBindRegistrations(Engine, ResolvedOutputDir));
	TableResults.Add(DumpBindDatabaseStructs(Engine, ResolvedOutputDir));
	TableResults.Add(DumpBindDatabaseClasses(Engine, ResolvedOutputDir));
	TableResults.Add(DumpToStringTypes(Engine, ResolvedOutputDir));
	TableResults.Add(DumpDocumentationStats(Engine, ResolvedOutputDir));
	TableResults.Add(DumpEngineSettings(Engine, ResolvedOutputDir));
	TableResults.Add(DumpHotReloadState(Engine, ResolvedOutputDir));
	TableResults.Add(DumpJITDatabase(Engine, ResolvedOutputDir));
	TableResults.Add(DumpPrecompiledData(Engine, ResolvedOutputDir));
	TableResults.Add(DumpStaticJITState(Engine, ResolvedOutputDir));
	TableResults.Add(DumpDebugServerState(Engine, ResolvedOutputDir));
	TableResults.Add(DumpDebugBreakpoints(Engine, ResolvedOutputDir));
	TableResults.Add(DumpCodeCoverage(Engine, ResolvedOutputDir));
	const FAngelscriptStateSnapshot Snapshot = CaptureSnapshot(Engine);
	TableResults.Add(DumpSnapshot(Snapshot, ResolvedOutputDir));
	TableResults.Append(DumpSnapshotCategoryTables(Snapshot, ResolvedOutputDir));

	const bool bHadExtensionHandlers = OnDumpExtensions.IsBound();
	OnDumpExtensions.Broadcast(ResolvedOutputDir);

	auto AddExtensionTableResult = [&ResolvedOutputDir, &TableResults, bHadExtensionHandlers](const FString& TableName)
	{
		const FString Filename = FPaths::Combine(ResolvedOutputDir, TableName);
		if (!IFileManager::Get().FileExists(*Filename))
		{
			if (bHadExtensionHandlers)
			{
				FTableResult Result;
				Result.TableName = TableName;
				Result.Status = TEXT("Error");
				Result.ErrorMessage = TEXT("Expected editor dump extension table was not generated.");
				TableResults.Add(Result);
			}
			return;
		}

		FString FileContents;
		int32 RowCount = 0;
		if (FFileHelper::LoadFileToString(FileContents, *Filename))
		{
			TArray<FString> Lines;
			FileContents.ParseIntoArrayLines(Lines, true);
			RowCount = FMath::Max(0, Lines.Num() - 1);
		}

		FTableResult Result;
		Result.TableName = TableName;
		Result.RowCount = RowCount;
		Result.Status = TEXT("Success");
		TableResults.Add(Result);
	};

	AddExtensionTableResult(TEXT("EditorReloadState.csv"));
	AddExtensionTableResult(TEXT("EditorMenuExtensions.csv"));

	const FTableResult SummaryResult = DumpSummary(TableResults, ResolvedOutputDir);

	int32 TotalRows = SummaryResult.RowCount;
	int32 SuccessCount = SummaryResult.Status == TEXT("Success") ? 1 : 0;
	int32 WrittenFileCount = SummaryResult.Status != TEXT("Error") ? 1 : 0;
	for (const FTableResult& TableResult : TableResults)
	{
		TotalRows += TableResult.RowCount;
		if (TableResult.Status == TEXT("Success"))
		{
			++SuccessCount;
		}
		if (TableResult.Status != TEXT("Error"))
		{
			++WrittenFileCount;
		}
	}

	UE_LOG(
		LogAngelscriptStateDump,
		Log,
		TEXT("Angelscript state dump wrote %d/%d CSV files with %d/%d Success statuses (%d total rows) to '%s'."),
		WrittenFileCount,
		TableResults.Num() + 1,
		SuccessCount,
		TableResults.Num() + 1,
		TotalRows,
		*ResolvedOutputDir);

	return ResolvedOutputDir;
}

FAngelscriptStateSnapshot FAngelscriptStateDump::CaptureSnapshot(FAngelscriptEngine& Engine)
{
	return FAngelscriptStateSnapshotBuilder::Capture(Engine);
}

FAngelscriptStateDiff FAngelscriptStateDump::DiffSnapshots(const FAngelscriptStateSnapshot& Before, const FAngelscriptStateSnapshot& After)
{
	return FAngelscriptStateDiffBuilder::Diff(Before, After);
}

FAngelscriptStateDump::FTableResult FAngelscriptStateDump::DumpSnapshot(const FAngelscriptStateSnapshot& Snapshot, const FString& OutputDir)
{
	FCSVWriter Writer;
	AddSnapshotHeader(Writer);
	for (const FAngelscriptStateSnapshotRow& Row : Snapshot.Rows)
	{
		AddSnapshotRow(Writer, Row);
	}

	return SaveTable(OutputDir, TEXT("EngineStateSnapshot.csv"), Writer);
}

TArray<FAngelscriptStateDump::FTableResult> FAngelscriptStateDump::DumpSnapshotCategoryTables(const FAngelscriptStateSnapshot& Snapshot, const FString& OutputDir)
{
	static const TArray<FString> TableNames = {
		TEXT("EngineMemberState.csv"),
		TEXT("EngineCollections.csv"),
		TEXT("AsEngineInternalState.csv"),
		TEXT("AsModuleInternalState.csv"),
		TEXT("AsTypeInternalState.csv"),
		TEXT("AsFunctionInternalState.csv")
	};

	TArray<FTableResult> Results;
	Results.Reserve(TableNames.Num());
	for (const FString& TableName : TableNames)
	{
		FCSVWriter Writer;
		AddSnapshotHeader(Writer);
		for (const FAngelscriptStateSnapshotRow& Row : Snapshot.Rows)
		{
			if (SnapshotRowMatchesCategoryTable(Row, TableName))
			{
				AddSnapshotRow(Writer, Row);
			}
		}

		Results.Add(SaveTable(OutputDir, TableName, Writer));
	}

	return Results;
}

TArray<FAngelscriptStateDump::FTableResult> FAngelscriptStateDump::DumpDiff(const FAngelscriptStateDiff& Diff, const FString& OutputDir)
{
	FCSVWriter DiffWriter;
	DiffWriter.AddHeader({
		TEXT("ChangeType"),
		TEXT("Category"),
		TEXT("Identity"),
		TEXT("Field"),
		TEXT("BeforeValue"),
		TEXT("AfterValue"),
		TEXT("Source")
	});

	TMap<FString, int32> SummaryCounts;
	for (const FAngelscriptStateDiffRow& Row : Diff.Rows)
	{
		const FString ChangeType = FAngelscriptStateDiffBuilder::ChangeTypeToString(Row.ChangeType);
		DiffWriter.AddRow({
			ChangeType,
			Row.Category,
			Row.Identity,
			Row.Field,
			Row.BeforeValue,
			Row.AfterValue,
			Row.Source
		});

		++SummaryCounts.FindOrAdd(FString::Printf(TEXT("%s|%s"), *Row.Category, *ChangeType));
	}

	TArray<FTableResult> Results;
	Results.Add(SaveTable(OutputDir, TEXT("StateDiff.csv"), DiffWriter));

	FCSVWriter SummaryWriter;
	SummaryWriter.AddHeader({
		TEXT("Category"),
		TEXT("ChangeType"),
		TEXT("Count")
	});

	TArray<FString> SummaryKeys;
	SummaryCounts.GetKeys(SummaryKeys);
	SummaryKeys.Sort();
	for (const FString& SummaryKey : SummaryKeys)
	{
		FString Category;
		FString ChangeType;
		SummaryKey.Split(TEXT("|"), &Category, &ChangeType);
		SummaryWriter.AddRow({
			Category,
			ChangeType,
			LexToString(SummaryCounts[SummaryKey])
		});
	}

	Results.Add(SaveTable(OutputDir, TEXT("StateDiffSummary.csv"), SummaryWriter));
	return Results;
}

FString FAngelscriptStateDump::ResolveOutputDir(const FString& OutputDir)
{
	if (!OutputDir.IsEmpty())
	{
		return OutputDir;
	}

	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Angelscript"), TEXT("Dump"), MakeTimestampDirectoryName());
}

bool FAngelscriptStateDump::EnsureOutputDir(const FString& OutputDir)
{
	return IFileManager::Get().DirectoryExists(*OutputDir)
		|| IFileManager::Get().MakeDirectory(*OutputDir, true);
}

FString FAngelscriptStateDump::MakeTimestampDirectoryName()
{
	return FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
}

FAngelscriptStateDump::FTableResult FAngelscriptStateDump::DumpEngineOverview(FAngelscriptEngine& Engine, const FString& OutputDir)
{
	const TArray<TSharedRef<FAngelscriptModuleDesc>> ActiveModules = Engine.GetActiveModules();

	int32 TotalClassCount = 0;
	int32 TotalEnumCount = 0;
	int32 TotalDelegateCount = 0;
	for (const TSharedRef<FAngelscriptModuleDesc>& Module : ActiveModules)
	{
		TotalClassCount += Module->Classes.Num();
		TotalEnumCount += Module->Enums.Num();
		TotalDelegateCount += Module->Delegates.Num();
	}

	FString DebugServerClientState = TEXT("NotCompiled");
#if WITH_AS_DEBUGSERVER
	if (Engine.DebugServer != nullptr)
	{
		DebugServerClientState = BoolToString(Engine.DebugServer->HasAnyClients());
	}
	else
	{
		DebugServerClientState = TEXT("false");
	}
#endif

	FCSVWriter Writer;
	Writer.AddHeader({
		TEXT("bIsInitialCompileFinished"),
		TEXT("bDidInitialCompileSucceed"),
		TEXT("bSimulateCooked"),
		TEXT("bUseEditorScripts"),
		TEXT("bTestErrors"),
		TEXT("bIsHotReloading"),
		TEXT("bScriptDevelopmentMode"),
		TEXT("bGeneratePrecompiledData"),
		TEXT("bUsePrecompiledData"),
		TEXT("bCompletedAssetScan"),
		TEXT("ActiveModuleCount"),
		TEXT("TotalClassCount"),
		TEXT("TotalEnumCount"),
		TEXT("TotalDelegateCount"),
		TEXT("RegisteredTypeCount"),
		TEXT("BindRegistrationCount"),
		TEXT("JITFunctionCount"),
		TEXT("DebugServerClients"),
		TEXT("ScriptRootPaths"),
		TEXT("DiagnosticsCount"),
		TEXT("ContextPoolSize"),
		TEXT("DumpTimestamp")
	});

	Writer.AddRow({
		BoolToString(Engine.bIsInitialCompileFinished),
		BoolToString(Engine.bDidInitialCompileSucceed),
		BoolToString(Engine.bSimulateCooked),
		BoolToString(Engine.bUseEditorScripts),
		BoolToString(Engine.bTestErrors),
		BoolToString(Engine.bIsHotReloading),
		BoolToString(Engine.bScriptDevelopmentMode),
		BoolToString(Engine.bGeneratePrecompiledData),
		BoolToString(Engine.bUsePrecompiledData),
		BoolToString(Engine.bCompletedAssetScan),
		LexToString(ActiveModules.Num()),
		LexToString(TotalClassCount),
		LexToString(TotalEnumCount),
		LexToString(TotalDelegateCount),
		LexToString(FAngelscriptType::GetTypes().Num()),
		LexToString(FAngelscriptBinds::GetAllRegisteredBindNames().Num()),
		LexToString(FJITDatabase::Get().Functions.Num()),
		MoveTemp(DebugServerClientState),
		JoinStrings(Engine.AllRootPaths),
		LexToString(Engine.Diagnostics.Num()),
		TEXT(""),
		FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S"))
	});

	return SaveTable(OutputDir, TEXT("EngineOverview.csv"), Writer);
}

FAngelscriptStateDump::FTableResult FAngelscriptStateDump::DumpRuntimeConfig(FAngelscriptEngine& Engine, const FString& OutputDir)
{
	const FAngelscriptEngineConfig& Config = Engine.GetRuntimeConfig();

	FCSVWriter Writer;
	Writer.AddHeader({
		TEXT("Key"),
		TEXT("Value")
	});

	auto AddConfigValue = [&Writer](const FString& Key, const FString& Value)
	{
		Writer.AddRow({ Key, Value });
	};

	AddConfigValue(TEXT("bForceThreadedInitialize"), BoolToString(Config.bForceThreadedInitialize));
	AddConfigValue(TEXT("bSkipThreadedInitialize"), BoolToString(Config.bSkipThreadedInitialize));
	AddConfigValue(TEXT("bSkipInitialCompile"), BoolToString(Config.bSkipInitialCompile));
	AddConfigValue(TEXT("bSimulateCooked"), BoolToString(Config.bSimulateCooked));
	AddConfigValue(TEXT("bTestErrors"), BoolToString(Config.bTestErrors));
	AddConfigValue(TEXT("bForcePreprocessEditorCode"), BoolToString(Config.bForcePreprocessEditorCode));
	AddConfigValue(TEXT("bGeneratePrecompiledData"), BoolToString(Config.bGeneratePrecompiledData));
	AddConfigValue(TEXT("bDevelopmentMode"), BoolToString(Config.bDevelopmentMode));
	AddConfigValue(TEXT("bIgnorePrecompiledData"), BoolToString(Config.bIgnorePrecompiledData));
	AddConfigValue(TEXT("bSkipStaticJITCodeGen"), BoolToString(Config.bSkipStaticJITCodeGen));
	AddConfigValue(TEXT("bSkipWriteBindDB"), BoolToString(Config.bSkipWriteBindDB));
	AddConfigValue(TEXT("bWriteBindDB"), BoolToString(Config.bWriteBindDB));
	AddConfigValue(TEXT("bExitOnError"), BoolToString(Config.bExitOnError));
	AddConfigValue(TEXT("bDumpDocumentation"), BoolToString(Config.bDumpDocumentation));
	AddConfigValue(TEXT("DebugServerPort"), LexToString(Config.DebugServerPort));
	AddConfigValue(TEXT("bIsEditor"), BoolToString(Config.bIsEditor));
	AddConfigValue(TEXT("bRunningCommandlet"), BoolToString(Config.bRunningCommandlet));
	AddConfigValue(TEXT("DisabledBindNames"), JoinNames(Config.DisabledBindNames));

	return SaveTable(OutputDir, TEXT("RuntimeConfig.csv"), Writer);
}

FAngelscriptStateDump::FTableResult FAngelscriptStateDump::DumpModules(FAngelscriptEngine& Engine, const FString& OutputDir)
{
	FCSVWriter Writer;
	Writer.AddHeader({
		TEXT("ModuleName"),
		TEXT("CodeSectionCount"),
		TEXT("CodeHash"),
		TEXT("CombinedDependencyHash"),
		TEXT("ClassCount"),
		TEXT("EnumCount"),
		TEXT("DelegateCount"),
		TEXT("ImportedModules"),
		TEXT("UnitTestCount"),
		TEXT("IntegrationTestCount"),
		TEXT("bCompileError"),
		TEXT("bLoadedPrecompiledCode")
	});

	const TArray<TSharedRef<FAngelscriptModuleDesc>> ActiveModules = Engine.GetActiveModules();
	for (const TSharedRef<FAngelscriptModuleDesc>& Module : ActiveModules)
	{
		Writer.AddRow({
			Module->ModuleName,
			LexToString(Module->Code.Num()),
			LexToString(Module->CodeHash),
			LexToString(Module->CombinedDependencyHash),
			LexToString(Module->Classes.Num()),
			LexToString(Module->Enums.Num()),
			LexToString(Module->Delegates.Num()),
			JoinStrings(Module->ImportedModules),
			LexToString(Module->UnitTestFunctions.Num()),
			LexToString(Module->IntegrationTestFunctions.Num()),
			BoolToString(Module->bCompileError),
			BoolToString(Module->bLoadedPrecompiledCode)
		});
	}

	return SaveTable(OutputDir, TEXT("Modules.csv"), Writer);
}

FAngelscriptStateDump::FTableResult FAngelscriptStateDump::DumpClasses(FAngelscriptEngine& Engine, const FString& OutputDir)
{
	FCSVWriter Writer;
	Writer.AddHeader({
		TEXT("ModuleName"),
		TEXT("ClassName"),
		TEXT("SuperClass"),
		TEXT("bIsStruct"),
		TEXT("bAbstract"),
		TEXT("bTransient"),
		TEXT("bPlaceable"),
		TEXT("bIsStaticsClass"),
		TEXT("bHideDropdown"),
		TEXT("bDefaultToInstanced"),
		TEXT("bEditInlineNew"),
		TEXT("bIsDeprecatedClass"),
		TEXT("ImplementedInterfaces"),
		TEXT("PropertyCount"),
		TEXT("MethodCount"),
		TEXT("LineNumber"),
		TEXT("Namespace"),
		TEXT("ConfigName"),
		TEXT("CodeSuperClass")
	});

	const TArray<TSharedRef<FAngelscriptModuleDesc>> ActiveModules = Engine.GetActiveModules();
	for (const TSharedRef<FAngelscriptModuleDesc>& Module : ActiveModules)
	{
		for (const TSharedRef<FAngelscriptClassDesc>& ClassDesc : Module->Classes)
		{
			Writer.AddRow({
				Module->ModuleName,
				ClassDesc->ClassName,
				ClassDesc->SuperClass,
				BoolToString(ClassDesc->bIsStruct),
				BoolToString(ClassDesc->bAbstract),
				BoolToString(ClassDesc->bTransient),
				BoolToString(ClassDesc->bPlaceable),
				BoolToString(ClassDesc->bIsStaticsClass),
				BoolToString(ClassDesc->bHideDropdown),
				BoolToString(ClassDesc->bDefaultToInstanced),
				BoolToString(ClassDesc->bEditInlineNew),
				BoolToString(ClassDesc->bIsDeprecatedClass),
				JoinStrings(ClassDesc->ImplementedInterfaces),
				LexToString(ClassDesc->Properties.Num()),
				LexToString(ClassDesc->Methods.Num()),
				LexToString(ClassDesc->LineNumber),
				ClassDesc->Namespace.IsSet() ? ClassDesc->Namespace.GetValue() : FString(),
				ClassDesc->ConfigName,
				ClassDesc->CodeSuperClass != nullptr ? ClassDesc->CodeSuperClass->GetFName().ToString() : FString()
			});
		}
	}

	return SaveTable(OutputDir, TEXT("Classes.csv"), Writer);
}

FAngelscriptStateDump::FTableResult FAngelscriptStateDump::DumpProperties(FAngelscriptEngine& Engine, const FString& OutputDir)
{
	FCSVWriter Writer;
	Writer.AddHeader({
		TEXT("ModuleName"),
		TEXT("ClassName"),
		TEXT("PropertyName"),
		TEXT("LiteralType"),
		TEXT("bBlueprintReadable"),
		TEXT("bBlueprintWritable"),
		TEXT("bEditableOnDefaults"),
		TEXT("bEditableOnInstance"),
		TEXT("bEditConst"),
		TEXT("bTransient"),
		TEXT("bReplicated"),
		TEXT("ReplicationCondition"),
		TEXT("bRepNotify"),
		TEXT("bConfig"),
		TEXT("bInterp"),
		TEXT("bSaveGame"),
		TEXT("bIsPrivate"),
		TEXT("bIsProtected"),
		TEXT("LineNumber")
	});

	const TArray<TSharedRef<FAngelscriptModuleDesc>> ActiveModules = Engine.GetActiveModules();
	for (const TSharedRef<FAngelscriptModuleDesc>& Module : ActiveModules)
	{
		for (const TSharedRef<FAngelscriptClassDesc>& ClassDesc : Module->Classes)
		{
			for (const TSharedRef<FAngelscriptPropertyDesc>& PropertyDesc : ClassDesc->Properties)
			{
				Writer.AddRow({
					Module->ModuleName,
					ClassDesc->ClassName,
					PropertyDesc->PropertyName,
					PropertyDesc->LiteralType,
					BoolToString(PropertyDesc->bBlueprintReadable),
					BoolToString(PropertyDesc->bBlueprintWritable),
					BoolToString(PropertyDesc->bEditableOnDefaults),
					BoolToString(PropertyDesc->bEditableOnInstance),
					BoolToString(PropertyDesc->bEditConst),
					BoolToString(PropertyDesc->bTransient),
					BoolToString(PropertyDesc->bReplicated),
					GetReplicationConditionString(PropertyDesc->ReplicationCondition),
					BoolToString(PropertyDesc->bRepNotify),
					BoolToString(PropertyDesc->bConfig),
					BoolToString(PropertyDesc->bInterp),
					BoolToString(PropertyDesc->bSaveGame),
					BoolToString(PropertyDesc->bIsPrivate),
					BoolToString(PropertyDesc->bIsProtected),
					LexToString(PropertyDesc->LineNumber)
				});
			}
		}
	}

	return SaveTable(OutputDir, TEXT("Properties.csv"), Writer);
}

FAngelscriptStateDump::FTableResult FAngelscriptStateDump::DumpFunctions(FAngelscriptEngine& Engine, const FString& OutputDir)
{
	FCSVWriter Writer;
	Writer.AddHeader({
		TEXT("ModuleName"),
		TEXT("ClassName"),
		TEXT("FunctionName"),
		TEXT("ScriptFunctionName"),
		TEXT("ReturnType"),
		TEXT("ArgumentCount"),
		TEXT("Arguments"),
		TEXT("bBlueprintCallable"),
		TEXT("bBlueprintEvent"),
		TEXT("bBlueprintPure"),
		TEXT("bNetMulticast"),
		TEXT("bNetClient"),
		TEXT("bNetServer"),
		TEXT("bUnreliable"),
		TEXT("bExec"),
		TEXT("bIsStatic"),
		TEXT("bIsConstMethod"),
		TEXT("bThreadSafe"),
		TEXT("bIsNoOp"),
		TEXT("bIsPrivate"),
		TEXT("bIsProtected"),
		TEXT("LineNumber")
	});

	const TArray<TSharedRef<FAngelscriptModuleDesc>> ActiveModules = Engine.GetActiveModules();
	for (const TSharedRef<FAngelscriptModuleDesc>& Module : ActiveModules)
	{
		for (const TSharedRef<FAngelscriptClassDesc>& ClassDesc : Module->Classes)
		{
			for (const TSharedRef<FAngelscriptFunctionDesc>& FunctionDesc : ClassDesc->Methods)
			{
				Writer.AddRow({
					Module->ModuleName,
					ClassDesc->ClassName,
					FunctionDesc->FunctionName,
					FunctionDesc->ScriptFunctionName,
					GetTypeUsageString(FunctionDesc->ReturnType),
					LexToString(FunctionDesc->Arguments.Num()),
					FormatArguments(FunctionDesc->Arguments),
					BoolToString(FunctionDesc->bBlueprintCallable),
					BoolToString(FunctionDesc->bBlueprintEvent),
					BoolToString(FunctionDesc->bBlueprintPure),
					BoolToString(FunctionDesc->bNetMulticast),
					BoolToString(FunctionDesc->bNetClient),
					BoolToString(FunctionDesc->bNetServer),
					BoolToString(FunctionDesc->bUnreliable),
					BoolToString(FunctionDesc->bExec),
					BoolToString(FunctionDesc->bIsStatic),
					BoolToString(FunctionDesc->bIsConstMethod),
					BoolToString(FunctionDesc->bThreadSafe),
					BoolToString(FunctionDesc->bIsNoOp),
					BoolToString(FunctionDesc->bIsPrivate),
					BoolToString(FunctionDesc->bIsProtected),
					LexToString(FunctionDesc->LineNumber)
				});
			}
		}
	}

	return SaveTable(OutputDir, TEXT("Functions.csv"), Writer);
}

FAngelscriptStateDump::FTableResult FAngelscriptStateDump::DumpEnums(FAngelscriptEngine& Engine, const FString& OutputDir)
{
	FCSVWriter Writer;
	Writer.AddHeader({
		TEXT("ModuleName"),
		TEXT("EnumName"),
		TEXT("ValueCount"),
		TEXT("Values"),
		TEXT("LineNumber")
	});

	const TArray<TSharedRef<FAngelscriptModuleDesc>> ActiveModules = Engine.GetActiveModules();
	for (const TSharedRef<FAngelscriptModuleDesc>& Module : ActiveModules)
	{
		for (const TSharedRef<FAngelscriptEnumDesc>& EnumDesc : Module->Enums)
		{
			Writer.AddRow({
				Module->ModuleName,
				EnumDesc->EnumName,
				LexToString(EnumDesc->ValueNames.Num()),
				FormatEnumValues(*EnumDesc),
				LexToString(EnumDesc->LineNumber)
			});
		}
	}

	return SaveTable(OutputDir, TEXT("Enums.csv"), Writer);
}

FAngelscriptStateDump::FTableResult FAngelscriptStateDump::DumpDelegates(FAngelscriptEngine& Engine, const FString& OutputDir)
{
	FCSVWriter Writer;
	Writer.AddHeader({
		TEXT("ModuleName"),
		TEXT("DelegateName"),
		TEXT("bIsMulticast"),
		TEXT("SignatureReturnType"),
		TEXT("SignatureArguments"),
		TEXT("LineNumber")
	});

	const TArray<TSharedRef<FAngelscriptModuleDesc>> ActiveModules = Engine.GetActiveModules();
	for (const TSharedRef<FAngelscriptModuleDesc>& Module : ActiveModules)
	{
		for (const TSharedRef<FAngelscriptDelegateDesc>& DelegateDesc : Module->Delegates)
		{
			const FAngelscriptFunctionDesc* SignatureDesc = DelegateDesc->Signature.Get();
			Writer.AddRow({
				Module->ModuleName,
				DelegateDesc->DelegateName,
				BoolToString(DelegateDesc->bIsMulticast),
				SignatureDesc != nullptr ? GetTypeUsageString(SignatureDesc->ReturnType) : FString(),
				SignatureDesc != nullptr ? FormatArguments(SignatureDesc->Arguments) : FString(),
				LexToString(DelegateDesc->LineNumber)
			});
		}
	}

	return SaveTable(OutputDir, TEXT("Delegates.csv"), Writer);
}

FAngelscriptStateDump::FTableResult FAngelscriptStateDump::DumpRegisteredTypes(FAngelscriptEngine& Engine, const FString& OutputDir)
{
	FCSVWriter Writer;
	Writer.AddHeader({
		TEXT("AngelscriptTypeName"),
		TEXT("AngelscriptDeclaration"),
		TEXT("HasUClass")
	});

	const TArray<TSharedRef<FAngelscriptType>>& Types = FAngelscriptType::GetTypes();
	for (const TSharedRef<FAngelscriptType>& Type : Types)
	{
		FAngelscriptTypeUsage Usage;
		Usage.Type = Type;
		Writer.AddRow({
			Type->GetAngelscriptTypeName(),
			Type->GetAngelscriptTypeName(),
			BoolToString(Usage.GetClass() != nullptr)
		});
	}

	return SaveTable(OutputDir, TEXT("RegisteredTypes.csv"), Writer);
}

FAngelscriptStateDump::FTableResult FAngelscriptStateDump::DumpDiagnostics(FAngelscriptEngine& Engine, const FString& OutputDir)
{
	FCSVWriter Writer;
	Writer.AddHeader({
		TEXT("Filename"),
		TEXT("Row"),
		TEXT("Column"),
		TEXT("bIsError"),
		TEXT("bIsInfo"),
		TEXT("Message")
	});

	for (const TPair<FString, FAngelscriptEngine::FDiagnostics>& DiagnosticsPair : Engine.Diagnostics)
	{
		for (const FAngelscriptEngine::FDiagnostic& Diagnostic : DiagnosticsPair.Value.Diagnostics)
		{
			Writer.AddRow({
				DiagnosticsPair.Key,
				LexToString(Diagnostic.Row),
				LexToString(Diagnostic.Column),
				BoolToString(Diagnostic.bIsError),
				BoolToString(Diagnostic.bIsInfo),
				Diagnostic.Message
			});
		}
	}

	return SaveTable(OutputDir, TEXT("Diagnostics.csv"), Writer);
}

FAngelscriptStateDump::FTableResult FAngelscriptStateDump::DumpScriptEngineState(FAngelscriptEngine& Engine, const FString& OutputDir)
{
	FCSVWriter Writer;
	Writer.AddHeader({
		TEXT("Key"),
		TEXT("Value")
	});

	asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
	if (ScriptEngine == nullptr)
	{
		FTableResult Result;
		Result.TableName = TEXT("ScriptEngineState.csv");
		Result.Status = TEXT("Error");
		Result.ErrorMessage = TEXT("Engine has no active asIScriptEngine.");
		return Result;
	}

	auto AddStat = [&Writer](const FString& Key, const FString& Value)
	{
		Writer.AddRow({ Key, Value });
	};

	asUINT ModuleFunctionCount = 0;
	asUINT ModuleGlobalVarCount = 0;
	const asUINT ModuleCount = ScriptEngine->GetModuleCount();
	for (asUINT ModuleIndex = 0; ModuleIndex < ModuleCount; ++ModuleIndex)
	{
		if (asIScriptModule* ScriptModule = ScriptEngine->GetModuleByIndex(ModuleIndex))
		{
			ModuleFunctionCount += ScriptModule->GetFunctionCount();
			ModuleGlobalVarCount += ScriptModule->GetGlobalVarCount();
		}
	}

	asUINT GCCurrentSize = 0;
	asUINT GCTotalDestroyed = 0;
	asUINT GCTotalDetected = 0;
	asUINT GCNewObjects = 0;
	asUINT GCTotalNewDestroyed = 0;
	ScriptEngine->GetGCStatistics(&GCCurrentSize, &GCTotalDestroyed, &GCTotalDetected, &GCNewObjects, &GCTotalNewDestroyed);

	AddStat(TEXT("ModuleCount"), LexToString(ModuleCount));
	AddStat(TEXT("GlobalFunctionCount"), LexToString(ScriptEngine->GetGlobalFunctionCount()));
	AddStat(TEXT("GlobalPropertyCount"), LexToString(ScriptEngine->GetGlobalPropertyCount()));
	AddStat(TEXT("ObjectTypeCount"), LexToString(ScriptEngine->GetObjectTypeCount()));
	AddStat(TEXT("EnumCount"), LexToString(ScriptEngine->GetEnumCount()));
	AddStat(TEXT("FuncdefCount"), LexToString(ScriptEngine->GetFuncdefCount()));
	AddStat(TEXT("TypedefCount"), LexToString(ScriptEngine->GetTypedefCount()));
	AddStat(TEXT("ModuleFunctionCount"), LexToString(ModuleFunctionCount));
	AddStat(TEXT("ModuleGlobalVarCount"), LexToString(ModuleGlobalVarCount));
	AddStat(TEXT("GCCurrentSize"), LexToString(GCCurrentSize));
	AddStat(TEXT("GCTotalDestroyed"), LexToString(GCTotalDestroyed));
	AddStat(TEXT("GCTotalDetected"), LexToString(GCTotalDetected));
	AddStat(TEXT("GCNewObjects"), LexToString(GCNewObjects));
	AddStat(TEXT("GCTotalNewDestroyed"), LexToString(GCTotalNewDestroyed));

	return SaveTable(OutputDir, TEXT("ScriptEngineState.csv"), Writer);
}

FAngelscriptStateDump::FTableResult FAngelscriptStateDump::DumpSummary(const TArray<FTableResult>& TableResults, const FString& OutputDir)
{
	FCSVWriter Writer;
	Writer.AddHeader({
		TEXT("Table"),
		TEXT("RowCount"),
		TEXT("Status"),
		TEXT("ErrorMessage")
	});

	for (const FTableResult& TableResult : TableResults)
	{
		Writer.AddRow({
			TableResult.TableName,
			LexToString(TableResult.RowCount),
			TableResult.Status,
			TableResult.ErrorMessage
		});
	}

	FTableResult SummaryRow;
	SummaryRow.TableName = TEXT("DumpSummary.csv");
	SummaryRow.RowCount = TableResults.Num() + 1;
	SummaryRow.Status = TEXT("Success");
	Writer.AddRow({
		SummaryRow.TableName,
		LexToString(SummaryRow.RowCount),
		SummaryRow.Status,
		SummaryRow.ErrorMessage
	});

	FTableResult SaveResult = SaveTable(OutputDir, TEXT("DumpSummary.csv"), Writer);
	if (SaveResult.Status == TEXT("Success"))
	{
		SaveResult.RowCount = SummaryRow.RowCount;
	}
	return SaveResult;
}

FAngelscriptStateDump::FTableResult FAngelscriptStateDump::DumpBindRegistrations(FAngelscriptEngine& Engine, const FString& OutputDir)
{
	const TSet<FName>& DisabledBindNames = Engine.GetRuntimeConfig().DisabledBindNames;
	const TArray<FAngelscriptBinds::FBindInfo> BindInfos = FAngelscriptBinds::GetBindInfoList(DisabledBindNames);

	FCSVWriter Writer;
	Writer.AddHeader({
		TEXT("BindName"),
		TEXT("BindModule"),
		TEXT("bIsSkipped"),
		TEXT("SkipReason")
	});

	for (const FAngelscriptBinds::FBindInfo& BindInfo : BindInfos)
	{
		const bool bIsSkipped = !BindInfo.bEnabled;
		Writer.AddRow({
			BindInfo.BindName.ToString(),
			FString(),
			BoolToString(bIsSkipped),
			bIsSkipped && DisabledBindNames.Contains(BindInfo.BindName) ? FString(TEXT("DisabledBindNames")) : FString()
		});
	}

	return SaveTable(OutputDir, TEXT("BindRegistrations.csv"), Writer);
}

FAngelscriptStateDump::FTableResult FAngelscriptStateDump::DumpBindDatabaseStructs(FAngelscriptEngine& Engine, const FString& OutputDir)
{
	const FAngelscriptBindDatabase& BindDatabase = FAngelscriptBindDatabase::Get();

	FCSVWriter Writer;
	Writer.AddHeader({
		TEXT("StructName"),
		TEXT("BindName"),
		TEXT("PropertyCount"),
		TEXT("MethodCount")
	});

	for (const FAngelscriptStructBind& StructBind : BindDatabase.Structs)
	{
		Writer.AddRow({
			StructBind.TypeName,
			StructBind.TypeName,
			LexToString(StructBind.Properties.Num()),
			TEXT("0")
		});
	}

	return SaveTable(OutputDir, TEXT("BindDatabase_Structs.csv"), Writer);
}

FAngelscriptStateDump::FTableResult FAngelscriptStateDump::DumpBindDatabaseClasses(FAngelscriptEngine& Engine, const FString& OutputDir)
{
	const FAngelscriptBindDatabase& BindDatabase = FAngelscriptBindDatabase::Get();

	FCSVWriter Writer;
	Writer.AddHeader({
		TEXT("ClassName"),
		TEXT("BindName"),
		TEXT("FunctionCount")
	});

	for (const FAngelscriptClassBind& ClassBind : BindDatabase.Classes)
	{
		Writer.AddRow({
			ClassBind.TypeName,
			ClassBind.TypeName,
			LexToString(ClassBind.Methods.Num())
		});
	}

	return SaveTable(OutputDir, TEXT("BindDatabase_Classes.csv"), Writer);
}

FAngelscriptStateDump::FTableResult FAngelscriptStateDump::DumpToStringTypes(FAngelscriptEngine& Engine, const FString& OutputDir)
{
	FCSVWriter Writer;
	Writer.AddHeader({ TEXT("TypeName") });

	FTableResult Result = SaveTable(OutputDir, TEXT("ToStringTypes.csv"), Writer);
	if (Result.Status == TEXT("Success"))
	{
		Result.Status = TEXT("NotAvailable");
		Result.ErrorMessage = TEXT("No public enumeration API is available for FToStringHelper registrations.");
	}
	return Result;
}

FAngelscriptStateDump::FTableResult FAngelscriptStateDump::DumpDocumentationStats(FAngelscriptEngine& Engine, const FString& OutputDir)
{
	FCSVWriter Writer;
	Writer.AddHeader({
		TEXT("Category"),
		TEXT("EntryCount")
	});

	Writer.AddRow({ TEXT("UnrealDocumentation"), LexToString(FAngelscriptDocs::GetUnrealDocumentationCount()) });
	Writer.AddRow({ TEXT("UnrealTypeDocumentation"), LexToString(FAngelscriptDocs::GetUnrealTypeDocumentationCount()) });
	Writer.AddRow({ TEXT("GlobalVariableDocumentation"), LexToString(FAngelscriptDocs::GetGlobalVariableDocumentationCount()) });
	Writer.AddRow({ TEXT("UnrealPropertyDocumentation"), LexToString(FAngelscriptDocs::GetUnrealPropertyDocumentationCount()) });

	return SaveTable(OutputDir, TEXT("DocumentationStats.csv"), Writer);
}

FAngelscriptStateDump::FTableResult FAngelscriptStateDump::DumpEngineSettings(FAngelscriptEngine& Engine, const FString& OutputDir)
{
	UAngelscriptSettings& Settings = UAngelscriptSettings::Get();

	FCSVWriter Writer;
	Writer.AddHeader({
		TEXT("Key"),
		TEXT("Value"),
		TEXT("Category")
	});

	for (TFieldIterator<FProperty> PropertyIt(UAngelscriptSettings::StaticClass()); PropertyIt; ++PropertyIt)
	{
		FProperty* Property = *PropertyIt;
		if (Property == nullptr)
		{
			continue;
		}

		// Property metadata is editor-only; cooked builds report an empty category.
#if WITH_EDITORONLY_DATA
		const FString PropertyCategory = Property->GetMetaData(TEXT("Category"));
#else
		const FString PropertyCategory;
#endif
		Writer.AddRow({
			Property->GetName(),
			ExportPropertyValue(*Property, &Settings),
			PropertyCategory
		});
	}

	return SaveTable(OutputDir, TEXT("EngineSettings.csv"), Writer);
}

FAngelscriptStateDump::FTableResult FAngelscriptStateDump::DumpHotReloadState(FAngelscriptEngine& Engine, const FString& OutputDir)
{
	FCSVWriter Writer;
	Writer.AddHeader({
		TEXT("FilePath"),
		TEXT("State"),
		TEXT("LastChangeTime")
	});

	for (const FAngelscriptEngine::FFilenamePair& FilenamePair : Engine.FileChangesDetectedForReload)
	{
		Writer.AddRow({
			GetFilenamePairPath(FilenamePair),
			TEXT("PendingReload"),
			FString()
		});
	}

	for (const FAngelscriptEngine::FFilenamePair& FilenamePair : Engine.FileDeletionsDetectedForReload)
	{
		Writer.AddRow({
			GetFilenamePairPath(FilenamePair),
			TEXT("PendingDeletion"),
			FString()
		});
	}

	FTableResult Result = SaveTable(OutputDir, TEXT("HotReloadState.csv"), Writer);
	if (Result.Status == TEXT("Success"))
	{
		Result.Status = TEXT("PartialExport");
		Result.ErrorMessage = TEXT("Private hot reload tracking data is not exported; only public reload queues are included.");
	}
	return Result;
}

FAngelscriptStateDump::FTableResult FAngelscriptStateDump::DumpJITDatabase(FAngelscriptEngine& Engine, const FString& OutputDir)
{
	const FJITDatabase& JITDatabase = FJITDatabase::Get();

	FCSVWriter Writer;
	Writer.AddHeader({
		TEXT("Category"),
		TEXT("EntryCount"),
		TEXT("Details")
	});

	Writer.AddRow({ TEXT("Functions"), LexToString(JITDatabase.Functions.Num()), FString() });
	Writer.AddRow({ TEXT("FunctionLookups"), LexToString(JITDatabase.FunctionLookups.Num()), FString() });
	Writer.AddRow({ TEXT("SystemFunctionPointerLookups"), LexToString(JITDatabase.SystemFunctionPointerLookups.Num()), FString() });
	Writer.AddRow({ TEXT("GlobalVarLookups"), LexToString(JITDatabase.GlobalVarLookups.Num()), FString() });
	Writer.AddRow({ TEXT("TypeInfoLookups"), LexToString(JITDatabase.TypeInfoLookups.Num()), FString() });
	Writer.AddRow({ TEXT("PropertyOffsetLookups"), LexToString(JITDatabase.PropertyOffsetLookups.Num()), FString() });

	return SaveTable(OutputDir, TEXT("JITDatabase.csv"), Writer);
}

FAngelscriptStateDump::FTableResult FAngelscriptStateDump::DumpPrecompiledData(FAngelscriptEngine& Engine, const FString& OutputDir)
{
	FCSVWriter Writer;
	Writer.AddHeader({
		TEXT("DataGuid"),
		TEXT("ModuleCount"),
		TEXT("FunctionMappingCount"),
		TEXT("ClassesLoadedCount"),
		TEXT("TimingData")
	});

	if (Engine.PrecompiledData == nullptr)
	{
		Writer.AddRow({ TEXT("NotLoaded"), TEXT("0"), TEXT("0"), TEXT("0"), FString() });
	}
	else
	{
		Writer.AddRow({
			Engine.PrecompiledData->DataGuid.ToString(EGuidFormats::DigitsWithHyphens),
			LexToString(Engine.PrecompiledData->Modules.Num()),
			LexToString(Engine.PrecompiledData->FunctionReferences.Num()),
			LexToString(Engine.PrecompiledData->ClassesLoadedFromPrecompiledData.Num()),
			FString()
		});
	}

	return SaveTable(OutputDir, TEXT("PrecompiledData.csv"), Writer);
}

FAngelscriptStateDump::FTableResult FAngelscriptStateDump::DumpStaticJITState(FAngelscriptEngine& Engine, const FString& OutputDir)
{
	FCSVWriter Writer;
	Writer.AddHeader({
		TEXT("JITFileCount"),
		TEXT("FunctionsToGenerateCount"),
		TEXT("SharedHeaderCount"),
		TEXT("ComputedOffsetsCount")
	});

	int32 JITFileCount = 0;
	int32 FunctionsToGenerateCount = 0;
	int32 SharedHeaderCount = 0;
	int32 ComputedOffsetsCount = 0;

#if AS_CAN_GENERATE_JIT
	if (Engine.StaticJIT != nullptr)
	{
		JITFileCount = Engine.StaticJIT->JITFiles.Num();
		FunctionsToGenerateCount = Engine.StaticJIT->FunctionsToGenerate.Num();
		SharedHeaderCount = Engine.StaticJIT->SharedHeaders.Num();
		ComputedOffsetsCount = Engine.StaticJIT->ComputedOffsets.Num();
	}
#endif

	Writer.AddRow({
		LexToString(JITFileCount),
		LexToString(FunctionsToGenerateCount),
		LexToString(SharedHeaderCount),
		LexToString(ComputedOffsetsCount)
	});

	return SaveTable(OutputDir, TEXT("StaticJITState.csv"), Writer);
}

FAngelscriptStateDump::FTableResult FAngelscriptStateDump::DumpDebugServerState(FAngelscriptEngine& Engine, const FString& OutputDir)
{
	FCSVWriter Writer;
	Writer.AddHeader({ TEXT("Key"), TEXT("Value") });

#if WITH_AS_DEBUGSERVER
	if (Engine.DebugServer != nullptr)
	{
		Writer.AddRow({ TEXT("HasAnyClients"), BoolToString(Engine.DebugServer->HasAnyClients()) });
		Writer.AddRow({ TEXT("BreakpointCount"), LexToString(Engine.DebugServer->BreakpointCount) });
		Writer.AddRow({ TEXT("DataBreakpointCount"), LexToString(Engine.DebugServer->DataBreakpoints.Num()) });
		Writer.AddRow({ TEXT("bIsPaused"), BoolToString(Engine.DebugServer->bIsPaused) });
		Writer.AddRow({ TEXT("bIsDebugging"), BoolToString(Engine.DebugServer->bIsDebugging) });
		Writer.AddRow({ TEXT("DebugAdapterVersion"), LexToString(AngelscriptDebugServer::DebugAdapterVersion) });
		return SaveTable(OutputDir, TEXT("DebugServerState.csv"), Writer);
	}
#endif

	FTableResult Result = SaveTable(OutputDir, TEXT("DebugServerState.csv"), Writer);
	if (Result.Status == TEXT("Success"))
	{
		Result.Status = TEXT("Skipped");
		Result.ErrorMessage = TEXT("Debug server support is not compiled or no debug server instance is active.");
	}
	return Result;
}

FAngelscriptStateDump::FTableResult FAngelscriptStateDump::DumpDebugBreakpoints(FAngelscriptEngine& Engine, const FString& OutputDir)
{
	FCSVWriter Writer;
	Writer.AddHeader({
		TEXT("Filename"),
		TEXT("Line"),
		TEXT("bIsEnabled"),
		TEXT("Condition")
	});

#if WITH_AS_DEBUGSERVER
	if (Engine.DebugServer != nullptr)
	{
		for (const TPair<FString, TSharedPtr<FAngelscriptDebugServer::FFileBreakpoints>>& BreakpointPair : Engine.DebugServer->Breakpoints)
		{
			if (!BreakpointPair.Value.IsValid())
			{
				continue;
			}

			for (const int32 Line : BreakpointPair.Value->Lines)
			{
				Writer.AddRow({
					BreakpointPair.Key,
					LexToString(Line),
					TEXT("true"),
					FString()
				});
			}
		}

		return SaveTable(OutputDir, TEXT("DebugBreakpoints.csv"), Writer);
	}
#endif

	FTableResult Result = SaveTable(OutputDir, TEXT("DebugBreakpoints.csv"), Writer);
	if (Result.Status == TEXT("Success"))
	{
		Result.Status = TEXT("Skipped");
		Result.ErrorMessage = TEXT("Debug server support is not compiled or no debug server instance is active.");
	}
	return Result;
}

FAngelscriptStateDump::FTableResult FAngelscriptStateDump::DumpCodeCoverage(FAngelscriptEngine& Engine, const FString& OutputDir)
{
	FCSVWriter Writer;
	Writer.AddHeader({
		TEXT("Filename"),
		TEXT("LineNumber"),
		TEXT("HitCount")
	});

#if WITH_AS_COVERAGE
	if (FAngelscriptCodeCoverage* CodeCoverageRecorder = FAngelscriptCodeCoverageExtension::GetForEngine(Engine))
	{
		const TArray<TSharedRef<FAngelscriptModuleDesc>> ActiveModules = Engine.GetActiveModules();
		for (const TSharedRef<FAngelscriptModuleDesc>& Module : ActiveModules)
		{
			const FLineCoverage* Coverage = CodeCoverageRecorder->GetLineCoverage(*Module);
			if (Coverage == nullptr)
			{
				continue;
			}

			for (const TPair<int, int>& HitPair : Coverage->HitCounts)
			{
				if (HitPair.Value <= 0)
				{
					continue;
				}

				Writer.AddRow({
					Coverage->AbsoluteFilename,
					LexToString(HitPair.Key),
					LexToString(HitPair.Value)
				});
			}
		}

		return SaveTable(OutputDir, TEXT("CodeCoverage.csv"), Writer);
	}
#endif

	FTableResult Result = SaveTable(OutputDir, TEXT("CodeCoverage.csv"), Writer);
	if (Result.Status == TEXT("Success"))
	{
		Result.Status = TEXT("Skipped");
		Result.ErrorMessage = TEXT("Code coverage support is not compiled or no coverage recorder is active.");
	}
	return Result;
}
