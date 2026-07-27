#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FExpressionChainTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Expressions.Chain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeCaseContext = AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeMessageCollector = AngelscriptNativeTestSupport::FNativeMessageCollector;
	using FNativeTestEngine = AngelscriptNativeTestSupport::FNativeTestEngine;

	inline static constexpr asPWORD ChainStateUserDataSlot =
		static_cast<asPWORD>(0x4E4154455843484Eull);

	enum class EChainOperation : uint8
	{
		Call,
		Member,
		Index,
		Cast,
	};

	enum class EChainState : uint8
	{
		Valid,
		NullReceiver,
		InvalidIntermediate,
		ExceptionIntermediate,
		MissingTerminal,
	};

	enum class EChainContext : uint8
	{
		Initializer,
		Argument,
		Return,
		Condition,
		Assignment,
	};

	struct FChainShapeCase
	{
		const ANSICHAR* CatalogName;
		const EChainOperation* Pattern;
		int32 PatternCount;
	};

	struct FChainDepthCase
	{
		const ANSICHAR* CatalogName;
		int32 Depth;
	};

	struct FChainStateCase
	{
		const ANSICHAR* CatalogName;
		EChainState State;
	};

	struct FChainContextCase
	{
		const ANSICHAR* CatalogName;
		EChainContext Context;
	};

	struct FExpressionChainState
	{
		void Reset(const bool bInReturnNull, const int32 InThrowAtStage)
		{
			check(LiveObjects == 0);
			bReturnNull = bInReturnNull;
			ThrowAtStage = InThrowAtStage;
			NextStage = 1;
			FactoryCalls = 0;
			Created = 0;
			Destroyed = 0;
			AddRefCalls = 0;
			ReleaseCalls = 0;
			LiveObjects = 0;
			VisitedStages.Reset();
			VisitedKinds.Reset();
		}

		bool bReturnNull = false;
		int32 ThrowAtStage = INDEX_NONE;
		int32 NextStage = 1;
		int32 FactoryCalls = 0;
		int32 Created = 0;
		int32 Destroyed = 0;
		int32 AddRefCalls = 0;
		int32 ReleaseCalls = 0;
		int32 LiveObjects = 0;
		TArray<int32> VisitedStages;
		TArray<int32> VisitedKinds;
	};

	struct FExpressionChainNode
	{
		explicit FExpressionChainNode(FExpressionChainState& InState) : State(&InState)
		{
			++State->Created;
			++State->LiveObjects;
		}

		~FExpressionChainNode()
		{
			++State->Destroyed;
			--State->LiveObjects;
		}

		void AddRef()
		{
			++ReferenceCount;
			++State->AddRefCalls;
		}

		void Release()
		{
			++State->ReleaseCalls;
			--ReferenceCount;
			if (ReferenceCount == 0)
			{
				delete this;
			}
		}

		bool Visit(const EChainOperation Operation)
		{
			const int32 Stage = State->NextStage++;
			State->VisitedStages.Add(Stage);
			State->VisitedKinds.Add(static_cast<int32>(Operation));
			Value += OperationValue(Operation);
			if (Stage == State->ThrowAtStage)
			{
				if (asIScriptContext* const Context = asGetActiveContext())
				{
					Context->SetException("Expression chain intermediate exception");
				}
				return false;
			}
			return true;
		}

		static int32 OperationValue(const EChainOperation Operation)
		{
			switch (Operation)
			{
			case EChainOperation::Call:
				return 1;
			case EChainOperation::Member:
				return 3;
			case EChainOperation::Index:
				return 5;
			case EChainOperation::Cast:
				return 7;
			default:
				return 0;
			}
		}

		FExpressionChainState* State = nullptr;
		int32 Value = 100;
		int32 ReferenceCount = 1;
	};

	inline static constexpr EChainOperation CallMemberPattern[] = {
		EChainOperation::Call,
		EChainOperation::Member,
	};

	inline static constexpr EChainOperation MemberCallPattern[] = {
		EChainOperation::Member,
		EChainOperation::Call,
	};

	inline static constexpr EChainOperation IndexMemberPattern[] = {
		EChainOperation::Index,
		EChainOperation::Member,
	};

	inline static constexpr EChainOperation MemberIndexPattern[] = {
		EChainOperation::Member,
		EChainOperation::Index,
	};

	inline static constexpr EChainOperation CastMemberPattern[] = {
		EChainOperation::Cast,
		EChainOperation::Member,
	};

	inline static constexpr EChainOperation CallIndexCastPattern[] = {
		EChainOperation::Call,
		EChainOperation::Index,
		EChainOperation::Cast,
	};

	inline static constexpr EChainOperation MemberCallIndexPattern[] = {
		EChainOperation::Member,
		EChainOperation::Call,
		EChainOperation::Index,
	};

	inline static constexpr EChainOperation CastCallMemberIndexPattern[] = {
		EChainOperation::Cast,
		EChainOperation::Call,
		EChainOperation::Member,
		EChainOperation::Index,
	};

	inline static constexpr FChainShapeCase ShapeCases[] = {
		{"call_member", CallMemberPattern, UE_ARRAY_COUNT(CallMemberPattern)},
		{"member_call", MemberCallPattern, UE_ARRAY_COUNT(MemberCallPattern)},
		{"index_member", IndexMemberPattern, UE_ARRAY_COUNT(IndexMemberPattern)},
		{"member_index", MemberIndexPattern, UE_ARRAY_COUNT(MemberIndexPattern)},
		{"cast_member", CastMemberPattern, UE_ARRAY_COUNT(CastMemberPattern)},
		{"call_index_cast", CallIndexCastPattern, UE_ARRAY_COUNT(CallIndexCastPattern)},
		{"member_call_index", MemberCallIndexPattern, UE_ARRAY_COUNT(MemberCallIndexPattern)},
		{"cast_call_member_index",
			CastCallMemberIndexPattern,
			UE_ARRAY_COUNT(CastCallMemberIndexPattern)},
	};

	inline static constexpr FChainDepthCase DepthCases[] = {
		{"two", 2},
		{"three", 3},
		{"eight", 8},
		{"deep_boundary", 32},
	};

	inline static constexpr FChainStateCase StateCases[] = {
		{"valid", EChainState::Valid},
		{"null_receiver", EChainState::NullReceiver},
		{"invalid_intermediate", EChainState::InvalidIntermediate},
		{"exception_intermediate", EChainState::ExceptionIntermediate},
		{"missing_terminal", EChainState::MissingTerminal},
	};

	inline static constexpr FChainContextCase ContextCases[] = {
		{"initializer", EChainContext::Initializer},
		{"argument", EChainContext::Argument},
		{"return", EChainContext::Return},
		{"condition", EChainContext::Condition},
		{"assignment", EChainContext::Assignment},
	};

	static FExpressionChainState* GetChainState(asIScriptGeneric& Generic)
	{
		return Generic.GetEngine() != nullptr
				   ? static_cast<FExpressionChainState*>(
						 Generic.GetEngine()->GetUserData(ChainStateUserDataSlot))
				   : nullptr;
	}

	static FExpressionChainNode* GetChainObject(asIScriptGeneric& Generic)
	{
		return static_cast<FExpressionChainNode*>(Generic.GetObject());
	}

	static void GenericChainAddRef(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			if (FExpressionChainNode* const Object = GetChainObject(*Generic))
			{
				Object->AddRef();
			}
		}
	}

	static void GenericChainRelease(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			if (FExpressionChainNode* const Object = GetChainObject(*Generic))
			{
				Object->Release();
			}
		}
	}

	static void ReturnVisitedChainObject(
		asIScriptGeneric* Generic, const EChainOperation Operation)
	{
		if (Generic == nullptr)
		{
			return;
		}

		FExpressionChainNode* const Object = GetChainObject(*Generic);
		if (Object != nullptr && Object->Visit(Operation))
		{
			Object->AddRef();
			Generic->SetReturnAddress(Object);
		}
		else
		{
			Generic->SetReturnAddress(nullptr);
		}
	}

	static void GenericChainCall(asIScriptGeneric* Generic)
	{
		ReturnVisitedChainObject(Generic, EChainOperation::Call);
	}

	static void GenericChainMember(asIScriptGeneric* Generic)
	{
		ReturnVisitedChainObject(Generic, EChainOperation::Member);
	}

	static void GenericChainIndex(asIScriptGeneric* Generic)
	{
		ReturnVisitedChainObject(Generic, EChainOperation::Index);
	}

	static void GenericChainCast(asIScriptGeneric* Generic)
	{
		ReturnVisitedChainObject(Generic, EChainOperation::Cast);
	}

	static void GenericChainBreakToInt(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			Generic->SetReturnDWord(31);
		}
	}

	static void GenericMakeChain(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}

		FExpressionChainState* const State = GetChainState(*Generic);
		if (State == nullptr)
		{
			Generic->SetReturnAddress(nullptr);
			return;
		}
		++State->FactoryCalls;
		Generic->SetReturnAddress(State->bReturnNull ? nullptr : new FExpressionChainNode(*State));
	}

	static bool RegisterChainReferenceType(asIScriptEngine& Engine, const ANSICHAR* TypeName)
	{
		return Engine.RegisterObjectType(TypeName, 0, asOBJ_REF | asOBJ_IMPLICIT_HANDLE) >= 0 &&
			   Engine.RegisterObjectBehaviour(TypeName,
				   asBEHAVE_ADDREF,
				   "void f()",
				   asFUNCTION(GenericChainAddRef),
				   asCALL_GENERIC) >= 0 &&
			   Engine.RegisterObjectBehaviour(TypeName,
				   asBEHAVE_RELEASE,
				   "void f()",
				   asFUNCTION(GenericChainRelease),
				   asCALL_GENERIC) >= 0 &&
			   Engine.RegisterObjectProperty(
				   TypeName, "int Value", asOFFSET(FExpressionChainNode, Value)) >= 0;
	}

	static bool RegisterChainSurface(asIScriptEngine& Engine, const ANSICHAR* TypeName)
	{
		return Engine.RegisterObjectMethod(TypeName,
				   "FChainNode StepCall()",
				   asFUNCTION(GenericChainCall),
				   asCALL_GENERIC) >= 0 &&
			   Engine.RegisterObjectMethod(TypeName,
				   "FChainNode GetMember()",
				   asFUNCTION(GenericChainMember),
				   asCALL_GENERIC) >= 0 &&
			   Engine.RegisterObjectMethod(TypeName,
				   "FChainNode opIndex(int Index)",
				   asFUNCTION(GenericChainIndex),
				   asCALL_GENERIC) >= 0 &&
			   Engine.RegisterObjectMethod(TypeName,
				   "int BreakToInt()",
				   asFUNCTION(GenericChainBreakToInt),
				   asCALL_GENERIC) >= 0;
	}

	static bool RegisterChainFixtures(asIScriptEngine& Engine, FExpressionChainState& State)
	{
		Engine.SetUserData(&State, ChainStateUserDataSlot);
		return RegisterChainReferenceType(Engine, "FChainNode") &&
			   RegisterChainReferenceType(Engine, "FChainView") &&
			   RegisterChainSurface(Engine, "FChainNode") &&
			   RegisterChainSurface(Engine, "FChainView") &&
			   Engine.RegisterObjectMethod("FChainNode",
				   "FChainView opCast()",
				   asFUNCTION(GenericChainCast),
				   asCALL_GENERIC) >= 0 &&
			   Engine.RegisterGlobalFunction(
				   "FChainNode MakeChain()", asFUNCTION(GenericMakeChain), asCALL_GENERIC) >= 0;
	}

	static EChainOperation OperationAt(const FChainShapeCase& ShapeCase, const int32 StageIndex)
	{
		return ShapeCase.Pattern[StageIndex % ShapeCase.PatternCount];
	}

	static int32 FailureStage(const FChainDepthCase& DepthCase)
	{
		return (DepthCase.Depth + 1) / 2;
	}

	static bool IsCompileFailure(const FChainStateCase& StateCase)
	{
		return StateCase.State == EChainState::InvalidIntermediate ||
			   StateCase.State == EChainState::MissingTerminal;
	}

	static FString AppendChainOperation(
		const FString& Receiver, const EChainOperation Operation, const int32 Stage)
	{
		switch (Operation)
		{
		case EChainOperation::Call:
			return Receiver + TEXT(".StepCall()");
		case EChainOperation::Member:
			return Receiver + TEXT(".GetMember()");
		case EChainOperation::Index:
			return Receiver + FString::Printf(TEXT("[%d]"), Stage);
		case EChainOperation::Cast:
			// The fork has no script-level cast<> expression; exercise the
			// registered conversion surface through its explicit method.
			return Receiver + TEXT(".opCast()");
		default:
			return Receiver;
		}
	}

	static FString BuildChainExpression(const FChainShapeCase& ShapeCase,
		const FChainDepthCase& DepthCase,
		const FChainStateCase& StateCase)
	{
		FString Expression = TEXT("MakeChain()");
		const int32 InjectedStage = FailureStage(DepthCase);
		for (int32 Stage = 1; Stage <= DepthCase.Depth; ++Stage)
		{
			if (StateCase.State == EChainState::InvalidIntermediate && Stage == InjectedStage)
			{
				Expression += TEXT(".BreakToInt()");
			}
			else
			{
				Expression =
					AppendChainOperation(Expression, OperationAt(ShapeCase, Stage - 1), Stage);
			}
		}

		if (StateCase.State == EChainState::MissingTerminal)
		{
			Expression += TEXT(".MissingTerminal");
		}
		else
		{
			Expression += TEXT(".Value");
		}
		return Expression;
	}

	static int32 ExpectedValue(const FChainShapeCase& ShapeCase, const FChainDepthCase& DepthCase)
	{
		int32 Value = 100;
		for (int32 Stage = 0; Stage < DepthCase.Depth; ++Stage)
		{
			Value += FExpressionChainNode::OperationValue(OperationAt(ShapeCase, Stage));
		}
		return Value;
	}

	static FString BuildExpressionChainSource(const FChainShapeCase& ShapeCase,
		const FChainDepthCase& DepthCase,
		const FChainStateCase& StateCase,
		const FChainContextCase& ContextCase)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString Expression = BuildChainExpression(ShapeCase, DepthCase, StateCase);
		const int32 Expected = ExpectedValue(ShapeCase, DepthCase);
		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int ObserveChain(int Value)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);

		if (ContextCase.Context == EChainContext::Return)
		{
			AppendGeneratedAsLine(Source, TEXT("int ProduceChainValue()"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\treturn %s;"), *Expression));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}

		AppendGeneratedAsLine(Source, TEXT("int RunExpressionChain()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		switch (ContextCase.Context)
		{
		case EChainContext::Initializer:
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tint Result = %s;"), *Expression));
			AppendGeneratedAsLine(Source, TEXT("\treturn Result;"));
			break;
		case EChainContext::Argument:
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("\treturn ObserveChain(%s);"), *Expression));
			break;
		case EChainContext::Return:
			AppendGeneratedAsLine(Source, TEXT("\treturn ProduceChainValue();"));
			break;
		case EChainContext::Condition:
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("\tif (%s == %d)"), *Expression, Expected));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn 1;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("\treturn 0;"));
			break;
		case EChainContext::Assignment:
			AppendGeneratedAsLine(Source, TEXT("\tint Result = 0;"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\tResult = %s;"), *Expression));
			AppendGeneratedAsLine(Source, TEXT("\treturn Result;"));
			break;
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RecoverExpressionChain()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 149;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static FString BuildExpressionChainRecoverySource()
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int RecoverExpressionChain()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 149;"));
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
		asIScriptModule* Module = nullptr;
		OutBuildResult =
			CompileNativeModule(Engine.Get(), ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
		return Module;
	}

	static bool HasLocatedError(const FNativeMessageCollector& Messages, const FString& ModuleName)
	{
		return Messages.Entries.ContainsByPredicate(
			[&ModuleName](const AngelscriptNativeTestSupport::FNativeMessageEntry& Entry)
			{
				return Entry.Type == asMSGTYPE_ERROR && Entry.Section == ModuleName &&
					   Entry.Row > 0 && Entry.Column > 0;
			});
	}

	void VerifyFixtureMetadata(const FNativeCaseContext& Case, asIScriptEngine& Engine)
	{
		asITypeInfo* const Node = Engine.GetTypeInfoByDecl("FChainNode");
		asITypeInfo* const View = Engine.GetTypeInfoByDecl("FChainView");
		ASSERT_THAT(
			IsNotNull(Node, *Case.Describe(TEXT("chain fixture should publish its node type"))));
		ASSERT_THAT(IsNotNull(
			View, *Case.Describe(TEXT("chain fixture should publish its cast view type"))));
		if (Node == nullptr || View == nullptr)
		{
			return;
		}

		asIScriptFunction* const NodeCall = Node->GetMethodByDecl("FChainNode StepCall()");
		asIScriptFunction* const NodeMember = Node->GetMethodByDecl("FChainNode GetMember()");
		asIScriptFunction* const NodeIndex = Node->GetMethodByName("opIndex");
		asIScriptFunction* const NodeCast = Node->GetMethodByDecl("FChainView opCast()");
		asIScriptFunction* const ViewCall = View->GetMethodByDecl("FChainNode StepCall()");
		ASSERT_THAT(IsNotNull(
			NodeCall, *Case.Describe(TEXT("chain node should retain its exact call declaration"))));
		ASSERT_THAT(IsNotNull(NodeMember,
			*Case.Describe(TEXT("chain node should retain its exact member accessor"))));
		ASSERT_THAT(IsNotNull(NodeIndex,
			*Case.Describe(TEXT("chain node should retain its exact index declaration"))));
		if (NodeIndex != nullptr)
		{
			int IndexTypeId = asTYPEID_VOID;
			const char* const ReturnDeclarationRaw =
				Engine.GetTypeDeclaration(NodeIndex->GetReturnTypeId());
			const FString ReturnDeclaration = UTF8_TO_TCHAR(
				ReturnDeclarationRaw != nullptr ? ReturnDeclarationRaw : "");
			ASSERT_THAT(IsTrue(ReturnDeclaration.Contains(TEXT("FChainNode")),
				*Case.Describe(TEXT("chain index should retain its node return type"))));
			ASSERT_THAT(AreEqual(static_cast<asUINT>(1),
				NodeIndex->GetParamCount(),
				*Case.Describe(TEXT("chain index should retain one parameter"))));
			ASSERT_THAT(AreEqual(asSUCCESS,
				NodeIndex->GetParam(0, &IndexTypeId),
				*Case.Describe(TEXT("chain index parameter metadata should be readable"))));
			ASSERT_THAT(AreEqual(asTYPEID_INT32,
				IndexTypeId,
				*Case.Describe(TEXT("chain index should retain its int parameter"))));
		}
		ASSERT_THAT(IsNotNull(
			NodeCast, *Case.Describe(TEXT("chain node should retain its exact explicit cast"))));
		ASSERT_THAT(IsNotNull(
			ViewCall, *Case.Describe(TEXT("cast view should retain the shared chain surface"))));
		if (NodeMember != nullptr)
		{
			ASSERT_THAT(IsFalse(NodeMember->IsProperty(),
				*Case.Describe(TEXT("current-fork chain member should use an explicit method"))));
		}

		const char* PropertyName = nullptr;
		int32 PropertyTypeId = asTYPEID_VOID;
		ASSERT_THAT(IsTrue(Node->GetProperty(0, &PropertyName, &PropertyTypeId) >= 0 &&
							   PropertyName != nullptr &&
							   FCStringAnsi::Strcmp(PropertyName, "Value") == 0 &&
							   PropertyTypeId == Engine.GetTypeIdByDecl("int"),
			*Case.Describe(TEXT("chain node terminal field should retain int metadata"))));
	}

	void VerifyVisitedStages(const FNativeCaseContext& Case,
		const FChainShapeCase& ShapeCase,
		const FChainDepthCase& DepthCase,
		const FChainStateCase& StateCase,
		const FExpressionChainState& State)
	{
		int32 ExpectedCount = DepthCase.Depth;
		if (StateCase.State == EChainState::NullReceiver)
		{
			ExpectedCount = 0;
		}
		else if (StateCase.State == EChainState::ExceptionIntermediate)
		{
			ExpectedCount = FailureStage(DepthCase);
		}
		ASSERT_THAT(AreEqual(ExpectedCount,
			State.VisitedStages.Num(),
			*Case.Describe(TEXT("chain should visit exactly the expected stage prefix"))));
		for (int32 Index = 0; Index < FMath::Min(ExpectedCount, State.VisitedStages.Num()); ++Index)
		{
			ASSERT_THAT(AreEqual(Index + 1,
				State.VisitedStages[Index],
				*Case.Describe(TEXT("chain stages should remain consecutively ordered"))));
			ASSERT_THAT(AreEqual(static_cast<int32>(OperationAt(ShapeCase, Index)),
				State.VisitedKinds[Index],
				*Case.Describe(TEXT("chain should execute the shape-specific operation kind"))));
		}
	}

	void ExecuteRecovery(const FNativeCaseContext& Case,
		asIScriptEngine& Engine,
		asIScriptModule& Module,
		asIScriptContext* ReusedContext = nullptr)
	{
		using namespace AngelscriptNativeTestSupport;

		asIScriptFunction* const Recovery =
			GetNativeFunctionByExactDecl(&Module, "int RecoverExpressionChain()");
		ASSERT_THAT(IsNotNull(
			Recovery, *Case.Describe(TEXT("chain recovery should resolve by exact declaration"))));
		asIScriptContext* Context = ReusedContext;
		if (Context == nullptr)
		{
			Context = Engine.CreateContext();
		}
		ASSERT_THAT(
			IsNotNull(Context, *Case.Describe(TEXT("chain recovery should obtain a context"))));
		if (Recovery != nullptr && Context != nullptr)
		{
			ASSERT_THAT(AreEqual(asSUCCESS,
				Context->Prepare(Recovery),
				*Case.Describe(TEXT("chain recovery should prepare"))));
			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
				Context->Execute(),
				*Case.Describe(TEXT("chain recovery should finish"))));
			ASSERT_THAT(AreEqual(149,
				static_cast<int32>(Context->GetReturnDWord()),
				*Case.Describe(TEXT("chain recovery should return its sentinel"))));
			ASSERT_THAT(AreEqual(asSUCCESS,
				Context->Unprepare(),
				*Case.Describe(TEXT("chain recovery should unprepare cleanly"))));
		}
		if (ReusedContext == nullptr && Context != nullptr)
		{
			Context->Release();
		}
	}

	void ExecuteRuntimeCell(const FNativeCaseContext& Case,
		const FChainShapeCase& ShapeCase,
		const FChainDepthCase& DepthCase,
		const FChainStateCase& StateCase,
		const FChainContextCase& ContextCase,
		FExpressionChainState& State,
		asIScriptEngine& Engine,
		asIScriptModule& Module,
		const FString& ModuleName)
	{
		using namespace AngelscriptNativeTestSupport;

		const bool bNullReceiver = StateCase.State == EChainState::NullReceiver;
		const int32 ThrowAtStage = StateCase.State == EChainState::ExceptionIntermediate
									   ? FailureStage(DepthCase)
									   : INDEX_NONE;
		State.Reset(bNullReceiver, ThrowAtStage);
		asIScriptFunction* const Entry =
			GetNativeFunctionByExactDecl(&Module, "int RunExpressionChain()");
		ASSERT_THAT(IsNotNull(
			Entry, *Case.Describe(TEXT("compiled chain should publish its exact entry"))));
		asIScriptContext* const Context = Engine.CreateContext();
		ASSERT_THAT(IsNotNull(
			Context, *Case.Describe(TEXT("compiled chain should create an execution context"))));
		if (Entry == nullptr || Context == nullptr)
		{
			if (Context != nullptr)
			{
				Context->Release();
			}
			return;
		}

		const int32 ExecuteResult = PrepareAndExecute(Context, Entry);
		const bool bExpectedException =
			bNullReceiver || StateCase.State == EChainState::ExceptionIntermediate;
		if (bExpectedException)
		{
			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_EXCEPTION),
				ExecuteResult,
				*Case.Describe(TEXT("failing chain should stop with an execution exception"))));
			const FString ExpectedException = bNullReceiver
												  ? TEXT("Null pointer access")
												  : TEXT("Expression chain intermediate exception");
			ASSERT_THAT(AreEqual(ExpectedException,
				FString(UTF8_TO_TCHAR(Context->GetExceptionString())),
				*Case.Describe(TEXT("failing chain should retain its exact exception cause"))));
			ASSERT_THAT(IsNotNull(Context->GetExceptionFunction(),
				*Case.Describe(TEXT("failing chain should retain its exception function"))));
			const char* ExceptionSection = nullptr;
			int32 ExceptionColumn = INDEX_NONE;
			ASSERT_THAT(
				IsTrue(Context->GetExceptionLineNumber(&ExceptionColumn, &ExceptionSection) > 0,
					*Case.Describe(TEXT("failing chain should retain a source line"))));
			ASSERT_THAT(AreEqual(ModuleName,
				FString(UTF8_TO_TCHAR(ExceptionSection != nullptr ? ExceptionSection : "")),
				*Case.Describe(TEXT("failing chain should retain its generated section"))));
			ASSERT_THAT(IsTrue(ExceptionColumn > 0,
				*Case.Describe(TEXT("failing chain should retain a source column"))));
		}
		else
		{
			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
				ExecuteResult,
				*Case.Describe(TEXT("valid chain should execute to completion"))));
			if (ExecuteResult == asEXECUTION_FINISHED)
			{
				const int32 ExpectedResult = ContextCase.Context == EChainContext::Condition
												 ? 1
												 : ExpectedValue(ShapeCase, DepthCase);
				ASSERT_THAT(AreEqual(ExpectedResult,
					static_cast<int32>(Context->GetReturnDWord()),
					*Case.Describe(TEXT("valid chain should preserve its typed context result"))));
			}
		}

		VerifyVisitedStages(Case, ShapeCase, DepthCase, StateCase, State);
		ASSERT_THAT(AreEqual(1,
			State.FactoryCalls,
			*Case.Describe(TEXT("runtime chain should evaluate its root factory once"))));
		ASSERT_THAT(AreEqual(asSUCCESS,
			Context->Unprepare(),
			*Case.Describe(TEXT("chain context should unprepare after finish or exception"))));
		const int32 ExpectedObjects = bNullReceiver ? 0 : 1;
		ASSERT_THAT(AreEqual(ExpectedObjects,
			State.Created,
			*Case.Describe(TEXT("chain should create only its expected native receiver"))));
		ASSERT_THAT(AreEqual(ExpectedObjects,
			State.Destroyed,
			*Case.Describe(TEXT("chain should destroy every created native receiver"))));
		ASSERT_THAT(AreEqual(0,
			State.LiveObjects,
			*Case.Describe(TEXT("chain cleanup should leave no native receiver live"))));
		ExecuteRecovery(Case, Engine, Module, Context);
		Context->Release();
	}

	void RunCell(FNativeTestEngine& Engine,
		FExpressionChainState& State,
		const FChainContextCase& ContextCase,
		const FChainDepthCase& DepthCase,
		const FChainShapeCase& ShapeCase,
		const FChainStateCase& StateCase)
	{
		using namespace AngelscriptNativeTestSupport;

		State.Reset(false, INDEX_NONE);
		const FNativeCaseContext Case(MakeNativeCaseId("LANG-EXPR-CHAIN",
			{
				ANSI_TO_TCHAR(ContextCase.CatalogName),
				ANSI_TO_TCHAR(DepthCase.CatalogName),
				ANSI_TO_TCHAR(ShapeCase.CatalogName),
				ANSI_TO_TCHAR(StateCase.CatalogName),
			}));
		const FString ModuleName = FString::Printf(TEXT("ExpressionChain_%hs_%hs_%hs_%hs"),
			ContextCase.CatalogName,
			DepthCase.CatalogName,
			ShapeCase.CatalogName,
			StateCase.CatalogName);
		const FString Source =
			BuildExpressionChainSource(ShapeCase, DepthCase, StateCase, ContextCase);
		int32 BuildResult = asERROR;
		asIScriptModule* Module =
			CompileAndReport(Engine, Case, Case.GetId(), ModuleName, Source, BuildResult);
		if (IsCompileFailure(StateCase))
		{
			ASSERT_THAT(IsTrue(BuildResult < 0,
				*Case.Describe(TEXT("invalid or missing chain stage should fail compilation"))));
			ASSERT_THAT(IsTrue(HasLocatedError(Engine.GetMessages(), ModuleName),
				*Case.Describe(TEXT("rejected chain should own a located diagnostic"))));
			ASSERT_THAT(AreEqual(0,
				State.FactoryCalls,
				*Case.Describe(TEXT("rejected chain should execute no factory"))));
			const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
			Engine.Get()->DiscardModule(ModuleNameUtf8.Get());
			const FString RecoverySource = BuildExpressionChainRecoverySource();
			int32 RecoveryBuildResult = asERROR;
			Module = CompileAndReport(Engine,
				Case,
				Case.GetId() + TEXT("-RECOVERY"),
				ModuleName,
				RecoverySource,
				RecoveryBuildResult);
			ASSERT_THAT(IsTrue(RecoveryBuildResult >= 0,
				*Case.DescribeResult("<recovery build>",
					TEXT("successful same-name recovery"),
					Engine.GetMessagesText())));
			if (RecoveryBuildResult >= 0 && Module != nullptr)
			{
				ExecuteRecovery(Case, *Engine.Get(), *Module);
			}
		}
		else
		{
			ASSERT_THAT(IsTrue(BuildResult >= 0,
				*Case.DescribeResult("<module build>",
					TEXT("successful chain compilation"),
					Engine.GetMessagesText())));
			ASSERT_THAT(
				IsNotNull(Module, *Case.Describe(TEXT("legal chain should publish a module"))));
			if (BuildResult >= 0 && Module != nullptr)
			{
				VerifyFixtureMetadata(Case, *Engine.Get());
				ExecuteRuntimeCell(Case,
					ShapeCase,
					DepthCase,
					StateCase,
					ContextCase,
					State,
					*Engine.Get(),
					*Module,
					ModuleName);
			}
		}

		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		Engine.Get()->DiscardModule(ModuleNameUtf8.Get());
		ASSERT_THAT(IsNull(Engine.Get()->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
			*Case.Describe(TEXT("chain cell should discard its isolated module"))));
		ASSERT_THAT(AreEqual(0,
			State.LiveObjects,
			*Case.Describe(TEXT("chain module discard should retain no native receiver"))));
	}

public:
	TEST_METHOD(ShapesByDepthStateAndContext)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-EXPR-CHAIN",
			ENativeEvidence::Compile | ENativeEvidence::Diagnostic | ENativeEvidence::Runtime |
				ENativeEvidence::Metadata | ENativeEvidence::Debug | ENativeEvidence::Cleanup);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("Expression chain product should create a standalone raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FExpressionChainState State;
		State.Reset(false, INDEX_NONE);
		ASSERT_THAT(IsTrue(RegisterChainFixtures(*ScriptEngine, State),
			*FNativeCaseContext(TEXT("LANG-EXPR-CHAIN-FIXTURES")).DescribeResult(
				"<fixture registration>",
				TEXT("registered native chain surfaces"),
				Engine.GetMessagesText())));
		ASSERT_THAT(AreEqual(static_cast<asPWORD>(3),
			ScriptEngine->GetEngineProperty(asEP_PROPERTY_ACCESSOR_MODE),
			TEXT("Expression chain engine should retain registered property accessor mode")));

		for (const FChainContextCase& ContextCase : ContextCases)
		{
			for (const FChainDepthCase& DepthCase : DepthCases)
			{
				for (const FChainShapeCase& ShapeCase : ShapeCases)
				{
					for (const FChainStateCase& StateCase : StateCases)
					{
						RunCell(Engine, State, ContextCase, DepthCase, ShapeCase, StateCase);
					}
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
