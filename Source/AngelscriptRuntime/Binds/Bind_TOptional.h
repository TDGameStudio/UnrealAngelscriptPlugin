#pragma once
#include "CoreMinimal.h"
#include "UObject/UnrealType.h"
#include "Containers/Set.h"
#include "AngelscriptType.h"
#include "AngelscriptEngine.h"

#include "StartAngelscriptHeaders.h"
//#include "as_context.h"
//#include "as_scriptfunction.h"
//#include "as_objecttype.h"
#include "source/as_context.h"
#include "source/as_scriptfunction.h"
#include "source/as_objecttype.h"
#include "EndAngelscriptHeaders.h"

struct FAngelscriptOptional {};

struct ANGELSCRIPTRUNTIME_API FOptionalOperations
{
	FAngelscriptTypeUsage Type;
	int32 TypeSize = 0;
	int32 Alignment = 0;
	bool bNeedDestruct = false;
	bool bNeedConstruct = false;
	bool bNeedCopy = false;
	bool bValid = false;

	explicit FOptionalOperations(const FAngelscriptTypeUsage& SubType);

	FORCEINLINE static FOptionalOperations* GetOptionalOperations(asCObjectType* Meta)
	{
		return (FOptionalOperations*)Meta->plainUserData;
	}

	FORCEINLINE bool* GetIsSetPtr(FAngelscriptOptional& Optional)
	{
		return (bool*)(((SIZE_T)&Optional) + TypeSize);
	}

	FORCEINLINE bool IsSet(FAngelscriptOptional& Optional)
	{
		return *GetIsSetPtr(Optional);
	}

	FORCEINLINE void* GetValuePtr(FAngelscriptOptional& Optional)
	{
		return &Optional;
	}

	void StoreValue(void* DestinationPtr, void* ValuePtr);
	void Reset(FAngelscriptOptional& Optional);
	void Set(FAngelscriptOptional& Optional, void* ValuePtr);

	static FOptionalOperations* ValidateOptionalOperations(asITypeInfo* TemplateType, asCString* ErrorMessage);
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptOptionalBinds
{
	static void Construct(FAngelscriptOptional& Optional, asCObjectType* Meta)
	{
		auto* Ops = FOptionalOperations::GetOptionalOperations(Meta);
		*Ops->GetIsSetPtr(Optional) = false;
	}

	template<typename T>
	static void Construct_Template(TOptional<T>& Optional)
	{
		new (&Optional) TOptional<T>();
	}

	static void Destruct(FAngelscriptOptional& Optional, asCObjectType* Meta)
	{
		auto* Ops = FOptionalOperations::GetOptionalOperations(Meta);
		Ops->Reset(Optional);
	}

	template<typename T>
	static void Destruct_Template(TOptional<T>& Optional)
	{
		Optional.~TOptional<T>();
	}

	static void CopyConstruct(FAngelscriptOptional& Optional, asCObjectType* Meta, FAngelscriptOptional& Other);

	template<typename T>
	static void CopyConstruct_Template(TOptional<T>& Optional, FAngelscriptOptional& Other)
	{
		new (&Optional) TOptional<T>(*(TOptional<T>*)&Other);
	}

	static void InitConstruct(FAngelscriptOptional& Optional, asCObjectType* Meta, void* ValuePtr);

	template<typename T>
	static void InitConstruct_Template(TOptional<T>& Optional, void* ValuePtr)
	{
		new (&Optional) TOptional<T>(*(T*)ValuePtr);
	}

	static bool IsSet(FAngelscriptOptional& Optional, asCObjectType* Meta);

	template<typename T>
	static bool IsSet_Template(TOptional<T>& Optional)
	{
		return Optional.IsSet();
	}

	static bool OpEquals(FAngelscriptOptional& Optional, asCObjectType* Meta, FAngelscriptOptional& Other);

	template<typename T>
	static bool OpEquals_Template(TOptional<T>& Optional, FAngelscriptOptional& Other)
	{
		return Optional == *(TOptional<T>*)&Other;
	}

	static FAngelscriptOptional& OpAssign(FAngelscriptOptional& Optional, asCObjectType* Meta, FAngelscriptOptional& Other);

	template<typename T>
	static FAngelscriptOptional& OpAssign_Template(TOptional<T>& Optional, FAngelscriptOptional& Other)
	{
		Optional = *(TOptional<T>*)&Other;
		return (FAngelscriptOptional&)Optional;
	}

	static FAngelscriptOptional& OpAssignValue(FAngelscriptOptional& Optional, asCObjectType* Meta, void* ValuePtr);

	template<typename T>
	static FAngelscriptOptional& OpAssignValue_Template(TOptional<T>& Optional, void* ValuePtr)
	{
		Optional = *(T*)ValuePtr;
		return (FAngelscriptOptional&)Optional;
	}

	static void Set(FAngelscriptOptional& Optional, asCObjectType* Meta, void* ValuePtr);

	template<typename T>
	static void Set_Template(TOptional<T>& Optional, void* ValuePtr)
	{
		Optional = *(T*)ValuePtr;
	}

	static void Reset(FAngelscriptOptional& Optional, asCObjectType* Meta);

	template<typename T>
	static void Reset_Template(TOptional<T>& Optional)
	{
		Optional.Reset();
	}

	static void* GetValue(FAngelscriptOptional& Optional, asCObjectType* Meta);

	template<typename T>
	static void* GetValue_Template(TOptional<T>& Optional)
	{
		if (!Optional.IsSet())
		{
			FAngelscriptEngine::Throw("GetValue() called on Optional when not set! Check the optional with IsSet() first.");
			return nullptr;
		}
		
		return &Optional.GetValue();
	}

	static void* Get(FAngelscriptOptional& Optional, asCObjectType* Meta, void* DefaultValuePtr);
	static bool ValidateTemplate(asITypeInfo* TemplateType, asCString* ErrorMessage);

	template<typename T>
	static void* Get_Template(TOptional<T>& Optional, void* DefaultValuePtr)
	{
		if (!Optional.IsSet())
			return DefaultValuePtr;
		return &Optional.GetValue();
	}
};
