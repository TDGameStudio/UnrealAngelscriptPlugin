#include "CoreMinimal.h"
#include "AngelscriptBinds.h"

namespace
{
	void BindDefaultSkipConfiguration(FAngelscriptBinds& Binds)
	{
		FAngelscriptBindState& BindState = Binds.GetTargetBindState();
		BindState.SkipBindNames.Add(MakeTuple(
			FName(TEXT("StaticMesh")),
			FName(TEXT("GetMinLODForQualityLevels"))));
		BindState.SkipBindNames.Add(MakeTuple(
			FName(TEXT("StaticMesh")),
			FName(TEXT("SetMinLODForQualityLevels"))));
		BindState.SkipBindNames.Add(MakeTuple(
			FName(TEXT("SkeletalMesh")),
			FName(TEXT("GetMinLODForQualityLevels"))));
		BindState.SkipBindNames.Add(MakeTuple(
			FName(TEXT("SkeletalMesh")),
			FName(TEXT("SetMinLODForQualityLevels"))));
		BindState.SkipBindNames.Add(MakeTuple(
			FName(TEXT("SourceEffectEQPreset")),
			FName(TEXT("SetSettings"))));
		BindState.SkipBindClasses.Add(FName(TEXT("ClothingSimulationInteractorNv")));
		BindState.SkipBindClasses.Add(FName(TEXT("NiagaraPreviewGrid")));
		BindState.SkipBindClasses.Add(FName(TEXT("GameplayCamerasSubsystem")));
		BindState.SkipBindClasses.Add(FName(TEXT("AsyncAction_PerformTargeting")));
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_Skip(
	TEXT("SkipBinds.Defaults"),
	EAngelscriptBindPhase::ManualBindings,
	&BindDefaultSkipConfiguration);
