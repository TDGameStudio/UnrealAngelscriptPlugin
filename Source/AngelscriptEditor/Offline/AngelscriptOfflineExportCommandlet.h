#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"

#include "AngelscriptOfflineExportCommandlet.generated.h"

UCLASS()
class ANGELSCRIPTEDITOR_API UAngelscriptOfflineExportCommandlet :
	public UCommandlet
{
	GENERATED_BODY()

public:
	virtual int32 Main(const FString& Params) override;
};
