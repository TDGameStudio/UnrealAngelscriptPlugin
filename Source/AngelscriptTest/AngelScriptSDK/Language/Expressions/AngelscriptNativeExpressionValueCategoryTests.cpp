#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FExpressionValueCategoryTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Expressions.ValueCategory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeCaseContext = AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeTestEngine = AngelscriptNativeTestSupport::FNativeTestEngine;

	inline static constexpr asPWORD MutationStateUserDataSlot =
		static_cast<asPWORD>(0x455850524D555441ull);

	enum class EValueCategory : uint8
	{
		MutableLValue,
		ConstLValue,
		Temporary,
		ReferenceAlias,
		Field,
		Property,
		InvalidNonLValue,
	};

	enum class EMutation : uint8
	{
		Assign,
		CompoundAssign,
		PrefixIncrement,
		PostfixIncrement,
		OutArgument,
	};

	enum class EPlacement : uint8
	{
		Local,
		Global,
		Member,
		Indexed,
	};

	struct FCategoryCase
	{
		const ANSICHAR* CatalogName;
		EValueCategory Category;
		bool bWritable;
	};

	struct FMutationCase
	{
		const ANSICHAR* CatalogName;
		EMutation Mutation;
		int32 ExpectedResult;
		int32 ExpectedAfter;
	};

	struct FPlacementCase
	{
		const ANSICHAR* CatalogName;
		EPlacement Placement;
	};

	inline static constexpr FCategoryCase CategoryCases[] = {
		{"mutable_lvalue", EValueCategory::MutableLValue, true},
		{"const_lvalue", EValueCategory::ConstLValue, false},
		{"temporary", EValueCategory::Temporary, false},
		{"reference_alias", EValueCategory::ReferenceAlias, true},
		{"field", EValueCategory::Field, true},
		// The fork intentionally removed the property decorator. Keep this
		// Cartesian cell as an explicit current-fork rejection; registered
		// property semantics are covered by the dedicated Properties owners.
		{"property", EValueCategory::Property, false},
		{"invalid_non_lvalue", EValueCategory::InvalidNonLValue, false},
	};

	inline static constexpr FMutationCase MutationCases[] = {
		{"assign", EMutation::Assign, 20, 20},
		{"compound_assign", EMutation::CompoundAssign, 15, 15},
		{"prefix_increment", EMutation::PrefixIncrement, 11, 11},
		{"postfix_increment", EMutation::PostfixIncrement, 10, 11},
		{"out_argument", EMutation::OutArgument, 31, 31},
	};

	inline static constexpr FPlacementCase PlacementCases[] = {
		{"local", EPlacement::Local},
		{"global", EPlacement::Global},
		{"member", EPlacement::Member},
		{"indexed", EPlacement::Indexed},
	};

	class FMutationOwner;

	struct FMutationState
	{
		void Reset()
		{
			GlobalTarget = 10;
			ConstGlobalTarget = 10;
			NextIdentity = 1;
			CreatedOwners = 0;
			DestroyedOwners = 0;
			LiveOwners = 0;
			OperandCalls = 0;
			OutCalls = 0;
			AliasBindCalls = 0;
			TemporaryCalls = 0;
			NonLValueCalls = 0;
			PropertyGetCalls = 0;
			PropertySetCalls = 0;
			IndexedGetCalls = 0;
			IndexedSetCalls = 0;
			OwnerRouteCalls = 0;
			RetainedOwner = nullptr;
		}

		int32 GlobalTarget = 10;
		int32 ConstGlobalTarget = 10;
		int32 NextIdentity = 1;
		int32 CreatedOwners = 0;
		int32 DestroyedOwners = 0;
		int32 LiveOwners = 0;
		int32 OperandCalls = 0;
		int32 OutCalls = 0;
		int32 AliasBindCalls = 0;
		int32 TemporaryCalls = 0;
		int32 NonLValueCalls = 0;
		int32 PropertyGetCalls = 0;
		int32 PropertySetCalls = 0;
		int32 IndexedGetCalls = 0;
		int32 IndexedSetCalls = 0;
		int32 OwnerRouteCalls = 0;
		FMutationOwner* RetainedOwner = nullptr;
	};

	class FMutationOwner
	{
	public:
		explicit FMutationOwner(FMutationState& InState)
			: State(&InState), Identity(InState.NextIdentity++)
		{
			++InState.CreatedOwners;
			++InState.LiveOwners;
		}

		~FMutationOwner()
		{
			if (State != nullptr)
			{
				++State->DestroyedOwners;
				--State->LiveOwners;
			}
		}

		void AddRef()
		{
			++ReferenceCount;
		}

		void Release()
		{
			--ReferenceCount;
			if (ReferenceCount == 0)
			{
				delete this;
			}
		}

		int32 DirectTarget = 10;
		int32 FieldTarget = 10;
		int32 PropertyTarget = 10;
		int32 IndexedTarget = 10;
		FMutationState* State = nullptr;
		int32 ReferenceCount = 1;
		int32 Identity = 0;
	};

	static void ReleaseRetainedOwner(FMutationState& State)
	{
		if (State.RetainedOwner != nullptr)
		{
			FMutationOwner* const Owner = State.RetainedOwner;
			State.RetainedOwner = nullptr;
			Owner->Release();
		}
	}

	struct FGeneratedTarget
	{
		FString TargetExpression;
		FString AliasObservationExpression;
	};

	static FMutationState* GetMutationState(asIScriptGeneric& Generic)
	{
		return Generic.GetEngine() != nullptr
				   ? static_cast<FMutationState*>(
						 Generic.GetEngine()->GetUserData(MutationStateUserDataSlot))
				   : nullptr;
	}

	static void ReturnOwner(asIScriptGeneric& Generic, FMutationOwner* Owner, const bool bAddRef)
	{
		if (Owner != nullptr && bAddRef)
		{
			Owner->AddRef();
		}
		Generic.SetReturnAddress(Owner);
	}

	static void GenericOwnerAddRef(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			if (FMutationOwner* const Owner = static_cast<FMutationOwner*>(Generic->GetObject()))
			{
				Owner->AddRef();
			}
		}
	}

	static void GenericOwnerRelease(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			if (FMutationOwner* const Owner = static_cast<FMutationOwner*>(Generic->GetObject()))
			{
				Owner->Release();
			}
		}
	}

	static void GenericMakeOwner(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		FMutationState* const State = GetMutationState(*Generic);
		ReturnOwner(*Generic, State != nullptr ? new FMutationOwner(*State) : nullptr, false);
	}

	static void GenericGetGlobalOwner(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		FMutationState* const State = GetMutationState(*Generic);
		if (State == nullptr)
		{
			ReturnOwner(*Generic, nullptr, false);
			return;
		}
		if (State->RetainedOwner == nullptr)
		{
			State->RetainedOwner = new FMutationOwner(*State);
		}
		++State->OwnerRouteCalls;
		ReturnOwner(*Generic, State->RetainedOwner, true);
	}

	static void GenericGetNestedOwner(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		FMutationOwner* const Owner = static_cast<FMutationOwner*>(Generic->GetObject());
		if (Owner != nullptr && Owner->State != nullptr)
		{
			++Owner->State->OwnerRouteCalls;
		}
		ReturnOwner(*Generic, Owner, true);
	}

	static void GenericGetOwnerItem(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		FMutationOwner* const Owner = static_cast<FMutationOwner*>(Generic->GetObject());
		if (Owner != nullptr && Owner->State != nullptr)
		{
			++Owner->State->OwnerRouteCalls;
		}
		ReturnOwner(*Generic, Owner, true);
	}

	static void GenericGetPropertyTarget(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		const FMutationOwner* const Owner =
			static_cast<const FMutationOwner*>(Generic->GetObject());
		if (Owner != nullptr && Owner->State != nullptr)
		{
			++Owner->State->PropertyGetCalls;
		}
		Generic->SetReturnDWord(Owner != nullptr ? static_cast<asDWORD>(Owner->PropertyTarget) : 0);
	}

	static void GenericSetPropertyTarget(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		FMutationOwner* const Owner = static_cast<FMutationOwner*>(Generic->GetObject());
		if (Owner != nullptr)
		{
			if (Owner->State != nullptr)
			{
				++Owner->State->PropertySetCalls;
			}
			Owner->PropertyTarget = static_cast<int32>(Generic->GetArgDWord(0));
		}
	}

	static void GenericGetIndexedTarget(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		const FMutationOwner* const Owner =
			static_cast<const FMutationOwner*>(Generic->GetObject());
		if (Owner != nullptr && Owner->State != nullptr)
		{
			++Owner->State->IndexedGetCalls;
		}
		Generic->SetReturnDWord(Owner != nullptr ? static_cast<asDWORD>(Owner->IndexedTarget) : 0);
	}

	static void GenericSetIndexedTarget(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		FMutationOwner* const Owner = static_cast<FMutationOwner*>(Generic->GetObject());
		if (Owner != nullptr)
		{
			if (Owner->State != nullptr)
			{
				++Owner->State->IndexedSetCalls;
			}
			Owner->IndexedTarget = static_cast<int32>(Generic->GetArgDWord(1));
		}
	}

	static void GenericGetIndexedAlias(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		FMutationOwner* const Owner = static_cast<FMutationOwner*>(Generic->GetObject());
		if (Owner != nullptr && Owner->State != nullptr)
		{
			++Owner->State->AliasBindCalls;
		}
		Generic->SetReturnAddress(Owner != nullptr ? &Owner->IndexedTarget : nullptr);
	}

	static void GenericNextOperand(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		if (FMutationState* const State = GetMutationState(*Generic))
		{
			++State->OperandCalls;
		}
		Generic->SetReturnDWord(Generic->GetArgDWord(0));
	}

	static void GenericMakeTemporary(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		if (FMutationState* const State = GetMutationState(*Generic))
		{
			++State->TemporaryCalls;
		}
		Generic->SetReturnDWord(Generic->GetArgDWord(0));
	}

	static void GenericReadNonLValue(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		if (FMutationState* const State = GetMutationState(*Generic))
		{
			++State->NonLValueCalls;
		}
		Generic->SetReturnDWord(Generic->GetArgDWord(0));
	}

	static void GenericWriteOut(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		if (FMutationState* const State = GetMutationState(*Generic))
		{
			++State->OutCalls;
		}
		if (int32* const Target = static_cast<int32*>(Generic->GetArgAddress(0)))
		{
			*Target = 31;
		}
	}

	static void GenericBindAlias(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		if (FMutationState* const State = GetMutationState(*Generic))
		{
			++State->AliasBindCalls;
		}
		Generic->SetReturnAddress(Generic->GetArgAddress(0));
	}

	static bool RegisterMutationFixtures(asIScriptEngine& ScriptEngine, FMutationState& State)
	{
		ScriptEngine.SetUserData(&State, MutationStateUserDataSlot);
		return ScriptEngine.RegisterObjectType("FMutationOwner", 0,
				asOBJ_REF | asOBJ_IMPLICIT_HANDLE) >= 0 &&
			   ScriptEngine.RegisterObjectBehaviour("FMutationOwner",
				   asBEHAVE_ADDREF,
				   "void f()",
				   asFUNCTION(GenericOwnerAddRef),
				   asCALL_GENERIC) >= 0 &&
			   ScriptEngine.RegisterObjectBehaviour("FMutationOwner",
				   asBEHAVE_RELEASE,
				   "void f()",
				   asFUNCTION(GenericOwnerRelease),
				   asCALL_GENERIC) >= 0 &&
			   ScriptEngine.RegisterObjectProperty("FMutationOwner",
				   "int DirectTarget",
				   asOFFSET(FMutationOwner, DirectTarget)) >= 0 &&
			   ScriptEngine.RegisterObjectProperty("FMutationOwner",
				   "int FieldTarget",
				   asOFFSET(FMutationOwner, FieldTarget)) >= 0 &&
			   ScriptEngine.RegisterObjectMethod("FMutationOwner",
				   "int GetPropertyTarget() const",
				   asFUNCTION(GenericGetPropertyTarget),
				   asCALL_GENERIC) >= 0 &&
			   ScriptEngine.RegisterObjectMethod("FMutationOwner",
				   "void SetPropertyTarget(int Value)",
				   asFUNCTION(GenericSetPropertyTarget),
				   asCALL_GENERIC) >= 0 &&
			   ScriptEngine.RegisterObjectMethod("FMutationOwner",
				   "int GetItem(int Index) const",
				   asFUNCTION(GenericGetIndexedTarget),
				   asCALL_GENERIC) >= 0 &&
			   ScriptEngine.RegisterObjectMethod("FMutationOwner",
				   "void SetItem(int Index, int Value)",
				   asFUNCTION(GenericSetIndexedTarget),
				   asCALL_GENERIC) >= 0 &&
			   ScriptEngine.RegisterObjectMethod("FMutationOwner",
				   "int& IndexedAlias(int Index)",
				   asFUNCTION(GenericGetIndexedAlias),
				   asCALL_GENERIC) >= 0 &&
			   ScriptEngine.RegisterGlobalFunction("FMutationOwner MakeMutationOwner()",
				   asFUNCTION(GenericMakeOwner),
				   asCALL_GENERIC) >= 0 &&
			   ScriptEngine.RegisterGlobalFunction("FMutationOwner GetGlobalMutationOwner()",
				   asFUNCTION(GenericGetGlobalOwner),
				   asCALL_GENERIC) >= 0 &&
			   ScriptEngine.RegisterGlobalFunction("int NextMutationOperand(int Value)",
				   asFUNCTION(GenericNextOperand),
				   asCALL_GENERIC) >= 0 &&
			   ScriptEngine.RegisterGlobalFunction("int MakeMutationTemporary(int Value)",
				   asFUNCTION(GenericMakeTemporary),
				   asCALL_GENERIC) >= 0 &&
			   ScriptEngine.RegisterGlobalFunction("int ReadMutationNonLValue(int Value)",
				   asFUNCTION(GenericReadNonLValue),
				   asCALL_GENERIC) >= 0 &&
			   ScriptEngine.RegisterGlobalFunction("void WriteMutationOut(int& out Target)",
				   asFUNCTION(GenericWriteOut),
				   asCALL_GENERIC) >= 0 &&
			   ScriptEngine.RegisterGlobalFunction("int& BindMutationAlias(int& inout Target)",
				   asFUNCTION(GenericBindAlias),
				   asCALL_GENERIC) >= 0 &&
			   ScriptEngine.RegisterGlobalProperty("int GlobalTarget", &State.GlobalTarget) >= 0 &&
			   ScriptEngine.RegisterGlobalProperty(
				   "const int ConstGlobalTarget", &State.ConstGlobalTarget) >= 0;
	}

	static void AppendOwnerLocal(FString& Source, const FString& Name, const bool bConst)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source,
			FString::Printf(TEXT("\t%sFMutationOwner %s = MakeMutationOwner();"),
				bConst ? TEXT("const ") : TEXT(""),
				*Name));
	}

	static FGeneratedTarget BuildMutablePlacementTarget(
		FString& Source, const FPlacementCase& Placement)
	{
		using namespace AngelscriptNativeTestSupport;
		FGeneratedTarget Result;
		switch (Placement.Placement)
		{
		case EPlacement::Local:
			AngelscriptNativeTestSupport::AppendGeneratedAsLine(
				Source, TEXT("\tint LocalTarget = 10;"));
			Result.TargetExpression = TEXT("LocalTarget");
			break;
		case EPlacement::Global:
			Result.TargetExpression = TEXT("GlobalTarget");
			break;
		case EPlacement::Member:
			AppendOwnerLocal(Source, TEXT("RootOwner"), false);
			Result.TargetExpression = TEXT("RootOwner.DirectTarget");
			break;
		case EPlacement::Indexed:
			AppendOwnerLocal(Source, TEXT("RootOwner"), false);
			AppendGeneratedAsLine(Source,
				TEXT("\tint& IndexedTarget = RootOwner.IndexedAlias(0);"));
			Result.TargetExpression = TEXT("IndexedTarget");
			break;
		default:
			break;
		}
		return Result;
	}

	static FGeneratedTarget BuildConstPlacementTarget(
		FString& Source, const FPlacementCase& Placement)
	{
		using namespace AngelscriptNativeTestSupport;
		FGeneratedTarget Result;
		switch (Placement.Placement)
		{
		case EPlacement::Local:
			AngelscriptNativeTestSupport::AppendGeneratedAsLine(
				Source, TEXT("\tconst int ConstLocalTarget = 10;"));
			Result.TargetExpression = TEXT("ConstLocalTarget");
			break;
		case EPlacement::Global:
			Result.TargetExpression = TEXT("ConstGlobalTarget");
			break;
		case EPlacement::Member:
			AppendOwnerLocal(Source, TEXT("ConstOwner"), true);
			Result.TargetExpression = TEXT("ConstOwner.DirectTarget");
			break;
		case EPlacement::Indexed:
			AppendOwnerLocal(Source, TEXT("ConstOwner"), true);
			AppendGeneratedAsLine(Source,
				TEXT("\tint& IndexedTarget = ConstOwner.IndexedAlias(0);"));
			Result.TargetExpression = TEXT("IndexedTarget");
			break;
		default:
			break;
		}
		return Result;
	}

	static FGeneratedTarget BuildReferenceAliasTarget(
		FString& Source, const FPlacementCase& Placement)
	{
		using namespace AngelscriptNativeTestSupport;

		FGeneratedTarget Result;
		switch (Placement.Placement)
		{
		case EPlacement::Local:
			AppendGeneratedAsLine(Source, TEXT("\tint LocalTarget = 10;"));
			AppendGeneratedAsLine(
				Source, TEXT("\tint& AliasTarget = BindMutationAlias(LocalTarget);"));
			Result.AliasObservationExpression = TEXT("LocalTarget");
			break;
		case EPlacement::Global:
			AppendGeneratedAsLine(
				Source, TEXT("\tint& AliasTarget = BindMutationAlias(GlobalTarget);"));
			Result.AliasObservationExpression = TEXT("GlobalTarget");
			break;
		case EPlacement::Member:
			AppendOwnerLocal(Source, TEXT("RootOwner"), false);
			AppendGeneratedAsLine(
				Source, TEXT("\tint& AliasTarget = BindMutationAlias(RootOwner.DirectTarget);"));
			Result.AliasObservationExpression = TEXT("RootOwner.DirectTarget");
			break;
		case EPlacement::Indexed:
			AppendOwnerLocal(Source, TEXT("RootOwner"), false);
			AppendGeneratedAsLine(Source, TEXT("\tint& AliasTarget = RootOwner.IndexedAlias(0);"));
			Result.AliasObservationExpression = TEXT("RootOwner.GetItem(0)");
			break;
		default:
			break;
		}
		Result.TargetExpression = TEXT("AliasTarget");
		return Result;
	}

	static FString OwnerRouteExpression(FString& Source, const FPlacementCase& Placement)
	{
		switch (Placement.Placement)
		{
		case EPlacement::Local:
			AppendOwnerLocal(Source, TEXT("LocalOwner"), false);
			return TEXT("LocalOwner");
		case EPlacement::Global:
			return TEXT("GetGlobalMutationOwner()");
		case EPlacement::Member:
			AppendOwnerLocal(Source, TEXT("RootOwner"), false);
			return TEXT("RootOwner");
		case EPlacement::Indexed:
			AppendOwnerLocal(Source, TEXT("RootOwner"), false);
			return TEXT("RootOwner");
		default:
			return TEXT("MakeMutationOwner()");
		}
	}

	static FGeneratedTarget BuildFieldOrPropertyTarget(
		FString& Source, const FCategoryCase& Category, const FPlacementCase& Placement)
	{
		using namespace AngelscriptNativeTestSupport;
		FGeneratedTarget Result;
		const FString Owner = OwnerRouteExpression(Source, Placement);
		if (Category.Category == EValueCategory::Field)
		{
			Result.TargetExpression = Placement.Placement == EPlacement::Indexed
				? TEXT("IndexedTarget")
				: Owner + TEXT(".FieldTarget");
			if (Placement.Placement == EPlacement::Indexed)
			{
				AppendGeneratedAsLine(Source,
					TEXT("\tint& IndexedTarget = RootOwner.IndexedAlias(0);"));
			}
		}
		else
		{
			Result.TargetExpression = Owner + TEXT(".GetPropertyTarget()");
		}
		return Result;
	}

	static FGeneratedTarget BuildGeneratedTarget(
		FString& Source, const FCategoryCase& Category, const FPlacementCase& Placement)
	{
		switch (Category.Category)
		{
		case EValueCategory::MutableLValue:
			return BuildMutablePlacementTarget(Source, Placement);
		case EValueCategory::ConstLValue:
			return BuildConstPlacementTarget(Source, Placement);
		case EValueCategory::Temporary:
		{
			FGeneratedTarget Result = BuildMutablePlacementTarget(Source, Placement);
			Result.TargetExpression =
				TEXT("MakeMutationTemporary(") + Result.TargetExpression + TEXT(")");
			return Result;
		}
		case EValueCategory::ReferenceAlias:
			return BuildReferenceAliasTarget(Source, Placement);
		case EValueCategory::Field:
		case EValueCategory::Property:
			return BuildFieldOrPropertyTarget(Source, Category, Placement);
		case EValueCategory::InvalidNonLValue:
		{
			FGeneratedTarget Result = BuildMutablePlacementTarget(Source, Placement);
			Result.TargetExpression =
				TEXT("(ReadMutationNonLValue(") + Result.TargetExpression + TEXT(") + 0)");
			return Result;
		}
		default:
			return FGeneratedTarget();
		}
	}

	static void AppendMutationOperation(
		FString& Source, const FString& Target, const FMutationCase& Mutation)
	{
		using namespace AngelscriptNativeTestSupport;

		switch (Mutation.Mutation)
		{
		case EMutation::Assign:
			AppendGeneratedAsLine(Source, TEXT("\t") + Target + TEXT(" = NextMutationOperand(20);"));
			AppendGeneratedAsLine(Source, TEXT("\tint Result = ") + Target + TEXT(";"));
			break;
		case EMutation::CompoundAssign:
			AppendGeneratedAsLine(Source, TEXT("\t") + Target + TEXT(" += NextMutationOperand(5);"));
			AppendGeneratedAsLine(Source, TEXT("\tint Result = ") + Target + TEXT(";"));
			break;
		case EMutation::PrefixIncrement:
			AppendGeneratedAsLine(Source, TEXT("\t++") + Target + TEXT(";"));
			AppendGeneratedAsLine(Source, TEXT("\tint Result = ") + Target + TEXT(";"));
			break;
		case EMutation::PostfixIncrement:
			AppendGeneratedAsLine(Source, TEXT("\tint Result = ") + Target + TEXT(";"));
			AppendGeneratedAsLine(Source, TEXT("\t++") + Target + TEXT(";"));
			break;
		case EMutation::OutArgument:
			AppendGeneratedAsLine(Source, TEXT("\tWriteMutationOut(") + Target + TEXT(");"));
			AppendGeneratedAsLine(Source, TEXT("\tint Result = 31;"));
			break;
		default:
			AppendGeneratedAsLine(Source, TEXT("\tint Result = 0;"));
			break;
		}
	}

	static FString BuildExpressionValueCategorySource(const FCategoryCase& Category,
		const FMutationCase& Mutation,
		const FPlacementCase& Placement)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int MutationMetadataWitness(int& inout Value)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunExpressionMutation()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		const FGeneratedTarget Target = BuildGeneratedTarget(Source, Category, Placement);
		AppendMutationOperation(Source, Target.TargetExpression, Mutation);
		if (Category.bWritable)
		{
			AppendGeneratedAsLine(
				Source, TEXT("\tint After = ") + Target.TargetExpression + TEXT(";"));
			AppendGeneratedAsLine(Source,
				TEXT("\tint AliasAfter = ") +
					(Target.AliasObservationExpression.IsEmpty()
							? TEXT("After")
							: Target.AliasObservationExpression) +
					TEXT(";"));
			AppendGeneratedAsLine(
				Source, TEXT("\treturn Result * 10000 + After * 100 + AliasAfter;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\treturn Result;"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RecoverExpressionMutation()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 941;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static FString BuildExpressionValueCategoryRecoverySource()
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int RecoverExpressionMutation()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 941;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static int CompileAndReport(FAutomationTestBase& Test,
		asIScriptEngine& ScriptEngine,
		const FString& SourceId,
		const FString& ModuleName,
		const FString& Source,
		asIScriptModule*& OutModule)
	{
		AngelscriptNativeTestSupport::PrintGeneratedAsSource(Test, SourceId, ModuleName, Source);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		return AngelscriptNativeTestSupport::CompileNativeModule(
			&ScriptEngine, ModuleNameUtf8.Get(), SourceUtf8.Get(), OutModule);
	}

	static void DiscardMutationModule(asIScriptEngine& ScriptEngine, const FString& ModuleName)
	{
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		ScriptEngine.DiscardModule(ModuleNameUtf8.Get());
	}

	static bool HasLocatedError(const FNativeTestEngine& Engine)
	{
		return Engine.GetMessages().Entries.ContainsByPredicate(
			[](const AngelscriptNativeTestSupport::FNativeMessageEntry& Message)
			{ return Message.Type == asMSGTYPE_ERROR && Message.Row > 0 && Message.Column > 0; });
	}

	static bool HasAnyError(const FNativeTestEngine& Engine)
	{
		return Engine.GetMessages().Entries.ContainsByPredicate(
			[](const AngelscriptNativeTestSupport::FNativeMessageEntry& Message)
			{
				return Message.Type == asMSGTYPE_ERROR;
			});
	}

	static int FindGlobalProperty(asIScriptEngine& ScriptEngine, const ANSICHAR* Name)
	{
		for (asUINT Index = 0; Index < ScriptEngine.GetGlobalPropertyCount(); ++Index)
		{
			const ANSICHAR* PropertyName = nullptr;
			if (ScriptEngine.GetGlobalPropertyByIndex(Index, &PropertyName) >= 0 &&
				PropertyName != nullptr && FCStringAnsi::Strcmp(PropertyName, Name) == 0)
			{
				return static_cast<int>(Index);
			}
		}
		return -1;
	}

	static int FindObjectProperty(
		asITypeInfo& TypeInfo,
		const ANSICHAR* Name)
	{
		for (asUINT Index = 0;
			 Index < TypeInfo.GetPropertyCount();
			 ++Index)
		{
			const ANSICHAR* PropertyName = nullptr;
			if (TypeInfo.GetProperty(
					Index,
					&PropertyName) >= 0
				&& PropertyName != nullptr
				&& FCStringAnsi::Strcmp(PropertyName, Name) == 0)
			{
				return static_cast<int>(Index);
			}
		}
		return -1;
	}

	void VerifyMetadata(const FNativeCaseContext& Case,
		const FCategoryCase& Category,
		const FPlacementCase& Placement,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module)
	{
		asIScriptFunction* const Entry = Module.GetFunctionByDecl("int RunExpressionMutation()");
		ASSERT_THAT(IsNotNull(
			Entry, *Case.Describe(TEXT("value-category cell should publish its exact entry"))));

		asIScriptFunction* const Witness =
			AngelscriptNativeTestSupport::GetNativeFunctionByDecl(
				&Module,
				"int MutationMetadataWitness(int&inout)");
		ASSERT_THAT(IsNotNull(Witness,
			*Case.Describe(TEXT("value-category cell should publish its inout metadata witness"))));
		if (Witness != nullptr)
		{
			int TypeId = asTYPEID_VOID;
			asDWORD Flags = 0;
			ASSERT_THAT(AreEqual(asSUCCESS,
				Witness->GetParam(0, &TypeId, &Flags),
				*Case.Describe(TEXT("mutation metadata parameter should be readable"))));
			ASSERT_THAT(AreEqual(asTYPEID_INT32,
				TypeId,
				*Case.Describe(TEXT("mutation metadata parameter should remain int"))));
			ASSERT_THAT(AreEqual(static_cast<asDWORD>(asTM_INOUTREF),
				Flags,
				*Case.Describe(TEXT("mutation metadata parameter should remain inout"))));
		}

		if (Placement.Placement == EPlacement::Global)
		{
			if (Category.Category == EValueCategory::Field ||
				Category.Category == EValueCategory::Property)
			{
				asIScriptFunction* const GlobalOwnerRoute =
					ScriptEngine.GetGlobalFunctionByDecl("FMutationOwner GetGlobalMutationOwner()");
				ASSERT_THAT(IsNotNull(GlobalOwnerRoute,
					*Case.Describe(TEXT("global owner placement should retain its exact route"))));
			}
			else
			{
				const bool bConst = Category.Category == EValueCategory::ConstLValue;
				const ANSICHAR* const Name = bConst ? "ConstGlobalTarget" : "GlobalTarget";
				const int PropertyIndex = FindGlobalProperty(ScriptEngine, Name);
				ASSERT_THAT(IsTrue(PropertyIndex >= 0,
					*Case.Describe(TEXT("global placement should retain its registered storage"))));
				if (PropertyIndex >= 0)
				{
					int TypeId = asTYPEID_VOID;
					bool bActualConst = false;
					void* Address = nullptr;
					ASSERT_THAT(AreEqual(asSUCCESS,
						ScriptEngine.GetGlobalPropertyByIndex(static_cast<asUINT>(PropertyIndex),
							nullptr,
							nullptr,
							&TypeId,
							&bActualConst,
							nullptr,
							&Address),
						*Case.Describe(TEXT("global placement metadata should be readable"))));
					ASSERT_THAT(AreEqual(asTYPEID_INT32,
						TypeId,
						*Case.Describe(TEXT("global placement should retain int storage"))));
					ASSERT_THAT(AreEqual(bConst,
						bActualConst,
						*Case.Describe(TEXT("global placement should retain its const trait"))));
					ASSERT_THAT(IsNotNull(Address,
						*Case.Describe(TEXT("global placement should retain its native address"))));
				}
			}
		}

		asITypeInfo* const OwnerType = ScriptEngine.GetTypeInfoByName("FMutationOwner");
		ASSERT_THAT(IsNotNull(
			OwnerType, *Case.Describe(TEXT("mutation owner metadata should remain registered"))));
		if (OwnerType == nullptr)
		{
			return;
		}
		const bool bUsesStoredField =
			Category.Category == EValueCategory::Field
			|| ((Category.Category == EValueCategory::MutableLValue
					|| Category.Category == EValueCategory::ReferenceAlias)
				&& Placement.Placement == EPlacement::Member);
		if (bUsesStoredField)
		{
			const bool bFieldCategory =
				Category.Category == EValueCategory::Field;
			const ANSICHAR* const PropertyName =
				bFieldCategory ? "FieldTarget" : "DirectTarget";
			const int PropertyIndex =
				FindObjectProperty(*OwnerType, PropertyName);
			ASSERT_THAT(IsTrue(
				PropertyIndex >= 0,
				*Case.Describe(
					TEXT("stored mutation target should retain its exact property"))));
			if (PropertyIndex >= 0)
			{
				int TypeId = asTYPEID_VOID;
				int Offset = INDEX_NONE;
				bool bPrivate = true;
				bool bProtected = true;
				bool bReference = true;
				ASSERT_THAT(AreEqual(
					asSUCCESS,
					OwnerType->GetProperty(
						static_cast<asUINT>(PropertyIndex),
						nullptr,
						&TypeId,
						&bPrivate,
						&bProtected,
						&Offset,
						&bReference),
					*Case.Describe(
						TEXT("stored mutation target metadata should be readable"))));
				ASSERT_THAT(AreEqual(
					asTYPEID_INT32,
					TypeId,
					*Case.Describe(
						TEXT("stored mutation target should retain int type"))));
				ASSERT_THAT(AreEqual(
					bFieldCategory
						? static_cast<int>(asOFFSET(FMutationOwner, FieldTarget))
						: static_cast<int>(asOFFSET(FMutationOwner, DirectTarget)),
					Offset,
					*Case.Describe(
						TEXT("stored mutation target should retain its native offset"))));
				ASSERT_THAT(IsFalse(
					bPrivate || bProtected || bReference,
					*Case.Describe(
						TEXT("stored mutation target should remain public direct storage"))));
			}
		}
		if (Category.Category == EValueCategory::Property)
		{
			asIScriptFunction* const Getter =
				OwnerType->GetMethodByDecl("int GetPropertyTarget() const");
			asIScriptFunction* const Setter =
				OwnerType->GetMethodByDecl("void SetPropertyTarget(int)");
			ASSERT_THAT(IsNotNull(Getter,
				*Case.Describe(
					TEXT("property target getter should retain its exact declaration"))));
			ASSERT_THAT(IsNotNull(Setter,
				*Case.Describe(
					TEXT("property target setter should retain its exact declaration"))));
			if (Getter != nullptr && Setter != nullptr)
			{
				ASSERT_THAT(IsFalse(Getter->IsProperty() || Setter->IsProperty(),
					*Case.Describe(
						TEXT("current-fork property replacement should use explicit methods"))));
			}
		}
		if (Category.Category == EValueCategory::ReferenceAlias)
		{
			asIScriptFunction* AliasRoute = nullptr;
			if (Placement.Placement == EPlacement::Indexed)
			{
				AliasRoute = OwnerType->GetMethodByName("IndexedAlias");
			}
			else
			{
				for (asUINT Index = 0; Index < ScriptEngine.GetGlobalFunctionCount(); ++Index)
				{
					asIScriptFunction* const Candidate =
						ScriptEngine.GetGlobalFunctionByIndex(Index);
					if (Candidate != nullptr &&
						FCStringAnsi::Strcmp(Candidate->GetName(), "BindMutationAlias") == 0)
					{
						AliasRoute = Candidate;
						break;
					}
				}
			}
			ASSERT_THAT(IsNotNull(
				AliasRoute,
				*Case.Describe(
					TEXT("reference-alias target should retain its exact reference route"))));
		}
		if ((Category.Category == EValueCategory::MutableLValue ||
				Category.Category == EValueCategory::ConstLValue) &&
			Placement.Placement == EPlacement::Indexed)
		{
			asIScriptFunction* const Getter = OwnerType->GetMethodByName("GetItem");
			asIScriptFunction* const Setter = OwnerType->GetMethodByName("SetItem");
			ASSERT_THAT(IsNotNull(Getter,
				*Case.Describe(TEXT("indexed target getter should retain its exact declaration"))));
			ASSERT_THAT(IsNotNull(Setter,
				*Case.Describe(TEXT("indexed target setter should retain its exact declaration"))));
			if (Getter != nullptr && Setter != nullptr)
			{
				ASSERT_THAT(IsFalse(Getter->IsProperty() || Setter->IsProperty(),
					*Case.Describe(
						TEXT("current-fork indexed replacement should use explicit methods"))));
			}
		}
	}

	void ExecuteRecovery(
		const FNativeCaseContext& Case, asIScriptEngine& ScriptEngine, asIScriptModule& Module)
	{
		asIScriptFunction* const Recovery =
			Module.GetFunctionByDecl("int RecoverExpressionMutation()");
		asIScriptContext* const Context = ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Recovery,
			*Case.Describe(TEXT("value-category recovery should publish its exact function"))));
		ASSERT_THAT(IsNotNull(
			Context, *Case.Describe(TEXT("value-category recovery should create a context"))));
		if (Recovery != nullptr && Context != nullptr)
		{
			ASSERT_THAT(AreEqual(asSUCCESS,
				Context->Prepare(Recovery),
				*Case.Describe(TEXT("value-category recovery should prepare"))));
			ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
				Context->Execute(),
				*Case.Describe(TEXT("value-category recovery should finish"))));
			ASSERT_THAT(AreEqual(941,
				static_cast<int32>(Context->GetReturnDWord()),
				*Case.Describe(TEXT("value-category recovery should return its sentinel"))));
			Context->Unprepare();
		}
		if (Context != nullptr)
		{
			Context->Release();
		}
	}

	void VerifyRuntimeCounters(const FNativeCaseContext& Case,
		const FCategoryCase& Category,
		const FMutationCase& Mutation,
		const FPlacementCase& Placement,
		const FMutationState& State)
	{
		const int32 ExpectedOperandCalls =
			Mutation.Mutation == EMutation::Assign || Mutation.Mutation == EMutation::CompoundAssign
				? 1
				: 0;
		ASSERT_THAT(AreEqual(ExpectedOperandCalls,
			State.OperandCalls,
			*Case.Describe(
				TEXT("mutation operand should be evaluated exactly once when present"))));
		ASSERT_THAT(AreEqual(Mutation.Mutation == EMutation::OutArgument ? 1 : 0,
			State.OutCalls,
			*Case.Describe(TEXT("out mutation should invoke its writer exactly once"))));
		const bool bIndexedWritableRoute = Placement.Placement == EPlacement::Indexed &&
			(Category.Category == EValueCategory::MutableLValue ||
				Category.Category == EValueCategory::Field);
		ASSERT_THAT(AreEqual((Category.Category == EValueCategory::ReferenceAlias ||
								 bIndexedWritableRoute)
								? 1
								: 0,
			State.AliasBindCalls,
			*Case.Describe(TEXT("reference-alias target should bind exactly once"))));
		ASSERT_THAT(AreEqual(0,
			State.TemporaryCalls,
			*Case.Describe(
				TEXT("writable mutation should not execute the rejected temporary path"))));
		ASSERT_THAT(AreEqual(0,
			State.NonLValueCalls,
			*Case.Describe(
				TEXT("writable mutation should not execute the rejected non-lvalue path"))));

		const bool bProperty = Category.Category == EValueCategory::Property;
		const int32 ExpectedReadCalls = Mutation.Mutation == EMutation::CompoundAssign ||
												Mutation.Mutation == EMutation::PrefixIncrement ||
												Mutation.Mutation == EMutation::PostfixIncrement
											? 2
											: 1;
		ASSERT_THAT(AreEqual(bProperty ? ExpectedReadCalls : 0,
			State.PropertyGetCalls,
			*Case.Describe(TEXT("property target should expose the exact getter count"))));
		ASSERT_THAT(AreEqual(bProperty ? 1 : 0,
			State.PropertySetCalls,
			*Case.Describe(TEXT("property target should expose exactly one setter write"))));

		const bool bDirectIndexed = false;
		const bool bAliasIndexed = Category.Category == EValueCategory::ReferenceAlias &&
			Placement.Placement == EPlacement::Indexed;
		ASSERT_THAT(AreEqual(bDirectIndexed ? ExpectedReadCalls : bAliasIndexed ? 1 : 0,
			State.IndexedGetCalls,
			*Case.Describe(TEXT("indexed target should expose the exact getter count"))));
		ASSERT_THAT(AreEqual(bDirectIndexed ? 1 : 0,
			State.IndexedSetCalls,
			*Case.Describe(TEXT("indexed target should expose exactly one setter write"))));
	}

	void RunCell(const FCategoryCase& Category,
		const FMutationCase& Mutation,
		const FPlacementCase& Placement)
	{
		using namespace AngelscriptNativeTestSupport;

		const FNativeCaseContext Case(MakeNativeCaseId("LANG-EXPR-VALUE-MUTATION",
			{
				ANSI_TO_TCHAR(Category.CatalogName),
				ANSI_TO_TCHAR(Mutation.CatalogName),
				ANSI_TO_TCHAR(Placement.CatalogName),
			}));
		FMutationState State;
		State.Reset();
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(
			ScriptEngine, *Case.Describe(TEXT("value-category cell should create a raw engine"))));
		if (ScriptEngine == nullptr)
		{
			return;
		}
		const bool bFixturesRegistered = RegisterMutationFixtures(*ScriptEngine, State);
		ASSERT_THAT(IsTrue(bFixturesRegistered,
			*Case.DescribeResult("<fixture registration>",
				TEXT("mutation fixtures"), Engine.GetMessagesText())));
		ASSERT_THAT(AreEqual(static_cast<asPWORD>(3),
			ScriptEngine->GetEngineProperty(asEP_PROPERTY_ACCESSOR_MODE),
			*Case.Describe(
				TEXT("bare SDK mutation engine should retain registered accessor mode"))));

		const FString ModuleName = FString::Printf(
			TEXT("ExpressionValueCategory_%s"),
			*Case.GetId());
		const FString Source = BuildExpressionValueCategorySource(Category, Mutation, Placement);
		Engine.ResetMessages();
		asIScriptModule* Module = nullptr;
		const int BuildResult =
			CompileAndReport(*TestRunner, *ScriptEngine, Case.GetId(), ModuleName, Source, Module);
		if (!Category.bWritable)
		{
			ASSERT_THAT(IsTrue(BuildResult < 0,
				*Case.Describe(
					TEXT("non-writable value category should fail at its mutation site"))));
			ASSERT_THAT(IsTrue(HasLocatedError(Engine),
				*Case.Describe(
					TEXT("non-writable value category should own a located diagnostic"))));
			ASSERT_THAT(AreEqual(0,
				State.CreatedOwners,
				*Case.Describe(TEXT("rejected mutation should execute no owner factory"))));
			ASSERT_THAT(AreEqual(0,
				State.OperandCalls + State.OutCalls + State.AliasBindCalls + State.TemporaryCalls +
					State.NonLValueCalls + State.PropertySetCalls + State.IndexedSetCalls,
				*Case.Describe(TEXT("rejected mutation should execute no native operation"))));
			DiscardMutationModule(*ScriptEngine, ModuleName);
			const FString RecoverySource = BuildExpressionValueCategoryRecoverySource();
			Engine.ResetMessages();
			asIScriptModule* RecoveryModule = nullptr;
			ASSERT_THAT(IsTrue(CompileAndReport(*TestRunner,
								   *ScriptEngine,
								   Case.GetId() + TEXT("-RECOVERY"),
								   ModuleName,
								   RecoverySource,
								   RecoveryModule) >= 0,
				*Case.Describe(TEXT("rejected mutation should permit same-name recovery"))));
			if (RecoveryModule != nullptr)
			{
				ExecuteRecovery(Case, *ScriptEngine, *RecoveryModule);
			}
		}
		else
		{
			ASSERT_THAT(IsTrue(BuildResult >= 0,
				*Case.Describe(TEXT("writable value-category source should compile"))));
			ASSERT_THAT(IsNotNull(Module,
				*Case.Describe(TEXT("writable value-category source should publish a module"))));
			ASSERT_THAT(IsFalse(HasAnyError(Engine),
				*Case.Describe(TEXT("writable value-category source should emit no errors"))));
			if (Module != nullptr)
			{
				VerifyMetadata(Case, Category, Placement, *ScriptEngine, *Module);
				asIScriptFunction* const Entry =
					Module->GetFunctionByDecl("int RunExpressionMutation()");
				asIScriptFunction* const Recovery =
					Module->GetFunctionByDecl("int RecoverExpressionMutation()");
				asIScriptContext* const Context = ScriptEngine->CreateContext();
				ASSERT_THAT(IsNotNull(Entry,
					*Case.Describe(TEXT("writable mutation should publish its exact entry"))));
				ASSERT_THAT(IsNotNull(Recovery,
					*Case.Describe(
						TEXT("writable mutation should publish same-context recovery"))));
				ASSERT_THAT(IsNotNull(
					Context, *Case.Describe(TEXT("writable mutation should create a context"))));
				if (Entry != nullptr && Recovery != nullptr && Context != nullptr)
				{
					ASSERT_THAT(AreEqual(asSUCCESS,
						Context->Prepare(Entry),
						*Case.Describe(TEXT("writable mutation should prepare"))));
					ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
						Context->Execute(),
						*Case.Describe(TEXT("writable mutation should finish"))));
					const int32 ExpectedEncoded = Mutation.ExpectedResult * 10000 +
												  Mutation.ExpectedAfter * 100 +
												  Mutation.ExpectedAfter;
					ASSERT_THAT(AreEqual(ExpectedEncoded,
						static_cast<int32>(Context->GetReturnDWord()),
						*Case.Describe(
							TEXT("writable mutation should preserve result, after-value, "
								 "and alias visibility"))));
					VerifyRuntimeCounters(Case, Category, Mutation, Placement, State);
					ASSERT_THAT(AreEqual(asSUCCESS,
						Context->Unprepare(),
						*Case.Describe(TEXT("writable mutation should unprepare"))));
					ASSERT_THAT(AreEqual(asSUCCESS,
						Context->Prepare(Recovery),
						*Case.Describe(TEXT(
							"writable mutation recovery should prepare on the same context"))));
					ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
						Context->Execute(),
						*Case.Describe(TEXT("writable mutation recovery should finish"))));
					ASSERT_THAT(AreEqual(941,
						static_cast<int32>(Context->GetReturnDWord()),
						*Case.Describe(
							TEXT("writable mutation recovery should return its sentinel"))));
					Context->Unprepare();
				}
				if (Context != nullptr)
				{
					Context->Release();
				}
			}
		}

		DiscardMutationModule(*ScriptEngine, ModuleName);
		ReleaseRetainedOwner(State);
		ScriptEngine->SetUserData(nullptr, MutationStateUserDataSlot);
		ASSERT_THAT(AreEqual(0,
			State.LiveOwners,
			*Case.Describe(TEXT("value-category cell should leave no live owners"))));
		ASSERT_THAT(AreEqual(State.CreatedOwners,
			State.DestroyedOwners,
			*Case.Describe(TEXT("value-category cell should destroy every created owner"))));
	}

public:
	TEST_METHOD(CategoriesByMutationAndPlacement)
	{
		AS_NATIVE_PRODUCT("LANG-EXPR-VALUE-MUTATION",
			AngelscriptNativeTestSupport::ENativeEvidence::Compile |
				AngelscriptNativeTestSupport::ENativeEvidence::Diagnostic |
				AngelscriptNativeTestSupport::ENativeEvidence::Runtime |
				AngelscriptNativeTestSupport::ENativeEvidence::Metadata);

		for (const FCategoryCase& Category : CategoryCases)
		{
			for (const FMutationCase& Mutation : MutationCases)
			{
				for (const FPlacementCase& Placement : PlacementCases)
				{
					RunCell(Category, Mutation, Placement);
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
