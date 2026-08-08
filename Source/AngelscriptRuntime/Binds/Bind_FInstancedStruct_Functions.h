#pragma once

class UScriptStruct;
struct FAngelscriptAnyStructParameter;
struct FInstancedStruct;
struct FScriptStructWildcard;

struct FAngelscriptFInstancedStructBinds
{
	static FInstancedStruct Make(void* Data, int TypeId);
	static void ImplicitConstructAnyStruct(FAngelscriptAnyStructParameter* Address, void* Data, int TypeId);
	static void ImplicitConstructAnyStructFromInstancedStruct(
		FAngelscriptAnyStructParameter* Address,
		const FInstancedStruct& InstancedStruct);
	static void InitializeAsStruct(FInstancedStruct* Self, void* Data, int TypeId);
	static void InitializeAsDefault(FInstancedStruct* Self, UScriptStruct* StructType);
	static FScriptStructWildcard& GetMemory(FInstancedStruct* Self, const UScriptStruct* StructType);
	static void CopyTo(const FInstancedStruct* Self, void* Data, int TypeId);
	static bool Contains(FInstancedStruct* Self, const UScriptStruct* StructType);
};
