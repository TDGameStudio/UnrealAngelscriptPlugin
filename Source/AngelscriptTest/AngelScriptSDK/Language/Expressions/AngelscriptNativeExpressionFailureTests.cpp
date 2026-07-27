#include "../References/AngelscriptNativeReferenceTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FExpressionFailureTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Expressions.Failure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeCaseContext = AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeTestEngine = AngelscriptNativeTestSupport::FNativeTestEngine;
	using FNativeLifecycleRecorder = AngelscriptNativeTestSupport::FNativeLifecycleRecorder;
	using FReferenceState = AngelscriptNativeReferenceTestSupport::FReferenceState;

	inline static constexpr asPWORD FailureStateUserDataSlot =
		static_cast<asPWORD>(0x4E41544558464149ull);
	inline static constexpr asDWORD PublicAccessMask = 0x1;
	inline static constexpr asDWORD HiddenAccessMask = 0x2;

	enum class EFailureKind : uint8
	{
		InvalidLValue,
		MissingDelimiter,
		MalformedTernary,
		MissingSymbol,
		AmbiguousSymbol,
		InaccessibleMember,
		MissingMember,
		DivideZero,
		IndexOutOfRange,
		NullAccess,
		ExceptionLeft,
		ExceptionRight,
	};

	enum class EFailureContext : uint8
	{
		Initializer,
		Assignment,
		Argument,
		Return,
		Condition,
		LoopClause,
		SwitchSelector,
		Index,
	};

	enum class ERecoveryRoute : uint8
	{
		FreshModule,
		RebuildOrContextReuse,
	};

	struct FFailureCase
	{
		const ANSICHAR* CatalogName;
		EFailureKind Failure;
	};

	struct FFailureContextCase
	{
		const ANSICHAR* CatalogName;
		EFailureContext Context;
	};

	struct FRecoveryCase
	{
		const ANSICHAR* CatalogName;
		ERecoveryRoute Recovery;
	};

	struct FExpressionFailureState
	{
		void Reset()
		{
			MarkerTrace.Reset();
			CallbackCalls = 0;
		}

		TArray<int32> MarkerTrace;
		int32 CallbackCalls = 0;
	};

	inline static constexpr FFailureCase FailureCases[] = {
		{"invalid_lvalue", EFailureKind::InvalidLValue},
		{"missing_delimiter", EFailureKind::MissingDelimiter},
		{"malformed_ternary", EFailureKind::MalformedTernary},
		{"missing_symbol", EFailureKind::MissingSymbol},
		{"ambiguous_symbol", EFailureKind::AmbiguousSymbol},
		{"inaccessible_member", EFailureKind::InaccessibleMember},
		{"missing_member", EFailureKind::MissingMember},
		{"divide_zero", EFailureKind::DivideZero},
		{"index_out_of_range", EFailureKind::IndexOutOfRange},
		{"null_access", EFailureKind::NullAccess},
		{"exception_left", EFailureKind::ExceptionLeft},
		{"exception_right", EFailureKind::ExceptionRight},
	};

	inline static constexpr FFailureContextCase ContextCases[] = {
		{"initializer", EFailureContext::Initializer},
		{"assignment", EFailureContext::Assignment},
		{"argument", EFailureContext::Argument},
		{"return", EFailureContext::Return},
		{"condition", EFailureContext::Condition},
		{"loop_clause", EFailureContext::LoopClause},
		{"switch_selector", EFailureContext::SwitchSelector},
		{"index", EFailureContext::Index},
	};

	inline static constexpr FRecoveryCase RecoveryCases[] = {
		{"fresh_module", ERecoveryRoute::FreshModule},
		{"rebuild_or_context_reuse", ERecoveryRoute::RebuildOrContextReuse},
	};

	static bool IsCompileFailure(const FFailureCase& FailureCase)
	{
		return FailureCase.Failure == EFailureKind::InvalidLValue ||
			   FailureCase.Failure == EFailureKind::MissingDelimiter ||
			   FailureCase.Failure == EFailureKind::MalformedTernary ||
			   FailureCase.Failure == EFailureKind::MissingSymbol ||
			   FailureCase.Failure == EFailureKind::AmbiguousSymbol ||
			   FailureCase.Failure == EFailureKind::InaccessibleMember ||
			   FailureCase.Failure == EFailureKind::MissingMember;
	}

	static FExpressionFailureState* GetFailureState(asIScriptGeneric& Generic)
	{
		return Generic.GetEngine() != nullptr
				   ? static_cast<FExpressionFailureState*>(
						 Generic.GetEngine()->GetUserData(FailureStateUserDataSlot))
				   : nullptr;
	}

	static void RecordFailureMarker(asIScriptGeneric& Generic, const int32 Marker)
	{
		if (FExpressionFailureState* const State = GetFailureState(Generic))
		{
			++State->CallbackCalls;
			State->MarkerTrace.Add(Marker);
		}
	}

	static void GenericRecordFailureStage(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		const int32 Marker = static_cast<int32>(Generic->GetArgDWord(0));
		RecordFailureMarker(*Generic, Marker);
		Generic->SetReturnDWord(static_cast<asDWORD>(Marker));
	}

	static void GenericThrowFailureStage(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		const int32 Marker = static_cast<int32>(Generic->GetArgDWord(0));
		RecordFailureMarker(*Generic, Marker);
		if (asIScriptContext* const Context = asGetActiveContext())
		{
			Context->SetException(Marker == 1 ? "Expression left operand exception"
											  : "Expression right operand exception");
		}
		Generic->SetReturnDWord(static_cast<asDWORD>(Marker));
	}

	static void GenericRaiseFailureIndex(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		RecordFailureMarker(*Generic, 99);
		if (asIScriptContext* const Context = asGetActiveContext())
		{
			Context->SetException("Expression index out of range");
		}
		Generic->SetReturnDWord(0);
	}

	static bool RegisterFailureFixtures(asIScriptEngine& Engine,
		FExpressionFailureState& State,
		FNativeLifecycleRecorder& Lifecycle,
		FReferenceState& ReferenceState)
	{
		using namespace AngelscriptNativeReferenceTestSupport;
		using namespace AngelscriptNativeTestSupport;

		Engine.SetUserData(&State, FailureStateUserDataSlot);
		if (!RegisterNativeCaseValue(Engine, Lifecycle) ||
			!RegisterReferenceFixtures(Engine, ReferenceState) ||
			Engine.RegisterGlobalFunction("int RecordFailureStage(int Marker)",
				asFUNCTION(GenericRecordFailureStage),
				asCALL_GENERIC) < 0 ||
			Engine.RegisterGlobalFunction("int ThrowFailureStage(int Marker)",
				asFUNCTION(GenericThrowFailureStage),
				asCALL_GENERIC) < 0 ||
			Engine.RegisterGlobalFunction("int RaiseFailureIndex()",
				asFUNCTION(GenericRaiseFailureIndex),
				asCALL_GENERIC) < 0)
		{
			return false;
		}

		return true;
	}

	static void AppendFailureDeclarations(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("int ResolveAmbiguousFailure(FRefRoot Value)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 1;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int ResolveAmbiguousFailure(FRefUnrelated Value)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 2;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);

		AppendGeneratedAsLine(Source, TEXT("struct FFailureRestricted"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tprivate int HiddenFailureMember;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);

		AppendGeneratedAsLine(Source, TEXT("struct FFailureIndex"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint opIndex(int Index) const"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn RaiseFailureIndex();"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);

		AppendGeneratedAsLine(Source, TEXT("struct FFailureContextIndex"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint opIndex(int Index) const"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn Index;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);

		AppendGeneratedAsLine(Source, TEXT("int ObserveFailure(int Value)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static FString FailureExpression(const FFailureCase& FailureCase)
	{
		switch (FailureCase.Failure)
		{
		case EFailureKind::InvalidLValue:
			return TEXT("((1 + 2) = 3)");
		case EFailureKind::MissingDelimiter:
			return TEXT("RecordFailureStage(1]");
		case EFailureKind::MalformedTernary:
			return TEXT("(true ? RecordFailureStage(1))");
		case EFailureKind::MissingSymbol:
			return TEXT("MissingFailureSymbol");
		case EFailureKind::AmbiguousSymbol:
			return TEXT("ResolveAmbiguousFailure(nullptr)");
		case EFailureKind::InaccessibleMember:
			return TEXT("HiddenOwner.HiddenFailureMember");
		case EFailureKind::MissingMember:
			return TEXT("MissingOwner.MissingFailureMember");
		case EFailureKind::DivideZero:
			return TEXT("RecordFailureStage(1) / RecordFailureStage(0)");
		case EFailureKind::IndexOutOfRange:
			return TEXT("FailureIndex[RecordFailureStage(9)]");
		case EFailureKind::NullAccess:
			return TEXT("RecordFailureStage(1) + NullFailure.GetValue()");
		case EFailureKind::ExceptionLeft:
			return TEXT("ThrowFailureStage(1) + RecordFailureStage(2)");
		case EFailureKind::ExceptionRight:
			return TEXT("RecordFailureStage(1) + ThrowFailureStage(2)");
		default:
			return TEXT("0");
		}
	}

	static FString FailureToken(const FFailureCase& FailureCase)
	{
		switch (FailureCase.Failure)
		{
		case EFailureKind::InvalidLValue:
			return TEXT("1 + 2");
		case EFailureKind::MissingDelimiter:
			return TEXT("RecordFailureStage(1]");
		case EFailureKind::MalformedTernary:
			return TEXT("true ? RecordFailureStage(1)");
		case EFailureKind::MissingSymbol:
			return TEXT("MissingFailureSymbol");
		case EFailureKind::AmbiguousSymbol:
			return TEXT("ResolveAmbiguousFailure(nullptr)");
		case EFailureKind::InaccessibleMember:
			return TEXT("HiddenFailureMember");
		case EFailureKind::MissingMember:
			return TEXT("MissingFailureMember");
		case EFailureKind::DivideZero:
			return TEXT("RecordFailureStage(0)");
		case EFailureKind::IndexOutOfRange:
			return TEXT("RaiseFailureIndex");
		case EFailureKind::NullAccess:
			return TEXT("NullFailure.GetValue");
		case EFailureKind::ExceptionLeft:
			return TEXT("ThrowFailureStage(1)");
		case EFailureKind::ExceptionRight:
			return TEXT("ThrowFailureStage(2)");
		default:
			return FString();
		}
	}

	static void AppendFailureSetup(FString& Source,
		const FFailureCase& FailureCase,
		const bool bRuntimeFailure,
		const TCHAR* Indent)
	{
		using namespace AngelscriptNativeTestSupport;

		if (bRuntimeFailure)
		{
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("%sFNativeCaseValue ScopeValue(77);"), Indent));
		}
		switch (FailureCase.Failure)
		{
		case EFailureKind::InaccessibleMember:
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("%sFFailureRestricted HiddenOwner;"), Indent));
			break;
		case EFailureKind::MissingMember:
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("%sFNativeCaseValue MissingOwner(37);"), Indent));
			break;
		case EFailureKind::IndexOutOfRange:
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("%sFFailureIndex FailureIndex;"), Indent));
			break;
		case EFailureKind::NullAccess:
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("%sFRefRoot NullFailure = nullptr;"), Indent));
			break;
		default:
			break;
		}
	}

	static void AppendFailureUse(
		FString& Source, const FFailureContextCase& ContextCase, const FString& Expression)
	{
		using namespace AngelscriptNativeTestSupport;

		switch (ContextCase.Context)
		{
		case EFailureContext::Initializer:
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tint Result = %s;"), *Expression));
			AppendGeneratedAsLine(Source, TEXT("\treturn Result;"));
			break;
		case EFailureContext::Assignment:
			AppendGeneratedAsLine(Source, TEXT("\tint Result = 0;"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tResult = %s;"), *Expression));
			AppendGeneratedAsLine(Source, TEXT("\treturn Result;"));
			break;
		case EFailureContext::Argument:
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("\treturn ObserveFailure(%s);"), *Expression));
			break;
		case EFailureContext::Condition:
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tif (%s != 0)"), *Expression));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 1;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 0;"));
			break;
		case EFailureContext::LoopClause:
			AppendGeneratedAsLine(Source, TEXT("\tint Count = 0;"));
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("\tfor (; %s != 0; ++Count)"), *Expression));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn Count;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Count;"));
			break;
		case EFailureContext::SwitchSelector:
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tswitch (%s)"), *Expression));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\tcase 0:"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 0;"));
			AppendGeneratedAsLine(Source, TEXT("\tdefault:"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 1;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			break;
		case EFailureContext::Index:
			AppendGeneratedAsLine(Source, TEXT("\tFFailureContextIndex ContextIndex;"));
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("\treturn ContextIndex[%s];"), *Expression));
			break;
		case EFailureContext::Return:
			checkNoEntry();
			break;
		}
	}

	static FString BuildExpressionFailureSource(
		const FFailureCase& FailureCase, const FFailureContextCase& ContextCase)
	{
		using namespace AngelscriptNativeTestSupport;

		const bool bRuntimeFailure = !IsCompileFailure(FailureCase);
		const FString Expression = FailureExpression(FailureCase);
		FString Source;
		AppendFailureDeclarations(Source);
		if (ContextCase.Context == EFailureContext::Return)
		{
			AppendGeneratedAsLine(Source, TEXT("int ProduceFailure()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendFailureSetup(Source, FailureCase, bRuntimeFailure, TEXT("\t"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\treturn %s;"), *Expression));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}

		AppendGeneratedAsLine(Source, TEXT("int RunExpressionFailure()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (ContextCase.Context == EFailureContext::Return)
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn ProduceFailure();"));
		}
		else
		{
			AppendFailureSetup(Source, FailureCase, bRuntimeFailure, TEXT("\t"));
			AppendFailureUse(Source, ContextCase, Expression);
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RecoverExpressionFailure()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 191;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static FString BuildExpressionFailureRecoverySource()
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int RecoverExpressionFailure()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 191;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	asIScriptModule* CompileAndReport(FNativeTestEngine& Engine,
		const FNativeCaseContext& Case,
		const FString& SourceId,
		const FString& ModuleName,
		const FString& Source,
		int32& OutBuildResult)
	{
		using namespace AngelscriptNativeTestSupport;

		PrintGeneratedAsSource(*TestRunner, SourceId, ModuleName, Source);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		Engine.ResetMessages();
		asIScriptModule* const Module =
			Engine.Get()->GetModule(ModuleNameUtf8.Get(), asGM_ALWAYS_CREATE);
		if (Module == nullptr)
		{
			OutBuildResult = asERROR;
			return nullptr;
		}
		Module->SetAccessMask(PublicAccessMask);
		const int32 AddResult =
			Module->AddScriptSection(ModuleNameUtf8.Get(), SourceUtf8.Get(), SourceUtf8.Length());
		OutBuildResult = AddResult >= 0 ? Module->Build() : AddResult;
		return Module;
	}

	static int32 LastSourceLineContaining(const FString& Source, const FString& Token)
	{
		TArray<FString> Lines;
		Source.ParseIntoArrayLines(Lines, false);
		for (int32 Index = Lines.Num() - 1; Index >= 0; --Index)
		{
			if (Lines[Index].Contains(Token))
			{
				return Index + 1;
			}
		}
		return INDEX_NONE;
	}

	static TArray<AngelscriptNativeTestSupport::FNativeMessageEntry> LocatedErrors(
		const AngelscriptNativeTestSupport::FNativeMessageCollector& Messages,
		const FString& ModuleName)
	{
		TArray<AngelscriptNativeTestSupport::FNativeMessageEntry> Errors;
		for (const AngelscriptNativeTestSupport::FNativeMessageEntry& Entry : Messages.Entries)
		{
			if (Entry.Type == asMSGTYPE_ERROR && Entry.Section == ModuleName && Entry.Row > 0 &&
				Entry.Column > 0)
			{
				Errors.Add(Entry);
			}
		}
		return Errors;
	}

	static TArray<int32> ExpectedTrace(const FFailureCase& FailureCase)
	{
		switch (FailureCase.Failure)
		{
		case EFailureKind::DivideZero:
			return {1, 0};
		case EFailureKind::IndexOutOfRange:
			return {9, 99};
		case EFailureKind::NullAccess:
		case EFailureKind::ExceptionLeft:
			return {1};
		case EFailureKind::ExceptionRight:
			return {1, 2};
		default:
			return {};
		}
	}

	static FString ExpectedExceptionText(const FFailureCase& FailureCase)
	{
		switch (FailureCase.Failure)
		{
		case EFailureKind::DivideZero:
			return TEXT("Divide by zero");
		case EFailureKind::IndexOutOfRange:
			return TEXT("Expression index out of range");
		case EFailureKind::NullAccess:
			return TEXT("Null pointer access");
		case EFailureKind::ExceptionLeft:
			return TEXT("Expression left operand exception");
		case EFailureKind::ExceptionRight:
			return TEXT("Expression right operand exception");
		default:
			return FString();
		}
	}

	static bool TracesEqual(const TArray<int32>& Left, const TArray<int32>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			if (Left[Index] != Right[Index])
			{
				return false;
			}
		}
		return true;
	}

	static bool HasLocalName(asIScriptFunction& Function, const ANSICHAR* ExpectedName)
	{
		for (asUINT Index = 0; Index < Function.GetVarCount(); ++Index)
		{
			const char* Name = nullptr;
			if (Function.GetVar(Index, &Name) >= 0 && Name != nullptr &&
				FCStringAnsi::Strcmp(Name, ExpectedName) == 0)
			{
				return true;
			}
		}
		return false;
	}

	void ExecuteRecovery(const FNativeCaseContext& Case,
		asIScriptEngine& Engine,
		asIScriptModule& Module,
		asIScriptContext* ReusedContext = nullptr)
	{
		using namespace AngelscriptNativeTestSupport;

		asIScriptFunction* const Recovery =
			GetNativeFunctionByExactDecl(&Module, "int RecoverExpressionFailure()");
		ASSERT_THAT(IsNotNull(
			Recovery, *Case.Describe(TEXT("expression failure recovery should resolve exactly"))));
		asIScriptContext* Context = ReusedContext;
		if (Context == nullptr)
		{
			Context = Engine.CreateContext();
		}
		ASSERT_THAT(IsNotNull(
			Context, *Case.Describe(TEXT("expression failure recovery should obtain a context"))));
		if (Recovery != nullptr && Context != nullptr)
		{
			ASSERT_THAT(AreEqual(asSUCCESS,
				Context->Prepare(Recovery),
				*Case.Describe(TEXT("expression failure recovery should prepare"))));
			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
				Context->Execute(),
				*Case.Describe(TEXT("expression failure recovery should finish"))));
			ASSERT_THAT(AreEqual(191,
				static_cast<int32>(Context->GetReturnDWord()),
				*Case.Describe(TEXT("expression failure recovery should return its sentinel"))));
			ASSERT_THAT(AreEqual(asSUCCESS,
				Context->Unprepare(),
				*Case.Describe(TEXT("expression failure recovery should unprepare cleanly"))));
		}
		if (ReusedContext == nullptr && Context != nullptr)
		{
			Context->Release();
		}
	}

	void ExecuteRuntimeFailure(const FNativeCaseContext& Case,
		const FFailureCase& FailureCase,
		const FFailureContextCase& ContextCase,
		const FRecoveryCase& RecoveryCase,
		const FString& ModuleName,
		const FString& Source,
		FExpressionFailureState& State,
		FNativeLifecycleRecorder& Lifecycle,
		FReferenceState& ReferenceState,
		asIScriptEngine& Engine,
		asIScriptModule& Module)
	{
		using namespace AngelscriptNativeTestSupport;

		asIScriptFunction* const Entry =
			GetNativeFunctionByExactDecl(&Module, "int RunExpressionFailure()");
		ASSERT_THAT(IsNotNull(
			Entry, *Case.Describe(TEXT("runtime expression failure should expose its entry"))));
		asIScriptFunction* const Owner = ContextCase.Context == EFailureContext::Return
											 ? Module.GetFunctionByDecl("int ProduceFailure()")
											 : Entry;
		ASSERT_THAT(IsNotNull(Owner,
			*Case.Describe(TEXT("runtime expression failure should retain its owning function"))));
		asIScriptContext* const Context = Engine.CreateContext();
		ASSERT_THAT(IsNotNull(
			Context, *Case.Describe(TEXT("runtime expression failure should create a context"))));
		if (Entry == nullptr || Owner == nullptr || Context == nullptr)
		{
			if (Context != nullptr)
			{
				Context->Release();
			}
			return;
		}
		ASSERT_THAT(AreEqual(Engine.GetTypeIdByDecl("int"),
			Entry->GetReturnTypeId(),
			*Case.Describe(
				TEXT("runtime expression failure should retain its exact return type"))));
		ASSERT_THAT(AreEqual(0,
			static_cast<int32>(Entry->GetParamCount()),
			*Case.Describe(TEXT("runtime expression failure should retain its exact arity"))));
		ASSERT_THAT(IsTrue(HasLocalName(*Owner, "ScopeValue"),
			*Case.Describe(
				TEXT("runtime expression failure should retain its tracked scope local"))));
		if (FailureCase.Failure == EFailureKind::IndexOutOfRange)
		{
			asITypeInfo* const IndexType = Module.GetTypeInfoByDecl("FFailureIndex");
			asIScriptFunction* const IndexMethod =
				IndexType != nullptr ? IndexType->GetMethodByName("opIndex") : nullptr;
			int IndexParameterTypeId = asTYPEID_VOID;
			ASSERT_THAT(IsTrue(IndexMethod != nullptr &&
							   IndexMethod->GetReturnTypeId() == asTYPEID_INT32 &&
							   IndexMethod->GetParamCount() == 1 &&
							   IndexMethod->GetParam(0, &IndexParameterTypeId) >= 0 &&
							   IndexParameterTypeId == asTYPEID_INT32,
				*Case.Describe(
					TEXT("index failure should retain its exact operator metadata"))));
		}
		if (FailureCase.Failure == EFailureKind::NullAccess)
		{
			asITypeInfo* const ReferenceType = Engine.GetTypeInfoByDecl("FRefRoot");
			ASSERT_THAT(IsTrue(ReferenceType != nullptr &&
					ReferenceType->GetMethodByDecl("int GetValue() const") != nullptr,
				*Case.Describe(
					TEXT("null failure should retain its exact member metadata"))));
		}

		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION),
			PrepareAndExecute(Context, Entry),
			*Case.Describe(TEXT("runtime expression failure should stop with an exception"))));
		ASSERT_THAT(AreEqual(ExpectedExceptionText(FailureCase),
			FString(UTF8_TO_TCHAR(Context->GetExceptionString())),
			*Case.Describe(TEXT("runtime expression failure should retain its exact cause"))));
		ASSERT_THAT(IsNotNull(Context->GetExceptionFunction(),
			*Case.Describe(TEXT("runtime expression failure should retain its function"))));
		const char* ExceptionSection = nullptr;
		int32 ExceptionColumn = INDEX_NONE;
		const int32 ExceptionLine =
			Context->GetExceptionLineNumber(&ExceptionColumn, &ExceptionSection);
		ASSERT_THAT(IsTrue(ExceptionLine > 0 && ExceptionColumn > 0,
			*Case.Describe(TEXT("runtime expression failure should retain line and column"))));
		ASSERT_THAT(AreEqual(ModuleName,
			FString(UTF8_TO_TCHAR(ExceptionSection != nullptr ? ExceptionSection : "")),
			*Case.Describe(
				TEXT("runtime expression failure should retain its generated section"))));
		const int32 ExpectedLine = LastSourceLineContaining(Source, FailureToken(FailureCase));
		ASSERT_THAT(AreEqual(ExpectedLine,
			ExceptionLine,
			*Case.Describe(
				TEXT("runtime expression failure should stop on its causal source line"))));
		ASSERT_THAT(IsTrue(TracesEqual(State.MarkerTrace, ExpectedTrace(FailureCase)),
			*Case.Describe(
				TEXT("runtime expression failure should stop at the exact marker prefix"))));
		ASSERT_THAT(AreEqual(State.MarkerTrace.Num(),
			State.CallbackCalls,
			*Case.Describe(TEXT("runtime expression failure should record every callback once"))));
		ASSERT_THAT(AreEqual(asSUCCESS,
			Context->Unprepare(),
			*Case.Describe(TEXT("runtime expression failure should unprepare cleanly"))));
		ASSERT_THAT(AreEqual(0,
			Lifecycle.GetLiveObjectCount(),
			*Case.Describe(TEXT("runtime expression failure should destroy its scope value"))));
		ASSERT_THAT(AreEqual(0,
			ReferenceState.LiveObjects,
			*Case.Describe(
				TEXT("runtime expression failure should retain no null reference object"))));
		if (RecoveryCase.Recovery == ERecoveryRoute::RebuildOrContextReuse)
		{
			ExecuteRecovery(Case, Engine, Module, Context);
		}
		Context->Release();
	}

	void CompileAndExecuteRecoveryModule(FNativeTestEngine& Engine,
		const FNativeCaseContext& Case,
		const FString& SourceId,
		const FString& ModuleName)
	{
		const FString RecoverySource = BuildExpressionFailureRecoverySource();
		int32 RecoveryBuildResult = asERROR;
		asIScriptModule* const RecoveryModule = CompileAndReport(
			Engine, Case, SourceId, ModuleName, RecoverySource, RecoveryBuildResult);
		ASSERT_THAT(IsTrue(RecoveryBuildResult >= 0,
			*Case.DescribeResult("<recovery build>",
				TEXT("successful expression failure recovery"),
				Engine.GetMessagesText())));
		if (RecoveryBuildResult >= 0 && RecoveryModule != nullptr)
		{
			ExecuteRecovery(Case, *Engine.Get(), *RecoveryModule);
		}
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		Engine.Get()->DiscardModule(ModuleNameUtf8.Get());
		ASSERT_THAT(IsNull(Engine.Get()->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
			*Case.Describe(TEXT("expression failure recovery should discard its module"))));
	}

	void RunCell(FNativeTestEngine& Engine,
		FExpressionFailureState& State,
		FNativeLifecycleRecorder& Lifecycle,
		FReferenceState& ReferenceState,
		const FFailureContextCase& ContextCase,
		const FFailureCase& FailureCase,
		const FRecoveryCase& RecoveryCase)
	{
		using namespace AngelscriptNativeTestSupport;

		State.Reset();
		ASSERT_THAT(AreEqual(0,
			Lifecycle.GetLiveObjectCount(),
			TEXT("expression failure lifecycle should be empty before reset")));
		Lifecycle.Reset();
		ReferenceState.ResetCounters();
		const FNativeCaseContext Case(MakeNativeCaseId("LANG-EXPR-FAILURE",
			{
				ANSI_TO_TCHAR(ContextCase.CatalogName),
				ANSI_TO_TCHAR(FailureCase.CatalogName),
				ANSI_TO_TCHAR(RecoveryCase.CatalogName),
			}));
		const FString ModuleName = FString::Printf(TEXT("ExpressionFailure_%hs_%hs_%hs"),
			ContextCase.CatalogName,
			FailureCase.CatalogName,
			RecoveryCase.CatalogName);
		const FString Source = BuildExpressionFailureSource(FailureCase, ContextCase);
		int32 BuildResult = asERROR;
		asIScriptModule* Module =
			CompileAndReport(Engine, Case, Case.GetId(), ModuleName, Source, BuildResult);

		if (IsCompileFailure(FailureCase))
		{
			ASSERT_THAT(IsTrue(BuildResult < 0,
				*Case.Describe(TEXT("compile-time expression failure should reject its source"))));
			const TArray<FNativeMessageEntry> Errors =
				LocatedErrors(Engine.GetMessages(), ModuleName);
			const int32 ExpectedLine = LastSourceLineContaining(Source, FailureToken(FailureCase));
			ASSERT_THAT(
				IsTrue(ExpectedLine > 0 && Errors.ContainsByPredicate(
											   [ExpectedLine](const FNativeMessageEntry& Entry)
											   { return Entry.Row == ExpectedLine; }),
					*Case.DescribeResult("<compile diagnostic>",
						TEXT("located error on the causal expression line"),
						Engine.GetMessagesText())));
			ASSERT_THAT(AreEqual(0,
				State.CallbackCalls,
				*Case.Describe(
					TEXT("compile-time expression failure should execute no callback"))));
			ASSERT_THAT(AreEqual(0,
				Lifecycle.GetLiveObjectCount(),
				*Case.Describe(TEXT("compile-time expression failure should retain no value"))));
		}
		else
		{
			ASSERT_THAT(IsTrue(BuildResult >= 0,
				*Case.DescribeResult("<module build>",
					TEXT("successful runtime-failure compilation"),
					Engine.GetMessagesText())));
			if (BuildResult >= 0 && Module != nullptr)
			{
				ExecuteRuntimeFailure(Case,
					FailureCase,
					ContextCase,
					RecoveryCase,
					ModuleName,
					Source,
					State,
					Lifecycle,
					ReferenceState,
					*Engine.Get(),
					*Module);
			}
		}

		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		Engine.Get()->DiscardModule(ModuleNameUtf8.Get());
		ASSERT_THAT(IsNull(Engine.Get()->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
			*Case.Describe(TEXT("expression failure should discard its isolated module"))));
		if (RecoveryCase.Recovery == ERecoveryRoute::FreshModule)
		{
			const FString RecoveryModuleName = ModuleName + TEXT("_FreshRecovery");
			CompileAndExecuteRecoveryModule(
				Engine, Case, Case.GetId() + TEXT("-FRESH-RECOVERY"), RecoveryModuleName);
		}
		else if (IsCompileFailure(FailureCase))
		{
			CompileAndExecuteRecoveryModule(
				Engine, Case, Case.GetId() + TEXT("-REBUILD-RECOVERY"), ModuleName);
		}

		ASSERT_THAT(AreEqual(0,
			Lifecycle.GetLiveObjectCount(),
			*Case.Describe(TEXT("expression failure cell should retain no native value"))));
		ASSERT_THAT(AreEqual(0,
			ReferenceState.LiveObjects,
			*Case.Describe(TEXT("expression failure cell should retain no reference object"))));
	}

public:
	TEST_METHOD(FailuresByContextAndRecovery)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-EXPR-FAILURE",
			ENativeEvidence::Compile | ENativeEvidence::Diagnostic | ENativeEvidence::Runtime |
				ENativeEvidence::Debug | ENativeEvidence::Lifecycle | ENativeEvidence::Cleanup |
				ENativeEvidence::Isolation);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("Expression failure product should create a standalone raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FExpressionFailureState State;
		FNativeLifecycleRecorder Lifecycle;
		FReferenceState ReferenceState;
		const bool bFixturesRegistered =
			RegisterFailureFixtures(*ScriptEngine, State, Lifecycle, ReferenceState);
		ASSERT_THAT(IsTrue(bFixturesRegistered,
			*FNativeCaseContext(TEXT("LANG-EXPR-FAILURE-FIXTURES")).DescribeResult(
				"<fixture registration>",
				TEXT("core failure fixtures"),
				Engine.GetMessagesText())));
		for (const FFailureContextCase& ContextCase : ContextCases)
		{
			for (const FFailureCase& FailureCase : FailureCases)
			{
				for (const FRecoveryCase& RecoveryCase : RecoveryCases)
				{
					RunCell(Engine,
						State,
						Lifecycle,
						ReferenceState,
						ContextCase,
						FailureCase,
						RecoveryCase);
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
