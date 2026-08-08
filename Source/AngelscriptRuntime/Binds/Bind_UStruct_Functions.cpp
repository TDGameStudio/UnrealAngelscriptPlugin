#include "Bind_UStruct.h"

#include "AngelscriptEngine.h"

#include "UObject/Class.h"
#include "UObject/UnrealType.h"

void FAngelscriptUStructBinds::NoopConstruct(void*)
{
}

void FAngelscriptUStructBinds::NoopDestruct(void*)
{
}

void FAngelscriptUStructBinds::ZeroConstruct(void* Destination)
{
	UScriptStruct::ICppStructOps* Ops =
		FAngelscriptEngine::GetCurrentFunctionUserData<UScriptStruct::ICppStructOps>();
	FMemory::Memzero(Destination, Ops->GetSize());
}

void FAngelscriptUStructBinds::Construct(void* Destination)
{
	UScriptStruct::ICppStructOps* Ops =
		FAngelscriptEngine::GetCurrentFunctionUserData<UScriptStruct::ICppStructOps>();
	Ops->Construct(Destination);
}

void FAngelscriptUStructBinds::Destruct(void* Destination)
{
	UScriptStruct::ICppStructOps* Ops =
		FAngelscriptEngine::GetCurrentFunctionUserData<UScriptStruct::ICppStructOps>();
	Ops->Destruct(Destination);
}

void FAngelscriptUStructBinds::PodCopyConstruct(void* Destination, void* Source)
{
	UScriptStruct::ICppStructOps* Ops =
		FAngelscriptEngine::GetCurrentFunctionUserData<UScriptStruct::ICppStructOps>();
	FMemory::Memcpy(Destination, Source, Ops->GetSize());
}

void* FAngelscriptUStructBinds::PodAssign(void* Destination, void* Source)
{
	UScriptStruct::ICppStructOps* Ops =
		FAngelscriptEngine::GetCurrentFunctionUserData<UScriptStruct::ICppStructOps>();
	FMemory::Memcpy(Destination, Source, Ops->GetSize());
	return Destination;
}

void FAngelscriptUStructBinds::CopyConstructWithoutInitialization(void* Destination, void* Source)
{
	UScriptStruct::ICppStructOps* Ops =
		FAngelscriptEngine::GetCurrentFunctionUserData<UScriptStruct::ICppStructOps>();
	Ops->Copy(Destination, Source, 1);
}

void FAngelscriptUStructBinds::CopyConstructWithZeroInitialization(void* Destination, void* Source)
{
	UScriptStruct::ICppStructOps* Ops =
		FAngelscriptEngine::GetCurrentFunctionUserData<UScriptStruct::ICppStructOps>();
	FMemory::Memzero(Destination, Ops->GetSize());
	Ops->Copy(Destination, Source, 1);
}

void FAngelscriptUStructBinds::CopyConstructWithInitialization(void* Destination, void* Source)
{
	UScriptStruct::ICppStructOps* Ops =
		FAngelscriptEngine::GetCurrentFunctionUserData<UScriptStruct::ICppStructOps>();
	Ops->Construct(Destination);
	Ops->Copy(Destination, Source, 1);
}

void* FAngelscriptUStructBinds::CopyAssign(void* Destination, void* Source)
{
	UScriptStruct::ICppStructOps* Ops =
		FAngelscriptEngine::GetCurrentFunctionUserData<UScriptStruct::ICppStructOps>();
	Ops->Copy(Destination, Source, 1);
	return Destination;
}

void FAngelscriptUStructBinds::GenericConstruct(void* Destination)
{
	UScriptStruct* Struct = FAngelscriptEngine::GetCurrentFunctionUserData<UScriptStruct>();
	Struct->InitializeStruct(Destination);
}

void FAngelscriptUStructBinds::GenericDestruct(void* Destination)
{
	UScriptStruct* Struct = FAngelscriptEngine::GetCurrentFunctionUserData<UScriptStruct>();
	Struct->DestroyStruct(Destination);
}

void FAngelscriptUStructBinds::GenericCopyConstruct(void* Destination, void* Source)
{
	UScriptStruct* Struct = FAngelscriptEngine::GetCurrentFunctionUserData<UScriptStruct>();
	Struct->InitializeStruct(Destination);
	Struct->CopyScriptStruct(Destination, Source);
}

void* FAngelscriptUStructBinds::GenericAssign(void* Destination, void* Source)
{
	UScriptStruct* Struct = FAngelscriptEngine::GetCurrentFunctionUserData<UScriptStruct>();
	Struct->CopyScriptStruct(Destination, Source);
	return Destination;
}
