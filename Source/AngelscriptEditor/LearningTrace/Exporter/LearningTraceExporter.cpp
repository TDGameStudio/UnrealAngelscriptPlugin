// Copyright Epic Games, Inc. All Rights Reserved.

#include "Exporter/LearningTraceExporter.h"

#include "Core/LearningTraceEventStream.h"
#include "Core/LearningTraceExample.h"
#include "Examples/LearningTraceExampleRegistry.h"
#include "Phases/TokenizerTap.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#include "angelscript.h"

namespace AngelscriptEditor::LearningTrace
{
	namespace
	{
		FString DefaultOutputDir()
		{
			return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("LearningTrace"));
		}

		TSharedRef<FJsonObject> BuildExampleEnvelope(
			const FLearningTraceExample& Example,
			const FLearningTraceEventStream& Stream)
		{
			TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
			Root->SetNumberField(TEXT("schemaVersion"), 2);
			Root->SetStringField(TEXT("generator"), TEXT("AngelscriptLearningTrace"));
			Root->SetStringField(TEXT("generatedAtUtc"), FDateTime::UtcNow().ToIso8601());

			TSharedRef<FJsonObject> ExampleObject = MakeShared<FJsonObject>();
			ExampleObject->SetStringField(TEXT("id"), Example.Id);
			ExampleObject->SetStringField(TEXT("title"), Example.Title);
			ExampleObject->SetStringField(TEXT("focus"), Example.Focus);
			ExampleObject->SetStringField(TEXT("source"), Example.Source);
			TArray<TSharedPtr<FJsonValue>> Topics;
			Topics.Reserve(Example.ExpectedTopics.Num());
			for (const FString& Topic : Example.ExpectedTopics)
			{
				Topics.Add(MakeShared<FJsonValueString>(Topic));
			}
			ExampleObject->SetArrayField(TEXT("expectedTopics"), Topics);

			Root->SetObjectField(TEXT("example"), ExampleObject);
			Root->SetArrayField(TEXT("events"), Stream.ToJsonArray());
			return Root;
		}

		bool WriteJsonFile(const FString& FilePath, const TSharedRef<FJsonObject>& Object, FString& OutErrorMessage)
		{
			FString Json;
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
			if (!FJsonSerializer::Serialize(Object, Writer))
			{
				OutErrorMessage = FString::Printf(TEXT("Failed to serialize JSON for %s"), *FilePath);
				return false;
			}
			if (!FFileHelper::SaveStringToFile(Json, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
			{
				OutErrorMessage = FString::Printf(TEXT("Failed to write %s"), *FilePath);
				return false;
			}
			return true;
		}

		// Builds the event stream for one example using the same pipeline
		// the Run() driver uses.
		bool RunPipeline(
			const asCScriptEngine* Engine,
			const FLearningTraceExample& Example,
			FLearningTraceEventStream& OutStream,
			FString& OutErrorMessage)
		{
			FTokenizerTap Tokenizer(Engine);
			if (!Tokenizer.Run(Example, OutStream, OutErrorMessage))
			{
				return false;
			}
			// Future phase taps are appended here.
			return true;
		}
	}

	FLearningTraceExporterResult FLearningTraceExporter::Run(const FLearningTraceExporterOptions& Options) const
	{
		FLearningTraceExporterResult Result;

		const FString OutputDir = Options.OutputDir.IsEmpty() ? DefaultOutputDir() : Options.OutputDir;
		const FString OutputDirAbs = FPaths::ConvertRelativePathToFull(OutputDir);
		IFileManager::Get().MakeDirectory(*OutputDirAbs, true);

		// Bare AS engine for tokenizer state access. No bindings, no UE bridge —
		// just enough to satisfy `engine->ep.allowUnicodeIdentifiers` reads.
		asIScriptEngine* AsEngineIface = asCreateScriptEngine();
		if (AsEngineIface == nullptr)
		{
			Result.ErrorMessage = TEXT("Failed to create bare asIScriptEngine.");
			return Result;
		}
		ON_SCOPE_EXIT { AsEngineIface->ShutDownAndRelease(); };
		const asCScriptEngine* Engine = static_cast<const asCScriptEngine*>(AsEngineIface);

		const TArray<FLearningTraceExample>& Examples = GetCuratedLearningTraceExamples();

		TArray<TSharedPtr<FJsonValue>> IndexEntries;
		IndexEntries.Reserve(Examples.Num());

		for (const FLearningTraceExample& Example : Examples)
		{
			FLearningTraceEventStream Stream;
			FString ErrorMessage;
			if (!RunPipeline(Engine, Example, Stream, ErrorMessage))
			{
				Result.ErrorMessage = FString::Printf(
					TEXT("Pipeline failed for example '%s': %s"), *Example.Id, *ErrorMessage);
				return Result;
			}

			const TSharedRef<FJsonObject> Envelope = BuildExampleEnvelope(Example, Stream);
			const FString FileName = FString::Printf(TEXT("%s.json"), *Example.Id);
			const FString FilePath = FPaths::Combine(OutputDirAbs, FileName);
			if (!WriteJsonFile(FilePath, Envelope, Result.ErrorMessage))
			{
				return Result;
			}

			TSharedRef<FJsonObject> IndexEntry = MakeShared<FJsonObject>();
			IndexEntry->SetStringField(TEXT("id"), Example.Id);
			IndexEntry->SetStringField(TEXT("title"), Example.Title);
			IndexEntry->SetStringField(TEXT("focus"), Example.Focus);
			IndexEntry->SetStringField(TEXT("file"), FileName);
			IndexEntry->SetNumberField(TEXT("eventCount"), Stream.Num());
			IndexEntries.Add(MakeShared<FJsonValueObject>(IndexEntry));
			++Result.ExamplesWritten;
		}

		TSharedRef<FJsonObject> Index = MakeShared<FJsonObject>();
		Index->SetNumberField(TEXT("schemaVersion"), 2);
		Index->SetStringField(TEXT("generator"), TEXT("AngelscriptLearningTrace"));
		Index->SetStringField(TEXT("generatedAtUtc"), FDateTime::UtcNow().ToIso8601());
		Index->SetArrayField(TEXT("examples"), IndexEntries);

		const FString IndexPath = FPaths::Combine(OutputDirAbs, TEXT("index.json"));
		if (!WriteJsonFile(IndexPath, Index, Result.ErrorMessage))
		{
			return Result;
		}

		Result.IndexFilePath = IndexPath;
		Result.bSuccess = true;
		return Result;
	}

	bool FLearningTraceExporter::RunSingleExampleForTesting(
		const FLearningTraceExample& Example,
		FLearningTraceEventStream& OutStream,
		FString& OutErrorMessage) const
	{
		asIScriptEngine* AsEngineIface = asCreateScriptEngine();
		if (AsEngineIface == nullptr)
		{
			OutErrorMessage = TEXT("Failed to create bare asIScriptEngine.");
			return false;
		}
		ON_SCOPE_EXIT { AsEngineIface->ShutDownAndRelease(); };
		const asCScriptEngine* Engine = static_cast<const asCScriptEngine*>(AsEngineIface);

		return RunPipeline(Engine, Example, OutStream, OutErrorMessage);
	}
}
