#include "AngelscriptBinds.h"
#include "AngelscriptBindsInternal.h"

#include "AngelscriptEngine.h"
#include "AngelscriptDocs.h"
#include "AngelscriptMemoryTags.h"
#include "AngelscriptPerformanceStats.h"
#include "HAL/MallocLeakDetection.h"
#include "AngelscriptSettings.h"
#include "Interfaces/IPluginManager.h"
#include "Modules/ModuleManager.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
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

namespace UE::Angelscript::Private
{
	static bool IsValidBindPhase(const EAngelscriptBindPhase Phase)
	{
		return Phase >= EAngelscriptBindPhase::TypeDeclarations && Phase <= EAngelscriptBindPhase::Finalization;
	}

	static const TCHAR* GetBindPhaseName(const EAngelscriptBindPhase Phase)
	{
		switch (Phase)
		{
		case EAngelscriptBindPhase::TypeDeclarations:
			return TEXT("TypeDeclarations");
		case EAngelscriptBindPhase::TypeInfrastructure:
			return TEXT("TypeInfrastructure");
		case EAngelscriptBindPhase::ManualBindings:
			return TEXT("ManualBindings");
		case EAngelscriptBindPhase::GeneratedBindings:
			return TEXT("GeneratedBindings");
		case EAngelscriptBindPhase::ReflectionBindings:
			return TEXT("ReflectionBindings");
		case EAngelscriptBindPhase::PostReflectionBindings:
			return TEXT("PostReflectionBindings");
		case EAngelscriptBindPhase::Finalization:
			return TEXT("Finalization");
		default:
			return TEXT("Invalid");
		}
	}

	static FString DescribeBindRecord(const FAngelscriptBindRecord& Record)
	{
		return FString::Printf(
			TEXT("owner='%s', bind='%s', phase='%s', source='%s:%d'"),
			*Record.OwnerModule.ToString(),
			*Record.BindName.ToString(),
			GetBindPhaseName(Record.Phase),
			Record.SourceFile != nullptr ? ANSI_TO_TCHAR(Record.SourceFile) : TEXT("<null>"),
			Record.SourceLine);
	}

	bool FAngelscriptBindCollection::Append(FAngelscriptBindRecord Record, FString& OutDiagnostic)
	{
		if (bSealed)
		{
			if (LateRegistrationFailureDiagnostic.IsEmpty())
			{
				LateRegistrationFailureDiagnostic = FString::Printf(
					TEXT("Cannot append an AngelScript bind after collection finalization; restart the process to load %s."),
					*DescribeBindRecord(Record));
			}
			OutDiagnostic = LateRegistrationFailureDiagnostic;
			#if WITH_DEV_AUTOMATION_TESTS
			FAngelscriptBindExecutionObservation::RecordLateRegistration(OutDiagnostic);
			#endif
			return false;
		}

		Records.Add(MoveTemp(Record));
		OutDiagnostic.Reset();
		return true;
	}

	bool FAngelscriptBindCollection::Finalize(FString& OutDiagnostic)
	{
		if (!LateRegistrationFailureDiagnostic.IsEmpty())
		{
			OutDiagnostic = LateRegistrationFailureDiagnostic;
			return false;
		}
		if (bSealed)
		{
			OutDiagnostic.Reset();
			return true;
		}

		#if WITH_DEV_AUTOMATION_TESTS
		const double FinalizationStartTime = FPlatformTime::Seconds();
		bool bFinalizationSucceeded = false;
		ON_SCOPE_EXIT
		{
			TArray<int32> PhaseProviderCounts;
			PhaseProviderCounts.Init(0, static_cast<int32>(EAngelscriptBindPhase::Finalization) + 1);
			TArray<FName> ProviderOrder;
			ProviderOrder.Reserve(Records.Num());
			for (const FAngelscriptBindRecord& ObservedRecord : Records)
			{
				const int32 PhaseIndex = static_cast<int32>(ObservedRecord.Phase);
				if (PhaseProviderCounts.IsValidIndex(PhaseIndex))
				{
					++PhaseProviderCounts[PhaseIndex];
				}
				ProviderOrder.Add(ObservedRecord.BindName);
			}
			FAngelscriptBindExecutionObservation::RecordCollectionFinalization(
				bFinalizationSucceeded,
				PhaseProviderCounts,
				ProviderOrder,
				FPlatformTime::Seconds() - FinalizationStartTime,
				OutDiagnostic);
		};
		#endif

		for (const FAngelscriptBindRecord& Record : Records)
		{
			if (Record.OwnerModule.IsNone())
			{
				OutDiagnostic = FString::Printf(TEXT("AngelScript bind owner module is required: %s."), *DescribeBindRecord(Record));
				return false;
			}
			if (Record.BindName.IsNone())
			{
				OutDiagnostic = FString::Printf(TEXT("AngelScript bind name is required: %s."), *DescribeBindRecord(Record));
				return false;
			}
			if (!IsValidBindPhase(Record.Phase))
			{
				OutDiagnostic = FString::Printf(TEXT("AngelScript bind phase is invalid: %s."), *DescribeBindRecord(Record));
				return false;
			}
			if (Record.SourceFile == nullptr || *Record.SourceFile == '\0' || Record.SourceLine <= 0)
			{
				OutDiagnostic = FString::Printf(TEXT("AngelScript bind source provenance is required: %s."), *DescribeBindRecord(Record));
				return false;
			}
			if (Record.Callback == nullptr)
			{
				OutDiagnostic = FString::Printf(TEXT("AngelScript bind callback is required: %s."), *DescribeBindRecord(Record));
				return false;
			}
		}

		Records.Sort([](const FAngelscriptBindRecord& Left, const FAngelscriptBindRecord& Right)
		{
			if (Left.Phase != Right.Phase)
			{
				return static_cast<uint8>(Left.Phase) < static_cast<uint8>(Right.Phase);
			}
			if (Left.OwnerModule != Right.OwnerModule)
			{
				return Left.OwnerModule.LexicalLess(Right.OwnerModule);
			}
			if (Left.BindName != Right.BindName)
			{
				return Left.BindName.LexicalLess(Right.BindName);
			}

			const int32 SourceFileComparison = FCStringAnsi::Strcmp(Left.SourceFile, Right.SourceFile);
			if (SourceFileComparison != 0)
			{
				return SourceFileComparison < 0;
			}
			return Left.SourceLine < Right.SourceLine;
		});

		for (int32 RecordIndex = 1; RecordIndex < Records.Num(); ++RecordIndex)
		{
			const FAngelscriptBindRecord& Previous = Records[RecordIndex - 1];
			const FAngelscriptBindRecord& Current = Records[RecordIndex];
			if (Previous.OwnerModule == Current.OwnerModule && Previous.BindName == Current.BindName && Previous.Phase == Current.Phase)
			{
				OutDiagnostic = FString::Printf(
					TEXT("Duplicate AngelScript bind identity (owner='%s', bind='%s', phase='%s') declared at '%s:%d' and '%s:%d'."),
					*Current.OwnerModule.ToString(),
					*Current.BindName.ToString(),
					GetBindPhaseName(Current.Phase),
					ANSI_TO_TCHAR(Previous.SourceFile),
					Previous.SourceLine,
					ANSI_TO_TCHAR(Current.SourceFile),
					Current.SourceLine);
				return false;
			}
		}

		bSealed = true;
		OutDiagnostic.Reset();
		#if WITH_DEV_AUTOMATION_TESTS
		bFinalizationSucceeded = true;
		#endif
		return true;
	}

	bool FAngelscriptBindCollection::PrepareForEngineInitialization(
		const TConstArrayView<FString> ModuleNames,
		const TFunctionRef<bool(FName, FString&)> LoadModule,
		FString& OutDiagnostic)
	{
		if (!LateRegistrationFailureDiagnostic.IsEmpty())
		{
			OutDiagnostic = LateRegistrationFailureDiagnostic;
			return false;
		}
		if (bSealed)
		{
			OutDiagnostic.Reset();
			return true;
		}
		if (bPreparing)
		{
			OutDiagnostic = TEXT("AngelScript bind collection preparation is already in progress.");
			return false;
		}

		TGuardValue<bool> PreparingGuard(bPreparing, true);
		for (const FString& RawModuleName : ModuleNames)
		{
			const FString ModuleNameString = RawModuleName.TrimStartAndEnd();
			if (ModuleNameString.IsEmpty())
			{
				continue;
			}

			const FName ModuleName(*ModuleNameString);
			FString LoadDiagnostic;
			if (!LoadModule(ModuleName, LoadDiagnostic))
			{
				OutDiagnostic = FString::Printf(
					TEXT("Failed to load generated AngelScript bind module '%s' before collection finalization: %s"),
					*ModuleName.ToString(),
					LoadDiagnostic.IsEmpty() ? TEXT("module loader returned failure") : *LoadDiagnostic);
				return false;
			}
		}

		return Finalize(OutDiagnostic);
	}

	bool FAngelscriptBindCollection::Execute(FAngelscriptBinds& Binds, FString& OutDiagnostic) const
	{
		return Execute(
			Binds,
			EAngelscriptBindPhase::TypeDeclarations,
			EAngelscriptBindPhase::Finalization,
			OutDiagnostic);
	}

	bool FAngelscriptBindCollection::Execute(
		FAngelscriptBinds& Binds,
		const EAngelscriptBindPhase FirstPhase,
		const EAngelscriptBindPhase LastPhase,
		FString& OutDiagnostic) const
	{
		AS_PERF_SCOPE_BINDS_EXECUTE_CALLBACKS();

		if (!bSealed)
		{
			OutDiagnostic = TEXT("Cannot execute AngelScript binds before collection finalization.");
			return false;
		}
		if (!LateRegistrationFailureDiagnostic.IsEmpty())
		{
			OutDiagnostic = LateRegistrationFailureDiagnostic;
			return false;
		}

		if (Binds.HasRegistrationFailure())
		{
			OutDiagnostic = Binds.GetRegistrationFailureDiagnostic();
			return false;
		}

		if (static_cast<uint8>(FirstPhase) > static_cast<uint8>(LastPhase))
		{
			OutDiagnostic = TEXT("Cannot execute an inverted AngelScript bind phase range.");
			return false;
		}

		#if WITH_DEV_AUTOMATION_TESTS
		bool bExecutionSucceeded = false;
		FAngelscriptBindExecutionObservation::BeginObservationPass(&Binds.GetTargetEngine());
		ON_SCOPE_EXIT
		{
			FAngelscriptBindExecutionObservation::EndObservationPass(
				bExecutionSucceeded,
				OutDiagnostic);
		};
		#endif

		for (const FAngelscriptBindRecord& Record : Records)
		{
			if (static_cast<uint8>(Record.Phase) < static_cast<uint8>(FirstPhase)
				|| static_cast<uint8>(Record.Phase) > static_cast<uint8>(LastPhase))
			{
				continue;
			}

			FAngelscriptBindState& BindState = Binds.GetTargetBindState();
			TGuardValue<FName> ActiveOwnerGuard(BindState.ActiveBindOwnerModule, Record.OwnerModule);
			TGuardValue<FName> ActiveProviderGuard(BindState.ActiveBindProvider, Record.BindName);
			TGuardValue<EAngelscriptBindPhase> ActivePhaseGuard(BindState.ActiveBindPhase, Record.Phase);
			TGuardValue<const ANSICHAR*> ActiveSourceFileGuard(BindState.ActiveBindSourceFile, Record.SourceFile);
			TGuardValue<int32> ActiveSourceLineGuard(BindState.ActiveBindSourceLine, Record.SourceLine);
			#if WITH_DEV_AUTOMATION_TESTS
			FAngelscriptBindExecutionObservation::BeginProvider(
				Record.OwnerModule,
				Record.BindName,
				Record.Phase,
				Record.SourceFile,
				Record.SourceLine);
			#endif
			Record.Callback(Binds);
			if (Binds.HasRegistrationFailure())
			{
				OutDiagnostic = Binds.GetRegistrationFailureDiagnostic();
				#if WITH_DEV_AUTOMATION_TESTS
				FAngelscriptBindExecutionObservation::EndProvider(false, OutDiagnostic);
				#endif
				return false;
			}
			#if WITH_DEV_AUTOMATION_TESTS
			FAngelscriptBindExecutionObservation::EndProvider(true);
			#endif
		}
		OutDiagnostic.Reset();
		#if WITH_DEV_AUTOMATION_TESTS
		bExecutionSucceeded = true;
		#endif
		return true;
	}
}

static UE::Angelscript::Private::FAngelscriptBindCollection& GetRegisteredBindCollection()
{
	static UE::Angelscript::Private::FAngelscriptBindCollection Collection;
	return Collection;
}

TArray<FAngelscriptBindMetadata> FAngelscriptBind::GetRegisteredBindMetadata()
{
	TArray<FAngelscriptBindMetadata> Metadata;
	const TConstArrayView<FAngelscriptBindRecord> Records =
		GetRegisteredBindCollection().GetRecords();
	Metadata.Reserve(Records.Num());
	for (const FAngelscriptBindRecord& Record : Records)
	{
		FAngelscriptBindMetadata& Entry = Metadata.AddDefaulted_GetRef();
		Entry.OwnerModule = Record.OwnerModule;
		Entry.BindName = Record.BindName;
		Entry.Phase = Record.Phase;
		Entry.SourceFile = Record.SourceFile != nullptr
			? ANSI_TO_TCHAR(Record.SourceFile)
			: FString();
		Entry.SourceLine = Record.SourceLine;
	}
	return Metadata;
}

#if WITH_DEV_AUTOMATION_TESTS
static int32 PrepareInvocationCountForTesting = 0;
#endif

static FAngelscriptBindRecord MakeDirectBindRecord(
	const FName BindName,
	const EAngelscriptBindPhase Phase,
	const FAngelscriptBindCallback Callback,
	const ANSICHAR* OwnerModule,
	const ANSICHAR* SourceFile,
	const int32 SourceLine)
{
	FAngelscriptBindRecord Record;
	Record.OwnerModule = FName(OwnerModule != nullptr ? ANSI_TO_TCHAR(OwnerModule) : TEXT(""));
	Record.BindName = BindName;
	Record.Phase = Phase;
	Record.SourceFile = SourceFile;
	Record.SourceLine = SourceLine;
	Record.Callback = Callback;
	return Record;
}

FAngelscriptBind::FAngelscriptBind(
	const FName BindName,
	const EAngelscriptBindPhase Phase,
	const FAngelscriptBindCallback Callback,
	const ANSICHAR* OwnerModule,
	const ANSICHAR* SourceFile,
	const int32 SourceLine)
{
	FString Diagnostic;
	if (!GetRegisteredBindCollection().Append(MakeDirectBindRecord(BindName, Phase, Callback, OwnerModule, SourceFile, SourceLine), Diagnostic))
	{
		UE_LOG(Angelscript, Error, TEXT("%s"), *Diagnostic);
	}
}

#if WITH_DEV_AUTOMATION_TESTS
FAngelscriptBind::FAngelscriptBind(
	UE::Angelscript::Private::FAngelscriptBindCollection& Collection,
	const FName BindName,
	const EAngelscriptBindPhase Phase,
	const FAngelscriptBindCallback Callback,
	const ANSICHAR* OwnerModule,
	const ANSICHAR* SourceFile,
	const int32 SourceLine)
{
	FString Diagnostic;
	if (!Collection.Append(MakeDirectBindRecord(BindName, Phase, Callback, OwnerModule, SourceFile, SourceLine), Diagnostic))
	{
		UE_LOG(Angelscript, Error, TEXT("%s"), *Diagnostic);
	}
}
#endif

bool FAngelscriptBind::FinalizeRegisteredBinds(FString& OutDiagnostic)
{
	return GetRegisteredBindCollection().Finalize(OutDiagnostic);
}

bool FAngelscriptBind::PrepareForEngineInitialization(FString& OutDiagnostic)
{
#if WITH_DEV_AUTOMATION_TESTS
	++PrepareInvocationCountForTesting;
#endif

	if (!IsInGameThread())
	{
		OutDiagnostic = TEXT("AngelScript bind modules must load and finalize on the Game Thread before engine initialization.");
		return false;
	}

	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("Angelscript"));
	if (!Plugin.IsValid())
	{
		OutDiagnostic = TEXT("Cannot locate the Angelscript plugin while preparing direct binds.");
		return false;
	}

	TArray<FString> ModuleNames;
	const FString CachePath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("BindModules.Cache"));
	if (IFileManager::Get().FileExists(*CachePath) && !FFileHelper::LoadFileToStringArray(ModuleNames, *CachePath))
	{
		OutDiagnostic = FString::Printf(TEXT("Failed to read generated AngelScript bind module cache '%s'."), *CachePath);
		return false;
	}

	return GetRegisteredBindCollection().PrepareForEngineInitialization(
		ModuleNames,
		[](const FName ModuleName, FString& LoadDiagnostic)
		{
			if (FModuleManager::Get().LoadModule(ModuleName) == nullptr)
			{
				LoadDiagnostic = TEXT("Unreal ModuleManager could not load the module");
				return false;
			}
			return true;
		},
		OutDiagnostic);
}

#if WITH_DEV_AUTOMATION_TESTS
void FAngelscriptBind::ResetPrepareInvocationCountForTesting()
{
	PrepareInvocationCountForTesting = 0;
}

int32 FAngelscriptBind::GetPrepareInvocationCountForTesting()
{
	return PrepareInvocationCountForTesting;
}

const void* FAngelscriptBind::GetRegisteredCollectionIdentityForTesting()
{
	return &GetRegisteredBindCollection();
}

int32 FAngelscriptBind::GetRegisteredBindCountForTesting()
{
	return GetRegisteredBindCollection().GetRecords().Num();
}

bool FAngelscriptBind::IsRegisteredCollectionSealedForTesting()
{
	return GetRegisteredBindCollection().IsSealed();
}
#endif

bool FAngelscriptBind::ExecuteRegisteredBinds(FAngelscriptBinds& Binds, FString& OutDiagnostic)
{
	return GetRegisteredBindCollection().Execute(Binds, OutDiagnostic);
}

bool FAngelscriptBind::ExecuteRegisteredBindPhases(
	FAngelscriptBinds& Binds,
	const EAngelscriptBindPhase FirstPhase,
	const EAngelscriptBindPhase LastPhase,
	FString& OutDiagnostic)
{
	return GetRegisteredBindCollection().Execute(Binds, FirstPhase, LastPhase, OutDiagnostic);
}

#if WITH_DEV_AUTOMATION_TESTS
static void CDECL InjectedDirectBindPublicationFailure()
{
}

static void ExecuteDirectBindArchitectureProbe(FAngelscriptBinds& Binds)
{
	if (Binds.GetTargetEngine().ShouldInjectDirectBindFailureForTesting())
	{
		Binds.BindGlobalFunctionForTarget(
			"void __InjectedDirectBindPublicationFailure()",
			&InjectedDirectBindPublicationFailure);
		Binds.BindGlobalFunctionForTarget(
			"void __InjectedDirectBindPublicationFailure()",
			&InjectedDirectBindPublicationFailure);
		return;
	}
	++Binds.GetTargetBindState().DirectCallbackExecutionCountForTesting;
}

AS_FORCE_LINK const FAngelscriptBind Bind_DirectBindArchitectureProbe(
	TEXT("DirectBindArchitectureProbe"),
	EAngelscriptBindPhase::Finalization,
	&ExecuteDirectBindArchitectureProbe);
#endif

static FAngelscriptBindState& GetBindState()
{
	FAngelscriptEngine& Engine = FAngelscriptEngine::Get();
	FAngelscriptBindState* State = Engine.GetBindState();
	checkf(State != nullptr, TEXT("AngelScript bind state is unavailable for the current engine."));
	return *State;
}

bool FAngelscriptBoundFunction::IsValid() const
{
	return TargetEngine != nullptr && FunctionId >= 0 && GetFunction() != nullptr;
}

FAngelscriptEngine& FAngelscriptBoundFunction::GetTargetEngine() const
{
	check(TargetEngine != nullptr);
	return *TargetEngine;
}

asIScriptFunction* FAngelscriptBoundFunction::GetFunction() const
{
	if (TargetEngine == nullptr || FunctionId < 0 || TargetEngine->GetScriptEngine() == nullptr)
	{
		return nullptr;
	}
	return TargetEngine->GetScriptEngine()->GetFunctionById(FunctionId);
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::EditorOnly(const bool bEditorOnly)
{
	if (asCScriptFunction* Function = static_cast<asCScriptFunction*>(GetFunction()))
	{
		Function->traits.SetTrait(asEFuncTrait::asTRAIT_EDITOR_ONLY, bEditorOnly);
	}
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::Deprecated(const ANSICHAR* DeprecationMessage)
{
	if (asCScriptFunction* Function = static_cast<asCScriptFunction*>(GetFunction()))
	{
		Function->traits.SetTrait(asEFuncTrait::asTRAIT_DEPRECATED, true);
#if WITH_EDITOR
		Function->deprecationMessage = DeprecationMessage != nullptr ? DeprecationMessage : "";
#endif
	}
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::PropertyAccessor(const bool bPropertyAccessor)
{
	if (asCScriptFunction* Function = static_cast<asCScriptFunction*>(GetFunction()))
	{
		Function->SetProperty(bPropertyAccessor);
	}
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::GeneratedAccessor(const bool bGeneratedAccessor)
{
	if (asCScriptFunction* Function = static_cast<asCScriptFunction*>(GetFunction()))
	{
		Function->traits.SetTrait(asEFuncTrait::asTRAIT_GENERATED_FUNCTION, bGeneratedAccessor);
	}
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::NoDiscard(const bool bNoDiscard)
{
	if (asCScriptFunction* Function = static_cast<asCScriptFunction*>(GetFunction()))
	{
		Function->traits.SetTrait(asEFuncTrait::asTRAIT_NODISCARD, bNoDiscard);
	}
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::WorldContext(const bool bRequiresWorldContext)
{
	if (asCScriptFunction* Function = static_cast<asCScriptFunction*>(GetFunction()))
	{
		Function->traits.SetTrait(asEFuncTrait::asTRAIT_USES_WORLDCONTEXT, bRequiresWorldContext);
	}
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::Callable(const bool bCallable)
{
	if (asCScriptFunction* Function = static_cast<asCScriptFunction*>(GetFunction()))
	{
		Function->traits.SetTrait(asEFuncTrait::asTRAIT_NOT_CALLABLE, !bCallable);
	}
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::ImplicitConstructor(const bool bImplicitConstructor)
{
	if (asCScriptFunction* Function = static_cast<asCScriptFunction*>(GetFunction()))
	{
		Function->traits.SetTrait(asEFuncTrait::asTRAIT_IMPLICITCONSTRUCTOR, bImplicitConstructor);
		if (asCObjectType* ObjectType = Function->objectType)
		{
			ObjectType->hasImplicitConstructors = bImplicitConstructor;
		}
	}
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::ForceConstArgumentExpressions(const bool bForceConst)
{
	if (asCScriptFunction* Function = static_cast<asCScriptFunction*>(GetFunction()))
	{
		Function->traits.SetTrait(asEFuncTrait::asTRAIT_FORCE_CONST_ARGUMENT_EXPRESSIONS, bForceConst);
	}
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::DeterminesOutputType(const int32 ArgumentIndex)
{
	if (asCScriptFunction* Function = static_cast<asCScriptFunction*>(GetFunction()))
	{
		Function->determinesOutputTypeArgumentIndex = static_cast<int8>(ArgumentIndex);
	}
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::PassScriptFunctionAsFirstParam()
{
	if (asCScriptFunction* Function = static_cast<asCScriptFunction*>(GetFunction()); Function != nullptr && Function->sysFuncIntf != nullptr)
	{
		Function->sysFuncIntf->passFirstParamMetaData = asEFirstParamMetaData::ScriptFunction;
	}
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::PassScriptObjectTypeAsFirstParam()
{
	if (asCScriptFunction* Function = static_cast<asCScriptFunction*>(GetFunction()); Function != nullptr && Function->sysFuncIntf != nullptr)
	{
		Function->sysFuncIntf->passFirstParamMetaData = asEFirstParamMetaData::ScriptObjectType;
	}
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::Documentation(FStringView Documentation, FStringView Category, UFunction* UnrealFunction)
{
#if WITH_EDITOR
	if (IsValid())
	{
		FAngelscriptDocs::AddUnrealDocumentation(GetTargetEngine(), FunctionId, Documentation, Category, UnrealFunction);
	}
#endif
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::NativeConstructor(const ANSICHAR* Name, const bool bTrivial, const ANSICHAR* CustomForm)
{
#if AS_CAN_GENERATE_JIT
	if (IsValid())
	{
		FScriptFunctionNativeForm::BindNativeConstructor(GetTargetEngine(), GetFunction(), Name, bTrivial, CustomForm);
	}
#endif
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::NativeDestructor(const ANSICHAR* Name, const bool bTrivial)
{
#if AS_CAN_GENERATE_JIT
	if (IsValid())
	{
		FScriptFunctionNativeForm::BindNativeDestructor(GetTargetEngine(), GetFunction(), Name, bTrivial);
	}
#endif
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::NativeAssignment(const ANSICHAR* Name, const bool bTrivial)
{
#if AS_CAN_GENERATE_JIT
	if (IsValid())
	{
		FScriptFunctionNativeForm::BindNativeAssignment(GetTargetEngine(), GetFunction(), Name, bTrivial);
	}
#else
	(void)Name;
	(void)bTrivial;
#endif
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::NativeUObjectCast(const FString& TargetType, const bool bGuaranteed)
{
#if AS_CAN_GENERATE_JIT
	if (IsValid())
	{
		FScriptFunctionNativeForm::BindNativeUObjectCast(GetTargetEngine(), GetFunction(), TargetType, bGuaranteed);
	}
#else
	(void)TargetType;
	(void)bGuaranteed;
#endif
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::NativeMethod(const ANSICHAR* Name, const bool bTrivial)
{
#if AS_CAN_GENERATE_JIT
	if (IsValid())
	{
		FScriptFunctionNativeForm::BindNativeMethod(GetTargetEngine(), GetFunction(), Name, bTrivial);
	}
#else
	(void)Name;
	(void)bTrivial;
#endif
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::NativeFunction(const ANSICHAR* Name, const bool bTrivial)
{
#if AS_CAN_GENERATE_JIT
	if (IsValid())
	{
		FScriptFunctionNativeForm::BindNativeFunction(GetTargetEngine(), GetFunction(), Name, bTrivial);
	}
#else
	(void)Name;
	(void)bTrivial;
#endif
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::NativeFunctionHeader(const ANSICHAR* Name, const ANSICHAR* Header, const bool bTrivial)
{
#if AS_CAN_GENERATE_JIT
	if (IsValid())
	{
		FScriptFunctionNativeForm::BindNativeFunctionHeader(GetTargetEngine(), GetFunction(), Name, bTrivial, Header);
	}
#else
	(void)Name;
	(void)Header;
	(void)bTrivial;
#endif
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::NativeUFunction(UFunction* Function, const FString& Name, const bool bTrivial)
{
#if AS_CAN_GENERATE_JIT
	if (IsValid())
	{
		FScriptFunctionNativeForm::BindUFunction(GetTargetEngine(), GetFunction(), Function, Name, bTrivial);
	}
#else
	(void)Function;
	(void)Name;
	(void)bTrivial;
#endif
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::NativeTArrayIndex()
{
#if AS_CAN_GENERATE_JIT
	if (IsValid())
	{
		FScriptFunctionNativeForm::BindTArrayIndex(GetTargetEngine(), GetFunction());
	}
#endif
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::NativeTArrayIteratorCreate()
{
#if AS_CAN_GENERATE_JIT
	if (IsValid())
	{
		FScriptFunctionNativeForm::BindTArrayIteratorCreate(GetTargetEngine(), GetFunction());
	}
#endif
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::NativeTArrayIteratorProceed()
{
#if AS_CAN_GENERATE_JIT
	if (IsValid())
	{
		FScriptFunctionNativeForm::BindTArrayIteratorProceed(GetTargetEngine(), GetFunction());
	}
#endif
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::NativeTemplateInstantiatedCall(
	const ANSICHAR* Name,
	const bool bTrivial,
	const bool bNeedsCompare,
	const bool bNeedsCopy)
{
#if AS_CAN_GENERATE_JIT
	if (IsValid())
	{
		FScriptFunctionNativeForm::BindTemplateInstantiatedCall(
			GetTargetEngine(),
			GetFunction(),
			Name,
			bTrivial,
			bNeedsCompare,
			bNeedsCopy);
	}
#else
	(void)Name;
	(void)bTrivial;
	(void)bNeedsCompare;
	(void)bNeedsCopy;
#endif
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::NativeDelegateExecute()
{
#if AS_CAN_GENERATE_JIT
	if (IsValid())
	{
		FScriptFunctionNativeForm::BindDelegateExecute(GetTargetEngine(), GetFunction());
	}
#endif
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::NativeMulticastExecute()
{
#if AS_CAN_GENERATE_JIT
	if (IsValid())
	{
		FScriptFunctionNativeForm::BindMulticastExecute(GetTargetEngine(), GetFunction());
	}
#endif
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::NativeEventFunctionExecute()
{
#if AS_CAN_GENERATE_JIT
	if (IsValid())
	{
		FScriptFunctionNativeForm::BindEventFunctionExecute(GetTargetEngine(), GetFunction());
	}
#endif
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::NativePushArgument()
{
#if AS_CAN_GENERATE_JIT
	if (IsValid())
	{
		FScriptFunctionNativeForm::BindPushArg(GetTargetEngine(), GetFunction());
	}
#endif
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::NativePushArgumentRef()
{
#if AS_CAN_GENERATE_JIT
	if (IsValid())
	{
		FScriptFunctionNativeForm::BindPushArgRef(GetTargetEngine(), GetFunction());
	}
#endif
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::CompileOutEntirely()
{
	if (asCScriptFunction* Function = static_cast<asCScriptFunction*>(GetFunction()))
	{
		Function->compileOutType = asECompileOutType::CompileOutEntirely;
	}
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::CompileOutAsMethodChain()
{
	if (asCScriptFunction* Function = static_cast<asCScriptFunction*>(GetFunction()))
	{
		Function->compileOutType = asECompileOutType::CompileOutAsMethodChain;
	}
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::CompileOutInTest()
{
	if (asCScriptFunction* Function = static_cast<asCScriptFunction*>(GetFunction()))
	{
		if (UE_BUILD_TEST || UE_BUILD_SHIPPING || (WITH_EDITOR && TargetEngine->bSimulateCooked))
		{
			Function->compileOutType = asECompileOutType::CompileOutEntirely;
		}
		if (TargetEngine->ConfigSettings != nullptr && TargetEngine->ConfigSettings->bForceConstWithinDevelopmentOnlyFunctions)
		{
			Function->traits.SetTrait(asEFuncTrait::asTRAIT_FORCE_CONST_ARGUMENT_EXPRESSIONS, true);
		}
	}
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::CompileOutIfNoLog()
{
	return CompileOutInTest();
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::CompileOutAsEnsure()
{
	if (asCScriptFunction* Function = static_cast<asCScriptFunction*>(GetFunction()))
	{
		Function->traits.SetTrait(asEFuncTrait::asTRAIT_NODISCARD, true);
		if (UE_BUILD_SHIPPING || (WITH_EDITOR && TargetEngine->bSimulateCooked))
		{
			Function->compileOutType = asECompileOutType::ReplaceWithFirstParam;
		}
	}
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::CompileOutAsCheck()
{
	if (asCScriptFunction* Function = static_cast<asCScriptFunction*>(GetFunction()))
	{
		if (UE_BUILD_SHIPPING || (WITH_EDITOR && TargetEngine->bSimulateCooked))
		{
			Function->compileOutType = asECompileOutType::CompileOutEntirely;
		}
		if (TargetEngine->ConfigSettings != nullptr && TargetEngine->ConfigSettings->bForceConstWithinDevelopmentOnlyFunctions)
		{
			Function->traits.SetTrait(asEFuncTrait::asTRAIT_FORCE_CONST_ARGUMENT_EXPRESSIONS, true);
		}
	}
	return *this;
}

FAngelscriptBoundFunction& FAngelscriptBoundFunction::ReplaceWithFirstArgInTest()
{
	if (asCScriptFunction* Function = static_cast<asCScriptFunction*>(GetFunction()))
	{
		if (UE_BUILD_TEST || UE_BUILD_SHIPPING || (WITH_EDITOR && TargetEngine->bSimulateCooked))
		{
			Function->compileOutType = asECompileOutType::ReplaceWithFirstParam;
		}
	}
	return *this;
}

bool FAngelscriptBoundProperty::IsValid() const
{
	if (TargetEngine == nullptr || PropertyId < 0 || TargetEngine->GetScriptEngine() == nullptr)
	{
		return false;
	}
	if (!bGlobal)
	{
		return true;
	}
	return static_cast<asUINT>(PropertyId) < TargetEngine->Engine->globalProperties.GetLength()
		&& TargetEngine->Engine->globalProperties[PropertyId] != nullptr;
}

FAngelscriptEngine& FAngelscriptBoundProperty::GetTargetEngine() const
{
	check(TargetEngine != nullptr);
	return *TargetEngine;
}

FAngelscriptBoundProperty& FAngelscriptBoundProperty::PureConstant(const asQWORD ConstantValue)
{
	if (IsValid() && bGlobal)
	{
		asCGlobalProperty* Property = TargetEngine->Engine->globalProperties[PropertyId];
		Property->isPureConstant = true;
		Property->storage = ConstantValue;
	}
	return *this;
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

TMap<UClass*, TMap<FString, FAngelscriptFunctionBinding>>& FAngelscriptBinds::GetClassFunctionBindings()
{
	return GetBindState().ClassFunctionBindings;
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

const FAngelscriptRegisteredFunctionProvenance*
FAngelscriptBinds::FindFunctionProvenance(const int32 FunctionId)
{
	return GetBindState().FunctionProvenance.Find(FunctionId);
}

const FAngelscriptRegisteredFunctionProvenance*
FAngelscriptBinds::FindFunctionProvenance(
	const asIScriptFunction& Function)
{
	if (const FAngelscriptRegisteredFunctionProvenance* ByPointer =
		GetBindState().FunctionProvenanceByPointer.Find(&Function))
	{
		return ByPointer;
	}
	return FindFunctionProvenance(Function.GetId());
}

bool FAngelscriptBinds::ShouldSkipBlueprintCallableFunction(const UFunction* Function)
{
	if (Function == nullptr)
	{
		return true;
	}

	if (!Function->HasAnyFunctionFlags(FUNC_Native))
	{
		return true;
	}

#if WITH_EDITORONLY_DATA
	// UFunction metadata is editor-only and stripped from cooked builds. In packaged
	// builds these gates fall through; the bind database baked at cook time already
	// reflects the editor-time decisions.
	static const FName NAME_Function_NotInAngelscript(TEXT("NotInAngelscript"));
	static const FName NAME_Function_BlueprintInternalUseOnly(TEXT("BlueprintInternalUseOnly"));
	static const FName NAME_Function_UsableInAngelscript(TEXT("UsableInAngelscript"));

	if (Function->HasMetaData(NAME_Function_NotInAngelscript))
	{
		return true;
	}

	if (Function->HasMetaData(NAME_Function_BlueprintInternalUseOnly) && !Function->HasMetaData(NAME_Function_UsableInAngelscript))
	{
		return true;
	}
#endif

	if (const UClass* OwningClass = Function->GetOuterUClass())
	{
		if (OwningClass == UActorComponent::StaticClass() && Function->GetFName() == FName(TEXT("GetOwner")))
		{
			return true;
		}
	}

	return false;
}

FAngelscriptBinds FAngelscriptBinds::ReferenceClassForTarget(FBindString Name, UClass* UnrealClass) const
{
	if (HasRegistrationFailure())
	{
		return FAngelscriptBinds(GetTargetEngine(), Name);
	}

	if (UnrealClass == nullptr)
	{
		FAngelscriptBinds Binds(GetTargetEngine(), Name);
		Binds.RecordRegistrationFailure(TEXT("reference class"), Name, asINVALID_ARG);
		return Binds;
	}

	constexpr asQWORD ReferenceFlags = asOBJ_REF | asOBJ_NOCOUNT | asOBJ_IMPLICIT_HANDLE;
	asITypeInfo* ExistingType = GetTargetScriptEngine().GetTypeInfoByName(Name.ToCString());
	if (ExistingType != nullptr)
	{
		FAngelscriptBinds Binds(GetTargetEngine(), Name);
		Binds.ScriptType = ExistingType;

		constexpr asQWORD ProviderSemanticFlags = asOBJ_EDITOR_ONLY | asOBJ_DISALLOW_INSTANTIATION;
		const asQWORD ExistingRegistrationFlags = ExistingType->GetFlags() & ~ProviderSemanticFlags;
		if (ExistingRegistrationFlags != ReferenceFlags
			|| ExistingType->GetSize() != UnrealClass->GetStructureSize())
		{
			Binds.RecordRegistrationFailure(
				TEXT("reference class compatibility"),
				Name,
				asALREADY_REGISTERED);
			Binds.ScriptType = nullptr;
			return Binds;
		}

		if (ExistingType->alignment != UnrealClass->GetMinAlignment())
		{
			Binds.RecordRegistrationFailure(
				TEXT("reference class alignment compatibility"),
				Name,
				asALREADY_REGISTERED);
			Binds.ScriptType = nullptr;
			return Binds;
		}

		void* AssociatedClass = ExistingType->GetUserData();
		if (AssociatedClass != nullptr && AssociatedClass != UnrealClass)
		{
			Binds.RecordRegistrationFailure(
				TEXT("reference class association compatibility"),
				Name,
				asALREADY_REGISTERED);
			Binds.ScriptType = nullptr;
			return Binds;
		}

		ExistingType->SetUserData(UnrealClass);
		return Binds;
	}

	FAngelscriptBinds Binds(
		GetTargetEngine(),
		Name,
		ReferenceFlags,
		UnrealClass->GetStructureSize());
	if (Binds.ScriptType == nullptr)
	{
		return Binds;
	}

	Binds.ScriptType->alignment = UnrealClass->GetMinAlignment();
	Binds.ScriptType->SetUserData(UnrealClass);
	return Binds;
}

FAngelscriptBinds FAngelscriptBinds::ExistingClassForTarget(FBindString Name) const
{
	return FAngelscriptBinds(GetTargetEngine(), Name);
}

FAngelscriptBinds::FAngelscriptBinds(FAngelscriptEngine& InTargetEngine)
	: TargetEngine(&InTargetEngine)
{
}

FAngelscriptEngine& FAngelscriptBinds::GetTargetEngine() const
{
	check(TargetEngine != nullptr);
	return *TargetEngine;
}

asIScriptEngine& FAngelscriptBinds::GetTargetScriptEngine() const
{
	asIScriptEngine* ScriptEngine = GetTargetEngine().GetScriptEngine();
	check(ScriptEngine != nullptr);
	return *ScriptEngine;
}

FAngelscriptBindState& FAngelscriptBinds::GetTargetBindState() const
{
	FAngelscriptBindState* BindState = GetTargetEngine().GetBindState();
	check(BindState != nullptr);
	return *BindState;
}

FAngelscriptTypeDatabase& FAngelscriptBinds::GetTargetTypeDatabase() const
{
	FAngelscriptTypeDatabase* TypeDatabase = GetTargetEngine().GetTypeDatabase();
	check(TypeDatabase != nullptr);
	return *TypeDatabase;
}

FAngelscriptBindDatabase& FAngelscriptBinds::GetTargetBindDatabase() const
{
	FAngelscriptBindDatabase* BindDatabase = GetTargetEngine().GetBindDatabase();
	check(BindDatabase != nullptr);
	return *BindDatabase;
}

TArray<FToStringType>& FAngelscriptBinds::GetTargetToStringList() const
{
	TArray<FToStringType>* ToStringList = GetTargetEngine().GetToStringList();
	check(ToStringList != nullptr);
	return *ToStringList;
}

FBlueprintEventSignatureRegistry& FAngelscriptBinds::GetTargetBlueprintEventSignatureRegistry() const
{
	FBlueprintEventSignatureRegistry* Registry = GetTargetEngine().GetBlueprintEventSignatureRegistry();
	check(Registry != nullptr);
	return *Registry;
}

void FAngelscriptBinds::RegisterTypeForTarget(TSharedRef<FAngelscriptType> Type) const
{
	FAngelscriptType::Register(GetTargetTypeDatabase(), Type);
}

void FAngelscriptBinds::RegisterTypeFinderForTarget(FAngelscriptType::FTypeFinder Finder) const
{
	FAngelscriptType::RegisterTypeFinder(GetTargetTypeDatabase(), MoveTemp(Finder));
}

void FAngelscriptBinds::RegisterFunctionBindingForTarget(
	UClass* Class,
	const FString& Name,
	const FAngelscriptFunctionBinding& Binding) const
{
	if (Class == nullptr)
	{
		return;
	}

	TMap<FString, FAngelscriptFunctionBinding>& FunctionMap = GetTargetBindState().ClassFunctionBindings.FindOrAdd(Class);
	if (FAngelscriptFunctionBinding* ExistingBinding = FunctionMap.Find(Name))
	{
		if (!ExistingBinding->FunctionPointer.IsBound()
			&& !ExistingBinding->bReflectiveFallbackBound
			&& Binding.FunctionPointer.IsBound())
		{
			*ExistingBinding = Binding;
		}
		return;
	}

	FunctionMap.Add(Name, Binding);
}

void FAngelscriptBinds::RegisterGeneratedFunctionBindingForTarget(
	UClass* Class,
	const FString& Name,
	FAngelscriptFunctionBinding Binding) const
{
	Binding.Origin = EAngelscriptFunctionBindingOrigin::Generated;
	RegisterFunctionBindingForTarget(Class, Name, Binding);
}

bool FAngelscriptBinds::HasRegistrationFailure() const
{
	return GetTargetBindState().bDirectBindFailed;
}

const FString& FAngelscriptBinds::GetRegistrationFailureDiagnostic() const
{
	return GetTargetBindState().DirectBindFailureDiagnostic;
}

void FAngelscriptBinds::RecordRegistrationFailure(const TCHAR* Operation, const FBindString Declaration, const int32 Result)
{
	FAngelscriptBindState& BindState = GetTargetBindState();
	if (BindState.bDirectBindFailed)
	{
		return;
	}

	BindState.bDirectBindFailed = true;
	BindState.DirectBindFailureDiagnostic = FString::Printf(
		TEXT("Direct AngelScript %s registration failed with result %d (owner='%s', bind='%s', phase='%s', source='%s:%d', declaration='%s')."),
		Operation,
		Result,
		*BindState.ActiveBindOwnerModule.ToString(),
		*BindState.ActiveBindProvider.ToString(),
		UE::Angelscript::Private::GetBindPhaseName(BindState.ActiveBindPhase),
		BindState.ActiveBindSourceFile != nullptr ? ANSI_TO_TCHAR(BindState.ActiveBindSourceFile) : TEXT("<unknown>"),
		BindState.ActiveBindSourceLine,
		*Declaration.ToFString());
}

FAngelscriptEngine& FAngelscriptBinds::ResolveTargetEngine() const
{
	check(TargetEngine != nullptr);
	return *TargetEngine;
}

FAngelscriptBinds FAngelscriptBinds::ValueClassForTarget(FBindString Name, FBindFlags Flags, int32 Size)
{
	if (HasRegistrationFailure())
	{
		return FAngelscriptBinds(GetTargetEngine(), Name);
	}

	auto AsFlags = asOBJ_VALUE | asOBJ_APP_CLASS | Flags.ExtraFlags;
	if (Flags.bPOD)
	{
		AsFlags |= asOBJ_POD;
	}
	if (Flags.bTemplate)
	{
		AsFlags |= asOBJ_TEMPLATE;
	}

	FAngelscriptBinds Binds(GetTargetEngine(), Name, AsFlags, Size);
	if (Flags.bTemplate && !Flags.TemplateType.IsEmpty() && Binds.ScriptType != nullptr)
	{
		int32 TemplatePos = Binds.ClassName.ToFString().Find(TEXT("<"));
		Binds.ClassName = Binds.ClassName.ToFString().Left(TemplatePos);
		Binds.ScriptType = GetTargetScriptEngine().GetTypeInfoByName(Binds.ClassName.ToCString());
		Binds.ClassName = Binds.ClassName.ToFString() + Flags.TemplateType.ToFString();
	}

	if (Flags.Alignment != -1 && Binds.ScriptType != nullptr)
	{
		if (Binds.ScriptType->alignment != 8 && Binds.ScriptType->alignment != Flags.Alignment)
		{
			Binds.RecordRegistrationFailure(TEXT("object type alignment"), Name, asINVALID_CONFIGURATION);
		}
		else
		{
			Binds.ScriptType->alignment = Flags.Alignment;
		}
	}
	return Binds;
}

FAngelscriptBinds::FAngelscriptBinds(FAngelscriptEngine& InTargetEngine, FBindString Name, const asQWORD Flags, const int32 Size)
	: TargetEngine(&InTargetEngine)
	, ClassName(Name)
{
	if (HasRegistrationFailure())
	{
		return;
	}

	asIScriptEngine& ScriptEngine = GetTargetScriptEngine();
	const int32 TypeId = ScriptEngine.RegisterObjectType(ClassName.ToCString(), Size, Flags);
	if (TypeId == asALREADY_REGISTERED)
	{
		ScriptType = ScriptEngine.GetTypeInfoByName(ClassName.ToCString());
		if (ScriptType == nullptr || ScriptType->GetSize() != Size || ScriptType->GetFlags() != Flags)
		{
			RecordRegistrationFailure(TEXT("object type compatibility"), ClassName, asALREADY_REGISTERED);
			ScriptType = nullptr;
		}
	}
	else if (TypeId < 0)
	{
		RecordRegistrationFailure(TEXT("object type"), ClassName, TypeId);
	}
	else
	{
		ScriptType = ScriptEngine.GetTypeInfoById(TypeId);
	}

	if (ScriptType == nullptr && TypeId >= 0 && ((Flags & asOBJ_TEMPLATE) == 0))
	{
		RecordRegistrationFailure(TEXT("object type lookup"), ClassName, asINVALID_TYPE);
	}
}

FAngelscriptBinds::FAngelscriptBinds(FAngelscriptEngine& InTargetEngine, FBindString Name)
	: TargetEngine(&InTargetEngine)
	, ClassName(Name)
{
}

FAngelscriptBoundFunction FAngelscriptBinds::GenericMethod(FBindString Signature, void(CDECL *Fun)(asIScriptGeneric*), void* UserData)
{
	if (TargetEngine != nullptr && HasRegistrationFailure())
	{
		return FAngelscriptBoundFunction(TargetEngine, asERROR);
	}
	auto& Manager = ResolveTargetEngine();
	int FunctionId = Manager.Engine->RegisterObjectMethod(ClassName.ToCString(), Signature.ToCString(), asFUNCTION(Fun), asCALL_GENERIC, nullptr);
	return OnBindForTarget(FunctionId, UserData, nullptr, TEXT("generic method"), Signature);
}

FAngelscriptBoundFunction FAngelscriptBinds::BindBehaviour(asEBehaviours Beh, FBindString Signature, asSFuncPtr Ptr, ASAutoCaller::FunctionCaller Caller)
{
	if (TargetEngine != nullptr && HasRegistrationFailure())
	{
		return FAngelscriptBoundFunction(TargetEngine, asERROR);
	}
	auto& Manager = ResolveTargetEngine();
	int FunctionId = Manager.Engine->RegisterObjectBehaviour(ClassName.ToCString(), Beh, Signature.ToCString(), Ptr, asCALL_THISCALL, *(asFunctionCaller*)&Caller);
	return OnBindForTarget(FunctionId, nullptr, nullptr, TEXT("object behaviour"), Signature);
}

FAngelscriptBoundFunction FAngelscriptBinds::OnBindForTarget(const int32 FunctionId, void* UserData, const FAngelscriptType::FBindParams* BindParams, const TCHAR* Operation, const FBindString Declaration)
{
	FAngelscriptEngine& Manager = ResolveTargetEngine();
	if (TargetEngine != nullptr && FunctionId < 0)
	{
		RecordRegistrationFailure(Operation, Declaration, FunctionId);
		return FAngelscriptBoundFunction(&Manager, FunctionId);
	}

	FAngelscriptBindState& BindState = GetTargetBindState();
	asCScriptFunction* ScriptFunction = static_cast<asCScriptFunction*>(Manager.Engine->GetFunctionById(FunctionId));
	if (ScriptFunction != nullptr)
	{
		FAngelscriptRegisteredFunctionProvenance& Provenance = BindState.FunctionProvenance.FindOrAdd(FunctionId);
		if (Provenance.Origin == EAngelscriptFunctionBindingOrigin::Unknown)
		{
			Provenance.Provider = BindState.ActiveBindProvider;
			if (!Provenance.Provider.IsNone())
			{
				const FString ProviderName = Provenance.Provider.ToString();
				if (ProviderName.StartsWith(TEXT("UHT.FunctionBinding.")))
				{
					Provenance.Origin = EAngelscriptFunctionBindingOrigin::Generated;
				}
				else if (!ProviderName.StartsWith(TEXT("Bind_BlueprintType")))
				{
					Provenance.Origin = EAngelscriptFunctionBindingOrigin::Manual;
				}
			}
		}
		BindState.FunctionProvenanceByPointer.Add(ScriptFunction, Provenance);
		if (UserData != nullptr)
		{
			ScriptFunction->SetUserData(UserData, 0);
		}
		if (BindParams != nullptr && BindParams->bProtected)
		{
			ScriptFunction->SetProtected(true);
		}
	}
	return FAngelscriptBoundFunction(&Manager, FunctionId);
}

FAngelscriptBoundFunction FAngelscriptBinds::BindExternBehaviour(asEBehaviours Beh, FBindString Signature, asSFuncPtr Ptr, ASAutoCaller::FunctionCaller Caller, void* UserData)
{
	if (TargetEngine != nullptr && HasRegistrationFailure())
	{
		return FAngelscriptBoundFunction(TargetEngine, asERROR);
	}
	auto& Manager = ResolveTargetEngine();
	int FunctionId = Manager.Engine->RegisterObjectBehaviour(ClassName.ToCString(), Beh, Signature.ToCString(), Ptr, asCALL_CDECL_OBJFIRST, *(asFunctionCaller*)&Caller);
	return OnBindForTarget(FunctionId, UserData, nullptr, TEXT("object behaviour"), Signature);
}

FAngelscriptBoundFunction FAngelscriptBinds::BindStaticBehaviour(asEBehaviours Beh, FBindString Signature, asSFuncPtr Ptr, ASAutoCaller::FunctionCaller Caller, void* UserData)
{
	if (TargetEngine != nullptr && HasRegistrationFailure())
	{
		return FAngelscriptBoundFunction(TargetEngine, asERROR);
	}
	auto& Manager = ResolveTargetEngine();
	int FunctionId = Manager.Engine->RegisterObjectBehaviour(ClassName.ToCString(), Beh, Signature.ToCString(), Ptr, asCALL_CDECL, *(asFunctionCaller*)&Caller);
	return OnBindForTarget(FunctionId, UserData, nullptr, TEXT("static behaviour"), Signature);
}

FAngelscriptBoundFunction FAngelscriptBinds::BindMethod(FBindString Signature, asSFuncPtr Ptr, ASAutoCaller::FunctionCaller Caller, void* UserData)
{
	if (TargetEngine != nullptr && HasRegistrationFailure())
	{
		return FAngelscriptBoundFunction(TargetEngine, asERROR);
	}
	auto& Manager = ResolveTargetEngine();
	int FunctionId = Manager.Engine->RegisterObjectMethod(ClassName.ToCString(), Signature.ToCString(), Ptr, asCALL_THISCALL, *(asFunctionCaller*)&Caller, nullptr);
	return OnBindForTarget(FunctionId, UserData, nullptr, TEXT("object method"), Signature);
}

FAngelscriptBoundFunction FAngelscriptBinds::BindExternMethod(FBindString Signature, asSFuncPtr Ptr, ASAutoCaller::FunctionCaller Caller, void* UserData)
{
	if (TargetEngine != nullptr && HasRegistrationFailure())
	{
		return FAngelscriptBoundFunction(TargetEngine, asERROR);
	}
	auto& Manager = ResolveTargetEngine();
	int FunctionId = Manager.Engine->RegisterObjectMethod(ClassName.ToCString(), Signature.ToCString(), Ptr, asCALL_CDECL_OBJFIRST, *(asFunctionCaller*)&Caller, nullptr);
	return OnBindForTarget(FunctionId, UserData, nullptr, TEXT("object method"), Signature);
}

FAngelscriptBoundFunction FAngelscriptBinds::BindExternMethod(FBindString Signature, asSFuncPtr Ptr, const FAngelscriptType::FBindParams& BindParams, ASAutoCaller::FunctionCaller Caller, void* UserData)
{
	if (TargetEngine != nullptr && HasRegistrationFailure())
	{
		return FAngelscriptBoundFunction(TargetEngine, asERROR);
	}
	auto& Manager = ResolveTargetEngine();
	const asUINT Access = asCScriptFunction::GenerateExposedType(BindParams.bCanEdit, BindParams.bCanRead, BindParams.bCanWrite);
	int FunctionId = Manager.Engine->RegisterObjectMethod(ClassName.ToCString(), Signature.ToCString(), Ptr, asCALL_CDECL_OBJFIRST, *(asFunctionCaller*)&Caller, nullptr, 0, false, Access);
	return OnBindForTarget(FunctionId, UserData, &BindParams, TEXT("object method"), Signature);
}

FAngelscriptBoundProperty FAngelscriptBinds::BindProperty(FBindString Signature, size_t Offset)
{
	if (TargetEngine != nullptr && HasRegistrationFailure())
	{
		return FAngelscriptBoundProperty(TargetEngine, asERROR, false);
	}
	auto& Manager = ResolveTargetEngine();
	const int32 PropertyId = Manager.Engine->RegisterObjectProperty(ClassName.ToCString(), Signature.ToCString(), Offset);
	if (TargetEngine != nullptr && PropertyId < 0)
	{
		RecordRegistrationFailure(TEXT("object property"), Signature, PropertyId);
	}
	return FAngelscriptBoundProperty(&Manager, PropertyId, false);
}

FAngelscriptBoundProperty FAngelscriptBinds::BindProperty(FBindString Signature, size_t Offset, const FAngelscriptType::FBindParams& BindParams)
{
	if (TargetEngine != nullptr && HasRegistrationFailure())
	{
		return FAngelscriptBoundProperty(TargetEngine, asERROR, false);
	}
	auto& Manager = ResolveTargetEngine();
	const asUINT Access = asCObjectProperty::GenerateExposedType(BindParams.bCanEdit, BindParams.bCanRead, BindParams.bCanWrite);
	const int32 PropertyId = Manager.Engine->RegisterObjectProperty(ClassName.ToCString(), Signature.ToCString(), Offset, 0, false, Access, BindParams.bProtected);
	if (TargetEngine != nullptr && PropertyId < 0)
	{
		RecordRegistrationFailure(TEXT("object property"), Signature, PropertyId);
	}
	return FAngelscriptBoundProperty(&Manager, PropertyId, false);
}

FAngelscriptBoundFunction FAngelscriptBinds::BindGlobalFunctionForTarget(FBindString Signature, asSFuncPtr Function, ASAutoCaller::FunctionCaller Caller, void* UserData)
{
	if (HasRegistrationFailure())
	{
		return FAngelscriptBoundFunction(TargetEngine, asERROR);
	}
	const int32 FunctionId = GetTargetScriptEngine().RegisterGlobalFunction(Signature.ToCString(), Function, asCALL_CDECL, *(asFunctionCaller*)&Caller, nullptr);
	return OnBindForTarget(FunctionId, UserData, nullptr, TEXT("global function"), Signature);
}

FAngelscriptBoundFunction FAngelscriptBinds::BindGlobalFunctionDirectForTarget(
	FBindString Signature,
	asSFuncPtr Function,
	asECallConvTypes CallConv,
	ASAutoCaller::FunctionCaller Caller,
	void* UserData)
{
	if (HasRegistrationFailure())
	{
		return FAngelscriptBoundFunction(TargetEngine, asERROR);
	}

	const int32 FunctionId = GetTargetScriptEngine().RegisterGlobalFunction(
		Signature.ToCString(),
		Function,
		CallConv,
		*(asFunctionCaller*)&Caller);
	return OnBindForTarget(FunctionId, UserData, nullptr, TEXT("global function"), Signature);
}

FAngelscriptBoundFunction FAngelscriptBinds::BindMethodDirectForTarget(
	FBindString ObjectTypeName,
	FBindString Signature,
	asSFuncPtr Function,
	asECallConvTypes CallConv,
	ASAutoCaller::FunctionCaller Caller,
	void* UserData)
{
	if (HasRegistrationFailure())
	{
		return FAngelscriptBoundFunction(TargetEngine, asERROR);
	}

	const int32 FunctionId = GetTargetScriptEngine().RegisterObjectMethod(
		ObjectTypeName.ToCString(),
		Signature.ToCString(),
		Function,
		CallConv,
		*(asFunctionCaller*)&Caller);
	return OnBindForTarget(FunctionId, UserData, nullptr, TEXT("object method"), Signature);
}

FAngelscriptBoundProperty FAngelscriptBinds::BindGlobalVariableForTarget(FBindString Signature, const void* Address)
{
	if (HasRegistrationFailure())
	{
		return FAngelscriptBoundProperty(TargetEngine, asERROR, true);
	}
	const int32 PropertyId = GetTargetScriptEngine().RegisterGlobalProperty(Signature.ToCString(), const_cast<void*>(Address));
	if (PropertyId < 0)
	{
		RecordRegistrationFailure(TEXT("global property"), Signature, PropertyId);
		return FAngelscriptBoundProperty(TargetEngine, PropertyId, true);
	}
	return FAngelscriptBoundProperty(TargetEngine, PropertyId, true);
}

FAngelscriptBoundFunction FAngelscriptBinds::BindGlobalGenericFunctionForTarget(
	FBindString Signature,
	void(CDECL* Function)(asIScriptGeneric*),
	void* UserData)
{
	if (HasRegistrationFailure())
	{
		return FAngelscriptBoundFunction(TargetEngine, asERROR);
	}

	const int32 FunctionId = GetTargetScriptEngine().RegisterGlobalFunction(
		Signature.ToCString(),
		asFUNCTION(Function),
		asCALL_GENERIC,
		nullptr,
		nullptr);
	return OnBindForTarget(FunctionId, UserData, nullptr, TEXT("global generic function"), Signature);
}

FAngelscriptBinds::FEnumBind::FEnumBind(FAngelscriptBinds& InTargetBinds, FBindString Name)
	: EnumName(Name)
	, TypeId(asERROR)
	, TargetBinds(&InTargetBinds)
{
	asIScriptEngine& ScriptEngine = ResolveScriptEngine();
	TypeId = ScriptEngine.RegisterEnum(Name.ToCString());
	if (TypeId == asALREADY_REGISTERED)
	{
		if (asITypeInfo* ExistingType = ScriptEngine.GetTypeInfoByName(Name.ToCString()))
		{
			TypeId = ExistingType->GetTypeId();
		}
	}
	else if (TypeId < 0)
	{
		TargetBinds->RecordRegistrationFailure(TEXT("enum"), Name, TypeId);
	}
}

asIScriptEngine& FAngelscriptBinds::FEnumBind::ResolveScriptEngine() const
{
	check(TargetBinds != nullptr);
	return TargetBinds->GetTargetScriptEngine();
}

asITypeInfo* FAngelscriptBinds::FEnumBind::GetTypeInfo()
{
	return ResolveScriptEngine().GetTypeInfoById(TypeId);
}

void FAngelscriptBinds::FEnumBind::FEnumElement::operator=(int32 Value)
{
	asIScriptEngine& ScriptEngine = Bind->ResolveScriptEngine();

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
	const int Result = ScriptEngine.RegisterEnumValue(AnsiEnumName, Name.ToCString(), Value);

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
	if (Result < 0 && Bind->TargetBinds != nullptr)
	{
		Bind->TargetBinds->RecordRegistrationFailure(TEXT("enum value"), Name, Result);
	}
}

FAngelscriptBinds::FNamespace::FNamespace(FAngelscriptEngine& InTargetEngine, FBindString Name)
	: TargetEngine(&InTargetEngine)
{
	asIScriptEngine* ScriptEngine = InTargetEngine.GetScriptEngine();
	check(ScriptEngine != nullptr);
	PrevNamespace.SetDynamic(ScriptEngine->GetDefaultNamespace());
	ScriptEngine->SetDefaultNamespace(Name.ToCString());
}

FAngelscriptBinds::FNamespace::~FNamespace()
{
	check(TargetEngine != nullptr);
	asIScriptEngine* ScriptEngine = TargetEngine->GetScriptEngine();
	check(ScriptEngine != nullptr);
	ScriptEngine->SetDefaultNamespace(PrevNamespace.ToCString());
}

asITypeInfo* FAngelscriptBinds::GetTypeInfo()
{
	if (TargetEngine != nullptr && HasRegistrationFailure())
	{
		return nullptr;
	}

	if (ScriptType == nullptr && !ClassName.IsEmpty())
	{
		auto& Manager = ResolveTargetEngine();
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
