#include "AngelscriptBinds.h"

#include "AngelscriptEngine.h"
#include "AngelscriptMemoryTags.h"
#include "AngelscriptPerformanceStats.h"
#include "HAL/MallocLeakDetection.h"
#include "AngelscriptSettings.h"
#include "Misc/Paths.h"
#include "Testing/AngelscriptBindExecutionObservation.h"
#include "Testing/AngelscriptEnumTableBaselineProbe.h"

#include "AngelscriptInclude.h"
#include "AngelscriptSettings.h"
//#include "as_property.h"
//#include "as_scriptfunction.h"
#include "source/as_property.h"
#include "source/as_scriptfunction.h"

#include "StartAngelscriptHeaders.h"
//#include "as_scriptfunction.h"
//#include "as_objecttype.h"
//#include "as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "source/as_objecttype.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

static FAngelscriptBindState& GetBindState()
{
	if (FAngelscriptEngine* Engine = FAngelscriptEngine::TryGetCurrentEngine())
	{
		if (FAngelscriptBindState* State = Engine->GetBindState())
		{
			return *State;
		}
	}
	static FAngelscriptBindState LegacyBindState;
	return LegacyBindState;
}

TMap<FString, TArray<TObjectPtr<UClass>>>& FAngelscriptBinds::GetRuntimeClassDB()
{
	return GetBindState().RuntimeClassDB;
}

#if WITH_EDITOR
TMap<FString, TArray<TObjectPtr<UClass>>>& FAngelscriptBinds::GetEditorClassDB()
{
	return GetBindState().EditorClassDB;
}
#endif

TMap<UClass*, TMap<FString, FFuncEntry>>& FAngelscriptBinds::GetClassFuncMaps()
{
	return GetBindState().ClassFuncMaps;
}

TArray<FString>& FAngelscriptBinds::GetBindModuleNames()
{
	return GetBindState().BindModuleNames;
}

TMap<UClass*, TSet<FString>>& FAngelscriptBinds::GetSkipBinds()
{
	return GetBindState().SkipBinds;
}

TSet<TTuple<FName, FName>>& FAngelscriptBinds::GetSkipBindNames()
{
	return GetBindState().SkipBindNames;
}

TSet<FName>& FAngelscriptBinds::GetSkipBindClasses()
{
	return GetBindState().SkipBindClasses;
}

int32& FAngelscriptBinds::GetPreviouslyBoundFunctionRef()
{
	return GetBindState().PreviouslyBoundFunction;
}

int32& FAngelscriptBinds::GetPreviouslyBoundGlobalPropertyRef()
{
	return GetBindState().PreviouslyBoundGlobalProperty;
}

static FGeneratedFunctionTableTimingSummary& GetGeneratedFunctionTableTimingSummaryRef()
{
	return GetBindState().GeneratedFunctionTableTimingSummary;
}

bool FAngelscriptBinds::ShouldSkipBlueprintCallableFunction(const UFunction* Function)
{
	static const FName NAME_Function_NotInAngelscript(TEXT("NotInAngelscript"));
	static const FName NAME_Function_BlueprintInternalUseOnly(TEXT("BlueprintInternalUseOnly"));
	static const FName NAME_Function_UsableInAngelscript(TEXT("UsableInAngelscript"));

	if (Function == nullptr)
	{
		return true;
	}

	if (!Function->HasAnyFunctionFlags(FUNC_Native))
	{
		return true;
	}

	if (Function->HasMetaData(NAME_Function_NotInAngelscript))
	{
		return true;
	}

	if (Function->HasMetaData(NAME_Function_BlueprintInternalUseOnly) && !Function->HasMetaData(NAME_Function_UsableInAngelscript))
	{
		return true;
	}

	if (const UClass* OwningClass = Function->GetOuterUClass())
	{
		if (OwningClass == UActorComponent::StaticClass() && Function->GetFName() == FName(TEXT("GetOwner")))
		{
			return true;
		}
	}

	return false;
}

struct FBindFunction
{
	FName BindName;
	int32 BindOrder;
	TFunction<void()> Function;

	bool operator<(const FBindFunction& Other) const
	{
		return BindOrder < Other.BindOrder;
	}
};

static TArray<FBindFunction>& GetBindArray()
{
	static TArray<FBindFunction> BindArray;
	return BindArray;
}

static FName MakeUnnamedBindName()
{
	static int32 NextUnnamedBindId = 0;
	return FName(*FString::Printf(TEXT("UnnamedBind_%d"), NextUnnamedBindId++));
}

static FName MakeBindNameFromCallerFile(const ANSICHAR* CallerFile)
{
	if (CallerFile == nullptr || *CallerFile == '\0')
	{
		return MakeUnnamedBindName();
	}

	const FString FullPath = ANSI_TO_TCHAR(CallerFile);
	const FString Stem = FPaths::GetBaseFilename(FullPath);
	if (Stem.IsEmpty())
	{
		return MakeUnnamedBindName();
	}

	static TMap<FName, int32> StemDuplicateCounter;
	const FName StemName(*Stem);
	int32& Count = StemDuplicateCounter.FindOrAdd(StemName);
	if (Count == 0)
	{
		++Count;
		return StemName;
	}

	const FName UniqueName(*FString::Printf(TEXT("%s#%d"), *Stem, Count));
	++Count;
	return UniqueName;
}

static TArray<FBindFunction> GetSortedBindArray()
{
	TArray<FBindFunction> SortedBinds = GetBindArray();
	SortedBinds.Sort();
	return SortedBinds;
}

void FAngelscriptBinds::RegisterBinds(FName BindName, int32 BindOrder, TFunction<void()> Function)
{
	GetBindArray().Add({BindName.IsNone() ? MakeUnnamedBindName() : BindName, BindOrder, MoveTemp(Function)});
}

void FAngelscriptBinds::RegisterBinds(int32 BindOrder, TFunction<void()> Function, const ANSICHAR* CallerFile)
{
	GetBindArray().Add({MakeBindNameFromCallerFile(CallerFile), BindOrder, MoveTemp(Function)});
}

TArray<FName> FAngelscriptBinds::GetAllRegisteredBindNames()
{
	TArray<FName> BindNames;
	TSet<FName> UniqueBindNames;
	for (const FBindFunction& Bind : GetSortedBindArray())
	{
		if (!UniqueBindNames.Contains(Bind.BindName))
		{
			UniqueBindNames.Add(Bind.BindName);
			BindNames.Add(Bind.BindName);
		}
	}
	return BindNames;
}

TArray<FAngelscriptBinds::FBindInfo> FAngelscriptBinds::GetBindInfoList(const TSet<FName>& DisabledBindNames)
{
	TArray<FBindInfo> BindInfos;
	for (const FBindFunction& Bind : GetSortedBindArray())
	{
		BindInfos.Add({Bind.BindName, Bind.BindOrder, !DisabledBindNames.Contains(Bind.BindName)});
	}
	return BindInfos;
}

void FAngelscriptBinds::ResetBindState()
{
	GetBindState() = FAngelscriptBindState();
}

void FAngelscriptBinds::ResetGeneratedFunctionTableTiming()
{
	GetGeneratedFunctionTableTimingSummaryRef() = FGeneratedFunctionTableTimingSummary();
}

void FAngelscriptBinds::RecordGeneratedFunctionTableShardTiming(const TCHAR* ModuleName, int32 ShardIndex, int32 ShardCount, int32 EntryCount, double ElapsedMilliseconds)
{
	FGeneratedFunctionTableTimingSummary& TimingSummary = GetGeneratedFunctionTableTimingSummaryRef();
	TimingSummary.TotalEntryCount += EntryCount;
	TimingSummary.TotalShardCount++;
	TimingSummary.TotalDurationMs += ElapsedMilliseconds;

	const FName ModuleFName(ModuleName);
	FGeneratedFunctionTableModuleTiming& ModuleTiming = TimingSummary.ModuleTimings.FindOrAdd(ModuleFName);
	ModuleTiming.EntryCount += EntryCount;
	ModuleTiming.ShardCount++;
	ModuleTiming.TotalDurationMs += ElapsedMilliseconds;

	if (!TimingSummary.bHasSlowestShard || ElapsedMilliseconds > TimingSummary.SlowestShard.DurationMs)
	{
		TimingSummary.bHasSlowestShard = true;
		TimingSummary.SlowestShard.ModuleName = ModuleFName;
		TimingSummary.SlowestShard.ShardIndex = ShardIndex;
		TimingSummary.SlowestShard.ShardCount = ShardCount;
		TimingSummary.SlowestShard.EntryCount = EntryCount;
		TimingSummary.SlowestShard.DurationMs = ElapsedMilliseconds;
	}
}

void FAngelscriptBinds::LogGeneratedFunctionTableTimingSummary()
{
	const FGeneratedFunctionTableTimingSummary& TimingSummary = GetGeneratedFunctionTableTimingSummaryRef();
	if (TimingSummary.TotalShardCount == 0)
	{
		return;
	}

	FName SlowestModuleName = NAME_None;
	FGeneratedFunctionTableModuleTiming SlowestModuleTiming;
	bool bHasSlowestModule = false;
	for (const TPair<FName, FGeneratedFunctionTableModuleTiming>& ModulePair : TimingSummary.ModuleTimings)
	{
		if (!bHasSlowestModule || ModulePair.Value.TotalDurationMs > SlowestModuleTiming.TotalDurationMs)
		{
			bHasSlowestModule = true;
			SlowestModuleName = ModulePair.Key;
			SlowestModuleTiming = ModulePair.Value;
		}
	}

	const FString SlowestModuleNameString = SlowestModuleName.ToString();
	const FString SlowestShardModuleNameString = TimingSummary.SlowestShard.ModuleName.ToString();
	UE_LOG(
		Angelscript,
		Log,
		TEXT("[UHT] Registered %d generated BlueprintCallable entries across %d shard(s) in %.3f ms (%d module(s); slowest module %s %.3f ms; slowest shard %s %d/%d %.3f ms, %d entries)"),
		TimingSummary.TotalEntryCount,
		TimingSummary.TotalShardCount,
		TimingSummary.TotalDurationMs,
		TimingSummary.ModuleTimings.Num(),
		bHasSlowestModule ? *SlowestModuleNameString : TEXT("<none>"),
		bHasSlowestModule ? SlowestModuleTiming.TotalDurationMs : 0.0,
		TimingSummary.bHasSlowestShard ? *SlowestShardModuleNameString : TEXT("<none>"),
		TimingSummary.bHasSlowestShard ? TimingSummary.SlowestShard.ShardIndex : 0,
		TimingSummary.bHasSlowestShard ? TimingSummary.SlowestShard.ShardCount : 0,
		TimingSummary.bHasSlowestShard ? TimingSummary.SlowestShard.DurationMs : 0.0,
		TimingSummary.bHasSlowestShard ? TimingSummary.SlowestShard.EntryCount : 0);
}

void FAngelscriptBinds::CallBinds()
{
	CallBinds(TSet<FName>());
}

void FAngelscriptBinds::CallBinds(const TSet<FName>& DisabledBindNames)
{
	AS_PERF_SCOPE_BINDS_CALL_BINDS();
	LLM_SCOPE_BYTAG(Angelscript);
	MALLOCLEAK_SCOPED_CONTEXT(TEXT("Angelscript/CallBinds"));

	#if WITH_DEV_AUTOMATION_TESTS
	FAngelscriptBindExecutionObservation::BeginObservationPass(DisabledBindNames);
	#endif

	for (const FBindFunction& Bind : GetSortedBindArray())
	{
		if (DisabledBindNames.Contains(Bind.BindName))
		{
			UE_LOG(Angelscript, Log, TEXT("Skipping bind '%s'"), *Bind.BindName.ToString());
			continue;
		}

		#if MALLOC_LEAKDETECTION
		const FString BindLeakContext = FString::Printf(TEXT("Angelscript/Bind/%s"), *Bind.BindName.ToString());
		MALLOCLEAK_SCOPED_CONTEXT(*BindLeakContext);
		#endif

		#if WITH_DEV_AUTOMATION_TESTS
		FAngelscriptBindExecutionObservation::RecordExecutedBind(Bind.BindName);
		{
			FAngelscriptEnumTableBaselineBindScope BindScope(Bind.BindName, Bind.BindOrder);
			Bind.Function();
		}
		#else
		Bind.Function();
		#endif
	}

	#if WITH_DEV_AUTOMATION_TESTS
	FAngelscriptBindExecutionObservation::EndObservationPass();
	#endif
}

FAngelscriptBinds FAngelscriptBinds::ReferenceClass(FBindString Name, UClass* UnrealClass)
{
	auto Binds = FAngelscriptBinds(Name, asOBJ_REF | asOBJ_NOCOUNT | asOBJ_IMPLICIT_HANDLE, 0);
	((asCObjectType*)Binds.ScriptType)->size = UnrealClass->GetStructureSize();
	Binds.ScriptType->alignment = UnrealClass->GetMinAlignment();
	return Binds;
}

FAngelscriptBinds FAngelscriptBinds::ExistingClass(FBindString Name)
{
	return FAngelscriptBinds(Name);
}

FAngelscriptBinds FAngelscriptBinds::ValueClass(FBindString Name, FBindFlags Flags, int32 Size)
{
	auto asFlags = asOBJ_VALUE | asOBJ_APP_CLASS | Flags.ExtraFlags;
	if (Flags.bPOD)
		asFlags |= asOBJ_POD;
	if (Flags.bTemplate)
		asFlags |= asOBJ_TEMPLATE;

	auto Binds = FAngelscriptBinds(Name, asFlags, Size);
	if (Flags.bTemplate && !Flags.TemplateType.IsEmpty())
	{
		auto& Manager = FAngelscriptEngine::Get();
		int32 TemplatePos = Binds.ClassName.ToFString().Find(TEXT("<"));
		Binds.ClassName = Binds.ClassName.ToFString().Left(TemplatePos);
		Binds.ScriptType = Manager.Engine->GetTypeInfoByName(Binds.ClassName.ToCString());
		Binds.ClassName = Binds.ClassName.ToFString() + Flags.TemplateType.ToFString();
	}

	if (Flags.Alignment != -1)
	{
		check(Binds.ScriptType->alignment == 8 || Binds.ScriptType->alignment == Flags.Alignment);
		Binds.ScriptType->alignment = Flags.Alignment;
	}
	return Binds;
}

FAngelscriptBinds::FAngelscriptBinds(FBindString Name, asQWORD Flags, int32 Size)
	: ClassName(Name)
{
	auto& Manager = FAngelscriptEngine::Get();
	int TypeId = Manager.Engine->RegisterObjectType(ClassName.ToCString(), Size, Flags);

	if (TypeId == asALREADY_REGISTERED)
	{
		ScriptType = Manager.Engine->GetTypeInfoByName(ClassName.ToCString());
		check(ScriptType != nullptr);
		check(ScriptType->GetSize() == Size);
	}
	else
	{
		ScriptType = Manager.Engine->GetTypeInfoById(TypeId);
	}

	ensure(ScriptType != nullptr || ((Flags & asOBJ_TEMPLATE) != 0));
}

FAngelscriptBinds::FAngelscriptBinds(FBindString Name)
	: ClassName(Name)
{
}

void FAngelscriptBinds::GenericMethod(FBindString Signature, void(CDECL *Fun)(asIScriptGeneric*), void* UserData)
{
	auto& Manager = FAngelscriptEngine::Get();
	int FunctionId = Manager.Engine->RegisterObjectMethod(ClassName.ToCString(), Signature.ToCString(), asFUNCTION(Fun), asCALL_GENERIC, nullptr);
	OnBind(FunctionId, UserData, nullptr);
}

void FAngelscriptBinds::BindBehaviour(asEBehaviours Beh, FBindString Signature, asSFuncPtr Ptr, ASAutoCaller::FunctionCaller Caller)
{
	auto& Manager = FAngelscriptEngine::Get();
	int FunctionId = Manager.Engine->RegisterObjectBehaviour(ClassName.ToCString(), Beh, Signature.ToCString(), Ptr, asCALL_THISCALL, *(asFunctionCaller*)&Caller);
	OnBind(FunctionId, nullptr, nullptr);
}

asIScriptFunction* FAngelscriptBinds::GetPreviousBind()
{
	if (GetPreviouslyBoundFunctionRef() == -1)
		return nullptr;

	auto& Manager = FAngelscriptEngine::Get();
	return Manager.Engine->GetFunctionById(GetPreviouslyBoundFunctionRef());
}

void FAngelscriptBinds::MarkAsImplicitConstructor()
{
	if (auto* Function = (asCScriptFunction*)GetPreviousBind())
	{
		Function->traits.SetTrait(asEFuncTrait::asTRAIT_IMPLICITCONSTRUCTOR, true);
		if (auto* ObjectType = Function->objectType)
			ObjectType->hasImplicitConstructors = true;
	}
}

void FAngelscriptBinds::DeprecatePreviousBind(const ANSICHAR* DeprecationMessage)
{
	if (auto* Function = (asCScriptFunction*)GetPreviousBind())
	{
		Function->traits.SetTrait(asEFuncTrait::asTRAIT_DEPRECATED, true);
#if WITH_EDITOR
		Function->deprecationMessage = DeprecationMessage;
#endif
	}
}

void FAngelscriptBinds::SetPreviousBindIsGeneratedAccessor(bool bIsAccessor)
{
	if (auto* Function = (asCScriptFunction*)GetPreviousBind())
	{
		Function->traits.SetTrait(asEFuncTrait::asTRAIT_GENERATED_FUNCTION, bIsAccessor);
	}
}

void FAngelscriptBinds::SetPreviousBindIsEditorOnly(bool bEditorOnly)
{
	if (auto* Function = (asCScriptFunction*)GetPreviousBind())
	{
		Function->traits.SetTrait(asEFuncTrait::asTRAIT_EDITOR_ONLY, bEditorOnly);
	}
}

void FAngelscriptBinds::SetPreviousBindRequiresWorldContext(bool bRequiresWorldContext)
{
	if (auto* Function = (asCScriptFunction*)GetPreviousBind())
	{
		Function->traits.SetTrait(asEFuncTrait::asTRAIT_USES_WORLDCONTEXT, bRequiresWorldContext);
	}
}

void FAngelscriptBinds::SetPreviousBindIsCallable(bool bIsCallable)
{
	if (auto* Function = (asCScriptFunction*)GetPreviousBind())
	{
		Function->traits.SetTrait(asEFuncTrait::asTRAIT_NOT_CALLABLE, !bIsCallable);
	}
}

void FAngelscriptBinds::SetPreviousBindNoDiscard(bool bNoDiscard)
{
	if (auto* Function = (asCScriptFunction*)GetPreviousBind())
	{
		Function->traits.SetTrait(asEFuncTrait::asTRAIT_NODISCARD, bNoDiscard);
	}
}

void FAngelscriptBinds::SetPreviousBindArgumentDeterminesOutputType(int ArgumentIndex)
{
	if (auto* Function = (asCScriptFunction*)GetPreviousBind())
	{
		Function->determinesOutputTypeArgumentIndex = ArgumentIndex;
	}
}

void FAngelscriptBinds::SetPreviousBindForceConstArgumentExpressions(bool bForceConst)
{
	if (auto* Function = (asCScriptFunction*)GetPreviousBind())
	{
		Function->traits.SetTrait(asEFuncTrait::asTRAIT_FORCE_CONST_ARGUMENT_EXPRESSIONS, bForceConst);
	}
}

void FAngelscriptBinds::PreviousBindPassScriptFunctionAsFirstParam()
{
	if (auto* Function = (asCScriptFunction*)GetPreviousBind())
	{
		Function->sysFuncIntf->passFirstParamMetaData = asEFirstParamMetaData::ScriptFunction;
	}
}

void FAngelscriptBinds::PreviousBindPassScriptObjectTypeAsFirstParam()
{
	if (auto* Function = (asCScriptFunction*)GetPreviousBind())
	{
		Function->sysFuncIntf->passFirstParamMetaData = asEFirstParamMetaData::ScriptObjectType;
	}
}

void FAngelscriptBinds::OnBind(int FunctionId, void* UserData, const FAngelscriptType::FBindParams* BindParams)
{
	auto& Manager = FAngelscriptEngine::Get();
	asCScriptFunction* ScriptFunction = (asCScriptFunction*)Manager.Engine->GetFunctionById(FunctionId);
	if (ScriptFunction != nullptr)
	{
		if (UserData != nullptr)
		{
			ScriptFunction->SetUserData(UserData, 0);
		}

		if (BindParams != nullptr && BindParams->bProtected)
		{
			ScriptFunction->SetProtected(true);
		}

	}

	GetPreviouslyBoundFunctionRef() = FunctionId;
}

void FAngelscriptBinds::BindExternBehaviour(asEBehaviours Beh, FBindString Signature, asSFuncPtr Ptr, ASAutoCaller::FunctionCaller Caller, void* UserData)
{
	auto& Manager = FAngelscriptEngine::Get();
	int FunctionId = Manager.Engine->RegisterObjectBehaviour(ClassName.ToCString(), Beh, Signature.ToCString(), Ptr, asCALL_CDECL_OBJFIRST, *(asFunctionCaller*)&Caller);
	OnBind(FunctionId, UserData, nullptr);
}

void FAngelscriptBinds::BindStaticBehaviour(asEBehaviours Beh, FBindString Signature, asSFuncPtr Ptr, ASAutoCaller::FunctionCaller Caller, void* UserData)
{
	auto& Manager = FAngelscriptEngine::Get();
	int FunctionId = Manager.Engine->RegisterObjectBehaviour(ClassName.ToCString(), Beh, Signature.ToCString(), Ptr, asCALL_CDECL, *(asFunctionCaller*)&Caller);
	OnBind(FunctionId, UserData, nullptr);
}

void FAngelscriptBinds::BindMethod(FBindString Signature, asSFuncPtr Ptr, ASAutoCaller::FunctionCaller Caller, void* UserData)
{
	auto& Manager = FAngelscriptEngine::Get();
	int FunctionId = Manager.Engine->RegisterObjectMethod(ClassName.ToCString(), Signature.ToCString(), Ptr, asCALL_THISCALL, *(asFunctionCaller*)&Caller, nullptr);
	OnBind(FunctionId, UserData, nullptr);
}

int FAngelscriptBinds::BindMethodDirect(FBindString ClassName, FBindString Signature, asSFuncPtr Function, asECallConvTypes CallConv, ASAutoCaller::FunctionCaller Caller, void* UserData)
{
	auto& Manager = FAngelscriptEngine::Get();
	int32 FunctionId = Manager.Engine->RegisterObjectMethod(ClassName.ToCString(), Signature.ToCString(), Function, CallConv, *(asFunctionCaller*)&Caller);
	OnBind(FunctionId, UserData, nullptr);
	return FunctionId;
}

int FAngelscriptBinds::CompileOutInTest(int FunctionId)
{
	auto& Manager = FAngelscriptEngine::Get();
	auto* Function = (asCScriptFunction*)Manager.Engine->GetFunctionById(FunctionId);

	if (UE_BUILD_TEST || UE_BUILD_SHIPPING || (WITH_EDITOR && FAngelscriptEngine::IsSimulatingCookedForCurrentContext()))
	{
		Function->compileOutType = asECompileOutType::CompileOutEntirely;
	}

	if (Manager.ConfigSettings->bForceConstWithinDevelopmentOnlyFunctions)
	{
		Function->traits.SetTrait(asTRAIT_FORCE_CONST_ARGUMENT_EXPRESSIONS, true);
	}

	return FunctionId;
}

int FAngelscriptBinds::CompileOutIfNoLog(int FunctionId)
{
	auto& Manager = FAngelscriptEngine::Get();
	auto* Function = (asCScriptFunction*)Manager.Engine->GetFunctionById(FunctionId);

	if (UE_BUILD_TEST || UE_BUILD_SHIPPING || (WITH_EDITOR && FAngelscriptEngine::IsSimulatingCookedForCurrentContext()))
	{
		Function->compileOutType = asECompileOutType::CompileOutEntirely;
	}

	if (Manager.ConfigSettings->bForceConstWithinDevelopmentOnlyFunctions)
	{
		Function->traits.SetTrait(asTRAIT_FORCE_CONST_ARGUMENT_EXPRESSIONS, true);
	}

	return FunctionId;
}

int FAngelscriptBinds::CompileOutAsEnsure(int FunctionId)
{
	auto& Manager = FAngelscriptEngine::Get();
	auto* Function = (asCScriptFunction*)Manager.Engine->GetFunctionById(FunctionId);
	Function->traits.SetTrait(asTRAIT_NODISCARD, true);

	if (UE_BUILD_SHIPPING || (WITH_EDITOR && FAngelscriptEngine::IsSimulatingCookedForCurrentContext()))
	{
		Function->compileOutType = asECompileOutType::ReplaceWithFirstParam;
	}
	return FunctionId;
}

int FAngelscriptBinds::CompileOutAsCheck(int FunctionId)
{
	auto& Manager = FAngelscriptEngine::Get();
	auto* Function = (asCScriptFunction*)Manager.Engine->GetFunctionById(FunctionId);

	if (UE_BUILD_SHIPPING || (WITH_EDITOR && FAngelscriptEngine::IsSimulatingCookedForCurrentContext()))
	{
		Function->compileOutType = asECompileOutType::CompileOutEntirely;
	}

	if (Manager.ConfigSettings->bForceConstWithinDevelopmentOnlyFunctions)
	{
		Function->traits.SetTrait(asTRAIT_FORCE_CONST_ARGUMENT_EXPRESSIONS, true);
	}

	return FunctionId;
}

int FAngelscriptBinds::CompileReplaceWithFirstArgInTest(int FunctionId)
{
	if (UE_BUILD_TEST || UE_BUILD_SHIPPING || (WITH_EDITOR && FAngelscriptEngine::IsSimulatingCookedForCurrentContext()))
	{
		auto& Manager = FAngelscriptEngine::Get();
		auto* Function = (asCScriptFunction*)Manager.Engine->GetFunctionById(FunctionId);
		Function->compileOutType = asECompileOutType::ReplaceWithFirstParam;
	}
	return FunctionId;
}

void FAngelscriptBinds::CompileOutPreviousBind()
{
	auto& Manager = FAngelscriptEngine::Get();
	auto* Function = (asCScriptFunction*)Manager.Engine->GetFunctionById(GetPreviouslyBoundFunctionRef());
	Function->compileOutType = asECompileOutType::CompileOutEntirely;
}

void FAngelscriptBinds::CompileOutPreviousBindAsMethodChain()
{
	auto& Manager = FAngelscriptEngine::Get();
	auto* Function = (asCScriptFunction*)Manager.Engine->GetFunctionById(GetPreviouslyBoundFunctionRef());
	Function->compileOutType = asECompileOutType::CompileOutAsMethodChain;
}

int FAngelscriptBinds::BindExternMethod(FBindString Signature, asSFuncPtr Ptr, ASAutoCaller::FunctionCaller Caller, void* UserData)
{
	auto& Manager = FAngelscriptEngine::Get();
	int FunctionId = Manager.Engine->RegisterObjectMethod(ClassName.ToCString(), Signature.ToCString(), Ptr, asCALL_CDECL_OBJFIRST, *(asFunctionCaller*)&Caller, nullptr);
	OnBind(FunctionId, UserData, nullptr);

	return FunctionId;
}

int FAngelscriptBinds::BindExternMethod(FBindString Signature, asSFuncPtr Ptr, const FAngelscriptType::FBindParams& BindParams, ASAutoCaller::FunctionCaller Caller, void* UserData)
{
	auto& Manager = FAngelscriptEngine::Get();
	const asUINT Access = asCScriptFunction::GenerateExposedType(BindParams.bCanEdit, BindParams.bCanRead, BindParams.bCanWrite);
	int FunctionId = Manager.Engine->RegisterObjectMethod(ClassName.ToCString(), Signature.ToCString(), Ptr, asCALL_CDECL_OBJFIRST, *(asFunctionCaller*)&Caller, nullptr, 0, false, Access);
	OnBind(FunctionId, UserData, &BindParams);

	return FunctionId;
}

void FAngelscriptBinds::BindProperty(FBindString Signature, size_t Offset)
{
	auto& Manager = FAngelscriptEngine::Get();
	Manager.Engine->RegisterObjectProperty(ClassName.ToCString(), Signature.ToCString(), Offset);
}

void FAngelscriptBinds::BindProperty(FBindString Signature, size_t Offset, const FAngelscriptType::FBindParams& BindParams)
{
	auto& Manager = FAngelscriptEngine::Get();
	const asUINT Access = asCObjectProperty::GenerateExposedType(BindParams.bCanEdit, BindParams.bCanRead, BindParams.bCanWrite);
	Manager.Engine->RegisterObjectProperty(ClassName.ToCString(), Signature.ToCString(), Offset, 0, false, Access, BindParams.bProtected);
}

int FAngelscriptBinds::BindGlobalFunction(FBindString Signature, asSFuncPtr Function, ASAutoCaller::FunctionCaller Caller, void* UserData)
{
	auto& Manager = FAngelscriptEngine::Get();
	int FunctionId = Manager.Engine->RegisterGlobalFunction(Signature.ToCString(), Function, asCALL_CDECL, *(asFunctionCaller*)&Caller, nullptr);
	OnBind(FunctionId, UserData, nullptr);

	return FunctionId;
}

int FAngelscriptBinds::BindGlobalFunction(FBindString Signature, asSFuncPtr Function, FBindString FuncName, bool bTrivial, ASAutoCaller::FunctionCaller Caller, void* UserData)
{
	int Result = BindGlobalFunction(Signature.ToCString(), Function, Caller, UserData);
	SCRIPT_NATIVE_FUNCTION(FuncName.ToCString_EnsureConstant(), bTrivial);
	return Result;
}

int FAngelscriptBinds::BindGlobalFunctionDirect(FBindString Signature, asSFuncPtr Function, asECallConvTypes CallConv, ASAutoCaller::FunctionCaller Caller, void* UserData)
{
	auto& Manager = FAngelscriptEngine::Get();
	int FunctionId = Manager.Engine->RegisterGlobalFunction(Signature.ToCString(), Function, CallConv, *(asFunctionCaller*)&Caller);
	OnBind(FunctionId, UserData, nullptr);

	return FunctionId;
}

int FAngelscriptBinds::BindGlobalGenericFunction(FBindString Signature, void(CDECL* Function)(asIScriptGeneric*), void* UserData)
{
	auto& Manager = FAngelscriptEngine::Get();
	int FunctionId = Manager.Engine->RegisterGlobalFunction(Signature.ToCString(), asFUNCTION(Function), asCALL_GENERIC, nullptr, nullptr);
	OnBind(FunctionId, UserData, nullptr);

	return FunctionId;
}

void FAngelscriptBinds::BindGlobalVariable(FBindString Signature, const void* Address)
{
	auto& Manager = FAngelscriptEngine::Get();
	GetPreviouslyBoundGlobalPropertyRef() = Manager.Engine->RegisterGlobalProperty(Signature.ToCString(), (void*)Address);
}

void FAngelscriptBinds::SetPreviousBoundPropertyPureConstant(asQWORD ConstantValue)
{
	auto& Manager = FAngelscriptEngine::Get();
	asCGlobalProperty* Property = Manager.Engine->globalProperties[GetPreviouslyBoundGlobalPropertyRef()];
	Property->isPureConstant = true;
	Property->storage = ConstantValue;
}

FAngelscriptBinds::FEnumBind::FEnumBind(FBindString Name)
{
	EnumName = Name;

	auto& Manager = FAngelscriptEngine::Get();
	auto* PrevNamespace = Manager.Engine->GetDefaultNamespace();

	TypeId = Manager.Engine->RegisterEnum(Name.ToCString());
	if (TypeId == asALREADY_REGISTERED)
	{
		if (asITypeInfo* ExistingType = Manager.Engine->GetTypeInfoByName(Name.ToCString()))
		{
			TypeId = ExistingType->GetTypeId();
		}
	}
}

asITypeInfo* FAngelscriptBinds::FEnumBind::GetTypeInfo()
{
	auto& Manager = FAngelscriptEngine::Get();
	return Manager.Engine->GetTypeInfoById(TypeId);
}

void FAngelscriptBinds::FEnumBind::FEnumElement::operator=(int32 Value)
{
	auto& Manager = FAngelscriptEngine::Get();
	auto* PrevNamespace = Manager.Engine->GetDefaultNamespace();

#if WITH_DEV_AUTOMATION_TESTS
	const double DedupeStartSeconds = FPlatformTime::Seconds();
	int32 DedupeStrcmpCount = 0;
	bool bSkippedByDedupe = false;
#endif

	if (asITypeInfo* ExistingEnum = Bind->GetTypeInfo())
	{
		for (asUINT Index = 0, Count = ExistingEnum->GetEnumValueCount(); Index < Count; ++Index)
		{
			int ExistingValue = 0;
			const char* ExistingName = ExistingEnum->GetEnumValueByIndex(Index, &ExistingValue);
#if WITH_DEV_AUTOMATION_TESTS
			if (ExistingName != nullptr)
			{
				++DedupeStrcmpCount;
			}
#endif
			if (ExistingName != nullptr && FCStringAnsi::Strcmp(ExistingName, Name.ToCString()) == 0)
			{
#if WITH_DEV_AUTOMATION_TESTS
				bSkippedByDedupe = true;
				const double DedupeElapsedSeconds = FPlatformTime::Seconds() - DedupeStartSeconds;
				FAngelscriptEnumTableBaselineProbe::RecordEnumValueRegister(
					Bind->EnumName.ToCString(),
					DedupeElapsedSeconds,
					DedupeStrcmpCount,
					0.0,
					true);
#endif
				return;
			}
		}
	}

#if WITH_DEV_AUTOMATION_TESTS
	const double DedupeElapsedSeconds = FPlatformTime::Seconds() - DedupeStartSeconds;
	const double RegisterStartSeconds = FPlatformTime::Seconds();
#endif

	auto AnsiEnumName = Bind->EnumName.ToCString();
	const int Result = Manager.Engine->RegisterEnumValue(AnsiEnumName, Name.ToCString(), Value);

#if WITH_DEV_AUTOMATION_TESTS
	const double RegisterElapsedSeconds = FPlatformTime::Seconds() - RegisterStartSeconds;
	FAngelscriptEnumTableBaselineProbe::RecordEnumValueRegister(
		AnsiEnumName,
		DedupeElapsedSeconds,
		DedupeStrcmpCount,
		RegisterElapsedSeconds,
		Result == asALREADY_REGISTERED);
#endif

	if (Result == asALREADY_REGISTERED)
	{
		return;
	}
}

FAngelscriptBinds::FNamespace::FNamespace(FBindString Name)
{
	auto& Manager = FAngelscriptEngine::Get();
	PrevNamespace.SetDynamic(Manager.Engine->GetDefaultNamespace());
	Manager.Engine->SetDefaultNamespace(Name.ToCString());
}

FAngelscriptBinds::FNamespace::~FNamespace()
{
	auto& Manager = FAngelscriptEngine::Get();
	Manager.Engine->SetDefaultNamespace(PrevNamespace.ToCString());
}

asITypeInfo* FAngelscriptBinds::GetTypeInfo()
{
	if (ScriptType == nullptr && !ClassName.IsEmpty())
	{
		auto& Manager = FAngelscriptEngine::Get();
		ScriptType = Manager.Engine->GetTypeInfoByName(ClassName.ToCString());
	}
	return ScriptType;
}

bool FAngelscriptBinds::HasMethod(const FString& MethodName)
{
	auto* Type = GetTypeInfo();
	if (!ensure(Type != nullptr))
		return false;
	return Type->GetMethodByName(TCHAR_TO_ANSI(*MethodName)) != nullptr;
}

bool FAngelscriptBinds::HasGetter(const FString& PropertyName)
{
	TArray<ANSICHAR, TInlineAllocator<64>> FuncName;
	FuncName.SetNumUninitialized(PropertyName.Len() + 4);
	FuncName[0] = 'G';
	FuncName[1] = 'e';
	FuncName[2] = 't';
	for (int32 i = 0, Count = PropertyName.Len(); i < Count; ++i)
		FuncName[i + 3] = (ANSICHAR)PropertyName[i];
	FuncName[FuncName.Num() - 1] = '\0';

	auto* Type = GetTypeInfo();
	if (!ensure(Type != nullptr))
		return false;
	return Type->GetMethodByName(&FuncName[0]) != nullptr;
}

bool FAngelscriptBinds::HasSetter(const FString& PropertyName)
{
	TArray<ANSICHAR, TInlineAllocator<64>> FuncName;
	FuncName.SetNumUninitialized(PropertyName.Len() + 4);
	FuncName[0] = 'S';
	FuncName[1] = 'e';
	FuncName[2] = 't';
	for (int32 i = 0, Count = PropertyName.Len(); i < Count; ++i)
		FuncName[i + 3] = (ANSICHAR)PropertyName[i];
	FuncName[FuncName.Num() - 1] = '\0';

	auto* Type = GetTypeInfo();
	if (!ensure(Type != nullptr))
		return false;
	return Type->GetMethodByName(&FuncName[0]) != nullptr;
}
