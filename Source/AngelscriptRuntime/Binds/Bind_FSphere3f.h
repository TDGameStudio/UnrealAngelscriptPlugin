#pragma once

#include "CoreMinimal.h"

#include "Helper_StructType.h"

struct FGetSphere3f
{
	static UScriptStruct* Get();
};

struct FSphere3fType : TAngelscriptCoreStructType<FSphere, FGetSphere3f, false>
{
	FString GetAngelscriptTypeName() const override;

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;
};

struct FAngelscriptFSphere3fBinds
{
	static void ConstructDefault(FSphere3f* Address);
	static void ConstructCenterRadius(FSphere3f* Address, FVector3f Center, float Radius);
	static void ConstructCopy(FSphere3f* Address, const FSphere3f& Sphere);
	static void ConstructFromSphere(FSphere3f* Address, const FSphere& Sphere);
	static void ConstructFromPoints(FSphere3f* Address, TArray<FVector3f>& Points);
};
