#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FLogicalNotOperatorTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Operators.LogicalNot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeCaseContext = AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeTestEngine = AngelscriptNativeTestSupport::FNativeTestEngine;

	inline static constexpr asPWORD LogicalNotStateSlot =
		static_cast<asPWORD>(0x4E41544C4F474E54ull);

	enum class ELogicalNotCategory : uint8
	{
		MutableLValue,
		ConstLValue,
		Temporary,
		Field,
		Alias,
	};

	struct FCategoryCase
	{
		const ANSICHAR* CatalogName;
		ELogicalNotCategory Category;
	};

	struct FValueCase
	{
		const ANSICHAR* CatalogName;
		bool Value;
	};

	struct FLogicalNotState
	{
		void Reset()
		{
			ObservedValues.Reset();
		}

		TArray<bool> ObservedValues;
	};

	inline static constexpr FCategoryCase CategoryCases[] = {
		{"mutable_lvalue", ELogicalNotCategory::MutableLValue},
		{"const_lvalue", ELogicalNotCategory::ConstLValue},
		{"temporary", ELogicalNotCategory::Temporary},
		{"field", ELogicalNotCategory::Field},
		{"alias", ELogicalNotCategory::Alias},
	};

	inline static constexpr FValueCase ValueCases[] = {
		{"false", false},
		{"true", true},
	};

	static FLogicalNotState* ActiveState()
	{
		asIScriptContext* const Context = asGetActiveContext();
		return Context != nullptr && Context->GetEngine() != nullptr
				   ? static_cast<FLogicalNotState*>(
						 Context->GetEngine()->GetUserData(LogicalNotStateSlot))
				   : nullptr;
	}

	static bool ObserveNotOperand(const bool Value)
	{
		if (FLogicalNotState* const State = ActiveState())
		{
			State->ObservedValues.Add(Value);
		}
		return Value;
	}

	static bool RegisterObserver(asIScriptEngine& Engine, FLogicalNotState& State)
	{
		Engine.SetUserData(&State, LogicalNotStateSlot);
		const ASAutoCaller::FunctionCaller Caller =
			ASAutoCaller::MakeFunctionCaller(ObserveNotOperand);
		return Engine.RegisterGlobalFunction("bool ObserveNotOperand(bool Value)",
				   asFUNCTION(ObserveNotOperand),
				   asCALL_CDECL,
				   *(asFunctionCaller*)&Caller) >= 0;
	}

	static FString SourceId(const FCategoryCase& CategoryCase)
	{
		return AngelscriptNativeTestSupport::MakeNativeCaseId(
			"LANG-OP-LOGICAL-NOT-SOURCE", {ANSI_TO_TCHAR(CategoryCase.CatalogName)});
	}

	static FString ModuleName(const FCategoryCase& CategoryCase)
	{
		return FString::Printf(TEXT("ASNativeLogicalNot_%hs"), CategoryCase.CatalogName);
	}

	static FString BuildLogicalNotSource(const FCategoryCase& CategoryCase)
	{
		using namespace AngelscriptNativeTestSupport;
		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int ObserveLogicalNotType(bool Value)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 301;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int ObserveLogicalNotType(int Value)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 399;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);

		if (CategoryCase.Category == ELogicalNotCategory::Temporary)
		{
			AppendGeneratedAsLine(Source, TEXT("bool MakeLogicalNotTemporary(bool Input)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Input;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else if (CategoryCase.Category == ELogicalNotCategory::Field)
		{
			AppendGeneratedAsLine(Source, TEXT("struct FLogicalNotOwner"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tbool Value;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else if (CategoryCase.Category == ELogicalNotCategory::Alias)
		{
			AppendGeneratedAsLine(Source, TEXT("bool ApplyLogicalNotAlias(bool& in Value)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn !ObserveNotOperand(Value);"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("int ObserveLogicalNotAliasType(bool& in Value)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(
				Source, TEXT("\treturn ObserveLogicalNotType(!ObserveNotOperand(Value));"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}

		TArray<FString> SetupLines;
		FString Expression;
		switch (CategoryCase.Category)
		{
		case ELogicalNotCategory::MutableLValue:
			SetupLines.Add(TEXT("\tbool Value = Input;"));
			Expression = TEXT("!ObserveNotOperand(Value)");
			break;
		case ELogicalNotCategory::ConstLValue:
			SetupLines.Add(TEXT("\tconst bool Value = Input;"));
			Expression = TEXT("!ObserveNotOperand(Value)");
			break;
		case ELogicalNotCategory::Temporary:
			Expression = TEXT("!MakeLogicalNotTemporary(ObserveNotOperand(Input))");
			break;
		case ELogicalNotCategory::Field:
			SetupLines.Add(TEXT("\tFLogicalNotOwner Owner;"));
			SetupLines.Add(TEXT("\tOwner.Value = Input;"));
			Expression = TEXT("!ObserveNotOperand(Owner.Value)");
			break;
		case ELogicalNotCategory::Alias:
			SetupLines.Add(TEXT("\tbool Value = Input;"));
			Expression = TEXT("ApplyLogicalNotAlias(Value)");
			break;
		}

		auto AppendSetup = [&Source, &SetupLines]()
		{
			for (const FString& SetupLine : SetupLines)
			{
				AppendGeneratedAsLine(Source, SetupLine);
			}
		};

		AppendGeneratedAsLine(Source, TEXT("bool EvaluateLogicalNot(bool Input)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendSetup();
		AppendGeneratedAsLine(Source, FString::Printf(TEXT("\treturn %s;"), *Expression));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);

		AppendGeneratedAsLine(Source, TEXT("int ObserveLogicalNotResultType(bool Input)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendSetup();
		if (CategoryCase.Category == ELogicalNotCategory::Alias)
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn ObserveLogicalNotAliasType(Value);"));
		}
		else
		{
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("\treturn ObserveLogicalNotType(%s);"), *Expression));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static asIScriptFunction* FindExactFunction(asIScriptModule& Module,
		const ANSICHAR* Name,
		const int ReturnTypeId,
		const int ParameterTypeId)
	{
		asIScriptFunction* Match = nullptr;
		for (asUINT FunctionIndex = 0; FunctionIndex < Module.GetFunctionCount(); ++FunctionIndex)
		{
			asIScriptFunction* const Candidate = Module.GetFunctionByIndex(FunctionIndex);
			if (Candidate == nullptr || FCStringAnsi::Strcmp(Candidate->GetName(), Name) != 0 ||
				Candidate->GetParamCount() != 1 || Candidate->GetReturnTypeId() != ReturnTypeId)
			{
				continue;
			}
			int ActualParameterTypeId = asINVALID_TYPE;
			if (Candidate->GetParam(0, &ActualParameterTypeId) < 0 ||
				ActualParameterTypeId != ParameterTypeId)
			{
				continue;
			}
			if (Match != nullptr)
			{
				return nullptr;
			}
			Match = Candidate;
		}
		return Match;
	}

	static bool VerifyCategoryMetadata(FAutomationTestBase& Test,
		asIScriptEngine& Engine,
		asIScriptModule& Module,
		asIScriptFunction& Evaluate,
		const FNativeCaseContext& Case,
		const FCategoryCase& CategoryCase)
	{
		FNoDiscardAsserter Assert(Test);
		const int BoolTypeId = Engine.GetTypeIdByDecl("bool");
		if (CategoryCase.Category == ELogicalNotCategory::Field)
		{
			asITypeInfo* const Owner = Module.GetTypeInfoByName("FLogicalNotOwner");
			if (!Assert.IsNotNull(
					Owner, *Case.Describe(TEXT("field category should publish its owner"))))
			{
				return false;
			}
			const char* Name = nullptr;
			int TypeId = asINVALID_TYPE;
			return Assert.AreEqual(asSUCCESS,
					   Owner->GetProperty(0, &Name, &TypeId),
					   *Case.Describe(TEXT("field metadata should be readable"))) &&
				   Assert.AreEqual(BoolTypeId,
					   TypeId,
					   *Case.Describe(TEXT("logical-not field should retain bool type"))) &&
				   Assert.AreEqual(FString(TEXT("Value")),
					   FString(UTF8_TO_TCHAR(Name != nullptr ? Name : "")),
					   *Case.Describe(TEXT("logical-not field should retain its name")));
		}
		if (CategoryCase.Category == ELogicalNotCategory::Alias)
		{
			asIScriptFunction* const Alias =
				FindExactFunction(Module, "ApplyLogicalNotAlias", BoolTypeId, BoolTypeId);
			if (!Assert.IsNotNull(
					Alias, *Case.Describe(TEXT("alias category should publish its helper"))))
			{
				return false;
			}
			int TypeId = asINVALID_TYPE;
			asDWORD Flags = asTM_NONE;
			return Assert.AreEqual(asSUCCESS,
					   Alias->GetParam(0, &TypeId, &Flags),
					   *Case.Describe(TEXT("alias parameter metadata should be readable"))) &&
				   Assert.AreEqual(BoolTypeId,
					   TypeId,
					   *Case.Describe(TEXT("logical-not alias should retain bool type"))) &&
				   Assert.AreEqual(static_cast<asDWORD>(asTM_INREF),
					   Flags,
					   *Case.Describe(TEXT("logical-not alias should retain input-reference")));
		}
		if (CategoryCase.Category == ELogicalNotCategory::Temporary)
		{
			return Assert.IsNotNull(
				FindExactFunction(Module, "MakeLogicalNotTemporary", BoolTypeId, BoolTypeId),
				*Case.Describe(TEXT("temporary category should publish its producer")));
		}

		for (asUINT VariableIndex = 0; VariableIndex < Evaluate.GetVarCount(); ++VariableIndex)
		{
			const char* Name = nullptr;
			int TypeId = asINVALID_TYPE;
			if (Evaluate.GetVar(VariableIndex, &Name, &TypeId) >= 0 && Name != nullptr &&
				FCStringAnsi::Strcmp(Name, "Value") == 0)
			{
				const FString Declaration = UTF8_TO_TCHAR(Evaluate.GetVarDecl(VariableIndex, true));
				return Assert.AreEqual(BoolTypeId,
						   TypeId,
						   *Case.Describe(TEXT("logical-not local should retain bool type"))) &&
					   Assert.AreEqual(CategoryCase.Category == ELogicalNotCategory::ConstLValue,
						   Declaration.Contains(TEXT("const ")),
						   *Case.Describe(
							   TEXT("logical-not local should retain selected constness")));
			}
		}
		return Assert.IsTrue(
			false, *Case.Describe(TEXT("logical-not local category should publish Value")));
	}

	static bool ExecuteFunction(FAutomationTestBase& Test,
		asIScriptContext& Context,
		asIScriptFunction& Function,
		FLogicalNotState& State,
		const FNativeCaseContext& Case,
		const bool Input,
		const int32 ExpectedReturn,
		const bool bBooleanReturn)
	{
		FNoDiscardAsserter Assert(Test);
		State.Reset();
		if (!Assert.AreEqual(asSUCCESS,
				Context.Prepare(&Function),
				*Case.Describe(TEXT("logical-not function should prepare"))) ||
			!Assert.AreEqual(asSUCCESS,
				Context.SetArgByte(0, Input ? 1 : 0),
				*Case.Describe(TEXT("logical-not function should receive bool input"))))
		{
			Context.Unprepare();
			return false;
		}
		const int ExecuteResult = Context.Execute();
		const int32 ActualReturn = bBooleanReturn ? (Context.GetReturnByte() != 0 ? 1 : 0)
												  : static_cast<int32>(Context.GetReturnDWord());
		const bool bExecuted = Assert.AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
			ExecuteResult,
			*Case.Describe(TEXT("logical-not function should finish")));
		const bool bReturned = Assert.AreEqual(ExpectedReturn,
			ActualReturn,
			*Case.Describe(TEXT("logical-not function should return its exact result")));
		const bool bObservedOnce = Assert.AreEqual(1,
			State.ObservedValues.Num(),
			*Case.Describe(TEXT("logical-not operand should execute exactly once")));
		const bool bObservedInput =
			State.ObservedValues.Num() == 1 &&
			Assert.AreEqual(Input,
				State.ObservedValues[0],
				*Case.Describe(TEXT("logical-not observer should receive the source value")));
		const bool bUnprepared = Assert.AreEqual(asSUCCESS,
			Context.Unprepare(),
			*Case.Describe(TEXT("logical-not function should unprepare")));
		return bExecuted && bReturned && bObservedOnce && bObservedInput && bUnprepared;
	}

public:
	TEST_METHOD(CategoriesByValue)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("LANG-OP-LOGICAL-NOT",
			ENativeEvidence::Compile | ENativeEvidence::Runtime | ENativeEvidence::Metadata);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		ASSERT_THAT(
			IsNotNull(Engine.Get(), TEXT("logical-not product should create a standalone engine")));
		if (Engine.Get() == nullptr)
		{
			return;
		}

		FLogicalNotState State;
		ASSERT_THAT(IsTrue(RegisterObserver(*Engine.Get(), State),
			TEXT("logical-not product should register its exact-once observer")));
		asIScriptContext* const Context = Engine.Get()->CreateContext();
		ASSERT_THAT(
			IsNotNull(Context, TEXT("logical-not product should create a reusable context")));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		const int BoolTypeId = Engine.Get()->GetTypeIdByDecl("bool");
		const int IntTypeId = Engine.Get()->GetTypeIdByDecl("int");
		TArray<FString> ConstructedIds;
		TSet<FString> UniqueIds;
		bool bAllCasesPassed = true;
		for (const FCategoryCase& CategoryCase : CategoryCases)
		{
			const FString CurrentModuleName = ModuleName(CategoryCase);
			const FString Source = BuildLogicalNotSource(CategoryCase);
			PrintGeneratedAsSource(*TestRunner, SourceId(CategoryCase), CurrentModuleName, Source);
			Engine.Reset(*TestRunner);
			const FTCHARToUTF8 ModuleNameUtf8(*CurrentModuleName);
			const FTCHARToUTF8 SourceUtf8(*Source);
			asIScriptModule* Module = nullptr;
			const int BuildResult =
				CompileNativeModule(Engine.Get(), ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
			ASSERT_THAT(IsTrue(BuildResult >= 0,
				*FString::Printf(TEXT("[%s] logical-not source should compile. Messages={%s}"),
					*SourceId(CategoryCase),
					*Engine.GetMessagesText())));
			ASSERT_THAT(IsNotNull(Module, TEXT("logical-not source should publish its module")));
			if (BuildResult < 0 || Module == nullptr)
			{
				return;
			}

			asIScriptFunction* const Evaluate =
				FindExactFunction(*Module, "EvaluateLogicalNot", BoolTypeId, BoolTypeId);
			asIScriptFunction* const TypeWitness =
				FindExactFunction(*Module, "ObserveLogicalNotResultType", IntTypeId, BoolTypeId);
			ASSERT_THAT(IsNotNull(
				Evaluate, TEXT("logical-not evaluator should resolve by exact declaration")));
			ASSERT_THAT(IsNotNull(
				TypeWitness, TEXT("logical-not type witness should resolve by exact declaration")));
			if (Evaluate == nullptr || TypeWitness == nullptr)
			{
				return;
			}

			for (const FValueCase& ValueCase : ValueCases)
			{
				const FNativeCaseContext Case(MakeNativeCaseId("LANG-OP-LOGICAL-NOT",
					{ANSI_TO_TCHAR(CategoryCase.CatalogName),
						ANSI_TO_TCHAR(ValueCase.CatalogName)}));
				ConstructedIds.Add(Case.GetId());
				const bool bUniqueCaseId = !UniqueIds.Contains(Case.GetId());
				UniqueIds.Add(Case.GetId());
				ASSERT_THAT(IsTrue(bUniqueCaseId,
					*Case.Describe(TEXT("logical-not case ID should be unique"))));
				bAllCasesPassed &= VerifyCategoryMetadata(
					*TestRunner, *Engine.Get(), *Module, *Evaluate, Case, CategoryCase);
				bAllCasesPassed &= ExecuteFunction(*TestRunner,
					*Context,
					*Evaluate,
					State,
					Case,
					ValueCase.Value,
					ValueCase.Value ? 0 : 1,
					true);
				bAllCasesPassed &= ExecuteFunction(
					*TestRunner, *Context, *TypeWitness, State, Case, ValueCase.Value, 301, false);
			}

			Engine.Get()->DiscardModule(ModuleNameUtf8.Get());
			ASSERT_THAT(IsNull(Engine.Get()->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
				TEXT("logical-not source module should discard after both bool values")));
		}

		ASSERT_THAT(AreEqual(10,
			ConstructedIds.Num(),
			TEXT("logical-not product should construct all ten catalog IDs")));
		ASSERT_THAT(AreEqual(ConstructedIds.Num(),
			UniqueIds.Num(),
			TEXT("logical-not product should construct no duplicate IDs")));
		ASSERT_THAT(IsTrue(bAllCasesPassed,
			TEXT(
				"every logical-not cell should satisfy category, value, and exact-once evidence")));
	}
};

#endif
