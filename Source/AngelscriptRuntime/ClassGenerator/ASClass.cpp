#include "ClassGenerator/ASClass.h"

UObject* UASClass::OverrideConstructingObject = nullptr;

#if WITH_EDITOR
thread_local bool GIsInAngelscriptThreadSafeFunction = false;
thread_local bool GIsAngelscriptWorldContextAvailable = false;

ANGELSCRIPTRUNTIME_API void SetAngelscriptWorldContextAvailable(bool bAvailable)
{
	GIsAngelscriptWorldContextAvailable = bAvailable;
}
#endif

UASClass::UASClass(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}
