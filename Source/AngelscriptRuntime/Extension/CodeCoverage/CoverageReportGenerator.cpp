#include "CoverageReportGenerator.h"

#include "AngelscriptEngine.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

void AddCoverageLeaf(FCoverageNode& Root, const FString& Path, const FLineCoverage& Coverage)
{
	static const TCHAR* PathDelimiters[] = {
		TEXT("/"),
		TEXT("\\"),
	};
	TArray<FString> PathComponents;
	Path.ParseIntoArray(PathComponents, PathDelimiters, 2, false);

	FCoverageNode* Current = &Root;
	for (const FString& Component : PathComponents)
	{
		if (!Current->Children.Contains(Component))
		{
			Current->Children.Add(Component, new FCoverageNode);
		}
		Current = Current->Children[Component];
	}

	Current->Counts.NumLinesHit = Coverage.NumLinesHit();
	Current->Counts.NumExecutableLines = Coverage.NumExecutableLines();
}

FCoverageCounts ComputeCoverage(FCoverageNode& Node)
{
	if (Node.Children.Num() == 0)
	{
		return Node.Counts;
	}

	Node.Counts = FCoverageCounts{ 0, 0 };
	for (const TPair<FString, FCoverageNode*>& Child : Node.Children)
	{
		FCoverageCounts ChildCoverage = ComputeCoverage(*Child.Value);
		Node.Counts.NumLinesHit += ChildCoverage.NumLinesHit;
		Node.Counts.NumExecutableLines += ChildCoverage.NumExecutableLines;
	}
	return Node.Counts;
}

namespace
{
	bool IsIgnoredForCoverage(const FString& AsFilePath, const TArray<FString>& ExcludePatterns, FString& OutPattern)
	{
		for (const FString& IgnoredPattern : ExcludePatterns)
		{
			if (AsFilePath.MatchesWildcard(IgnoredPattern))
			{
				OutPattern = IgnoredPattern;
				return true;
			}
		}

		OutPattern.Reset();
		return false;
	}

	int32 CountSourceLines(const FString& AbsoluteFilename)
	{
		FString Contents;
		if (!FFileHelper::LoadFileToString(Contents, *AbsoluteFilename))
		{
			UE_LOG(Angelscript, Warning, TEXT("Failed reading source file for coverage JSON: %s"), *AbsoluteFilename);
			return 0;
		}

		TArray<FString> Lines;
		return Contents.ParseIntoArrayLines(Lines, false);
	}

	FCoverageCounts CountsForCoverage(const FLineCoverage& Coverage)
	{
		FCoverageCounts Counts;
		Counts.NumLinesHit = Coverage.NumLinesHit();
		Counts.NumExecutableLines = Coverage.NumExecutableLines();
		return Counts;
	}

	TSharedRef<FJsonObject> CountsToJson(const FCoverageCounts& Counts)
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetNumberField(TEXT("lines_hit"), Counts.NumLinesHit);
		Json->SetNumberField(TEXT("lines_total"), Counts.NumExecutableLines);
		Json->SetBoolField(TEXT("has_executable_lines"), Counts.NumExecutableLines > 0);
		if (Counts.NumExecutableLines > 0)
		{
			Json->SetNumberField(TEXT("coverage_pct"), static_cast<double>(Counts.NumLinesHit) / Counts.NumExecutableLines * 100.0);
		}
		else
		{
			Json->SetNumberField(TEXT("coverage_pct"), 0.0);
		}
		return Json;
	}

	TArray<TSharedPtr<FJsonValue>> LinesToJson(const FLineCoverage& Coverage)
	{
		TArray<int32> LineNumbers;
		LineNumbers.Reserve(Coverage.HitCounts.Num());
		for (const TPair<int, int>& HitPair : Coverage.HitCounts)
		{
			LineNumbers.Add(HitPair.Key);
		}
		LineNumbers.Sort();

		TArray<TSharedPtr<FJsonValue>> Lines;
		Lines.Reserve(LineNumbers.Num());
		for (const int32 LineNumber : LineNumbers)
		{
			TSharedRef<FJsonObject> LineObject = MakeShared<FJsonObject>();
			LineObject->SetNumberField(TEXT("line"), LineNumber);
			LineObject->SetNumberField(TEXT("hits"), Coverage.HitCounts.FindRef(LineNumber));
			Lines.Add(MakeShared<FJsonValueObject>(LineObject));
		}
		return Lines;
	}

	TArray<TSharedPtr<FJsonValue>> StringArrayToJson(const TArray<FString>& Values)
	{
		TArray<TSharedPtr<FJsonValue>> JsonValues;
		JsonValues.Reserve(Values.Num());
		for (const FString& Value : Values)
		{
			JsonValues.Add(MakeShared<FJsonValueString>(Value));
		}
		return JsonValues;
	}

	void AddDirectoryRows(
		const FCoverageNode& Node,
		const FString& Path,
		TArray<TSharedPtr<FJsonValue>>& OutDirectories)
	{
		TArray<FString> ChildNames;
		Node.Children.GenerateKeyArray(ChildNames);
		ChildNames.Sort();

		for (const FString& ChildName : ChildNames)
		{
			const FCoverageNode* Child = Node.Children.FindRef(ChildName);
			if (Child == nullptr)
			{
				continue;
			}

			const FString ChildPath = Path.IsEmpty() ? ChildName : FPaths::Combine(Path, ChildName);
			if (Child->Children.Num() > 0)
			{
				TSharedRef<FJsonObject> DirectoryObject = MakeShared<FJsonObject>();
				DirectoryObject->SetStringField(TEXT("path"), ChildPath);
				DirectoryObject->SetObjectField(TEXT("summary"), CountsToJson(Child->Counts));
				OutDirectories.Add(MakeShared<FJsonValueObject>(DirectoryObject));
				AddDirectoryRows(*Child, ChildPath, OutDirectories);
			}
		}
	}
}

bool WriteCoverageSummaryJson(
	TMap<FString, FLineCoverage>& FilesToCoverage,
	const FString& OutputDir,
	const FCoverageJsonExportOptions& Options)
{
	TArray<FString> RelativeFilenames;
	FilesToCoverage.GenerateKeyArray(RelativeFilenames);
	RelativeFilenames.Sort();

	FCoverageNode Root;
	TArray<TSharedPtr<FJsonValue>> Files;
	Files.Reserve(RelativeFilenames.Num());

	for (const FString& RelativeFilename : RelativeFilenames)
	{
		FLineCoverage& Coverage = FilesToCoverage.FindChecked(RelativeFilename);
		const int32 SourceLineCount = CountSourceLines(Coverage.AbsoluteFilename);
		Coverage.PruneGeneratedCode(SourceLineCount);

		FString ExcludePattern;
		const bool bExcluded = IsIgnoredForCoverage(RelativeFilename, Options.ExcludePatterns, ExcludePattern);
		if (!bExcluded)
		{
			AddCoverageLeaf(Root, RelativeFilename, Coverage);
		}

		TSharedRef<FJsonObject> FileObject = MakeShared<FJsonObject>();
		FileObject->SetStringField(TEXT("relative_path"), RelativeFilename);
		FileObject->SetStringField(TEXT("absolute_path"), Coverage.AbsoluteFilename);
		FileObject->SetNumberField(TEXT("source_line_count"), SourceLineCount);
		FileObject->SetBoolField(TEXT("included_in_summary"), !bExcluded);
		FileObject->SetStringField(TEXT("exclude_pattern"), ExcludePattern);
		FileObject->SetObjectField(TEXT("summary"), CountsToJson(CountsForCoverage(Coverage)));
		FileObject->SetArrayField(TEXT("lines"), LinesToJson(Coverage));
		Files.Add(MakeShared<FJsonValueObject>(FileObject));
	}

	ComputeCoverage(Root);

	TArray<TSharedPtr<FJsonValue>> Directories;
	TSharedRef<FJsonObject> RootDirectoryObject = MakeShared<FJsonObject>();
	RootDirectoryObject->SetStringField(TEXT("path"), TEXT(""));
	RootDirectoryObject->SetObjectField(TEXT("summary"), CountsToJson(Root.Counts));
	Directories.Add(MakeShared<FJsonValueObject>(RootDirectoryObject));
	AddDirectoryRows(Root, FString(), Directories);

	TSharedRef<FJsonObject> GeneratorObject = MakeShared<FJsonObject>();
	GeneratorObject->SetStringField(TEXT("name"), TEXT("AngelscriptRuntime.CodeCoverage"));
	GeneratorObject->SetStringField(TEXT("format"), TEXT("coverage_summary"));

	TSharedRef<FJsonObject> SettingsObject = MakeShared<FJsonObject>();
	SettingsObject->SetArrayField(TEXT("exclude_patterns"), StringArrayToJson(Options.ExcludePatterns));

	TSharedRef<FJsonObject> CaptureCapabilitiesObject = MakeShared<FJsonObject>();
	CaptureCapabilitiesObject->SetStringField(TEXT("line_coverage"), TEXT("collected"));
	CaptureCapabilitiesObject->SetStringField(TEXT("function_timing"), TEXT("not_collected"));

	TSharedRef<FJsonObject> ExtensionsObject = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> DiagnosticsObject = MakeShared<FJsonObject>();
	DiagnosticsObject->SetStringField(TEXT("function_timing"), TEXT("not_collected"));
	ExtensionsObject->SetObjectField(TEXT("diagnostics"), DiagnosticsObject);

	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("schema_version"), 1);
	Json->SetObjectField(TEXT("generator"), GeneratorObject);
	Json->SetStringField(TEXT("generated_at_utc"), FDateTime::UtcNow().ToIso8601());
	Json->SetObjectField(TEXT("summary"), CountsToJson(Root.Counts));
	Json->SetArrayField(TEXT("directories"), Directories);
	Json->SetArrayField(TEXT("files"), Files);
	Json->SetObjectField(TEXT("settings"), SettingsObject);
	Json->SetObjectField(TEXT("capture_capabilities"), CaptureCapabilitiesObject);
	Json->SetObjectField(TEXT("extensions"), ExtensionsObject);

	FString JsonString;
	const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> JsonWriter =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&JsonString, 0);
	if (!FJsonSerializer::Serialize(Json, JsonWriter, true))
	{
		UE_LOG(Angelscript, Error, TEXT("Failed serializing coverage JSON"));
		return false;
	}

	const FString OutputFile = FPaths::Combine(OutputDir, TEXT("coverage_summary.json"));
	if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutputFile), true))
	{
		UE_LOG(Angelscript, Error, TEXT("Failed creating directory for %s"), *OutputFile);
		return false;
	}

	if (!FFileHelper::SaveStringToFile(JsonString, *OutputFile,
			FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), EFileWrite::FILEWRITE_None))
	{
		UE_LOG(Angelscript, Error, TEXT("Failed writing %s"), *OutputFile);
		return false;
	}

	return true;
}
