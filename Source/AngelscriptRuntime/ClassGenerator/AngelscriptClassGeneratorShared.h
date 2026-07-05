#pragma once

#include "CoreMinimal.h"

class UFunction;
extern UFunction* GetBlueprintEventByScriptName(UClass* Class, const FString& ScriptName);

namespace AngelscriptClassGeneratorNames
{
inline const FName NAME_ExposeOnSpawn(TEXT("ExposeOnSpawn"));
inline const FName NAME_EditFixedSize(TEXT("EditFixedSize"));
inline const FName NAME_DisplayName(TEXT("DisplayName"));
inline const FName NAME_Evt_ScriptName(TEXT("ScriptName"));
inline const FName NAME_AllowPrivateAccess(TEXT("AllowPrivateAccess"));
inline const FName NAME_Meta_EditorOnly(TEXT("EditorOnly"));

inline const FName NAME_Class_DefaultConfig(TEXT("DefaultConfig"));
inline const FName NAME_Actor_DefaultComponent(TEXT("DefaultComponent"));
inline const FName NAME_Actor_OverrideComponent(TEXT("OverrideComponent"));
inline const FName NAME_Actor_RootComponent(TEXT("RootComponent"));
inline const FName NAME_Actor_Attach(TEXT("Attach"));
inline const FName NAME_Actor_AttachSocket(TEXT("AttachSocket"));
inline const FName NAME_AnyStructRef(TEXT("__ANY_STRUCT_REF"));
inline const FName NAME_Function_MixinArgument(TEXT("MixinArgument"));
inline const FName NAME_Function_DefaultToSelf(TEXT("DefaultToSelf"));

inline const FName FUNCMETA_BlueprintThreadSafe("BlueprintThreadSafe");
inline const FName FUNCMETA_NotBlueprintThreadSafe("NotBlueprintThreadSafe");
inline const FName FUNCMETA_BlueprintProtected("BlueprintProtected");
inline const FName FUNCMETA_CrumbFunction("CrumbFunction");
inline const FName FUNCMETA_ScriptNoOp("ScriptNoOp");

inline const FName CLASSMETA_NotAngelscriptSpawnable("NotAngelscriptSpawnable");

inline const FString STR_Arg_WorldContext(TEXT("WorldContext"));
inline const FName NAME_Arg_WorldContext("WorldContext");
inline const FName NAME_Arg_AdvancedDisplay("AdvancedDisplay");
inline const FName NAME_Component_Spawnable("BlueprintSpawnableComponent");
}