#pragma once

#include "CoreMinimal.h"

#include "AngelscriptEngine.h"
#include "AngelscriptDebugValue.h"
#include "UObject/GarbageCollectionSchema.h"
#include "AngelscriptInclude.h"
#include "ClassGenerator/ASFunction.h"
//#include "angelscript.h"
//#include "FunctionCallers.h"
#include "ASClass.generated.h"

UCLASS()
class ANGELSCRIPTRUNTIME_API UASClass : public UClass
{
	GENERATED_BODY()
public:
	UClass* CodeSuperClass = nullptr;
	UASClass* NewerVersion = nullptr;
	bool bHasASClassParent = false;
	bool bCanEverTick = true;
	bool bStartWithTickEnabled = true;
	int32 ContainerSize = 0;
	int32 ScriptPropertyOffset = 0;
	class asIScriptFunction* ConstructFunction;
	class asIScriptFunction* DefaultsFunction;
	UClass* ComposeOntoClass = nullptr;
	//WILL-EDIT
	void* ScriptTypePtr = nullptr;
	void* OwnerScriptEngine = nullptr;
	bool bIsScriptClass = false;
	//TMap<FName, TPair<FGenericFuncPtr, ASAutoCaller::FunctionCaller>> GenericFuncPtrMap;
	//static TMap<FName, TMap<FName, TPair<FGenericFuncPtr, ASAutoCaller::FunctionCaller>>> GFuncMaps;

	//END WILL

	struct FDefaultComponent
	{
		UClass* ComponentClass;
		FName ComponentName;
		SIZE_T VariableOffset;
		bool bIsRoot;
		bool bEditorOnly;
		FName Attach;
		FName AttachSocket;
	};
	
	TArray<FDefaultComponent> DefaultComponents;

	struct FOverrideComponent
	{
		UClass* ComponentClass;
		FName OverrideComponentName;
		FName VariableName;
		SIZE_T VariableOffset;
	};
	
	TArray<FOverrideComponent> OverrideComponents;

	struct ANGELSCRIPTRUNTIME_API FScopeSetDefaultConstructorOuter
	{
		UObject* PrevOuter;
		FScopeSetDefaultConstructorOuter(UObject* NewOuter);
		~FScopeSetDefaultConstructorOuter();
	};

	FDebugValuePrototype DebugValues;

	UASClass(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	static UASClass* GetFirstASClass(UClass* Object);
	static UASClass* GetFirstASClass(UObject* Object);
	static UClass* GetFirstASOrNativeClass(UClass* Object);

	static UObject* GetConstructingASObject();
	//WILL-EDIT
	virtual UClass* GetMostUpToDateClass(); // { return this; }
	virtual void RuntimeAddReferencedObjects(UObject* InThis, FReferenceCollector& Collector) {}

	//WILL-EDIT
	virtual void GetLifetimeScriptReplicationList(TArray<class FLifetimeProperty>& OutLifetimeProps) const;
	
	virtual void RuntimeDestroyObject(UObject* Object);

	virtual bool IsFunctionImplementedInScript(FName InFunctionName) const;

	//WILL-EDIT
	//virtual int32 GetContainerSize() const override { return ContainerSize; }
	virtual int32 GetContainerSize() const { return ContainerSize; }

	void ApplyScriptDefaults(UObject* Object);

	virtual bool IsSafeForRootSet() const override { return true; }
	
	//WILL-EDIT
	UFUNCTION(BlueprintCallable, Category = "Angelscript")
	FString GetSourceFilePath() const;

	//WILL-EDIT
	UFUNCTION(BlueprintCallable, Category = "Angelscript")
	FString GetRelativeSourceFilePath() const;

	//WILL-EDIT	
	UFUNCTION(BlueprintCallable, Category = "Angelscript")
	bool IsDeveloperOnly() const;

	static UObject* GetDefaultConstructorOuter();
	static void* AllocScriptObject(class asITypeInfo* ScriptType, size_t Size);
	static void FinishConstructObject(class asIScriptObject* ScriptObject, class asITypeInfo* ScriptType);

	static void StaticActorConstructor(const FObjectInitializer& Initializer);
	static void StaticComponentConstructor(const FObjectInitializer& Initializer);
	static void StaticObjectConstructor(const FObjectInitializer& Initializer);

	static void StaticDestructor(const FObjectInitializer& Initializer);

	static UObject* OverrideConstructingObject;
};

inline bool IsAngelscriptGenerated(const UFunction* Function)
{
	return Cast<const UASFunction>(Function) != nullptr;
}

inline bool IsAngelscriptGenerated(const FProperty* Property)
{
	if (Property == nullptr)
	{
		return false;
	}

	const UObject* Owner = Property->GetOwner<UObject>();
	if (Cast<const UASClass>(Owner) != nullptr)
	{
		return true;
	}

	const UASFunction* Function = Cast<const UASFunction>(Owner);
	if (Function == nullptr)
	{
		return false;
	}

	if (Function->ReturnArgument.Property == Property)
	{
		return true;
	}

	for (const UASFunction::FArgument& Argument : Function->Arguments)
	{
		if (Argument.Property == Property)
		{
			return true;
		}
	}

	return false;
}

inline bool IsAngelscriptWorldContextProperty(const FProperty* Property)
{
	if (Property == nullptr)
	{
		return false;
	}

	const UASFunction* Function = Cast<const UASFunction>(Property->GetOwner<UObject>());
	if (Function == nullptr || Function->WorldContextIndex < 0 || !Function->Arguments.IsValidIndex(Function->WorldContextIndex))
	{
		return false;
	}

	const UASFunction::FArgument& WorldContextArgument = Function->Arguments[Function->WorldContextIndex];
	if (WorldContextArgument.Property == Property)
	{
		return true;
	}

	return Function->WorldContextOffsetInParms >= 0
		&& Property->GetOffset_ForUFunction() == Function->WorldContextOffsetInParms;
}

