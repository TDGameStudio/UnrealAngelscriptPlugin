#include "Testing/AngelscriptScriptTestWorld.h"

#include "ClassGenerator/ASClass.h"
#include "UObject/Class.h"

UClass* FAngelscriptScriptTestWorld::ResolveCurrentClass(
	UClass* Candidate)
{
	if (UASClass* ScriptClass = Cast<UASClass>(Candidate))
	{
		return ScriptClass->GetMostUpToDateClass();
	}
	return Candidate;
}

bool FAngelscriptScriptTestWorld::AreTickArgumentsValid(
	float DeltaSeconds,
	int32 NumTicks)
{
	return FMath::IsFinite(DeltaSeconds)
		&& DeltaSeconds >= 0.0f
		&& NumTicks >= 0;
}
