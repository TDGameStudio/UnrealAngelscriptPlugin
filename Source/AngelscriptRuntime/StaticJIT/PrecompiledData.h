#pragma once
#include "CoreMinimal.h"
#include "AngelscriptStaticJIT.h"
#include "StaticJIT/StringInArchive.h"
#include "StaticJIT/PrecompiledDataAllocator.h"
#include "AngelscriptEngine.h"

struct FAngelscriptPrecompiledData;

struct FAngelscriptPrecompiledReference
{
	int64 OldReference = 0;

	bool IsNull() const
	{
		return OldReference == 0;
	}

	inline friend FArchive& operator<<(FArchive& Ar, FAngelscriptPrecompiledReference& Data)
	{
		Ar << Data.OldReference;
		return Ar;
	}
};

struct FAngelscriptPrecompiledDataType
{
	bool bIsReference = false;
	bool bIsObjectConst = false;
	bool bIsObjectHandle = false;
	bool bIsConstHandle = false;
	bool bIsAuto = false;
	bool bIfHandleThenConst = false;

	FAngelscriptPrecompiledReference TypeInfo;
	int32 TokenType = -1;

	inline friend FArchive& operator<<(FArchive& Ar, FAngelscriptPrecompiledDataType& Data)
	{
		Ar << Data.bIsReference;
		Ar << Data.bIsObjectConst;
		Ar << Data.bIsObjectHandle;
		Ar << Data.bIsConstHandle;
		Ar << Data.bIsAuto;
		Ar << Data.bIfHandleThenConst;
		Ar << Data.TypeInfo;
		Ar << Data.TokenType;
		return Ar;
	}

	void InitFrom(FAngelscriptPrecompiledData& Context, class asCDataType& DataType);
	void Create(FAngelscriptPrecompiledData& Context, class asCDataType& DataType) const;
};

struct FAngelscriptPrecompiledFunctionSignature
{
	FStringInArchive Name;
	FStringInArchive Namespace;
	TArray<FAngelscriptPrecompiledDataType, TPrecompiledAllocator<>> ParameterTypes;
	TArray<int32, TPrecompiledAllocator<>> ParameterFlags;
	TArray<FStringInArchive, TPrecompiledAllocator<>> ParameterDefaultArgs;
	FAngelscriptPrecompiledDataType ReturnType;

	inline friend FArchive& operator<<(FArchive& Ar, FAngelscriptPrecompiledFunctionSignature& Data)
	{
		Ar << Data.Name;
		Ar << Data.Namespace;
		Ar << Data.ParameterTypes;
		Ar << Data.ParameterFlags;
		Ar << Data.ParameterDefaultArgs;
		Ar << Data.ReturnType;
		return Ar;
	}

	void InitFrom(FAngelscriptPrecompiledData& Context, class asCScriptFunction* Function);
	void Create(FAngelscriptPrecompiledData& Context, struct FAppliedFunctionSignature& OutSignature) const;
};

struct FAngelscriptPrecompiledFunction
{
	// Angelscript data
	FStringInArchive FunctionName;
	FStringInArchive Namespace;
	FAngelscriptPrecompiledDataType ReturnType;
	TArray<FAngelscriptPrecompiledDataType, TPrecompiledAllocator<>> ParameterTypes;
	TArray<FStringInArchive, TPrecompiledAllocator<>> ParameterNames;
	TArray<int32, TPrecompiledAllocator<>> ParameterFlags;
	TArray<FStringInArchive, TPrecompiledAllocator<>> ParameterDefaultArgs;
	int32 FunctionTraits;
	TArray<int32, TPrecompiledAllocator<>> ByteCode;
	TArray<int32, TPrecompiledAllocator<>> ByteCodeReferences;
	int32 VariableSpace = -1;
	TArray<FAngelscriptPrecompiledReference, TPrecompiledAllocator<>> ObjVariableTypes;
	TArray<int32, TPrecompiledAllocator<>> ObjVariablePos;
	int32 ObjVariablesOnHeap = -1;
	TArray<int32, TPrecompiledAllocator<>> VariableInfoProgramPos;
	TArray<int32, TPrecompiledAllocator<>> VariableInfoOffset;
	TArray<int32, TPrecompiledAllocator<>> VariableInfoOption;
	int32 StackNeeded = -1;
	uint32 Id;
	int32 DeclaredAt = 0;
	TArray<int32, TPrecompiledAllocator<>> LineNumbers;

	// Preprocessor data
	bool bIsUFunction = false;
	FStringInArchive UnrealFunctionName;
	TArray<FStringInArchive, TPrecompiledAllocator<>> MetaSpec;
	TArray<FStringInArchive, TPrecompiledAllocator<>> MetaValues;
	bool bBlueprintCallable;
	bool bBlueprintOverride;
	bool bBlueprintEvent;
	bool bBlueprintPure;
	bool bNetMulticast;
	bool bNetClient;
	bool bNetServer;
	bool bNetValidate;
	bool bUnreliable;
	bool bBlueprintAuthorityOnly;
	bool bExec;
	bool bCanOverrideEvent;
	bool bIsStatic;
	bool bIsConstMethod;
	bool bThreadSafe;
	bool bIsNoOp;

	inline friend FArchive& operator<<(FArchive& Ar, FAngelscriptPrecompiledFunction& Data)
	{
		Ar << Data.FunctionName;
		Ar << Data.Namespace;
		Ar << Data.ReturnType;
		Ar << Data.ParameterTypes;
		Ar << Data.ParameterNames;
		Ar << Data.ParameterFlags;
		Ar << Data.ParameterDefaultArgs;
		Ar << Data.FunctionTraits;
		Ar << Data.ByteCode;
		Ar << Data.ByteCodeReferences;
		Ar << Data.VariableSpace;
		Ar << Data.ObjVariableTypes;
		Ar << Data.ObjVariablePos;
		Ar << Data.ObjVariablesOnHeap;
		Ar << Data.VariableInfoProgramPos;
		Ar << Data.VariableInfoOffset;
		Ar << Data.VariableInfoOption;
		Ar << Data.StackNeeded;
		Ar << Data.Id;
		Ar << Data.DeclaredAt;
		Ar << Data.LineNumbers;

		Ar << Data.bIsUFunction;
		if (Data.bIsUFunction)
		{
			Ar << Data.UnrealFunctionName;
			Ar << Data.MetaSpec;
			Ar << Data.MetaValues;
			Ar << Data.bBlueprintCallable;
			Ar << Data.bBlueprintOverride;
			Ar << Data.bBlueprintEvent;
			Ar << Data.bBlueprintPure;
			Ar << Data.bNetMulticast;
			Ar << Data.bNetClient;
			Ar << Data.bNetServer;
			Ar << Data.bNetValidate;
			Ar << Data.bUnreliable;
			Ar << Data.bBlueprintAuthorityOnly;
			Ar << Data.bExec;
			Ar << Data.bCanOverrideEvent;
			Ar << Data.bIsStatic;
			Ar << Data.bIsConstMethod;
			Ar << Data.bThreadSafe;
			Ar << Data.bIsNoOp;
		}
		return Ar;
	}

	void InitFrom(FAngelscriptPrecompiledData& Context, class asCModule* Module, class asCScriptFunction* Function);

	asCScriptFunction* Create(FAngelscriptPrecompiledData& Context, class asCModule* Module) const;
	ANGELSCRIPTRUNTIME_API void Process(FAngelscriptPrecompiledData& Context, asCScriptFunction* Function) const;

	TSharedPtr<FAngelscriptFunctionDesc> MakeDesc() const;
};

struct FAngelscriptPrecompiledProperty
{
	// Angelscript data
	FStringInArchive Name;
	FAngelscriptPrecompiledDataType Type;
	bool bIsPrivate = false;
	bool bIsProtected = false;

	// Preprocessor data
	bool bIsUnrealProperty;
	TArray<FStringInArchive, TPrecompiledAllocator<>> MetaSpec;
	TArray<FStringInArchive, TPrecompiledAllocator<>> MetaValues;
	bool bBlueprintReadable;
	bool bBlueprintWritable;
	bool bEditConst;
	bool bEditableOnDefaults;
	bool bEditableOnInstance;
	bool bInstancedReference;
	bool bPersistentInstance;
	bool bAdvancedDisplay;
	bool bTransient;
	bool bReplicated;
	int32 ReplicationCondition;
	bool bSkipReplication;
	bool bSkipSerialization;
	bool bSaveGame;
	bool bRepNotify;
	bool bConfig;
	bool bInterp;
	bool bAssetRegistrySearchable;

	inline friend FArchive& operator<<(FArchive& Ar, FAngelscriptPrecompiledProperty& Data)
	{
		Ar << Data.Name;
		Ar << Data.Type;
		Ar << Data.bIsPrivate;
		Ar << Data.bIsProtected;

		Ar << Data.bIsUnrealProperty;
		if (Data.bIsUnrealProperty)
		{
			Ar << Data.MetaSpec;
			Ar << Data.MetaValues;
			Ar << Data.bBlueprintReadable;
			Ar << Data.bBlueprintWritable;
			Ar << Data.bEditConst;
			Ar << Data.bEditableOnDefaults;
			Ar << Data.bEditableOnInstance;
			Ar << Data.bInstancedReference;
			Ar << Data.bPersistentInstance;
			Ar << Data.bAdvancedDisplay;
			Ar << Data.bTransient;
			Ar << Data.bReplicated;
			Ar << Data.bSkipReplication;
			Ar << Data.bSkipSerialization;
			Ar << Data.bSaveGame;
			if (Data.bReplicated)
			{
				Ar << Data.ReplicationCondition;
				Ar << Data.bRepNotify;
			}
			Ar << Data.bConfig;
			Ar << Data.bInterp;
			Ar << Data.bAssetRegistrySearchable;
		}
		return Ar;
	}

	void InitFrom(FAngelscriptPrecompiledData& Context, class asCObjectProperty* Property);
	class asCObjectProperty* Create(FAngelscriptPrecompiledData& Context) const;
};

struct FAngelscriptPrecompiledClass
{
	// Angelscript data
	FStringInArchive ClassName;
	FStringInArchive Namespace;
	asQWORD Flags;
	TArray<FAngelscriptPrecompiledProperty, TPrecompiledAllocator<>> Properties;
	TArray<FAngelscriptPrecompiledFunction, TPrecompiledAllocator<>> Methods;
	TArray<int32, TPrecompiledAllocator<>> MethodTable;
	FAngelscriptPrecompiledReference DerivedFrom;
	FAngelscriptPrecompiledReference ShadowType;
	TArray<FAngelscriptPrecompiledFunction, TPrecompiledAllocator<>> Constructors;
	TArray<FAngelscriptPrecompiledReference, TPrecompiledAllocator<>> FactoryRefs;
	TArray<FAngelscriptPrecompiledReference, TPrecompiledAllocator<>> BehaviorRefs;
	TArray<FAngelscriptPrecompiledFunction, TPrecompiledAllocator<>> BehaviorFunctions;
	TArray<int32, TPrecompiledAllocator<>> BehaviorFunctionTypes;

	// Preprocessor data
	bool bIsInPreprocessor;
	FStringInArchive SuperClass;
	FStringInArchive CodeSuperClass;
	bool bSuperIsCodeClass;
	bool bIsStaticsClass;
	bool bAbstract;
	bool bTransient;
	bool bHideDropdown;
	bool bDefaultToInstanced;
	bool bEditInlineNew;
	bool bIsDeprecatedClass;
	FStringInArchive ConfigName;
	FStringInArchive StaticClassGlobalVariableName;
	bool bPlaceable;
	TArray<FStringInArchive, TPrecompiledAllocator<>> MetaSpec;
	TArray<FStringInArchive, TPrecompiledAllocator<>> MetaValues;
	FStringInArchive ComposeOntoClassName;

	// TRANSIENT
	mutable bool bFunctionsPreProcessed = false;
	// END TRANSIENT

	inline friend FArchive& operator<<(FArchive& Ar, FAngelscriptPrecompiledClass& Data)
	{
		Ar << Data.ClassName;
		Ar << Data.Namespace;
		Ar << Data.Flags;
		Ar << Data.Properties;
		Ar << Data.Methods;
		Ar << Data.MethodTable;
		Ar << Data.DerivedFrom;
		Ar << Data.ShadowType;
		Ar << Data.Constructors;
		Ar << Data.FactoryRefs;
		Ar << Data.BehaviorRefs;
		Ar << Data.BehaviorFunctions;
		Ar << Data.BehaviorFunctionTypes;

		Ar << Data.bIsInPreprocessor;
		if (Data.bIsInPreprocessor)
		{
			Ar << Data.SuperClass;
			Ar << Data.CodeSuperClass;
			Ar << Data.bSuperIsCodeClass;
			Ar << Data.bAbstract;
			Ar << Data.bTransient;
			Ar << Data.bHideDropdown;
			Ar << Data.bDefaultToInstanced;
			Ar << Data.bEditInlineNew;
			Ar << Data.bIsDeprecatedClass;
			Ar << Data.ConfigName;
			Ar << Data.StaticClassGlobalVariableName;
			Ar << Data.bPlaceable;
			Ar << Data.MetaSpec;
			Ar << Data.MetaValues;
			Ar << Data.ComposeOntoClassName;
		}
		return Ar;
	}

	ANGELSCRIPTRUNTIME_API void InitFrom(FAngelscriptPrecompiledData& Context, class asCModule* Module, class asCObjectType* Type);

	ANGELSCRIPTRUNTIME_API asCObjectType* Create(FAngelscriptPrecompiledData& Context, class asCModule* Module) const;
	void ProcessProperties(FAngelscriptPrecompiledData& Context, asCObjectType* Type) const;
	void CreateFunctions(FAngelscriptPrecompiledData& Context, asCObjectType* Type) const;
	void PreProcessFunctions(FAngelscriptPrecompiledData& Context, asCObjectType* Type) const;
	void ProcessFunctions(FAngelscriptPrecompiledData& Context, asCObjectType* Type) const;
	void ResetRuntimeState() const;
};

struct FAngelscriptPrecompiledEnum
{
	FStringInArchive Name;
	FStringInArchive Namespace;
	TArray<FStringInArchive, TPrecompiledAllocator<>> EnumNames;
	TArray<int32, TPrecompiledAllocator<>> EnumValues;

	inline friend FArchive& operator<<(FArchive& Ar, FAngelscriptPrecompiledEnum& Data)
	{
		Ar << Data.Name;
		Ar << Data.Namespace;
		Ar << Data.EnumNames;
		Ar << Data.EnumValues;
		return Ar;
	}

	void InitFrom(FAngelscriptPrecompiledData& Context, class asCModule* Module, class asCEnumType* Type);
	class asCEnumType* Create(FAngelscriptPrecompiledData& Context, class asCModule* Module) const;
};

struct FAngelscriptPrecompiledGlobalVariable
{
	FStringInArchive Name;
	FStringInArchive Namespace;
	FAngelscriptPrecompiledDataType Type;
	bool bIsPureConstant = false;
	uint64 PureConstantValue;
	bool bIsDefaultInit = false;
	bool bHasInitFunction = false;
	FAngelscriptPrecompiledFunction InitFunc;

	inline friend FArchive& operator<<(FArchive& Ar, FAngelscriptPrecompiledGlobalVariable& Data)
	{
		Ar << Data.Name;
		Ar << Data.Namespace;
		Ar << Data.Type;

		Ar << Data.bIsDefaultInit;
		if (!Data.bIsDefaultInit)
		{
			Ar << Data.bIsPureConstant;
			if (Data.bIsPureConstant)
			{
				Ar << Data.PureConstantValue;
			}
			else
			{
				Ar << Data.bHasInitFunction;
				Ar << Data.InitFunc;
			}
		}

		return Ar;
	}

	void InitFrom(FAngelscriptPrecompiledData& Context, class asCModule* Module, class asCGlobalProperty* Property);
	class asCGlobalProperty* Create(FAngelscriptPrecompiledData& Context, class asCModule* Module) const;
	void Process(FAngelscriptPrecompiledData& Context, class asCModule* Module, class asCGlobalProperty* Property) const;
};

struct FAngelscriptPrecompiledFunctionImport
{
	FStringInArchive ImportedFromModule;
	FAngelscriptPrecompiledFunctionSignature Signature;

	inline friend FArchive& operator<<(FArchive& Ar, FAngelscriptPrecompiledFunctionImport& Data)
	{
		Ar << Data.ImportedFromModule;
		Ar << Data.Signature;
		return Ar;
	}

	void InitFrom(FAngelscriptPrecompiledData& Context, class asCModule* Module, struct sBindInfo* BindInfo);
	void AddToModule(FAngelscriptPrecompiledData& Context, class asCModule* Module) const;
};

struct FAngelscriptPrecompiledModule
{
	FStringInArchive ModuleName;
	TArray<FAngelscriptPrecompiledFunction, TPrecompiledAllocator<>> Functions;
	TArray<FAngelscriptPrecompiledClass, TPrecompiledAllocator<>> Classes;
	TArray<FAngelscriptPrecompiledEnum, TPrecompiledAllocator<>> Enums;
	TArray<FAngelscriptPrecompiledGlobalVariable, TPrecompiledAllocator<>> GlobalVariables;
	TArray<FAngelscriptPrecompiledFunctionImport, TPrecompiledAllocator<>> FunctionImports;

	int64 CodeHash = 0;
	TArray<FStringInArchive, TPrecompiledAllocator<>> ImportedModules;
	FStringInArchive StaticsClassName;
	TArray<FStringInArchive, TPrecompiledAllocator<>> DeclaredEvents;
	TArray<FStringInArchive, TPrecompiledAllocator<>> DeclaredDelegates;
	FStringInArchive ScriptRelativeFilename;
	TArray<FStringInArchive, TPrecompiledAllocator<>> PostInitFunctions;

	// TRANSIENT:
	mutable TArray<class asCObjectType*> ClassTypes;
	mutable TArray<asCGlobalProperty*> GlobalProperties;
	mutable TArray<asCScriptFunction*> GlobalFunctions;
	void ProcessProperties(FAngelscriptPrecompiledData& Context, asITypeInfo* TypeInfo) const;
	// END TRANSIENT

	inline friend FArchive& operator<<(FArchive& Ar, FAngelscriptPrecompiledModule& Data)
	{
		Ar << Data.ModuleName;
		Ar << Data.Functions;
		Ar << Data.Classes;
		Ar << Data.Enums;
		Ar << Data.GlobalVariables;
		Ar << Data.FunctionImports;
		Ar << Data.CodeHash;
		Ar << Data.ImportedModules;
		Ar << Data.StaticsClassName;
		Ar << Data.DeclaredEvents;
		Ar << Data.DeclaredDelegates;
		Ar << Data.ScriptRelativeFilename;
		Ar << Data.PostInitFunctions;
		return Ar;
	}

	void InitFrom(FAngelscriptPrecompiledData& Context, class asCModule* Module);

	void ApplyToModule_Stage1(FAngelscriptPrecompiledData& Context, class asIScriptModule* Module) const;
	void ApplyToModule_Stage2(FAngelscriptPrecompiledData& Context, class asIScriptModule* Module) const;
	void ApplyToModule_Stage3(FAngelscriptPrecompiledData& Context, class asIScriptModule* Module) const;
	void ResetRuntimeState() const;
};

struct FAngelscriptTypeReference
{
	FStringInArchive Name;
	FStringInArchive Module;
	FStringInArchive Namespace;
	TArray<FAngelscriptPrecompiledDataType, TPrecompiledAllocator<>> SubTypes;

	inline friend FArchive& operator<<(FArchive& Ar, FAngelscriptTypeReference& Data)
	{
		Ar << Data.Name;
		Ar << Data.Module;
		Ar << Data.Namespace;
		Ar << Data.SubTypes;
		return Ar;
	}
};

struct FAngelscriptFunctionReference
{
	FStringInArchive Name;
	FStringInArchive Module;
	FStringInArchive Namespace;
	bool bIsConst = false;
	bool bIsImportedDecl = false;
	bool bIsMethod = false;
	FAngelscriptPrecompiledReference ObjectType;
	TArray<FAngelscriptPrecompiledDataType, TPrecompiledAllocator<>> ParameterTypes;
	FAngelscriptPrecompiledDataType ReturnType;

	inline friend FArchive& operator<<(FArchive& Ar, FAngelscriptFunctionReference& Data)
	{
		Ar << Data.Name;
		Ar << Data.Module;
		Ar << Data.Namespace;
		Ar << Data.bIsConst;
		Ar << Data.bIsImportedDecl;
		Ar << Data.bIsMethod;
		Ar << Data.ObjectType;
		Ar << Data.ParameterTypes;
		Ar << Data.ReturnType;
		return Ar;
	}

	bool MatchesSignature(FAngelscriptPrecompiledData& Context, class asCScriptFunction* Function) const;
};

struct FAngelscriptGlobalReference
{
	FStringInArchive Name;
	FStringInArchive Module;
	FStringInArchive Namespace;
	bool bIsString = false;

	inline friend FArchive& operator<<(FArchive& Ar, FAngelscriptGlobalReference& Data)
	{
		Ar << Data.Name;
		Ar << Data.Module;
		Ar << Data.Namespace;
		Ar << Data.bIsString;
		return Ar;
	}
};

struct FAngelscriptPropertyReference
{
	FStringInArchive Name;
	int32 OldTypeId;

	inline friend FArchive& operator<<(FArchive& Ar, FAngelscriptPropertyReference& Data)
	{
		Ar << Data.Name;
		Ar << Data.OldTypeId;
		return Ar;
	}
};

struct FAngelscriptPrecompiledData
{
	// AllocMark must be at the top of this struct, because 
	// all the other members are allocated from that memstack,
	// and the topmost member will be destructed *last*.
	FMemMark AllocMark;

	template<typename ValueType>
	struct FIntAsPointerKeyFuncs : public TDefaultMapHashableKeyFuncs<int64, ValueType, false>
	{
		static FORCEINLINE uint32 GetKeyHash(int64 Key)
		{
			return GetTypeHash((void*)Key);
		}
	};

	template<typename K, typename V>
	struct TMapAsPtr : public TMap<K, V, FDefaultSetAllocator, FIntAsPointerKeyFuncs<V>>
	{};

	// Saved data
	FGuid DataGuid;
	TMap<FString, FAngelscriptPrecompiledModule> Modules;
	TMapAsPtr<int64, FAngelscriptTypeReference> TypeReferences;
	TMap<int, int64> TypeIdReferenceToPointer;
	TMapAsPtr<int64, FAngelscriptFunctionReference> FunctionReferences;
	TMap<int, int64> FunctionIdReferenceToPointer;
	TMapAsPtr<int64, FAngelscriptGlobalReference> GlobalReferences;
	TMapAsPtr<int64, FAngelscriptPropertyReference> PropertyReferences;
	TArray<FStringInArchive> StaticNames;
	int32 BuildIdentifier = -1;

	// We keep the original loaded data so we can reference strings inside it
	TArray<uint8> LoadedData;

	// Minimize memory usage mode reduces the amount of memory we spend on
	// angelscript data, but removes some debugging information.
	bool bMinimizeMemoryUsage = false;

	inline friend FArchive& operator<<(FArchive& Ar, FAngelscriptPrecompiledData& Data)
	{
		Ar << Data.DataGuid;
		Ar << Data.BuildIdentifier;
		Ar << Data.Modules;
		Ar << Data.TypeReferences;
		Ar << Data.TypeIdReferenceToPointer;
		Ar << Data.FunctionReferences;
		Ar << Data.FunctionIdReferenceToPointer;
		Ar << Data.GlobalReferences;
		Ar << Data.StaticNames;
		Ar << Data.PropertyReferences;
		return Ar;
	}

	ANGELSCRIPTRUNTIME_API FAngelscriptPrecompiledData(asIScriptEngine* InEngine);
	ANGELSCRIPTRUNTIME_API ~FAngelscriptPrecompiledData();

	ANGELSCRIPTRUNTIME_API void InitFromActiveScript();
	static void OutputTimingData();

	ANGELSCRIPTRUNTIME_API int32 GetCurrentBuildIdentifier();
	ANGELSCRIPTRUNTIME_API bool IsValidForCurrentBuild();

	ANGELSCRIPTRUNTIME_API void Save(const FString& Filename);
	ANGELSCRIPTRUNTIME_API void Load(const FString& Filename);

	struct asSNameSpace* GetNamespace(const FStringInArchive& Namespace);
	void AddRefTo(const class asCDataType& DataType);

	void PrepareToFinalizePrecompiledModules();
	void ClearUnneededRuntimeData();
	// Clears resolved runtime pointers and lookup caches, while preserving serialized archive data.
	void ResetRuntimeState();

	// Serve loaded data that would normally come from preprocessor
	TArray<TSharedRef<FAngelscriptModuleDesc>> GetModulesToCompile();

	// Processed function id maps
	TMap<asIScriptFunction*, uint32> ProcessedFunctionToId;
	TMap<uint32, asIScriptFunction*> ProcessedIdToFunction;

	TMap<void*, const FAngelscriptPrecompiledClass*> ClassesLoadedFromPrecompiledData;
	void ProcessProperties(asITypeInfo* TypeInfo);

	uint32 CreateFunctionId(asIScriptFunction* Function);
	void MapFunctionId(asIScriptFunction* Function, uint32 Id);

	bool GetIdForFunction(asIScriptFunction* Function, uint32& OutId);

	// Context state and reference maps
	asCScriptEngine* Engine = nullptr;
	TMapAsPtr<int64, void*> CachedPointerReferences;

	int32 ScriptSectionIdx = -1;
	const FStringInArchive* ScriptRelativeFilename = nullptr;

	TSharedPtr<FAngelscriptModuleDesc> ModuleDesc;
	TSharedPtr<FAngelscriptClassDesc> ClassDesc;
	TSharedPtr<FAngelscriptPropertyDesc> PropertyDesc;
	TSharedPtr<FAngelscriptFunctionDesc> FunctionDesc;

	int32 GetScriptSection();

	// References
	FAngelscriptPrecompiledReference ReferenceTypeInfo(class asITypeInfo* TypeInfo);
	class asCTypeInfo* GetTypeInfo(const FAngelscriptPrecompiledReference& Reference, bool bAddRef = false);

	FAngelscriptPrecompiledReference ReferenceTypeId(int TypeId);
	int GetTypeId(const FAngelscriptPrecompiledReference& Reference, bool bAddRef = false);

	FAngelscriptPrecompiledReference ReferenceFunction(class asIScriptFunction* Function);
	class asCScriptFunction* GetFunction(const FAngelscriptPrecompiledReference& Reference, bool bAddRef = false, bool bMarkInUse = true);

	FAngelscriptPrecompiledReference ReferenceFunctionId(int FunctionId);
	int GetFunctionId(const FAngelscriptPrecompiledReference& Reference, bool bAddRef = false, bool bMarkInUse = true);

	FAngelscriptPrecompiledReference ReferenceGlobalVariable(void* Pointer, FString* OutName = nullptr);
	void* GetGlobalVariable(const FAngelscriptPrecompiledReference& Reference, class asCGlobalProperty** OutProperty = nullptr);

	FAngelscriptPrecompiledReference ReferenceProperty(int Offset, int TypeId, class asCObjectProperty** OutProperty = nullptr, class asCObjectType** OutObjectType = nullptr);
	int64 GetPropertyReferenceId(int Offset, int TypeId);
	short GetPropertyOffset(const FAngelscriptPrecompiledReference& Reference);
	short GetPropertyOffset(int OldOffset, int OldTypeId);
	short GetTypeSize(int NewTypeId, short OldSize);
};
