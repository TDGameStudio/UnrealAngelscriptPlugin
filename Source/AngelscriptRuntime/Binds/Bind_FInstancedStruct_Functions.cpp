#include "Bind_FInstancedStruct_Functions.h"

#include "StructUtils/InstancedStruct.h"

#include "AngelscriptAnyStructParameter.h"
#include "AngelscriptEngine.h"
#include "ClassGenerator/ASStruct.h"

namespace
{
	FScriptStructWildcard EmptyInstancedStructWildcard;

	const UScriptStruct* ResolveScriptStruct(const int TypeId)
	{
		const UStruct* StructDef = FAngelscriptEngine::Get().GetUnrealStructFromAngelscriptTypeId(TypeId);
		if (StructDef == nullptr)
		{
			FAngelscriptEngine::Throw("Not a valid USTRUCT");
			return nullptr;
		}

		const UScriptStruct* ScriptStructDef = Cast<UScriptStruct>(StructDef);
		if (ScriptStructDef == nullptr)
		{
			FAngelscriptEngine::Throw("Not a valid UScriptStruct");
			return nullptr;
		}

		return ScriptStructDef;
	}
}

FInstancedStruct FAngelscriptFInstancedStructBinds::Make(void* Data, const int TypeId)
{
	const UScriptStruct* ScriptStructDef = ResolveScriptStruct(TypeId);
	if (ScriptStructDef == nullptr)
	{
		return FInstancedStruct();
	}

	FInstancedStruct InstancedStruct;
	InstancedStruct.InitializeAs(ScriptStructDef, static_cast<uint8*>(Data));
	return InstancedStruct;
}

void FAngelscriptFInstancedStructBinds::ImplicitConstructAnyStruct(
	FAngelscriptAnyStructParameter* Address,
	void* Data,
	const int TypeId)
{
	new (Address) FAngelscriptAnyStructParameter();

	const UScriptStruct* ScriptStructDef = ResolveScriptStruct(TypeId);
	if (ScriptStructDef != nullptr)
	{
		Address->InstancedStruct.InitializeAs(ScriptStructDef, static_cast<uint8*>(Data));
	}
}

void FAngelscriptFInstancedStructBinds::ImplicitConstructAnyStructFromInstancedStruct(
	FAngelscriptAnyStructParameter* Address,
	const FInstancedStruct& InstancedStruct)
{
	new (Address) FAngelscriptAnyStructParameter();
	Address->InstancedStruct = InstancedStruct;
}

void FAngelscriptFInstancedStructBinds::InitializeAsStruct(
	FInstancedStruct* Self,
	void* Data,
	const int TypeId)
{
	const UScriptStruct* ScriptStructDef = ResolveScriptStruct(TypeId);
	if (ScriptStructDef != nullptr)
	{
		Self->InitializeAs(ScriptStructDef, static_cast<uint8*>(Data));
	}
}

void FAngelscriptFInstancedStructBinds::InitializeAsDefault(
	FInstancedStruct* Self,
	UScriptStruct* StructType)
{
	Self->InitializeAs(StructType);
}

FScriptStructWildcard& FAngelscriptFInstancedStructBinds::GetMemory(
	FInstancedStruct* Self,
	const UScriptStruct* StructType)
{
	if (!Self->IsValid())
	{
		FAngelscriptEngine::Throw("Source is empty or not valid. Check `IsValid()` before trying to `Get()` the underlying struct.");
		return EmptyInstancedStructWildcard;
	}

	if (StructType != Self->GetScriptStruct())
	{
		const FString Debug = FString::Printf(
			TEXT("Mismatching types. FInstancedStruct contains a %s but tried to Get a %s."),
			*Self->GetScriptStruct()->GetStructCPPName(),
			*StructType->GetStructCPPName());
		FAngelscriptEngine::Throw(TCHAR_TO_ANSI(*Debug));
		return EmptyInstancedStructWildcard;
	}

	return *reinterpret_cast<FScriptStructWildcard*>(Self->GetMutableMemory());
}

void FAngelscriptFInstancedStructBinds::CopyTo(
	const FInstancedStruct* Self,
	void* Data,
	const int TypeId)
{
	if (!Self->IsValid())
	{
		FAngelscriptEngine::Throw("Source is empty or not valid. Check `IsValid()` before trying to `Get()` the underlying struct.");
		return;
	}

	const UScriptStruct* ScriptStructDef = ResolveScriptStruct(TypeId);
	if (ScriptStructDef == nullptr)
	{
		return;
	}

	if (ScriptStructDef != Self->GetScriptStruct())
	{
		const FString Debug = FString::Printf(
			TEXT("\nMismatching types. Got %s but expected %s."),
			*ScriptStructDef->GetStructCPPName(),
			*Self->GetScriptStruct()->GetStructCPPName());
		FAngelscriptEngine::Throw(TCHAR_TO_ANSI(*Debug));
		return;
	}

	ScriptStructDef->CopyScriptStruct(Data, Self->GetMemory());
}

bool FAngelscriptFInstancedStructBinds::Contains(
	FInstancedStruct* Self,
	const UScriptStruct* StructType)
{
	return Self->IsValid() && StructType == Self->GetScriptStruct();
}
