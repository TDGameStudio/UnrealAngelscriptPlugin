#pragma once

#include "CoreMinimal.h"
#include "CollisionQueryParams.h"
#include "Helper_CppType.h"

struct FCollisionQueryParamsType : TAngelscriptCppType<FCollisionQueryParams>
{
	FString GetAngelscriptTypeName() const override;
	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;
};

struct FCollisionEnabledMaskType : TAngelscriptCppType<FCollisionEnabledMask>
{
	FString GetAngelscriptTypeName() const override;
	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;
};

struct FComponentQueryParamsType : TAngelscriptCppType<FComponentQueryParams>
{
	FString GetAngelscriptTypeName() const override;
	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;
};

struct FCollisionResponseParamsType : TAngelscriptCppType<FCollisionResponseParams>
{
	FString GetAngelscriptTypeName() const override;
	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;
};

struct FCollisionObjectQueryParamsType : TAngelscriptCppType<FCollisionObjectQueryParams>
{
	FString GetAngelscriptTypeName() const override;
	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;
};

struct FAngelscriptFCollisionQueryParamsBinds
{
	static void ConstructCollisionQueryParams(FCollisionQueryParams* Address);
	static void ConstructCollisionQueryParamsCopy(FCollisionQueryParams* Address, const FCollisionQueryParams& Other);
	static void ConstructCollisionQueryParamsFromTraceTag(FCollisionQueryParams* Address, FName InTraceTag, bool bInTraceComplex, const AActor* InIgnoreActor);
	static TArray<uint32> GetCollisionQueryParamsIgnoredComponents(const FCollisionQueryParams* Address);
	static TArray<uint32> GetCollisionQueryParamsIgnoredActors(const FCollisionQueryParams* Address);

	static void ConstructCollisionEnabledMask(FCollisionEnabledMask* Address);
	static void ConstructCollisionEnabledMaskFromCollisionEnabled(FCollisionEnabledMask* Address, ECollisionEnabled::Type CollisionEnabled);

	static void ConstructComponentQueryParams(FComponentQueryParams* Address);
	static void ConstructComponentQueryParamsCopy(FComponentQueryParams* Address, const FComponentQueryParams& Other);
	static void ConstructComponentQueryParamsFromTraceTag(FComponentQueryParams* Address, FName InTraceTag, const AActor* InIgnoreActor, FCollisionEnabledMask CollisionEnabledMask);
	static TArray<uint32> GetComponentQueryParamsIgnoredComponents(const FComponentQueryParams* Address);
	static TArray<uint32> GetComponentQueryParamsIgnoredActors(const FComponentQueryParams* Address);

	static void ConstructCollisionResponseParams(FCollisionResponseParams* Address);
	static void ConstructCollisionResponseParamsFromDefaultResponse(FCollisionResponseParams* Address, ECollisionResponse DefaultResponse);
	static void ConstructCollisionResponseParamsFromContainer(FCollisionResponseParams* Address, const FCollisionResponseContainer& ResponseContainer);

	static void ConstructCollisionObjectQueryParams(FCollisionObjectQueryParams* Address);
	static void ConstructCollisionObjectQueryParamsFromChannel(FCollisionQueryParams* Address, ECollisionChannel QueryChannel);
	static void ConstructCollisionObjectQueryParamsFromInitType(FCollisionObjectQueryParams* Address, FCollisionObjectQueryParams::InitType QueryType);
	static void ConstructCollisionObjectQueryParamsFromObjectTypes(FCollisionObjectQueryParams* Address, int32 InObjectTypesToQuery);

	static void ConstructCollisionResponseContainer(FCollisionResponseContainer* Address, ECollisionResponse DefaultResponse);
};
