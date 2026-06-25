#pragma once

#include "CoreMinimal.h"

#include "AngelscriptEngine.h"

#include "ASStruct.generated.h"

class asITypeInfo;

UCLASS()
class ANGELSCRIPTRUNTIME_API UASStruct : public UScriptStruct
{
	GENERATED_BODY()
public:
	struct FScriptStructValueHeader
	{
		static constexpr uint32 MagicValue = 0x41535354;

		uint32 Magic = 0;
		asITypeInfo* ScriptType = nullptr;
		UScriptStruct::ICppStructOps* CppStructOps = nullptr;

		bool IsValid() const
		{
			return Magic == MagicValue && ScriptType != nullptr && CppStructOps != nullptr;
		}
	};

	static constexpr int32 ScriptValueOffset = static_cast<int32>(
		(sizeof(FScriptStructValueHeader) + alignof(void*) - 1) & ~(alignof(void*) - 1));

	static FScriptStructValueHeader* GetValueHeader(void* Address)
	{
		return reinterpret_cast<FScriptStructValueHeader*>(Address);
	}

	static const FScriptStructValueHeader* GetValueHeader(const void* Address)
	{
		return reinterpret_cast<const FScriptStructValueHeader*>(Address);
	}

	static asITypeInfo* GetScriptTypeFromValue(const void* Address)
	{
		const FScriptStructValueHeader* Header = GetValueHeader(Address);
		return Header != nullptr && Header->IsValid() ? Header->ScriptType : nullptr;
	}

	UASStruct* NewerVersion = nullptr;
	class asITypeInfo* ScriptType = nullptr;
	FGuid Guid;
	bool bIsScriptStruct;

	UScriptStruct* GetNewestVersion()
	{
#if !AS_CAN_HOTRELOAD
		return this;
#else
		if (NewerVersion == nullptr)
			return this;

		UASStruct* NewerStruct = NewerVersion;
		while (NewerStruct->NewerVersion != nullptr)
			NewerStruct = NewerStruct->NewerVersion;
		return NewerStruct;
#endif
	}

	class asIScriptFunction* GetToStringFunction() const;

	FGuid GetCustomGuid() const override
	{
		return Guid;
	}

	void SetGuid(FName FromName);

	UASStruct(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void UpdateScriptType();

	void InitializeStruct(void* Dest, int32 ArrayDim = 1) const override;
	void DestroyStruct(void* Dest, int32 ArrayDim = 1) const override;
	void PrepareCppStructOps() override;
	ICppStructOps* CreateCppStructOps();

	void SetCppStructOps(ICppStructOps* Ops)
	{
		CppStructOps = Ops;
	} 
};

USTRUCT(BlueprintType)
struct FScriptStructWildcard
{
	GENERATED_BODY()
};
