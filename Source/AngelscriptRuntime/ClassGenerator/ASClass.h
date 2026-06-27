#pragma once

#include "CoreMinimal.h"

#include "AngelscriptEngine.h"
#include "AngelscriptDebugValue.h"
#include "UObject/GarbageCollectionSchema.h"
#include "AngelscriptInclude.h"
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

UCLASS()
class ANGELSCRIPTRUNTIME_API UASFunction : public UFunction
{
	GENERATED_BODY()
public:

	class asIScriptFunction* ScriptFunction = nullptr;
	int32 GeneratedSourceLineNumber = -1;

	// Cached pointer to RPC _Validate function (if applicable)
	UFunction* ValidateFunction = nullptr;

	enum class EArgumentParmBehavior : uint8
	{
		Reference,
		ReferencePOD,
		Value1Byte,
		Value2Byte,
		Value4Byte,
		Value8Byte,
		FloatExtendedToDouble,
		ReturnObjectPointer,
	};

	enum class EArgumentVMBehavior : uint8
	{
		FloatExtendedToDouble,
		WorldContextObject,
		ObjectPointer,

		ReferencePOD,
		Reference,

		Value1Byte,
		Value2Byte,
		Value4Byte,
		Value8Byte,

		ReturnObjectValue,
		ReturnObjectPOD,

		None,
	};

	struct FArgument
	{
		FProperty* Property = nullptr;
		FAngelscriptTypeUsage Type;
		int32 ValueBytes = 0;
		int32 StackOffset = 0;
		int32 PosInParmStruct = 0;
		EArgumentParmBehavior ParmBehavior;
		EArgumentVMBehavior VMBehavior;

		FArgument() {}
		FArgument(FProperty* InProperty, const FAngelscriptTypeUsage& InType)
			: Property(InProperty), Type(InType)
		{
		}
	};

	bool bIsWorldContextGenerated = false;
	bool bIsNoOp = false;
	int32 WorldContextOffsetInParms = -1;
	int32 WorldContextIndex = -1;

	TArray<FArgument> Arguments;
	TArray<FArgument> DestroyArguments;
	int32 ArgStackSize = 0;
	FArgument ReturnArgument;

	asJITFunction JitFunction = nullptr;
	asJITFunction_ParmsEntry JitFunction_ParmsEntry = nullptr;
	asJITFunction_Raw JitFunction_Raw = nullptr;

	FString GetSourceFilePath() const;
	int GetSourceLineNumber() const;

	void FinalizeArguments();

	uint8 OptimizedCall_ByteReturn(UObject* Object);
	void OptimizedCall_FloatArg(UObject* Object, float Argument);
	void OptimizedCall_DoubleArg(UObject* Object, double Argument);
	void OptimizedCall(UObject* Object);
	void OptimizedCall_RefArg(UObject* Object, void* Argument);
	uint8 OptimizedCall_RefArg_ByteReturn(UObject* Object, void* Argument);

	//WILL-EDIT
	virtual void RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL);
	virtual void RuntimeCallEvent(UObject* Object, void* Parms);
	virtual UFunction* GetRuntimeValidateFunction();
	//END-WILL

	static UASFunction* AllocateFunctionFor(UClass* InClass, FName ObjectName, TSharedPtr<FAngelscriptFunctionDesc> FunctionDesc);
};

ANGELSCRIPTRUNTIME_API void UASFunctionNativeThunk(UObject* Object, FFrame& Stack, RESULT_DECL);

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

UCLASS()
class ANGELSCRIPTRUNTIME_API UASFunction_NotThreadSafe : public UASFunction
{
	GENERATED_BODY()
public:
	virtual void RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL) override;
	virtual void RuntimeCallEvent(UObject* Object, void* Parms) override;
};

UCLASS()
class ANGELSCRIPTRUNTIME_API UASFunction_NoParams : public UASFunction
{
	GENERATED_BODY()
public:
	virtual void RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL) override;
	virtual void RuntimeCallEvent(UObject* Object, void* Parms) override;
};

UCLASS()
class ANGELSCRIPTRUNTIME_API UASFunction_DWordArg : public UASFunction
{
	GENERATED_BODY()
public:
	virtual void RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL) override;
	virtual void RuntimeCallEvent(UObject* Object, void* Parms) override;
};

UCLASS()
class ANGELSCRIPTRUNTIME_API UASFunction_QWordArg : public UASFunction
{
	GENERATED_BODY()
public:
	virtual void RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL) override;
	virtual void RuntimeCallEvent(UObject* Object, void* Parms) override;
};

UCLASS()
class ANGELSCRIPTRUNTIME_API UASFunction_FloatArg : public UASFunction
{
	GENERATED_BODY()
public:
	virtual void RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL) override;
	virtual void RuntimeCallEvent(UObject* Object, void* Parms) override;
};

UCLASS()
class ANGELSCRIPTRUNTIME_API UASFunction_FloatExtendedToDoubleArg : public UASFunction
{
	GENERATED_BODY()
public:
	virtual void RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL) override;
	virtual void RuntimeCallEvent(UObject* Object, void* Parms) override;
};

UCLASS()
class ANGELSCRIPTRUNTIME_API UASFunction_FloatExtendedToDoubleReturn : public UASFunction
{
	GENERATED_BODY()
public:
	virtual void RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL) override;
	virtual void RuntimeCallEvent(UObject* Object, void* Parms) override;
};

UCLASS()
class ANGELSCRIPTRUNTIME_API UASFunction_DoubleArg : public UASFunction
{
	GENERATED_BODY()
public:
	virtual void RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL) override;
	virtual void RuntimeCallEvent(UObject* Object, void* Parms) override;
};

UCLASS()
class ANGELSCRIPTRUNTIME_API UASFunction_ByteArg : public UASFunction
{
	GENERATED_BODY()
public:
	virtual void RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL) override;
	virtual void RuntimeCallEvent(UObject* Object, void* Parms) override;
};

UCLASS()
class ANGELSCRIPTRUNTIME_API UASFunction_ReferenceArg : public UASFunction
{
	GENERATED_BODY()
public:
	virtual void RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL) override;
	virtual void RuntimeCallEvent(UObject* Object, void* Parms) override;
};

UCLASS()
class ANGELSCRIPTRUNTIME_API UASFunction_ObjectReturn : public UASFunction
{
	GENERATED_BODY()
public:
	virtual void RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL) override;
	virtual void RuntimeCallEvent(UObject* Object, void* Parms) override;
};

UCLASS()
class ANGELSCRIPTRUNTIME_API UASFunction_DWordReturn : public UASFunction
{
	GENERATED_BODY()
public:
	virtual void RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL) override;
	virtual void RuntimeCallEvent(UObject* Object, void* Parms) override;
};

UCLASS()
class ANGELSCRIPTRUNTIME_API UASFunction_FloatReturn : public UASFunction
{
	GENERATED_BODY()
public:
	virtual void RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL) override;
	virtual void RuntimeCallEvent(UObject* Object, void* Parms) override;
};

UCLASS()
class ANGELSCRIPTRUNTIME_API UASFunction_DoubleReturn : public UASFunction
{
	GENERATED_BODY()
public:
	virtual void RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL) override;
	virtual void RuntimeCallEvent(UObject* Object, void* Parms) override;
};

UCLASS()
class ANGELSCRIPTRUNTIME_API UASFunction_ByteReturn : public UASFunction
{
	GENERATED_BODY()
public:
	virtual void RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL) override;
	virtual void RuntimeCallEvent(UObject* Object, void* Parms) override;
};

UCLASS()
class ANGELSCRIPTRUNTIME_API UASFunction_JIT : public UASFunction
{
	GENERATED_BODY()
public:
	virtual void RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL) override;
	virtual void RuntimeCallEvent(UObject* Object, void* Parms) override;
};

UCLASS()
class ANGELSCRIPTRUNTIME_API UASFunction_NotThreadSafe_JIT : public UASFunction
{
	GENERATED_BODY()
public:
	virtual void RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL) override;
	virtual void RuntimeCallEvent(UObject* Object, void* Parms) override;
};

UCLASS()
class ANGELSCRIPTRUNTIME_API UASFunction_NoParams_JIT : public UASFunction
{
	GENERATED_BODY()
public:
	virtual void RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL) override;
	virtual void RuntimeCallEvent(UObject* Object, void* Parms) override;
};

UCLASS()
class ANGELSCRIPTRUNTIME_API UASFunction_DWordArg_JIT : public UASFunction
{
	GENERATED_BODY()
public:
	virtual void RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL) override;
	virtual void RuntimeCallEvent(UObject* Object, void* Parms) override;
};

UCLASS()
class ANGELSCRIPTRUNTIME_API UASFunction_QWordArg_JIT : public UASFunction
{
	GENERATED_BODY()
public:
	virtual void RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL) override;
	virtual void RuntimeCallEvent(UObject* Object, void* Parms) override;
};

UCLASS()
class ANGELSCRIPTRUNTIME_API UASFunction_FloatArg_JIT : public UASFunction
{
	GENERATED_BODY()
public:
	virtual void RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL) override;
	virtual void RuntimeCallEvent(UObject* Object, void* Parms) override;
};

UCLASS()
class ANGELSCRIPTRUNTIME_API UASFunction_FloatExtendedToDoubleArg_JIT : public UASFunction
{
	GENERATED_BODY()
public:
	virtual void RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL) override;
	virtual void RuntimeCallEvent(UObject* Object, void* Parms) override;
};

UCLASS()
class ANGELSCRIPTRUNTIME_API UASFunction_FloatExtendedToDoubleReturn_JIT : public UASFunction
{
	GENERATED_BODY()
public:
	virtual void RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL) override;
	virtual void RuntimeCallEvent(UObject* Object, void* Parms) override;
};

UCLASS()
class ANGELSCRIPTRUNTIME_API UASFunction_DoubleArg_JIT : public UASFunction
{
	GENERATED_BODY()
public:
	virtual void RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL) override;
	virtual void RuntimeCallEvent(UObject* Object, void* Parms) override;
};

UCLASS()
class ANGELSCRIPTRUNTIME_API UASFunction_ByteArg_JIT : public UASFunction
{
	GENERATED_BODY()
public:
	virtual void RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL) override;
	virtual void RuntimeCallEvent(UObject* Object, void* Parms) override;
};

UCLASS()
class ANGELSCRIPTRUNTIME_API UASFunction_ReferenceArg_JIT : public UASFunction
{
	GENERATED_BODY()
public:
	virtual void RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL) override;
	virtual void RuntimeCallEvent(UObject* Object, void* Parms) override;
};

UCLASS()
class ANGELSCRIPTRUNTIME_API UASFunction_ObjectReturn_JIT : public UASFunction
{
	GENERATED_BODY()
public:
	virtual void RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL) override;
	virtual void RuntimeCallEvent(UObject* Object, void* Parms) override;
};

UCLASS()
class ANGELSCRIPTRUNTIME_API UASFunction_DWordReturn_JIT : public UASFunction
{
	GENERATED_BODY()
public:
	virtual void RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL) override;
	virtual void RuntimeCallEvent(UObject* Object, void* Parms) override;
};

UCLASS()
class ANGELSCRIPTRUNTIME_API UASFunction_FloatReturn_JIT : public UASFunction
{
	GENERATED_BODY()
public:
	virtual void RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL) override;
	virtual void RuntimeCallEvent(UObject* Object, void* Parms) override;
};

UCLASS()
class ANGELSCRIPTRUNTIME_API UASFunction_DoubleReturn_JIT : public UASFunction
{
	GENERATED_BODY()
public:
	virtual void RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL) override;
	virtual void RuntimeCallEvent(UObject* Object, void* Parms) override;
};

UCLASS()
class ANGELSCRIPTRUNTIME_API UASFunction_ByteReturn_JIT : public UASFunction
{
	GENERATED_BODY()
public:
	virtual void RuntimeCallFunction(UObject* Object, FFrame& Stack, RESULT_DECL) override;
	virtual void RuntimeCallEvent(UObject* Object, void* Parms) override;
};
