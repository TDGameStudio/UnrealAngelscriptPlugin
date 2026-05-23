#pragma once
#include "CoreMinimal.h"

#include "AngelscriptType.h"

#include "StartAngelscriptHeaders.h"
#include "AngelscriptInclude.h"
//#include "as_string.h"
//#include "as_scriptfunction.h"
//#include "as_objecttype.h"
#include "source/as_string.h"
#include "source/as_scriptfunction.h"
#include "source/as_objecttype.h"
#include "EndAngelscriptHeaders.h"

#include "StaticJIT/StaticJITConfig.h"

struct FJITDatabase
{
	struct FJITFunctions
	{
		asJITFunction VMEntry;
		asJITFunction_ParmsEntry ParmsEntry;
		asJITFunction_Raw RawFunction;
	};

	TMap<uint32, FJITFunctions> Functions;
	TArray<struct FJitRef_Function*> FunctionLookups;
	TArray<struct FJitRef_SystemFunctionPointer*> SystemFunctionPointerLookups;
	TArray<struct FJitRef_GlobalVar*> GlobalVarLookups;
	TArray<struct FJitRef_Type*> TypeInfoLookups;
	TArray<TPair<uint64, uint32*>> PropertyOffsetLookups;

#if AS_JIT_VERIFY_PROPERTY_OFFSETS
	TArray<TPair<uint64, SIZE_T>> VerifyPropertyOffsets;
	TArray<TPair<uint64, SIZE_T>> VerifyTypeSizes;
	TArray<TPair<uint64, SIZE_T>> VerifyTypeAlignments;
#endif

	static void Clear();
	static FJITDatabase& Get();
};

#if AS_CAN_GENERATE_JIT
struct FJITFile
{
	FString Filename;
	FString JITPrefix;
	FString ModuleName;
	TArray<FString> Content;
	TSet<FString> Headers;
	TSet<FString> SharedHeaderDependencies;
	TMap<void*, FString> ExternalDeclarations;
	TMap<asCScriptFunction*, FString> ExternalSystemFunctionPointerDeclarations;
	TSet<asCScriptFunction*> ExternFunctions;
};

struct FStaticJITContext
{
	uint32 FunctionId;
	FString FunctionName;
	FString FunctionDecl;
	asCScriptFunction* ScriptFunction = nullptr;
	asDWORD* BC_FunctionStart = nullptr;
	asDWORD* BC_EntryStart = nullptr;
	asDWORD* BC_End = nullptr;
	asDWORD* BC = nullptr;

	struct FAngelscriptStaticJIT* JIT = nullptr;
	FJITFile* File = nullptr;
	asITypeInfo* LastPushedTypeInfo = nullptr;
	int32 LastPushedTypeId = 0;

	enum class EVariableType : uint8
	{
		Byte,
		Word,
		DWord,
		QWord,
		Float,
		Double,
		Object,
		Pointer,
	};

	struct FVariableAsLocal
	{
		FAngelscriptTypeUsage Type;
		FString LiteralType;
		FString LocalName;
		bool bIsPointer = false;
		bool bIsReference = false;
		bool bIsValue = false;
		bool bIsConstant = false;
		EVariableType RepresentedType;
	};

	TMap<int32, FVariableAsLocal> LocalVariables;
	TSet<FString> LocalNamesUsed;

	FString LiteralObjectType;
	bool bHasThisPointer = false;

	FString LiteralReturnType;
	bool bReturnIsPointer = false;
	bool bReturnIsReference = false;
	bool bReturnIsValue = false;
	bool bReturnOnStack = false;
	bool bReturnValueDirectly = false;
	bool bReturnIsFloatingPoint = false;
	int ReturnSizeBytes = 0;
	EVariableType ReturnVariableType;

	enum class EValueRegisterState : uint8
	{
		ValueRegister,
		DWordRegister,
		ByteRegister,
		FloatRegister,
		DoubleRegister,

		// We don't have any information about this value register state yet
		Indeterminate,
	};

	enum class EEntryType : uint8
	{
		Line,
	};

	struct FEntryContent
	{
		FString Line;
		asDWORD* BC;
		int32 Id;
		int32 StackPosition;
		EEntryType Type;
	};

	FString FunctionHead;
	TArray<FEntryContent> FunctionContent;
	FString FunctionFoot;

	struct FInstruction
	{
		TArray<int32> JumpTargets;
		TArray<int32> ReceivedJumps;
		asDWORD* BC;
		struct FAngelscriptBytecode* Bytecode;
		FString Label;
		bool bMarked = false;
		int32 StackOffsetBeforeInstr = 0;
		bool bJumpPartOfSwitch = false;
		bool bIsConditionalJump = false;

		EValueRegisterState ValueRegisterStateFromJump = EValueRegisterState::Indeterminate;
		bool bHasMultipleValueRegisterStates = false;
	};
	TArray<FInstruction> Instructions;
	int32 CurrentInstructionIndex = -1;
	bool IsPartOfSwitch();
	
	void AddLabel(int32 RelativeBCOffset, bool bConditional = true);
	FInstruction& GetLabelInstruction(int32 RelativeBCOffset);

	void NewInstruction();

	int32 UsageFlags = 0;

	void AdvanceBC(int32 AdvanceAmount);
	void AdvanceBC();
	int32 GetBCOffset();

	asEBCInstr GetInstr();
	int32 GetInstrSize();

	asDWORD* GetNextBC(asDWORD* FromBC);
	asEBCInstr GetInstr(asDWORD* FromBC);

	void GenerateNewFunction(asIScriptFunction* ScriptFunction);
	void WriteOutFunction();

	bool bInLocalState = false;
	int32 LocalVariableCount = 0;
	FString AllocateLocalVariable(const ANSICHAR* Type);

	void AddHeader(const FString& Header);

	void DebugLineNumber();

	// * Retrieve values from bytecode instructions
	FString BCConstant_QWord();
	FString BCConstant_DWord(int32 Offset = 0);

	FString ReferenceFunction(asCScriptFunction* Function);
	FString ReferenceSystemFunctionPointer(asCScriptFunction* Function);
	FString ReferenceTypeInfo(asITypeInfo* TypeInfo);
	FString ReferenceTypeSize(asITypeInfo* TypeInfo);
	FString ReferenceTypeAlignment(asITypeInfo* TypeInfo);
	FString ReferenceObjectType(asITypeInfo* TypeInfo);
	FString ReferenceTypeId(asDWORD TypeId);
	FString ReferenceGlobalVariable(void* GlobalPtr);
	FString ReferencePropertyOffset(short Offset, int TypeId);

	// * Local variable management
	FString VArg(int32 ArgumentIndex);
	FString VArg_Address(int32 ArgumentIndex);
	FString VArg_Var(int32 ArgumentIndex);
	FString VArg_SignedVar(int32 ArgumentIndex);
	int32 VArg_Offset(int32 ArgumentIndex);
	EVariableType VArg_Type(int32 ArgumentIndex);
	FString VArg_LiteralTypename(int32 ArgumentIndex);
	FString VArg_SignedTypename(int32 ArgumentIndex);
	bool VArg_IsNumeric(int32 ArgumentIndex);
	bool VArg_IsConstant(int32 ArgumentIndex);
	bool VArg_ContainsNonNullPtr(int32 ArgumentIndex);

	FString VAddress(short VariableOffset);
	FString VVar(short VariableOffset);

	// * Stack management
	struct FStackExpression
	{
		int32 DWords;
		FString Expression;
		int32 OffsetOnStack;
		int32 VarOffsetLiteral = 0;
		bool bIsVolatile = false;
		bool bIsVarLiteral = false;
		bool bGuaranteeNonNull = false;
		bool bIsVArgValue = false;
		int32 VArgValueOffset = 0;
		EVariableType ValueType;
	};

	TArray<FStackExpression> FloatingStackExpressions;
	int32 LocalStackOffset = 0;
	int32 LocalStackSize = 0;

	int32 ParmStructSize = 0;
	struct FPropertyInParm
	{
		int TypeId;
		int32 Offset;
		bool bIsRef;
		FString SourceExpr;
	};

	TArray<FPropertyInParm> PropertiesInParm;
	bool bParmIsExecuted = false;

	void Push(int32 DWords, EVariableType Type, const FString& Expression, bool bAssumeNonNull = false);
	void PushVolatile(int32 DWords, EVariableType Type, const FString& Expression, bool bAssumeNonNull = false);
	FStackExpression& PushStatic(int32 DWords);
	void PushVArgValue(int32 DWords, int32 ArgumentIndex);
	void Pop(int32 DWords);
	void PopMultiple(int32 TotalDWords);

	bool IsStackOffsetMaterialized(int32 Offset);
	FStackExpression& GetStaticAtStackOffset(int32 Offset);

	template<typename... Args>
	void PushPtr(const ANSICHAR* Fmt, Args... InArgs)
	{
		Push(2, EVariableType::Pointer, *Format(Fmt, InArgs...));
	}

	template<typename... Args>
	void PushQWord(const ANSICHAR* Fmt, Args... InArgs)
	{
		Push(2, EVariableType::QWord, *Format(Fmt, InArgs...));
	}

	template<typename... Args>
	void PushDWord(const ANSICHAR* Fmt, Args... InArgs)
	{
		Push(1, EVariableType::DWord, *Format(Fmt, InArgs...));
	}

	template<typename... Args>
	void PushVolatilePtr(const ANSICHAR* Fmt, Args... InArgs)
	{
		PushVolatile(2, EVariableType::Pointer, *Format(Fmt, InArgs...));
	}

	template<typename... Args>
	void PushVolatileQWord(const ANSICHAR* Fmt, Args... InArgs)
	{
		PushVolatile(2, EVariableType::QWord, *Format(Fmt, InArgs...));
	}

	template<typename... Args>
	void PushVolatileDWord(const ANSICHAR* Fmt, Args... InArgs)
	{
		PushVolatile(1, EVariableType::DWord, *Format(Fmt, InArgs...));
	}

	void MaterializeStackAtOffset(int32 Offset);
	void MaterializeWholeStack();
	void MaterializeAllPushedVariables();
	void MaterializePushedVariableAtOffset(int32 VariableOffset);
	void MaterializeStackVolatiles();

	FString StackAddress_At(int32 Offset);
	FString StackValue_At(int32 Offset, int32 DWordSize);
	bool IsStackValueNonNull(int32 Offset);

	// * Entry State Vars
	struct FStateVar
	{
		FString Type;
		FString Name;
	};

	TArray<FStateVar> StateVars;

	void AddStateVar(const FString& Type, const FString& Name);


	// * Code Output
	void Entry(EEntryType Type);

	void Line(const TCHAR* Content);

	void Line(const ANSICHAR* Content)
	{
		Line(ANSI_TO_TCHAR(Content));
	}

	void LexToArgs(TArray<FStringFormatArg>& OrdArgs)
	{
	}

	template<typename HeadType>
	void LexToArgs(TArray<FStringFormatArg>& OrdArgs, HeadType InArg)
	{
		OrdArgs.Add(FStringFormatArg(InArg));
	}

	template<typename HeadType, typename... Args>
	void LexToArgs(TArray<FStringFormatArg>& OrdArgs, HeadType InArg, Args... InArgs)
	{
		LexToArgs(OrdArgs, InArg);
		LexToArgs(OrdArgs, InArgs...);
	}

	template<typename... Args>
	void Line(const ANSICHAR* Content, Args... InArgs)
	{
		Line(*Format(Content, InArgs...));
	}

	template<typename... Args>
	FString Format(const ANSICHAR* Fmt, Args... InArgs)
	{
		FString FmtStr = ANSI_TO_TCHAR(Fmt);

		TArray<FStringFormatArg> OrdArgs;
		LexToArgs(OrdArgs, InArgs...);

		return FString::Format(*FmtStr, OrdArgs);
	}

	// * Jumps
	void Jump(FInstruction& TargetInstruction);
	void ReturnEmptyValue();

	// * Value Register Management
	EValueRegisterState ValueRegisterState = EValueRegisterState::ValueRegister;

	void MaterializeValueRegister(bool bAllowIndeterminate = false);
	FString GetValueRegisterForUnsignedComparisonMaybeMaterialize();
	FString GetValueRegisterForSignedComparisonMaybeMaterialize();

	// * Exception handling
	void ExceptionCleanupAndReturn(bool bAfterCurrentOp, bool bSetNullptrException);

	struct FExceptionCleanup
	{
		FString Label;
		TArray<int> Positions;
		TArray<asCObjectType*> Types;

		bool bCalledAddNullptrException = false;
		bool bCalledWithoutAddNullptrException = false;
	};

	TArray<FExceptionCleanup> ExceptionCleanupLabels;

	class asCScriptEngine* GetEngine();
};
#endif

struct FAngelscriptStaticJIT : public asIJITCompiler
{
	// Interface for asIJITCompiler
	int CompileFunction(asIScriptFunction* ScriptFunction, asJITFunction* OutJITFunction) override;
    void ReleaseJITFunction(asJITFunction JITFunction) override;

	struct FAngelscriptPrecompiledData* PrecompiledData = nullptr;

#if AS_CAN_GENERATE_JIT
	bool bGenerateOutputCode = false;
	bool bEmitDebugMetadataInOutput = true;
#if AS_WITH_STATIC_JIT_DIAGNOSTICS
	bool bEmitDiagnosticEntryMarkersInOutput = false;
#endif
	bool bAllowDevirtualize = true;
	bool bAllowComprehensiveJIT = true;

	TMap<asIScriptModule*, TSharedPtr<FJITFile>> JITFiles;
	TSet<FString> UsedUniqueNames;
	class asCScriptEngine* Engine = nullptr;

	struct FGenerateFunction
	{
		FString FunctionSymbolName;
		FString FunctionDeclaration;
	};

	TMap<asCScriptFunction*, FGenerateFunction> FunctionsToGenerate;
	TSet<asCScriptFunction*> FunctionsWithVirtualOverrides;

	asCScriptFunction* DevirtualizeFunction(asCScriptFunction* VirtualFunction);
	bool IsFunctionAlwaysJIT(asCScriptFunction* VirtualFunction);

	TMap<void*, FString> ExternalReferenceNames;
	TMap<asCScriptFunction*, FString> ExternalSystemFunctionPointerReferenceNames;
	TSet<void*> VerifiedExternalReferences;

	FJITFile& GetFile(asIScriptModule* Module);
	FString GetUniqueFunctionName(FJITFile& File, asIScriptFunction* Function, int32 EntryCount);
	FString GetUniqueLabelName(FJITFile& File, asIScriptFunction* Function, FStringView LabelSuffix);
	FString GetUniqueModuleName(FJITFile& File, asIScriptModule* Module);
	FString GetUniqueSymbolName(const FString& SymbolName);

	struct FComputedPropertyOffset
	{
		bool bCanHardcodeOffset = false;
		SIZE_T HardcodedOffset = 0;
		SIZE_T HardcodedAlignment = 0;

		FString ComputedOffsetVariable;
		FString ComputedAlignmentVariable;
	};

	struct FComputedTypeOffsets
	{
		bool bCanHardcodeSize = false;
		SIZE_T HardcodedSize = 0;
		SIZE_T HardcodedAlignment = 0;

		bool bHasComputedSize = false;

		FString ContainingSharedHeader;
		FString AdditionalHeader;

		FString ComputedSizeVariable;
		FString ComputedAlignmentVariable;

		TMap<FString, FComputedPropertyOffset> Properties;
	};

	TMap<asCObjectType*, TSharedPtr<FComputedTypeOffsets>> ComputedOffsets;
	TSharedPtr<FComputedTypeOffsets> GetComputedOffsets(asCObjectType* ObjectType);

	struct FSharedHeader
	{
		FString Filename;
		TSet<FString> Includes;
		TSet<FString> SharedHeaderDependencies;
		TArray<FString> Content;

		TSet<asIScriptModule*> DependentModules;
		bool bHasDependentSharedHeaders = false;
	};

	TMap<FString, TSharedPtr<FSharedHeader>> SharedHeaders;

	FString LatestUsedSharedHeaderName;
	TSharedPtr<FSharedHeader> ActiveSharedHeader;

	TSharedPtr<FSharedHeader> GetActiveSharedHeader();
	TSharedPtr<FSharedHeader> GetSharedHeader(const FString& ModuleName);

	TMap<asCObjectType*, bool> CachedPotentiallyDifferent;
	bool IsTypePotentiallyDifferent(asITypeInfo* ObjectType);

	void GenerateCppCode(asIScriptFunction* ScriptFunction, FGenerateFunction& Generate);
	void WriteOutputCode(TMap<FString, FString>* OutGeneratedFiles = nullptr);

	void SanitizeSymbolName(FString& InOutSymbol);

	void DetectScriptType(asCObjectType* ObjectType);
	void AnalyzeScriptFunction(asCScriptFunction* Function, FGenerateFunction& Generate);
#endif
};

#if AS_WITH_STATIC_JIT_DIAGNOSTICS && AS_CAN_GENERATE_JIT
ANGELSCRIPTRUNTIME_API bool GenerateStaticJITSourceTextForDiagnostics(
	class asIScriptModule* Module,
	FString& OutSourceText,
	bool bEmitDebugMetadata,
	FString* OutError = nullptr);

ANGELSCRIPTRUNTIME_API bool GenerateStaticJITFilesForDiagnostics(
	class asIScriptModule* Module,
	TMap<FString, FString>& OutGeneratedFiles,
	bool bEmitDebugMetadata,
	bool bEmitDiagnosticEntryMarkers,
	FString* OutError = nullptr);

ANGELSCRIPTRUNTIME_API bool GenerateStaticJITAotArtifactsForDiagnostics(
	class asIScriptModule* Module,
	const FString& PrecompiledCacheFilename,
	const FGuid& PrecompiledDataGuid,
	TMap<FString, FString>& OutGeneratedFiles,
	bool bEmitDebugMetadata,
	bool bEmitDiagnosticEntryMarkers,
	FString* OutError = nullptr);
#endif
