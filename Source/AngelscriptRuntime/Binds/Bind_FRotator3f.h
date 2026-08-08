#pragma once

#include "CoreMinimal.h"

#include "Helper_StructType.h"

struct FRotator3fType : TAngelscriptVariantStructType<FRotator3f>
{
	FString GetAngelscriptTypeName() const override;

	void ConstructValue(const FAngelscriptTypeUsage& Usage, void* DestinationPtr) const override;

	bool NeedConstruct(const FAngelscriptTypeUsage& Usage) const override;

	bool NeedDestruct(const FAngelscriptTypeUsage& Usage) const override;

	bool DefaultValue_UnrealToAngelscript(
			const FAngelscriptTypeUsage& Usage,
			const FString& InValue,
			FString& OutValue) const override;

	bool DefaultValue_AngelscriptToUnreal(
			const FAngelscriptTypeUsage& Usage,
			const FString& CppForm,
			FString& OutForm) const override;

	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;
};

struct FAngelscriptFRotator3fBinds
{
	static void ConstructComponents(FRotator3f* Address, float Pitch, float Yaw, float Roll);
	static void ConstructDefault(FRotator3f* Address);
	static void ConstructScalar(FRotator3f* Address, float Value);
	static void ConstructCopy(FRotator3f* Address, const FRotator3f& Other);
	static void ConstructFromQuat4f(FRotator3f* Address, const FQuat4f& Quat);
	static void ConstructFromRotator(FRotator3f* Address, const FRotator& Rotator);
	static FString ToColorString(const FRotator3f& Rotator);
	static void AppendToString(void* Address, FString& OutString);
};
