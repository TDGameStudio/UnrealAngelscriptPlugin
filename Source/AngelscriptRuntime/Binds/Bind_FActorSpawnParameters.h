#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Helper_CppType.h"

struct FActorSpawnParametersType : TAngelscriptCppType<FActorSpawnParameters>
{
	FString GetAngelscriptTypeName() const override;
	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;
};

struct FAngelscriptActorSpawnParametersBinds
{
	static void Construct(FActorSpawnParameters* Address);
	static void CopyConstruct(FActorSpawnParameters* Address, const FActorSpawnParameters& Other);

	static bool GetNoFail(const FActorSpawnParameters* Parameters);
	static void SetNoFail(FActorSpawnParameters* Parameters, bool bNoFail);
	static bool GetDeferConstruction(const FActorSpawnParameters* Parameters);
	static void SetDeferConstruction(FActorSpawnParameters* Parameters, bool bDeferConstruction);
	static bool GetAllowDuringConstructionScript(const FActorSpawnParameters* Parameters);
	static void SetAllowDuringConstructionScript(FActorSpawnParameters* Parameters, bool bAllowDuringConstructionScript);
};
