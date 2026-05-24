#pragma once

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"

struct FAngelscriptPerformanceMetric
{
	FString Name;
	TArray<double> Samples;
	double Median = 0.0;
	FString Unit;
	FString Source;
};

inline double ComputeMedian(TArray<double> Samples)
{
	if (Samples.Num() == 0)
	{
		return 0.0;
	}

	Samples.Sort();
	const int32 MiddleIndex = Samples.Num() / 2;
	if ((Samples.Num() % 2) == 0)
	{
		return (Samples[MiddleIndex - 1] + Samples[MiddleIndex]) * 0.5;
	}

	return Samples[MiddleIndex];
}

inline FString GetPerformanceRunDirectory(const FString& RunId)
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("AngelscriptPerformance"), RunId);
}

inline FString WritePerformanceMetricsArtifact(const FString& RunId, const FString& TestGroup, const TArray<FAngelscriptPerformanceMetric>& Metrics, const TArray<FString>& Notes)
{
	const FString RunDirectory = GetPerformanceRunDirectory(RunId);
	const FString MetricsDirectory = FPaths::Combine(RunDirectory, TEXT("Metrics"));
	IFileManager::Get().MakeDirectory(*MetricsDirectory, true);

	TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetStringField(TEXT("run_id"), RunId);
	RootObject->SetStringField(TEXT("test_group"), TestGroup);

	TArray<TSharedPtr<FJsonValue>> MetricValues;
	for (const FAngelscriptPerformanceMetric& Metric : Metrics)
	{
		TSharedRef<FJsonObject> MetricObject = MakeShared<FJsonObject>();
		MetricObject->SetStringField(TEXT("name"), Metric.Name);
		MetricObject->SetNumberField(TEXT("median"), Metric.Median);
		if (!Metric.Unit.IsEmpty())
		{
			MetricObject->SetStringField(TEXT("unit"), Metric.Unit);
		}
		if (!Metric.Source.IsEmpty())
		{
			MetricObject->SetStringField(TEXT("source"), Metric.Source);
		}

		TArray<TSharedPtr<FJsonValue>> SampleValues;
		for (const double Sample : Metric.Samples)
		{
			SampleValues.Add(MakeShared<FJsonValueNumber>(Sample));
		}
		MetricObject->SetArrayField(TEXT("samples"), SampleValues);
		MetricValues.Add(MakeShared<FJsonValueObject>(MetricObject));
	}
	RootObject->SetArrayField(TEXT("metrics"), MetricValues);

	TArray<TSharedPtr<FJsonValue>> NoteValues;
	for (const FString& Note : Notes)
	{
		NoteValues.Add(MakeShared<FJsonValueString>(Note));
	}
	RootObject->SetArrayField(TEXT("notes"), NoteValues);

	FString Output;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Output);
	FJsonSerializer::Serialize(RootObject, Writer);

	const FString MetricsPath = FPaths::Combine(MetricsDirectory, TEXT("metrics.json"));
	FFileHelper::SaveStringToFile(Output, *MetricsPath);
	return MetricsPath;
}

inline void LogPerformanceMetric(const FString& MetricName, const TArray<double>& Samples)
{
	for (const double Sample : Samples)
	{
		UE_LOG(LogTemp, Log, TEXT("[PERF] %s=%.6f"), *MetricName, Sample);
	}
}

