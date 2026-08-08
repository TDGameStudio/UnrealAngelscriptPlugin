#pragma once

#include "CoreMinimal.h"

struct FAngelscriptUStructBinds
{
	static void NoopConstruct(void* Destination);
	static void NoopDestruct(void* Destination);
	static void ZeroConstruct(void* Destination);
	static void Construct(void* Destination);
	static void Destruct(void* Destination);
	static void PodCopyConstruct(void* Destination, void* Source);
	static void* PodAssign(void* Destination, void* Source);
	static void CopyConstructWithoutInitialization(void* Destination, void* Source);
	static void CopyConstructWithZeroInitialization(void* Destination, void* Source);
	static void CopyConstructWithInitialization(void* Destination, void* Source);
	static void* CopyAssign(void* Destination, void* Source);
	static void GenericConstruct(void* Destination);
	static void GenericDestruct(void* Destination);
	static void GenericCopyConstruct(void* Destination, void* Source);
	static void* GenericAssign(void* Destination, void* Source);
};
