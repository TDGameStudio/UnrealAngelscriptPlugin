#pragma once

#include "CoreMinimal.h"
#include "ClassGenerator/AngelscriptAdditionalCompileChecks.h"
#include "AngelscriptEngine.h"
// FOnAngelscriptClassReload / FOnAngelscriptEnumCreated / etc. and the
// `EnumNameList` typedef now live directly on `FAngelscriptEngine` (engine-owned).
// They are declared in `Core/AngelscriptEngine.h`, included above. Subscribers
// should register through `FAngelscriptEngine::Get().GetOnXxx()`
// instead of the previous `FAngelscriptClassGenerator::OnXxx` static fields.

// Generic call callback for dispatching interface method calls.
// Resolves the real UFunction on the implementing object via FindFunction + ProcessEvent.
extern ANGELSCRIPTRUNTIME_API void CallInterfaceMethod(class asIScriptGeneric* InGeneric);

struct FAngelscriptClassGenerator
{
	enum EReloadRequirement
	{
		SoftReload,
		FullReloadSuggested,
		FullReloadRequired,
		Error,
	};

	ANGELSCRIPTRUNTIME_API void AddModule(TSharedRef<FAngelscriptModuleDesc> Module);

	ANGELSCRIPTRUNTIME_API EReloadRequirement Setup();

	void PerformFullReload();
	void PerformSoftReload();

	void GetFullReloadLines(TSharedRef<FAngelscriptModuleDesc> Module, TArray<int32>& OutLines);
	ANGELSCRIPTRUNTIME_API bool WantsFullReload(TSharedRef<FAngelscriptModuleDesc> Module);
	ANGELSCRIPTRUNTIME_API bool NeedsFullReload(TSharedRef<FAngelscriptModuleDesc> Module);

private:
	struct FReloadPropagation
	{
		EReloadRequirement ReloadReq = EReloadRequirement::SoftReload;
		int32 ReloadPlannerNodeIndex = INDEX_NONE;
	};

	struct FClassData : public FReloadPropagation
	{
		int32 DataIndex;
		TSharedPtr<FAngelscriptClassDesc> NewClass;
		TSharedPtr<FAngelscriptClassDesc> OldClass;
		bool bAnalyzed = false;
		bool bReloaded = false;
		UASClass* ReplacedClass = nullptr;
		UStruct* ReplacedStruct = nullptr;
		TArray<int32> ReloadReqLines;
		bool bHasEvalTick = false;
		bool bFinalized = false;
		UObject* CDONoDefaults = nullptr;
	};

	struct FEnumData
	{
		int32 DataIndex;
		TSharedPtr<FAngelscriptEnumDesc> NewEnum;
		TSharedPtr<FAngelscriptEnumDesc> OldEnum;
		bool bNeedReload = false;
	};

	struct FDelegateData : public FReloadPropagation
	{
		int32 DataIndex;
		TSharedPtr<FAngelscriptDelegateDesc> NewDelegate;
		TSharedPtr<FAngelscriptDelegateDesc> OldDelegate;
		bool bAnalyzed = false;
		TArray<int32> ReloadReqLines;
	};

	struct FModuleData
	{
		int32 ModuleIndex;
		TSharedPtr<FAngelscriptModuleDesc> NewModule;
		TSharedPtr<FAngelscriptModuleDesc> OldModule;
		TArray<FEnumData> Enums;
		TArray<FClassData> Classes;
		TArray<FDelegateData> Delegates;
		EReloadRequirement ReloadReq = EReloadRequirement::SoftReload;
		TArray<int32> ReloadReqLines;

		TArray<TSharedPtr<FAngelscriptClassDesc>> RemovedClasses;
	};

	struct FDataRef
	{
		bool bIsClass = false;
		bool bIsEnum = false;
		bool bIsDelegate = false;
		int32 ModuleIndex;
		int32 DataIndex;

		FDataRef() {}

		FDataRef(const FModuleData& ModuleData, const FClassData& ClassData)
		{
			bIsClass = true;
			ModuleIndex = ModuleData.ModuleIndex;
			DataIndex = ClassData.DataIndex;
		}

		FDataRef(const FModuleData& ModuleData, const FDelegateData& DelegateData)
		{
			bIsDelegate = true;
			ModuleIndex = ModuleData.ModuleIndex;
			DataIndex = DelegateData.DataIndex;
		}

		FDataRef(const FModuleData& ModuleData, const FEnumData& EnumData)
		{
			bIsEnum = true;
			ModuleIndex = ModuleData.ModuleIndex;
			DataIndex = EnumData.DataIndex;
		}
	};

	TMap<FString, int32> ModuleIndexByName;
	TMap<class asIScriptModule*, int32> ModuleIndexByNewScriptModule;
	TMap<FString, FDataRef> DataRefByName;
	TMap<class asITypeInfo*, FDataRef> DataRefByNewScriptType;
	TArray<UClass*> ReinstancedSubsystems;

	bool bIsDoingFullReload = false;
	bool bReinstancingAny = false;
	int32 NextUniqueNumber = 0;

	static int32 UniqueCounter();

	bool ShouldFullReload(FClassData& Class);
	bool ShouldFullReload(FEnumData& Enum);
	bool ShouldFullReload(FDelegateData& Enum);
	bool HasInterfaceListChanged(FClassData& Class) const;
	EReloadRequirement GetReloadRequirementForNewScriptType(class asITypeInfo* ScriptType) const;
	bool HasReloadedReflectedScriptType(const FAngelscriptTypeUsage& OldType, const FAngelscriptTypeUsage& NewType) const;
	bool HasReloadedReflectedScriptType(const FAngelscriptFunctionDesc& OldFunction, const FAngelscriptFunctionDesc& NewFunction) const;

	void PerformReload(bool bFullReload);

	void RegisterReloadPlannerNodes(struct FAngelscriptClassReloadPlanner& Planner);
	void AddReloadDependency(struct FAngelscriptClassReloadPlanner& Planner, FReloadPropagation* Source, const FAngelscriptTypeUsage& Type);
	void AddReloadDependency(struct FAngelscriptClassReloadPlanner& Planner, FReloadPropagation* Source, class asITypeInfo* TypeInfo);

	void CollectReloadDependencies(struct FAngelscriptClassReloadPlanner& Planner, FModuleData& Module, FClassData& Class);
	void CollectReloadDependencies(struct FAngelscriptClassReloadPlanner& Planner, FModuleData& Module, FDelegateData& Delegate);
	void ApplyReloadPlannerResults(struct FAngelscriptClassReloadPlanner& Planner);

	TArray<FModuleData> Modules;
	TMap<class asITypeInfo*, class asITypeInfo*> UpdatedScriptTypeMap;
	TArray<UClass*> AddedClasses;

	bool GetDataFor(int TypeId, FModuleData*& OutModule, FClassData*& OutClass, FDelegateData*& OutDelegate);
	bool GetDataFor(class asITypeInfo* Type, FModuleData*& OutModule, FClassData*& OutClass, FDelegateData*& OutDelegate);

	UClass* ResolveCodeSuperForProperty(const FAngelscriptTypeUsage& Usage);

	void EnsureReloaded(FModuleData& Module, FClassData& Class);
	void EnsureReloaded(UASClass* Class);
	void EnsureReloaded(int TypeId);
	void EnsureClassFinalized(UASClass* Class);

	void Analyze(FModuleData& Module);
	void Analyze(FModuleData& Module, FClassData& Class);
	void Analyze(FModuleData& Module, FDelegateData& Class);
	void TryGenerateClassRenameRedirects(FModuleData& Module);

	void SetupModule(FModuleData& Module);

	void InitEnums(FModuleData& Module);
	void AnalyzeEnums(FModuleData& Module);

	void CreateFullReloadClass(FModuleData& Module, FClassData& Class);
	UASClass* ResolveClassRedirectReplacedClass(FModuleData& Module, FClassData& Class);
	void FullReloadRemoveClass(FModuleData& Module, TSharedPtr<FAngelscriptClassDesc> RemovedClass);
	void CreateFullReloadStruct(FModuleData& Module, FClassData& Class);
	void CreateFullReloadDelegate(FModuleData& Module, FDelegateData& Delegate);
	void DoFullReload(FModuleData& Module, FClassData& Class);
	void DoFullReloadClass(FModuleData& Module, FClassData& Class);
	void DoFullReloadStruct(FModuleData& Module, FClassData& Class);
	void DoFullReload(FModuleData& Module, FEnumData& Enum);
	void DoFullReload(FModuleData& Module, FDelegateData& DelegateData);

#if WITH_EDITOR
	void CopyClassInheritedMetaData(UClass* SuperClass, UClass* NewClass);
#endif

	void SoftReloadFunction(UFunction* Function);
	void SoftReloadType(FAngelscriptTypeUsage& Usage);

	int32 AddClassProperties(TSharedPtr<FAngelscriptClassDesc> Class);
	FProperty* AddFunctionReturnType(UFunction* NewFunction, const FAngelscriptTypeUsage& ReturnType);
	FProperty* AddFunctionArgument(UFunction* NewFunction, const FAngelscriptArgumentDesc& ArgDesc, bool bAddToArgList = true);

	void LinkSoftReloadClasses(FModuleData& Module, FEnumData& Enum);
	void LinkSoftReloadClasses(FModuleData& Module, FClassData& Class);
	void LinkSoftReloadClasses(FModuleData& Module, FDelegateData& Delegate);

	void PrepareSoftReload(FModuleData& Module, FClassData& Class);
	void DoSoftReload(FModuleData& Module, FClassData& Class);

	void CleanupRemovedClass(TSharedPtr<FAngelscriptClassDesc> Class);

	bool IsReloadingModule(TSharedPtr<FAngelscriptModuleDesc> Module);
	TSharedPtr<FAngelscriptClassDesc> EnsureClassAnalyzed(const FString& ClassName);
	TSharedPtr<FAngelscriptClassDesc> GetClassDesc(const FString& ClassName);

	void DestructScriptObject(class asCScriptObject* ScriptObject, UASClass* NewClass, class asCObjectType* ObjectTypeToDestruct);
	void ReinitializeScriptObject(class asCScriptObject* ScriptObject, UASClass* NewClass, class asCObjectType* ObjectTypeToConstruct);

	void DetectAngelscriptReferences(TSharedPtr<FAngelscriptClassDesc> Class);
	void CreateDebugValuePrototype(TSharedPtr<FAngelscriptClassDesc> Class);

	void FinalizeClass(FModuleData& ModuleData, FClassData& ClassData);
	void FinalizeActorClass(FModuleData& ModuleData, TSharedPtr<FAngelscriptClassDesc> ClassDesc);
	void FinalizeComponentClass(TSharedPtr<FAngelscriptClassDesc> ClassDesc);
	void FinalizeObjectClass(TSharedPtr<FAngelscriptClassDesc> ClassDesc);

	void VerifyClass(FModuleData& ModuleData, TSharedPtr<FAngelscriptClassDesc> ClassDesc);

	void InitClassTickSettings(FClassData& ClassData);
	void CallPostInitFunctions();
	void InitDefaultObjects();
	void InitDefaultObject(FModuleData& ModuleData, FClassData& ClassData);

	void SetScriptStaticClass(TSharedPtr<FAngelscriptClassDesc> ClassDesc, UClass* Class);
	void UpdateConstructAndDefaultsFunctions(TSharedPtr<FAngelscriptClassDesc> ClassDesc, UASClass* Class);

	asITypeInfo* GetNamespacedTypeInfoForClass(TSharedPtr<FAngelscriptClassDesc> ClassDesc, TSharedPtr<FAngelscriptModuleDesc> ModuleDesc) const;

	FString GetUnrealName(bool bIsStruct, const FString& ClassName);
	TSet<FString> UsedUnrealNames;
};
