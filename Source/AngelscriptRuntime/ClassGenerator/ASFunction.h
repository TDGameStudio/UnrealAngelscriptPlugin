#pragma once

#include "CoreMinimal.h"

#include "AngelscriptEngine.h"
#include "AngelscriptInclude.h"
#include "ASFunction.generated.h"

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
