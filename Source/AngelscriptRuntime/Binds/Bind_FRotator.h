#pragma once

#include "CoreMinimal.h"

#include "Helper_StructType.h"

struct FRotatorType : TAngelscriptBaseStructType<FRotator>
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

struct FAngelscriptFRotatorBinds
{
	static void ConstructComponents(FRotator* Address, double Pitch, double Yaw, double Roll);
	static void ConstructDefault(FRotator* Address);
	static void ConstructScalar(FRotator* Address, double Value);
	static void ConstructCopy(FRotator* Address, const FRotator& Other);
	static FVector GetRightVector(const FRotator& Rotator);
	static FVector GetUpVector(const FRotator& Rotator);
	static void ConstructFromQuat(FRotator* Address, const FQuat& Quat);
	static void ConstructFromRotator3f(FRotator* Address, const FRotator3f& Rotator);
	static FString ToColorString(const FRotator& Rotator);
	static void AppendToString(void* Address, FString& OutString);
};
