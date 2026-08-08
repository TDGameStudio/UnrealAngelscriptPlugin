#pragma once
#include "CoreMinimal.h"

class asITypeInfo;
struct FAngelscriptBinds;

struct FToStringHelper
{
	typedef void(*FToStringFunction)(void*, FString&);

	template<typename T>
	static void Register(
		FAngelscriptBinds& Binds,
		const FString& TypeName,
		T ToString,
		bool bImplicitConversion = false,
		bool bIsHandleType = false)
	{
		Register(Binds, TypeName, (FToStringFunction)ToString, bImplicitConversion, bIsHandleType);
	}

	static void ANGELSCRIPTRUNTIME_API Register(
		FAngelscriptBinds& Binds,
		const FString& TypeName,
		FToStringFunction ToString,
		bool bImplicitConversion = false,
		bool bIsHandleType = false);
	static void ANGELSCRIPTRUNTIME_API Generic_AppendToString(FString& AppendTo, void* ValuePtr, int TypeId);
};

struct FToStringType
{
	FString TypeName;
	asITypeInfo* TypeInfo = nullptr;
	FToStringHelper::FToStringFunction ToString;
	bool bImplicitConversion;
	bool bIsHandleType;
};
