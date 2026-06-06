// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"

#include "AngelscriptLearningTraceCommandlet.generated.h"

namespace AngelscriptEditor::LearningTrace
{
	struct FLearningTraceExporterOptions;
}

UCLASS()
class ANGELSCRIPTEDITOR_API UAngelscriptLearningTraceCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
#if WITH_DEV_AUTOMATION_TESTS
	static bool BuildOptionsForTesting(
		const FString& Params,
		AngelscriptEditor::LearningTrace::FLearningTraceExporterOptions& OutOptions,
		FString& OutErrorMessage);
#endif

	virtual int32 Main(const FString& Params) override;
};
