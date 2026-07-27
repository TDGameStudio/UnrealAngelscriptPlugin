#include "../../Support/AngelscriptNativeDebugTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FNativeLocalVariableTests,
	"Angelscript.TestModule.AngelScriptSDK.Runtime.Debug.LocalVariables",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:

	inline static constexpr asPWORD LocalRecorderUserDataSlot = static_cast<asPWORD>(0x4E4154444C4F434Cull);

	struct FNativeDebugLocalObject
	{
		int32 Value = 0;
	};

	static void ConstructNativeDebugLocalObject(FNativeDebugLocalObject* Address)
	{
		new (Address) FNativeDebugLocalObject();
	}

	struct FLocalEntry
	{
		FString Name;
		FString Declaration;
		FString NamespaceDeclaration;
		int32 TypeId = asINVALID_TYPE;
		void* Address = nullptr;
		void* ReferencedObject = nullptr;
		bool bInScope = false;
	};

	struct FLocalObservation
	{
		TArray<TArray<FLocalEntry>> Snapshots;
		bool bInvalidVariableIndexWasSafe = false;
		FString TargetDeclaration;
	};

	static FString BuildSource()
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int ProbeLocals(int Parameter)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Local = Parameter + 1;"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tint Local = 7;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tint Nested = Local;"));
		AppendGeneratedAsLine(Source, TEXT("\t\tLocal += Nested;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("\tfor (int LoopLocal = 0; LoopLocal < 1; ++LoopLocal)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tLocal += LoopLocal;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Local;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("enum ENativeDebugLocalEnum"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tNativeDebugLocalEnumValue = 3"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("// NativeDebugLocalAlias is registered through the raw SDK before build."));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("class NativeDebugLocalValue"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("class NativeDebugLocalReference"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int ProbeTypedLocals()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint8 Int8Value = -8;"));
		AppendGeneratedAsLine(Source, TEXT("\tint16 Int16Value = -16;"));
		AppendGeneratedAsLine(Source, TEXT("\tint IntValue = 20;"));
		AppendGeneratedAsLine(Source, TEXT("\tint64 Int64Value = 64;"));
		AppendGeneratedAsLine(Source, TEXT("\tuint8 UInt8Value = 8;"));
		AppendGeneratedAsLine(Source, TEXT("\tuint16 UInt16Value = 16;"));
		AppendGeneratedAsLine(Source, TEXT("\tuint UIntValue = 21;"));
		AppendGeneratedAsLine(Source, TEXT("\tuint64 UInt64Value = 128;"));
		AppendGeneratedAsLine(Source, TEXT("\tfloat32 Float32Value = 1.5f;"));
		AppendGeneratedAsLine(Source, TEXT("\tfloat64 Float64Value = 2.5;"));
		AppendGeneratedAsLine(Source, TEXT("\tbool BoolValue = true;"));
		AppendGeneratedAsLine(Source, TEXT("\tENativeDebugLocalEnum EnumValue = ENativeDebugLocalEnum::NativeDebugLocalEnumValue;"));

		AppendGeneratedAsLine(Source, TEXT("\tNativeDebugLocalAlias AliasValue = 9;"));
		AppendGeneratedAsLine(Source, TEXT("\tNativeDebugLocalValue ValueObject = NativeDebugLocalValue();"));
		AppendGeneratedAsLine(Source, TEXT("\tValueObject.Value = 7;"));
		AppendGeneratedAsLine(Source, TEXT("\tNativeDebugNativeObject NativeObject;"));
		AppendGeneratedAsLine(Source, TEXT("\tNativeObject.Value = 4;"));
		AppendGeneratedAsLine(Source, TEXT("\tNativeDebugLocalReference ReferenceValue = NativeDebugLocalReference();"));
		AppendGeneratedAsLine(Source, TEXT("\tNativeDebugLocalReference NullValue = nullptr;"));
		AppendGeneratedAsLine(Source, TEXT("\treturn IntValue + AliasValue + ValueObject.Value + (BoolValue ? 6 : 0);"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int ProbeOptimizationShape(int Input)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = Input;"));
		AppendGeneratedAsLine(Source, TEXT("\tValue++;"));
		AppendGeneratedAsLine(Source, TEXT("\tValue++;"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunLocalEntry()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn ProbeLocals(20);"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static void CaptureLocals(asCContext* Context)
	{
		if (Context == nullptr)
		{
			return;
		}
		FLocalObservation* const Observation = static_cast<FLocalObservation*>(Context->GetUserData(LocalRecorderUserDataSlot));
		asIScriptFunction* const CurrentFunction = Context->GetFunction(0);
		if (Observation == nullptr
			|| CurrentFunction == nullptr
			|| Observation->TargetDeclaration.IsEmpty()
			|| Observation->TargetDeclaration != UTF8_TO_TCHAR(CurrentFunction->GetDeclaration()))
		{
			return;
		}

		const int32 VariableCount = Context->GetVarCount(0);
		if (VariableCount < 0)
		{
			return;
		}
		TArray<FLocalEntry> Snapshot;
		for (int32 VariableIndex = 0; VariableIndex < VariableCount; ++VariableIndex)
		{
			FLocalEntry Entry;
			const char* const Name = Context->GetVarName(VariableIndex, 0);
			Entry.Name = UTF8_TO_TCHAR(Name != nullptr ? Name : "");
			Entry.Declaration = UTF8_TO_TCHAR(Context->GetVarDeclaration(VariableIndex, 0, false));
			Entry.NamespaceDeclaration = UTF8_TO_TCHAR(Context->GetVarDeclaration(VariableIndex, 0, true));
			Entry.TypeId = Context->GetVarTypeId(VariableIndex, 0);
			Entry.Address = Context->GetAddressOfVar(VariableIndex, 0);
			if (Entry.Address != nullptr && (Entry.TypeId & asTYPEID_OBJHANDLE) != 0)
			{
				Entry.ReferencedObject = *reinterpret_cast<void**>(Entry.Address);
			}
			Entry.bInScope = Context->IsVarInScope(VariableIndex, 0);
			Snapshot.Add(MoveTemp(Entry));
		}
		Observation->bInvalidVariableIndexWasSafe |= Context->GetVarName(VariableCount, 0) == nullptr
			&& Context->GetVarDeclaration(VariableCount, 0, false) == nullptr
			&& Context->GetAddressOfVar(VariableCount, 0) == nullptr
			&& !Context->IsVarInScope(VariableCount, 0);
		Observation->Snapshots.Add(MoveTemp(Snapshot));
	}

	static const FLocalEntry* FindLiveEntry(const FLocalObservation& Observation, const TCHAR* Name)
	{
		for (const TArray<FLocalEntry>& Snapshot : Observation.Snapshots)
		{
			for (const FLocalEntry& Entry : Snapshot)
			{
				if (Entry.Name == Name && Entry.bInScope && Entry.Address != nullptr)
				{
					return &Entry;
				}
			}
		}
		return nullptr;
	}

	static bool BytecodeContainsOpcode(asIScriptFunction* Function, const asEBCInstr Opcode)
	{
		if (Function == nullptr)
		{
			return false;
		}

		asUINT BytecodeLength = 0;
		const asDWORD* const Bytecode = Function->GetByteCode(&BytecodeLength);
		if (Bytecode == nullptr || BytecodeLength == 0)
		{
			return false;
		}

		asUINT DwordIndex = 0;
		while (DwordIndex < BytecodeLength)
		{
			const asEBCInstr CurrentOpcode = static_cast<asEBCInstr>(*reinterpret_cast<const asBYTE*>(&Bytecode[DwordIndex]));
			if (CurrentOpcode == Opcode)
			{
				return true;
			}
			if (static_cast<int32>(CurrentOpcode) > static_cast<int32>(asBC_MAXBYTECODE))
			{
				return false;
			}

			const int32 InstructionSize = asBCTypeSize[asBCInfo[CurrentOpcode].type];
			if (InstructionSize <= 0)
			{
				return false;
			}
			DwordIndex += static_cast<asUINT>(InstructionSize);
		}

		return false;
	}

	static TArray<asDWORD> CopyBytecode(asIScriptFunction* Function)
	{
		TArray<asDWORD> Result;
		if (Function == nullptr)
		{
			return Result;
		}

		asUINT BytecodeLength = 0;
		const asDWORD* const Bytecode = Function->GetByteCode(&BytecodeLength);
		if (Bytecode != nullptr && BytecodeLength > 0)
		{
			Result.Append(Bytecode, static_cast<int32>(BytecodeLength));
		}
		return Result;
	}

	static void ReportGeneratedModule(FAutomationTestBase& Test, const FString& ModuleName, const FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		PrintGeneratedAsSource(Test, TEXT("DBG-LOCAL-TYPE-ROLE-QUERY"), ModuleName, Source);
	}

public:
	TEST_METHOD(TypesByRoleQueryAndOptimization)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("DBG-LOCAL-TYPE-ROLE-QUERY",
			ENativeEvidence::Runtime
			| ENativeEvidence::Debug
			| ENativeEvidence::Metadata);

		AngelscriptNativeTestSupport::FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};

		FScopedNativeDebugCallbacks DebugCallbacks;
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Local variable product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		// The fork keeps float spelling in a process-wide data-type display flag.
		// Pin it for this raw-engine test so prior SDK cases cannot change the
		// expected float32/float64 declarations.
		const asPWORD PreviousFloatIsFloat64 = ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64);
		ASSERT_THAT(AreEqual(asSUCCESS, ScriptEngine->SetEngineProperty(asEP_FLOAT_IS_FLOAT64, 0),
			TEXT("Local variable product should pin float spelling while inspecting declarations")));
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetEngineProperty(asEP_FLOAT_IS_FLOAT64, PreviousFloatIsFloat64);
		};
		const int NativeObjectTypeResult = ScriptEngine->RegisterObjectType(
			"NativeDebugNativeObject", sizeof(FNativeDebugLocalObject),
			asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<FNativeDebugLocalObject>() | asOBJ_APP_CLASS_ALLINTS);
		ASSERT_THAT(IsTrue(NativeObjectTypeResult >= 0,
			*FString::Printf(TEXT("Local variable product should register the native-object type. Result=%d Messages={%s}"),
				NativeObjectTypeResult, *Engine.GetMessagesText())));
		const ASAutoCaller::FunctionCaller ConstructorCaller = ASAutoCaller::MakeFunctionCaller(ConstructNativeDebugLocalObject);
		ASSERT_THAT(IsTrue(ScriptEngine->RegisterObjectBehaviour(
			"NativeDebugNativeObject", asBEHAVE_CONSTRUCT, "void f()",
			asFUNCTION(ConstructNativeDebugLocalObject), asCALL_CDECL_OBJLAST,
			*(asFunctionCaller*)&ConstructorCaller) >= 0,
			TEXT("Local variable product should register the native-object constructor")));
		ASSERT_THAT(AreEqual(asSUCCESS, ScriptEngine->RegisterObjectProperty("NativeDebugNativeObject", "int Value", asOFFSET(FNativeDebugLocalObject, Value)), TEXT("Local variable product should register the native-object property")));
		const int AliasResult = ScriptEngine->RegisterTypedef("NativeDebugLocalAlias", "int");
		ASSERT_THAT(IsTrue(AliasResult >= 0,
			*FString::Printf(TEXT("Local variable product should register its raw alias. Result=%d Messages={%s}"),
				AliasResult, *Engine.GetMessagesText())));
		const asPWORD PreviousOptimization = ScriptEngine->GetEngineProperty(asEP_OPTIMIZE_BYTECODE);
		ASSERT_THAT(AreEqual(asSUCCESS, ScriptEngine->SetEngineProperty(asEP_OPTIMIZE_BYTECODE, 0), TEXT("Local variable product should build its inspectable source without bytecode optimization")));
		ON_SCOPE_EXIT
		{
			ScriptEngine->SetEngineProperty(asEP_OPTIMIZE_BYTECODE, PreviousOptimization);
		};

		const FString ModuleName = TEXT("NativeDebugLocals");
		const FString OptimizedModuleName = TEXT("NativeDebugLocalsOptimized");
		const FString Source = BuildSource();
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 OptimizedModuleNameUtf8(*OptimizedModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		asIScriptModule* Module = nullptr;
		ReportGeneratedModule(*TestRunner, ModuleName, Source);
		const int BuildResult = CompileNativeModule(ScriptEngine, ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
		ASSERT_THAT(IsTrue(BuildResult >= 0,
			*FString::Printf(TEXT("Local variable source should compile. Build=%d Messages={%s}"),
				BuildResult, *Engine.GetMessagesText())));
		ASSERT_THAT(IsNotNull(Module, TEXT("Local variable source should publish a module")));
		if (BuildResult < 0 || Module == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
			ScriptEngine->DiscardModule(OptimizedModuleNameUtf8.Get());
		};

		asIScriptFunction* const Entry = GetNativeFunctionByExactDecl(Module, "int RunLocalEntry()");
		asIScriptFunction* const TypedEntry = GetNativeFunctionByExactDecl(Module, "int ProbeTypedLocals()");
		// The fork canonicalizes value parameters as const in function declarations,
		// even when the source omits the qualifier.
		asIScriptFunction* const UnoptimizedOptimizationShape = GetNativeFunctionByExactDecl(Module, "int ProbeOptimizationShape(const int)");
		ASSERT_THAT(IsNotNull(Entry, TEXT("Local variable product should resolve its entry exactly")));
		ASSERT_THAT(IsNotNull(TypedEntry, TEXT("Local variable product should resolve its typed local entry exactly")));
		ASSERT_THAT(IsNotNull(UnoptimizedOptimizationShape, TEXT("Local variable product should resolve its unoptimized optimization-shape entry exactly")));
		if (Entry == nullptr || TypedEntry == nullptr || UnoptimizedOptimizationShape == nullptr)
		{
			return;
		}
		const TArray<asDWORD> UnoptimizedShapeBytecode = CopyBytecode(UnoptimizedOptimizationShape);
		ASSERT_THAT(IsTrue(UnoptimizedShapeBytecode.Num() > 0, TEXT("Local variable product should expose unoptimized optimization-shape bytecode")));
		ASSERT_THAT(IsTrue(BytecodeContainsOpcode(UnoptimizedOptimizationShape, asBC_INCi), TEXT("Local variable product should retain the split load-and-increment form when bytecode optimization is disabled")));
		ASSERT_THAT(IsFalse(BytecodeContainsOpcode(UnoptimizedOptimizationShape, asBC_IncVi), TEXT("Local variable product should not synthesize the combined increment form when bytecode optimization is disabled")));

		asIScriptContext* const Context = ScriptEngine->CreateContext();
		ASSERT_THAT(IsNotNull(Context, TEXT("Local variable product should create a context")));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};
		asCContext* const RawContext = static_cast<asCContext*>(Context);
		FLocalObservation Observation;
		Observation.TargetDeclaration = TEXT("int ProbeLocals(const int)");
		Context->SetUserData(&Observation, LocalRecorderUserDataSlot);
		ASSERT_THAT(AreEqual(asSUCCESS, RawContext->SetLineCallback(CaptureLocals), TEXT("Local variable product should install its raw line observer")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, Entry), TEXT("Local variable product should execute the nested and loop scopes")));
		ASSERT_THAT(AreEqual(21, static_cast<int32>(Context->GetReturnDWord()), TEXT("Local variable product should preserve the outer local across nested and loop scopes")));
		ASSERT_THAT(IsTrue(Observation.Snapshots.Num() > 0, TEXT("Local variable product should capture active scope snapshots")));
		ASSERT_THAT(IsTrue(Observation.bInvalidVariableIndexWasSafe, TEXT("Local variable product should characterize invalid variable indexes without dereferencing them")));

		const int32 IntTypeId = ScriptEngine->GetTypeIdByDecl("int");
		const FLocalEntry* const Parameter = FindLiveEntry(Observation, TEXT("Parameter"));
		const FLocalEntry* const Local = FindLiveEntry(Observation, TEXT("Local"));
		const FLocalEntry* const Nested = FindLiveEntry(Observation, TEXT("Nested"));
		const FLocalEntry* const LoopLocal = FindLiveEntry(Observation, TEXT("LoopLocal"));
		ASSERT_THAT(IsNotNull(Parameter, TEXT("Local variable product should expose the live parameter role")));
		ASSERT_THAT(IsNotNull(Local, TEXT("Local variable product should expose the live local role")));
		ASSERT_THAT(IsNotNull(Nested, TEXT("Local variable product should expose the live nested-local role")));
		ASSERT_THAT(IsNotNull(LoopLocal, TEXT("Local variable product should expose the live loop-local role")));
		for (const FLocalEntry* const EntryObservation : { Parameter, Local, Nested, LoopLocal })
		{
			if (EntryObservation != nullptr)
			{
				ASSERT_THAT(AreEqual(IntTypeId, EntryObservation->TypeId, TEXT("Local variable product should retain the exact int type id")));
				const FString ExpectedDeclaration = EntryObservation->Name == TEXT("Parameter")
					? FString(TEXT("const int Parameter"))
					: FString(TEXT("int ")) + EntryObservation->Name;
				ASSERT_THAT(IsTrue(EntryObservation->Declaration == ExpectedDeclaration,
					*FString::Printf(TEXT("Local variable product should retain the unqualified declaration for %s (actual={%s})"),
						*EntryObservation->Name, *EntryObservation->Declaration)));
				ASSERT_THAT(IsTrue(EntryObservation->NamespaceDeclaration == EntryObservation->Declaration, TEXT("Local variable product should retain the namespace-qualified declaration for global int locals")));
			}
		}
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Local variable product should unprepare before invalid-state queries")));

		Observation.Snapshots.Reset();
		Observation.bInvalidVariableIndexWasSafe = false;
		Observation.TargetDeclaration = TEXT("int ProbeTypedLocals()");
		const int TypedExecuteResult = PrepareAndExecute(Context, TypedEntry);
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), TypedExecuteResult,
			*FString::Printf(TEXT("Local variable product should execute its typed local roles. Result=%d Exception={%s} Messages={%s}"),
				TypedExecuteResult,
				UTF8_TO_TCHAR(Context->GetExceptionString() != nullptr ? Context->GetExceptionString() : ""),
				*Engine.GetMessagesText())));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()), TEXT("Local variable product should retain the typed local runtime result")));
		ASSERT_THAT(IsTrue(Observation.Snapshots.Num() > 0, TEXT("Local variable product should capture typed local snapshots")));
		struct FExpectedTypedLocal
		{
			const TCHAR* Name;
			const TCHAR* TypeDeclaration;
		};
		const FExpectedTypedLocal ExpectedTypedLocals[] =
		{
			{ TEXT("Int8Value"), TEXT("int8") }, { TEXT("Int16Value"), TEXT("int16") }, { TEXT("IntValue"), TEXT("int") }, { TEXT("Int64Value"), TEXT("int64") },
			{ TEXT("UInt8Value"), TEXT("uint8") }, { TEXT("UInt16Value"), TEXT("uint16") }, { TEXT("UIntValue"), TEXT("uint") }, { TEXT("UInt64Value"), TEXT("uint64") },
			{ TEXT("Float32Value"), TEXT("float") }, { TEXT("Float64Value"), TEXT("float64") }, { TEXT("BoolValue"), TEXT("bool") }, { TEXT("EnumValue"), TEXT("ENativeDebugLocalEnum") },
			// Raw typedefs are intentionally reported using their underlying type by
			// GetVarDeclaration in this fork; the alias type-id is checked below.
			{ TEXT("AliasValue"), TEXT("int") }, { TEXT("ValueObject"), TEXT("NativeDebugLocalValue") }, { TEXT("NativeObject"), TEXT("NativeDebugNativeObject") }, { TEXT("ReferenceValue"), TEXT("NativeDebugLocalReference") }, { TEXT("NullValue"), TEXT("NativeDebugLocalReference") },
		};
		for (const FExpectedTypedLocal& Expected : ExpectedTypedLocals)
		{
			const FLocalEntry* const TypedLocal = FindLiveEntry(Observation, Expected.Name);
			ASSERT_THAT(IsNotNull(TypedLocal, *FString::Printf(TEXT("Local variable product should expose typed local %s"), Expected.Name)));
			if (TypedLocal != nullptr)
			{
				// The fork's display spelling for ttFloat32 is "float", while the
				// public type-id query remains "float32".
				const TCHAR* const TypeIdDeclaration = FCString::Strcmp(Expected.Name, TEXT("Float32Value")) == 0
					? TEXT("float32")
					: Expected.TypeDeclaration;
				const FTCHARToUTF8 TypeUtf8(TypeIdDeclaration);
				int32 ExpectedTypeId = asINVALID_TYPE;
				FString ModuleTypeName = TypeIdDeclaration;
				ModuleTypeName.RemoveFromEnd(TEXT("@"));
				const FTCHARToUTF8 ModuleTypeNameUtf8(*ModuleTypeName);
				if (asITypeInfo* const ModuleType = Module->GetTypeInfoByName(ModuleTypeNameUtf8.Get()))
				{
					ExpectedTypeId = ModuleType->GetTypeId();
				}
				else
				{
					ExpectedTypeId = ScriptEngine->GetTypeIdByDecl(TypeUtf8.Get());
				}
				// Script classes are implicit handles in the active fork.  The
				// handle bit is part of the local variable type id even when the
				// declaration display omits an explicit '@'.
				if ((TypedLocal->TypeId & asTYPEID_OBJHANDLE) != 0)
				{
					ExpectedTypeId |= asTYPEID_OBJHANDLE;
				}
				ASSERT_THAT(AreEqual(ExpectedTypeId, TypedLocal->TypeId,
					*FString::Printf(TEXT("Local variable product should retain the exact type id for %s (expected=%d actual=%d declaration={%s})"),
						Expected.Name, ExpectedTypeId, TypedLocal->TypeId, *TypedLocal->Declaration)));
				ASSERT_THAT(AreEqual(FString::Printf(TEXT("%s %s"), Expected.TypeDeclaration, Expected.Name), TypedLocal->Declaration,
					*FString::Printf(TEXT("Local variable product should retain the exact declaration for %s (actual={%s})"),
						Expected.Name, *TypedLocal->Declaration)));
			}
		}
		if (const FLocalEntry* const AliasValue = FindLiveEntry(Observation, TEXT("AliasValue")))
		{
			ASSERT_THAT(AreEqual(ScriptEngine->GetTypeIdByDecl("NativeDebugLocalAlias"), AliasValue->TypeId,
				TEXT("Local variable product should preserve the raw typedef type id even when declaration display uses int")));
		}
		const FLocalEntry* const ReferenceValue = FindLiveEntry(Observation, TEXT("ReferenceValue"));
		const FLocalEntry* const NullValue = FindLiveEntry(Observation, TEXT("NullValue"));
		if (ReferenceValue != nullptr && NullValue != nullptr)
		{
			ASSERT_THAT(IsNotNull(ReferenceValue->ReferencedObject, TEXT("Local variable product should expose the non-null reference payload")));
			ASSERT_THAT(IsNull(NullValue->ReferencedObject, TEXT("Local variable product should expose the null reference payload")));
		}
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Local variable product should unprepare after typed local observation")));
		ASSERT_THAT(AreEqual(asINVALID_ARG, Context->GetVarCount(0), TEXT("Local variable unprepared count query should return the documented invalid value")));
		ASSERT_THAT(IsNull(Context->GetVarName(0, 0), TEXT("Local variable unprepared name query should be null")));
		ASSERT_THAT(IsNull(Context->GetAddressOfVar(0, 0), TEXT("Local variable unprepared address query should be null")));
		ASSERT_THAT(IsFalse(Context->IsVarInScope(0, 0), TEXT("Local variable unprepared scope query should be false")));
		RawContext->ClearLineCallback();
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(UnoptimizedOptimizationShape), TEXT("Local variable product should prepare its unoptimized optimization-shape entry")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->SetArgDWord(0, 40), TEXT("Local variable product should set the unoptimized optimization-shape argument")));
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Context->Execute(), TEXT("Local variable product should execute its unoptimized optimization-shape entry")));
		ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()), TEXT("Local variable product should retain the unoptimized optimization-shape runtime result")));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Local variable product should unprepare after its unoptimized optimization-shape entry")));

		ASSERT_THAT(AreEqual(asSUCCESS, ScriptEngine->SetEngineProperty(asEP_OPTIMIZE_BYTECODE, 1), TEXT("Local variable product should rebuild under bytecode optimization")));
		asIScriptModule* OptimizedModule = nullptr;
		ReportGeneratedModule(*TestRunner, OptimizedModuleName, Source);
		ASSERT_THAT(IsTrue(CompileNativeModule(ScriptEngine, OptimizedModuleNameUtf8.Get(), SourceUtf8.Get(), OptimizedModule) >= 0, TEXT("Local variable optimized source should compile")));
		ASSERT_THAT(IsNotNull(OptimizedModule, TEXT("Local variable optimized source should publish a module")));
		if (OptimizedModule != nullptr)
		{
			asIScriptFunction* const OptimizedTypedEntry = GetNativeFunctionByExactDecl(OptimizedModule, "int ProbeTypedLocals()");
			asIScriptFunction* const OptimizedOptimizationShape = GetNativeFunctionByExactDecl(OptimizedModule, "int ProbeOptimizationShape(const int)");
			ASSERT_THAT(IsNotNull(OptimizedTypedEntry, TEXT("Local variable optimized source should retain its typed entry declaration")));
			ASSERT_THAT(IsNotNull(OptimizedOptimizationShape, TEXT("Local variable optimized source should retain its optimization-shape entry declaration")));
			if (OptimizedOptimizationShape != nullptr)
			{
				const TArray<asDWORD> OptimizedShapeBytecode = CopyBytecode(OptimizedOptimizationShape);
				const bool bBytecodeBuffersMatch = OptimizedShapeBytecode.Num() == UnoptimizedShapeBytecode.Num()
					&& (OptimizedShapeBytecode.Num() == 0
						|| FMemory::Memcmp(OptimizedShapeBytecode.GetData(), UnoptimizedShapeBytecode.GetData(), OptimizedShapeBytecode.Num() * sizeof(asDWORD)) == 0);
				ASSERT_THAT(IsTrue(OptimizedShapeBytecode.Num() > 0, TEXT("Local variable optimized source should expose optimization-shape bytecode")));
				ASSERT_THAT(IsTrue(BytecodeContainsOpcode(OptimizedOptimizationShape, asBC_IncVi), TEXT("Local variable optimized source should combine the selected load-and-increment sequence")));
				ASSERT_THAT(IsFalse(BytecodeContainsOpcode(OptimizedOptimizationShape, asBC_INCi), TEXT("Local variable optimized source should not retain the split increment instruction")));
				ASSERT_THAT(IsFalse(bBytecodeBuffersMatch, TEXT("Local variable product should expose distinct bytecode for enabled and disabled optimization")));
				ASSERT_THAT(AreEqual(asSUCCESS, Context->Prepare(OptimizedOptimizationShape), TEXT("Local variable optimized source should prepare its optimization-shape entry")));
				ASSERT_THAT(AreEqual(asSUCCESS, Context->SetArgDWord(0, 40), TEXT("Local variable optimized source should set its optimization-shape argument")));
				ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), Context->Execute(), TEXT("Local variable optimized source should execute its optimization-shape entry")));
				ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()), TEXT("Local variable optimized source should retain the unoptimized runtime result")));
				ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Local variable optimized source should unprepare after its optimization-shape entry")));
			}
			if (OptimizedTypedEntry != nullptr)
			{
				Observation.Snapshots.Reset();
				Observation.TargetDeclaration = TEXT("int ProbeTypedLocals()");
				ASSERT_THAT(AreEqual(asSUCCESS, RawContext->SetLineCallback(CaptureLocals), TEXT("Local variable optimized source should install its raw line observer")));
				ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, OptimizedTypedEntry), TEXT("Local variable optimized source should execute its typed local roles")));
				ASSERT_THAT(AreEqual(42, static_cast<int32>(Context->GetReturnDWord()), TEXT("Local variable optimized source should retain the typed local runtime result")));
				ASSERT_THAT(IsNotNull(FindLiveEntry(Observation, TEXT("IntValue")), TEXT("Local variable optimized source should retain a live scalar-local query")));
				ASSERT_THAT(IsNotNull(FindLiveEntry(Observation, TEXT("ValueObject")), TEXT("Local variable optimized source should retain a live value-object query")));
				ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(), TEXT("Local variable optimized source should unprepare after local observation")));
				RawContext->ClearLineCallback();
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
