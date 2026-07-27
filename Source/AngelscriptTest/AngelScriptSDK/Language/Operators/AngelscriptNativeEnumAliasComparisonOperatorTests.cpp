#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#include <limits>
#include <type_traits>

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FEnumAliasComparisonOperatorTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Operators.Comparison",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeCaseContext = AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeTestEngine = AngelscriptNativeTestSupport::FNativeTestEngine;

	inline static constexpr asPWORD ComparisonTraceSlot =
		static_cast<asPWORD>(0x4E4154434F4D5054ull);
	inline static constexpr asPWORD ReferenceStateSlot =
		static_cast<asPWORD>(0x4E4154434F4D5052ull);
	inline static constexpr asPWORD OverloadStateSlot = static_cast<asPWORD>(0x4E4154434F4D504Full);

	enum class EComparisonOperator : uint8
	{
		Less,
		LessEqual,
		Greater,
		GreaterEqual,
		Equal,
		NotEqual,
	};

	enum class EFloatType : uint8
	{
		Float32,
		Float64,
	};

	enum class EFloatValue : uint8
	{
		NegativeZero,
		PositiveZero,
		NaN,
		PositiveInfinity,
		NegativeInfinity,
		Minimum,
		Maximum,
		EqualPair,
	};

	enum class EComparisonOrder : uint8
	{
		LeftRight,
		RightLeft,
	};

	enum class EIntegralFamily : uint8
	{
		Enum,
		Alias,
	};

	enum class EIntegralPair : uint8
	{
		EqualZero,
		EqualNamed,
		BelowAdjacent,
		AboveAdjacent,
		MinimumBoundary,
		MaximumBoundary,
	};

	enum class EReferenceRelation : uint8
	{
		SameNonNull,
		DifferentNonNull,
		LeftNull,
		RightNull,
		BothNull,
		DerivedBaseSame,
		SiblingDifferent,
		ConstSame,
	};

	enum class EReferenceKind : uint8
	{
		Root = 1,
		Derived = 2,
		Sibling = 3,
	};

	enum class EOverloadRelation : uint8
	{
		Less,
		Equal,
		Greater,
		Unequal,
	};

	enum class EReceiverConstness : uint8
	{
		Mutable,
		Const,
	};

	struct FOperatorCase
	{
		const ANSICHAR* CatalogName;
		const TCHAR* Token;
		EComparisonOperator Operator;
	};

	struct FFloatTypeCase
	{
		const ANSICHAR* CatalogName;
		EFloatType Type;
	};

	struct FFloatValueCase
	{
		const ANSICHAR* CatalogName;
		EFloatValue Value;
	};

	struct FOrderCase
	{
		const ANSICHAR* CatalogName;
		const ANSICHAR* FunctionName;
		EComparisonOrder Order;
	};

	struct FIntegralFamilyCase
	{
		const ANSICHAR* CatalogName;
		EIntegralFamily Family;
	};

	struct FIntegralPairCase
	{
		const ANSICHAR* CatalogName;
		EIntegralPair Pair;
	};

	struct FReferenceRelationCase
	{
		const ANSICHAR* CatalogName;
		EReferenceRelation Relation;
		int32 ExpectedObjects;
		bool bEqual;
	};

	struct FOverloadRelationCase
	{
		const ANSICHAR* CatalogName;
		EOverloadRelation Relation;
		int32 Left;
		int32 Right;
	};

	struct FReceiverCase
	{
		const ANSICHAR* CatalogName;
		EReceiverConstness Constness;
	};

	struct FComparisonTrace
	{
		void Reset()
		{
			Markers.Reset();
			Bits.Reset();
			IntegerValues.Reset();
		}

		TArray<int32> Markers;
		TArray<uint64> Bits;
		TArray<int32> IntegerValues;
	};

	struct FReferenceState
	{
		void Reset()
		{
			NextObjectId = 1;
			Created = 0;
			Destroyed = 0;
			Live = 0;
			AddRefCalls = 0;
			ReleaseCalls = 0;
			CreatedIds.Reset();
			DestroyedIds.Reset();
			CreationOrder.Reset();
			DestructionOrder.Reset();
			CreatedValues.Reset();
			DestroyedValues.Reset();
			Markers.Reset();
			Identities.Reset();
		}

		int32 NextObjectId = 1;
		int32 Created = 0;
		int32 Destroyed = 0;
		int32 Live = 0;
		int32 AddRefCalls = 0;
		int32 ReleaseCalls = 0;
		TSet<int32> CreatedIds;
		TSet<int32> DestroyedIds;
		TArray<int32> CreationOrder;
		TArray<int32> DestructionOrder;
		TArray<int32> CreatedValues;
		TArray<int32> DestroyedValues;
		TArray<int32> Markers;
		TArray<int32> Identities;
	};

	struct FOverloadState
	{
		void Reset()
		{
			NextObjectId = 1;
			CreatedIds.Reset();
			DestroyedIds.Reset();
			CreationOrder.Reset();
			DestructionOrder.Reset();
			CreatedValues.Reset();
			DestroyedValues.Reset();
			Markers.Reset();
			LeftValues.Reset();
			RightValues.Reset();
		}

		int32 NextObjectId = 1;
		TSet<int32> CreatedIds;
		TSet<int32> DestroyedIds;
		TArray<int32> CreationOrder;
		TArray<int32> DestructionOrder;
		TArray<int32> CreatedValues;
		TArray<int32> DestroyedValues;
		TArray<int32> Markers;
		TArray<int32> LeftValues;
		TArray<int32> RightValues;
	};

	class FComparisonReference
	{
	public:
		FComparisonReference(FReferenceState& InState, const EReferenceKind InKind)
			: State(InState), Kind(InKind), ObjectId(State.NextObjectId++)
		{
			++State.Created;
			++State.Live;
			State.CreatedIds.Add(ObjectId);
		}

		virtual ~FComparisonReference()
		{
			++State.Destroyed;
			--State.Live;
			State.DestroyedIds.Add(ObjectId);
		}

		void AddRef()
		{
			++ReferenceCount;
			++State.AddRefCalls;
		}

		void Release()
		{
			++State.ReleaseCalls;
			--ReferenceCount;
			if (ReferenceCount == 0)
			{
				delete this;
			}
		}

		int32 GetIdentity() const
		{
			return ObjectId;
		}

		int32 GetKind() const
		{
			return static_cast<int32>(Kind);
		}

	private:
		FReferenceState& State;
		EReferenceKind Kind;
		int32 ObjectId = INDEX_NONE;
		int32 ReferenceCount = 1;
	};

	class FComparisonDerived final : public FComparisonReference
	{
	public:
		explicit FComparisonDerived(FReferenceState& State)
			: FComparisonReference(State, EReferenceKind::Derived)
		{
		}
	};

	class FComparisonSibling final : public FComparisonReference
	{
	public:
		explicit FComparisonSibling(FReferenceState& State)
			: FComparisonReference(State, EReferenceKind::Sibling)
		{
		}
	};

	template <typename ValueType> struct TValuePair
	{
		ValueType Left;
		ValueType Right;
	};

	inline static constexpr FOperatorCase OperatorCases[] = {
		{"less", TEXT("<"), EComparisonOperator::Less},
		{"less_equal", TEXT("<="), EComparisonOperator::LessEqual},
		{"greater", TEXT(">"), EComparisonOperator::Greater},
		{"greater_equal", TEXT(">="), EComparisonOperator::GreaterEqual},
		{"equal", TEXT("=="), EComparisonOperator::Equal},
		{"not_equal", TEXT("!="), EComparisonOperator::NotEqual},
	};

	inline static constexpr FFloatTypeCase FloatTypeCases[] = {
		{"float32", EFloatType::Float32},
		{"float64", EFloatType::Float64},
	};

	inline static constexpr FFloatValueCase FloatValueCases[] = {
		{"negative_zero", EFloatValue::NegativeZero},
		{"positive_zero", EFloatValue::PositiveZero},
		{"nan", EFloatValue::NaN},
		{"positive_infinity", EFloatValue::PositiveInfinity},
		{"negative_infinity", EFloatValue::NegativeInfinity},
		{"minimum", EFloatValue::Minimum},
		{"maximum", EFloatValue::Maximum},
		{"equal_pair", EFloatValue::EqualPair},
	};

	inline static constexpr FOrderCase OrderCases[] = {
		{"left_right", "CompareLeftRight", EComparisonOrder::LeftRight},
		{"right_left", "CompareRightLeft", EComparisonOrder::RightLeft},
	};

	inline static constexpr FIntegralFamilyCase IntegralFamilyCases[] = {
		{"enum", EIntegralFamily::Enum},
		{"alias", EIntegralFamily::Alias},
	};

	inline static constexpr FIntegralPairCase IntegralPairCases[] = {
		{"equal_zero", EIntegralPair::EqualZero},
		{"equal_named", EIntegralPair::EqualNamed},
		{"below_adjacent", EIntegralPair::BelowAdjacent},
		{"above_adjacent", EIntegralPair::AboveAdjacent},
		{"minimum_boundary", EIntegralPair::MinimumBoundary},
		{"maximum_boundary", EIntegralPair::MaximumBoundary},
	};

	inline static constexpr FReferenceRelationCase ReferenceRelationCases[] = {
		{"same_non_null", EReferenceRelation::SameNonNull, 1, true},
		{"different_non_null", EReferenceRelation::DifferentNonNull, 2, false},
		{"left_null", EReferenceRelation::LeftNull, 1, false},
		{"right_null", EReferenceRelation::RightNull, 1, false},
		{"both_null", EReferenceRelation::BothNull, 0, true},
		{"derived_base_same", EReferenceRelation::DerivedBaseSame, 1, true},
		{"sibling_different", EReferenceRelation::SiblingDifferent, 2, false},
		{"const_same", EReferenceRelation::ConstSame, 1, true},
	};

	inline static constexpr FOverloadRelationCase OverloadRelationCases[] = {
		{"less", EOverloadRelation::Less, 1, 2},
		{"equal", EOverloadRelation::Equal, 2, 2},
		{"greater", EOverloadRelation::Greater, 3, 2},
		{"unequal", EOverloadRelation::Unequal, -4, 5},
	};

	inline static constexpr FReceiverCase ReceiverCases[] = {
		{"mutable", EReceiverConstness::Mutable},
		{"const", EReceiverConstness::Const},
	};

	static FComparisonTrace* ActiveTrace()
	{
		asIScriptContext* const Context = asGetActiveContext();
		return Context != nullptr && Context->GetEngine() != nullptr
				   ? static_cast<FComparisonTrace*>(
						 Context->GetEngine()->GetUserData(ComparisonTraceSlot))
				   : nullptr;
	}

	template <typename FloatType> static uint64 FloatingBits(const FloatType Value)
	{
		static_assert(std::is_same_v<FloatType, float> || std::is_same_v<FloatType, double>);
		if constexpr (std::is_same_v<FloatType, float>)
		{
			uint32 Bits = 0;
			FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
			return Bits;
		}
		else
		{
			uint64 Bits = 0;
			FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
			return Bits;
		}
	}

	static float TraceFloat32Operand(const int32 Marker, const float Value)
	{
		if (FComparisonTrace* const Trace = ActiveTrace())
		{
			Trace->Markers.Add(Marker);
			Trace->Bits.Add(FloatingBits(Value));
		}
		return Value;
	}

	static double TraceFloat64Operand(const int32 Marker, const double Value)
	{
		if (FComparisonTrace* const Trace = ActiveTrace())
		{
			Trace->Markers.Add(Marker);
			Trace->Bits.Add(FloatingBits(Value));
		}
		return Value;
	}

	static void RecordComparisonIntegerOperand(const int32 Marker, const int32 Value)
	{
		if (FComparisonTrace* const Trace = ActiveTrace())
		{
			Trace->Markers.Add(Marker);
			Trace->IntegerValues.Add(Value);
		}
	}

	static FString FloatTypeDeclaration(
		const FFloatTypeCase& TypeCase, const asIScriptEngine& Engine)
	{
		if (TypeCase.Type == EFloatType::Float32)
		{
			return Engine.GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0 ? TEXT("float32")
																		: TEXT("float");
		}
		return Engine.GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0 ? TEXT("float")
																									: TEXT("float64");
	}

	static int PublicFloatTypeId(asIScriptEngine& Engine, const FFloatTypeCase& TypeCase)
	{
		return Engine.GetTypeIdByDecl(TypeCase.Type == EFloatType::Float32 ? "float32" : "float64");
	}

	static bool RegisterTraceFunctions(asIScriptEngine& Engine, FComparisonTrace& Trace)
	{
		Engine.SetUserData(&Trace, ComparisonTraceSlot);
		const FString Float32Declaration =
			FString::Printf(TEXT("%s TraceFloat32Operand(int Marker, %s Value)"),
				*FloatTypeDeclaration(FloatTypeCases[0], Engine),
				*FloatTypeDeclaration(FloatTypeCases[0], Engine));
		const FString Float64Declaration =
			TEXT("float64 TraceFloat64Operand(int Marker, float64 Value)");
		const FTCHARToUTF8 Float32Utf8(*Float32Declaration);
		const FTCHARToUTF8 Float64Utf8(*Float64Declaration);
		const ASAutoCaller::FunctionCaller Float32Caller =
			ASAutoCaller::MakeFunctionCaller(TraceFloat32Operand);
		const ASAutoCaller::FunctionCaller Float64Caller =
			ASAutoCaller::MakeFunctionCaller(TraceFloat64Operand);
		const ASAutoCaller::FunctionCaller IntegerCaller =
			ASAutoCaller::MakeFunctionCaller(RecordComparisonIntegerOperand);
		const int Float32Result = Engine.RegisterGlobalFunction(Float32Utf8.Get(),
				   asFUNCTION(TraceFloat32Operand),
				   asCALL_CDECL,
				   *(asFunctionCaller*)&Float32Caller);
		const int Float64Result = Engine.RegisterGlobalFunction(Float64Utf8.Get(),
				   asFUNCTION(TraceFloat64Operand),
				   asCALL_CDECL,
				   *(asFunctionCaller*)&Float64Caller);
		const int IntegerResult = Engine.RegisterGlobalFunction(
				   "void RecordComparisonIntegerOperand(int Marker, int Value)",
				   asFUNCTION(RecordComparisonIntegerOperand),
				   asCALL_CDECL,
				   *(asFunctionCaller*)&IntegerCaller);
		return Float32Result >= 0 && Float64Result >= 0 && IntegerResult >= 0;
	}

	static FReferenceState* GetReferenceState(asIScriptGeneric& Generic)
	{
		return Generic.GetEngine() != nullptr
				   ? static_cast<FReferenceState*>(
						 Generic.GetEngine()->GetUserData(ReferenceStateSlot))
				   : nullptr;
	}

	static void GenericReferenceAddRef(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			if (FComparisonReference* const Object =
					static_cast<FComparisonReference*>(Generic->GetObject()))
			{
				Object->AddRef();
			}
		}
	}

	static void GenericReferenceRelease(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			if (FComparisonReference* const Object =
					static_cast<FComparisonReference*>(Generic->GetObject()))
			{
				Object->Release();
			}
		}
	}

	static void GenericReferenceIdentity(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		const FComparisonReference* const Object =
			static_cast<const FComparisonReference*>(Generic->GetObject());
		Generic->SetReturnDWord(
			Object != nullptr ? static_cast<asDWORD>(Object->GetIdentity()) : 0);
	}

	static void GenericReferenceKind(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		const FComparisonReference* const Object =
			static_cast<const FComparisonReference*>(Generic->GetObject());
		Generic->SetReturnDWord(Object != nullptr ? static_cast<asDWORD>(Object->GetKind()) : 0);
	}

	static void ReturnReference(asIScriptGeneric& Generic, FComparisonReference* Object)
	{
		Generic.SetReturnAddress(Object);
	}

	static void GenericMakeComparisonRoot(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		FReferenceState* const State = GetReferenceState(*Generic);
		ReturnReference(*Generic,
			State != nullptr ? new FComparisonReference(*State, EReferenceKind::Root) : nullptr);
	}

	static void GenericMakeComparisonDerived(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		FReferenceState* const State = GetReferenceState(*Generic);
		ReturnReference(*Generic, State != nullptr ? new FComparisonDerived(*State) : nullptr);
	}

	static void GenericMakeComparisonSiblingAsRoot(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		FReferenceState* const State = GetReferenceState(*Generic);
		ReturnReference(*Generic, State != nullptr ? new FComparisonSibling(*State) : nullptr);
	}

	static void GenericComparisonToRoot(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		FComparisonReference* const Object =
			static_cast<FComparisonReference*>(Generic->GetObject());
		if (Object != nullptr)
		{
			Object->AddRef();
		}
		Generic->SetReturnAddress(Object);
	}

	static void RecordReferenceOperand(const int32 Marker, const int32 Identity)
	{
		asIScriptContext* const Context = asGetActiveContext();
		FReferenceState* const State =
			Context != nullptr && Context->GetEngine() != nullptr
				? static_cast<FReferenceState*>(
					  Context->GetEngine()->GetUserData(ReferenceStateSlot))
				: nullptr;
		if (State != nullptr)
		{
			State->Markers.Add(Marker);
			State->Identities.Add(Identity);
		}
	}

	static bool RegisterReferenceType(asIScriptEngine& Engine, const ANSICHAR* TypeName)
	{
		return Engine.RegisterObjectType(TypeName, 0, asOBJ_REF | asOBJ_IMPLICIT_HANDLE) >= 0 &&
			   Engine.RegisterObjectBehaviour(TypeName,
				   asBEHAVE_ADDREF,
				   "void f()",
				   asFUNCTION(GenericReferenceAddRef),
				   asCALL_GENERIC) >= 0 &&
			   Engine.RegisterObjectBehaviour(TypeName,
				   asBEHAVE_RELEASE,
				   "void f()",
				   asFUNCTION(GenericReferenceRelease),
				   asCALL_GENERIC) >= 0 &&
			   Engine.RegisterObjectMethod(TypeName,
				   "int GetIdentity() const",
				   asFUNCTION(GenericReferenceIdentity),
				   asCALL_GENERIC) >= 0 &&
			   Engine.RegisterObjectMethod(TypeName,
				   "int GetKind() const",
				   asFUNCTION(GenericReferenceKind),
				   asCALL_GENERIC) >= 0;
	}

	static bool RegisterReferenceFixtures(asIScriptEngine& Engine, FReferenceState& State)
	{
		Engine.SetUserData(&State, ReferenceStateSlot);
		const ASAutoCaller::FunctionCaller RecordCaller =
			ASAutoCaller::MakeFunctionCaller(RecordReferenceOperand);
		const bool bRoot = RegisterReferenceType(Engine, "FComparisonReferenceRoot");
		const bool bDerived = RegisterReferenceType(Engine, "FComparisonReferenceDerived");
		const bool bSibling = RegisterReferenceType(Engine, "FComparisonReferenceSibling");
		const int DerivedCast = Engine.RegisterObjectMethod("FComparisonReferenceDerived",
				   "FComparisonReferenceRoot opImplCast()",
				   asFUNCTION(GenericComparisonToRoot),
				   asCALL_GENERIC);
		const int SiblingCast = Engine.RegisterObjectMethod("FComparisonReferenceSibling",
				   "FComparisonReferenceRoot opImplCast()",
				   asFUNCTION(GenericComparisonToRoot),
				   asCALL_GENERIC);
		const int MakeRoot = Engine.RegisterGlobalFunction("FComparisonReferenceRoot MakeComparisonRoot()",
				   asFUNCTION(GenericMakeComparisonRoot),
				   asCALL_GENERIC);
		const int MakeDerived = Engine.RegisterGlobalFunction("FComparisonReferenceDerived MakeComparisonDerived()",
				   asFUNCTION(GenericMakeComparisonDerived),
				   asCALL_GENERIC);
		const int MakeDerivedRoot = Engine.RegisterGlobalFunction(
				   "FComparisonReferenceRoot MakeComparisonDerivedAsRoot()",
				   asFUNCTION(GenericMakeComparisonDerived),
				   asCALL_GENERIC);
		const int MakeSiblingRoot = Engine.RegisterGlobalFunction(
				   "FComparisonReferenceRoot MakeComparisonSiblingAsRoot()",
				   asFUNCTION(GenericMakeComparisonSiblingAsRoot),
				   asCALL_GENERIC);
		const int Record = Engine.RegisterGlobalFunction(
				   "void RecordReferenceOperand(int Marker, int Identity)",
				   asFUNCTION(RecordReferenceOperand),
				   asCALL_CDECL,
				   *(asFunctionCaller*)&RecordCaller);
		return bRoot && bDerived && bSibling && DerivedCast >= 0 && SiblingCast >= 0 &&
			   MakeRoot >= 0 && MakeDerived >= 0 && MakeDerivedRoot >= 0 &&
			   MakeSiblingRoot >= 0 && Record >= 0;
	}

	static FOverloadState* ActiveOverloadState()
	{
		asIScriptContext* const Context = asGetActiveContext();
		return Context != nullptr && Context->GetEngine() != nullptr
				   ? static_cast<FOverloadState*>(
						 Context->GetEngine()->GetUserData(OverloadStateSlot))
				   : nullptr;
	}

	static int32 BeginOverloadedComparisonValue(const int32 Value)
	{
		FOverloadState* const State = ActiveOverloadState();
		if (State == nullptr)
		{
			return INDEX_NONE;
		}
		const int32 ObjectId = State->NextObjectId++;
		State->CreatedIds.Add(ObjectId);
		State->CreationOrder.Add(ObjectId);
		State->CreatedValues.Add(Value);
		return ObjectId;
	}

	static void EndOverloadedComparisonValue(const int32 ObjectId, const int32 Value)
	{
		if (FOverloadState* const State = ActiveOverloadState())
		{
			State->DestroyedIds.Add(ObjectId);
			State->DestructionOrder.Add(ObjectId);
			State->DestroyedValues.Add(Value);
		}
	}

	static void RecordComparisonOverload(const int32 Marker, const int32 Left, const int32 Right)
	{
		if (FOverloadState* const State = ActiveOverloadState())
		{
			State->Markers.Add(Marker);
			State->LeftValues.Add(Left);
			State->RightValues.Add(Right);
		}
	}

	static bool RegisterOverloadFixtures(asIScriptEngine& Engine, FOverloadState& State)
	{
		Engine.SetUserData(&State, OverloadStateSlot);
		const ASAutoCaller::FunctionCaller BeginCaller =
			ASAutoCaller::MakeFunctionCaller(BeginOverloadedComparisonValue);
		const ASAutoCaller::FunctionCaller EndCaller =
			ASAutoCaller::MakeFunctionCaller(EndOverloadedComparisonValue);
		const ASAutoCaller::FunctionCaller RecordCaller =
			ASAutoCaller::MakeFunctionCaller(RecordComparisonOverload);
		return Engine.RegisterGlobalFunction("int BeginOverloadedComparisonValue(int Value)",
				   asFUNCTION(BeginOverloadedComparisonValue),
				   asCALL_CDECL,
				   *(asFunctionCaller*)&BeginCaller) >= 0 &&
			   Engine.RegisterGlobalFunction(
				   "void EndOverloadedComparisonValue(int ObjectId, int Value)",
				   asFUNCTION(EndOverloadedComparisonValue),
				   asCALL_CDECL,
				   *(asFunctionCaller*)&EndCaller) >= 0 &&
			   Engine.RegisterGlobalFunction(
				   "void RecordComparisonOverload(int Marker, int Left, int Right)",
				   asFUNCTION(RecordComparisonOverload),
				   asCALL_CDECL,
				   *(asFunctionCaller*)&RecordCaller) >= 0;
	}

	static FString TraceFunctionName(const FFloatTypeCase& TypeCase)
	{
		return TypeCase.Type == EFloatType::Float32 ? TEXT("TraceFloat32Operand")
													: TEXT("TraceFloat64Operand");
	}

	static FString BuildFloatComparisonSource(
		asIScriptEngine& Engine, const FFloatTypeCase& TypeCase, const FOperatorCase& OperatorCase)
	{
		using namespace AngelscriptNativeTestSupport;
		const FString Type = FloatTypeDeclaration(TypeCase, Engine);
		const FString TraceName = TraceFunctionName(TypeCase);
		FString Source;
		AppendGeneratedAsLine(Source,
			FString::Printf(TEXT("bool CompareLeftRight(%s Left, %s Right)"), *Type, *Type));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source,
			FString::Printf(TEXT("\treturn %s(1, Left) %s %s(2, Right);"),
				*TraceName,
				OperatorCase.Token,
				*TraceName));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source,
			FString::Printf(TEXT("bool CompareRightLeft(%s Left, %s Right)"), *Type, *Type));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source,
			FString::Printf(TEXT("\treturn %s(2, Right) %s %s(1, Left);"),
				*TraceName,
				OperatorCase.Token,
				*TraceName));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static FString IntegralTypeDeclaration(const FIntegralFamilyCase& FamilyCase)
	{
		return FamilyCase.Family == EIntegralFamily::Enum ? TEXT("EComparisonEnum")
														  : TEXT("ComparisonAlias");
	}

	static FString IntegralTraceFunctionName(const FIntegralFamilyCase& FamilyCase)
	{
		return FamilyCase.Family == EIntegralFamily::Enum ? TEXT("TraceEnumOperand")
														  : TEXT("TraceAliasOperand");
	}

	static FString BuildIntegralComparisonSource(
		const FIntegralFamilyCase& FamilyCase, const FOperatorCase& OperatorCase)
	{
		using namespace AngelscriptNativeTestSupport;
		FString Source;
		if (FamilyCase.Family == EIntegralFamily::Enum)
		{
			AppendGeneratedAsLine(Source, TEXT("enum EComparisonEnum"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tMinimum = -128,"));
			AppendGeneratedAsLine(Source, TEXT("\tMinusOne = -1,"));
			AppendGeneratedAsLine(Source, TEXT("\tZero = 0,"));
			AppendGeneratedAsLine(Source, TEXT("\tOne = 1,"));
			AppendGeneratedAsLine(Source, TEXT("\tMaximum = 127"));
			AppendGeneratedAsLine(Source, TEXT("}"));
		}
		else
		{
			AppendGeneratedAsLine(
				Source, TEXT("// ComparisonAlias is registered through the raw SDK before build."));
		}
		AppendGeneratedAsLine(Source);

		const FString Type = IntegralTypeDeclaration(FamilyCase);
		const FString TraceName = IntegralTraceFunctionName(FamilyCase);
		AppendGeneratedAsLine(
			Source, FString::Printf(TEXT("%s %s(int Marker, %s Value)"), *Type, *TraceName, *Type));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source, TEXT("\tRecordComparisonIntegerOperand(Marker, int(Value));"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source,
			FString::Printf(TEXT("bool CompareLeftRight(%s Left, %s Right)"), *Type, *Type));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source,
			FString::Printf(TEXT("\treturn %s(1, Left) %s %s(2, Right);"),
				*TraceName,
				OperatorCase.Token,
				*TraceName));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source,
			FString::Printf(TEXT("bool CompareRightLeft(%s Left, %s Right)"), *Type, *Type));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source,
			FString::Printf(TEXT("\treturn %s(2, Right) %s %s(1, Left);"),
				*TraceName,
				OperatorCase.Token,
				*TraceName));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static void AppendReferenceTraceHelpers(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;
		AppendGeneratedAsLine(Source,
			TEXT("FComparisonReferenceRoot TraceReferenceOperand("
				 "int Marker, FComparisonReferenceRoot Value)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source,
			TEXT("\tRecordReferenceOperand("
				 "Marker, Value == nullptr ? 0 : Value.GetIdentity());"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source,
			TEXT("const FComparisonReferenceRoot TraceConstReferenceOperand("
				 "int Marker, const FComparisonReferenceRoot Value)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source,
			TEXT("\tRecordReferenceOperand("
				 "Marker, Value == nullptr ? 0 : Value.GetIdentity());"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendReferenceSetup(FString& Source, const FReferenceRelationCase& RelationCase)
	{
		using namespace AngelscriptNativeTestSupport;
		switch (RelationCase.Relation)
		{
		case EReferenceRelation::SameNonNull:
			AppendGeneratedAsLine(
				Source, TEXT("\tFComparisonReferenceRoot Left = MakeComparisonRoot();"));
			AppendGeneratedAsLine(Source, TEXT("\tFComparisonReferenceRoot Right = Left;"));
			break;
		case EReferenceRelation::DifferentNonNull:
			AppendGeneratedAsLine(
				Source, TEXT("\tFComparisonReferenceRoot Left = MakeComparisonRoot();"));
			AppendGeneratedAsLine(
				Source, TEXT("\tFComparisonReferenceRoot Right = MakeComparisonRoot();"));
			break;
		case EReferenceRelation::LeftNull:
			AppendGeneratedAsLine(Source, TEXT("\tFComparisonReferenceRoot Left = nullptr;"));
			AppendGeneratedAsLine(
				Source, TEXT("\tFComparisonReferenceRoot Right = MakeComparisonRoot();"));
			break;
		case EReferenceRelation::RightNull:
			AppendGeneratedAsLine(
				Source, TEXT("\tFComparisonReferenceRoot Left = MakeComparisonRoot();"));
			AppendGeneratedAsLine(Source, TEXT("\tFComparisonReferenceRoot Right = nullptr;"));
			break;
		case EReferenceRelation::BothNull:
			AppendGeneratedAsLine(Source, TEXT("\tFComparisonReferenceRoot Left = nullptr;"));
			AppendGeneratedAsLine(Source, TEXT("\tFComparisonReferenceRoot Right = nullptr;"));
			break;
		case EReferenceRelation::DerivedBaseSame:
			// The current fork's implicit-handle opImplCast path is exercised by the
			// dedicated conversion suite. Keep this comparison identity case on the
			// root-typed factory path so a cast implementation defect cannot corrupt
			// the reference lifetime checks in this product.
			AppendGeneratedAsLine(
				Source, TEXT("\tFComparisonReferenceRoot Left = MakeComparisonDerivedAsRoot();"));
			AppendGeneratedAsLine(Source, TEXT("\tFComparisonReferenceRoot Right = Left;"));
			break;
		case EReferenceRelation::SiblingDifferent:
			AppendGeneratedAsLine(
				Source, TEXT("\tFComparisonReferenceRoot Left = MakeComparisonDerivedAsRoot();"));
			AppendGeneratedAsLine(
				Source, TEXT("\tFComparisonReferenceRoot Right = MakeComparisonSiblingAsRoot();"));
			break;
		case EReferenceRelation::ConstSame:
			AppendGeneratedAsLine(
				Source, TEXT("\tFComparisonReferenceRoot Mutable = MakeComparisonRoot();"));
			AppendGeneratedAsLine(Source, TEXT("\tconst FComparisonReferenceRoot Left = Mutable;"));
			AppendGeneratedAsLine(
				Source, TEXT("\tconst FComparisonReferenceRoot Right = Mutable;"));
			break;
		}
	}

	static FString BuildReferenceComparisonSource(const FReferenceRelationCase& RelationCase,
		const FOperatorCase& OperatorCase,
		const FOrderCase& OrderCase)
	{
		using namespace AngelscriptNativeTestSupport;
		FString Source;
		AppendReferenceTraceHelpers(Source);
		AppendGeneratedAsLine(Source, TEXT("bool RunReferenceComparison()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendReferenceSetup(Source, RelationCase);
		const TCHAR* TraceName = RelationCase.Relation == EReferenceRelation::ConstSame
									 ? TEXT("TraceConstReferenceOperand")
									 : TEXT("TraceReferenceOperand");
		if (OrderCase.Order == EComparisonOrder::LeftRight)
		{
			AppendGeneratedAsLine(Source,
				FString::Printf(TEXT("\treturn %s(1, Left) %s %s(2, Right); "
									 "// REFERENCE_COMPARE_CAUSE"),
					TraceName,
					OperatorCase.Token,
					TraceName));
		}
		else
		{
			AppendGeneratedAsLine(Source,
				FString::Printf(TEXT("\treturn %s(2, Right) %s %s(1, Left); "
									 "// REFERENCE_COMPARE_CAUSE"),
					TraceName,
					OperatorCase.Token,
					TraceName));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static FString BuildReferenceRecoverySource()
	{
		using namespace AngelscriptNativeTestSupport;
		FString Source;
		AppendGeneratedAsLine(Source, TEXT("bool RunReferenceComparison()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(
			Source, TEXT("\tFComparisonReferenceRoot Value = MakeComparisonRoot();"));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value == Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static bool IsReferenceOrdering(const FOperatorCase& OperatorCase)
	{
		return OperatorCase.Operator == EComparisonOperator::Less ||
			   OperatorCase.Operator == EComparisonOperator::LessEqual ||
			   OperatorCase.Operator == EComparisonOperator::Greater ||
			   OperatorCase.Operator == EComparisonOperator::GreaterEqual;
	}

	static void AppendOverloadedComparisonType(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;
		AppendGeneratedAsLine(Source, TEXT("struct FOverloadedComparisonValue"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value;"));
		AppendGeneratedAsLine(Source, TEXT("\tint ObjectId;"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tFOverloadedComparisonValue(int InValue)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tValue = InValue;"));
		AppendGeneratedAsLine(
			Source, TEXT("\t\tObjectId = BeginOverloadedComparisonValue(Value);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\t~FOverloadedComparisonValue()"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\tEndOverloadedComparisonValue(ObjectId, Value);"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("\tint opCmp(const FOverloadedComparisonValue& Other)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source, TEXT("\t\tRecordComparisonOverload(101, Value, Other.Value);"));
		AppendGeneratedAsLine(Source, TEXT("\t\tif (Value < Other.Value)"));
		AppendGeneratedAsLine(Source, TEXT("\t\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\t\treturn -1;"));
		AppendGeneratedAsLine(Source, TEXT("\t\t}"));
		AppendGeneratedAsLine(Source, TEXT("\t\tif (Value > Other.Value)"));
		AppendGeneratedAsLine(Source, TEXT("\t\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\t\treturn 1;"));
		AppendGeneratedAsLine(Source, TEXT("\t\t}"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn 0;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source, TEXT("\tint opCmp(const FOverloadedComparisonValue& Other) const"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source, TEXT("\t\tRecordComparisonOverload(102, Value, Other.Value);"));
		AppendGeneratedAsLine(Source, TEXT("\t\tif (Value < Other.Value)"));
		AppendGeneratedAsLine(Source, TEXT("\t\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\t\treturn -1;"));
		AppendGeneratedAsLine(Source, TEXT("\t\t}"));
		AppendGeneratedAsLine(Source, TEXT("\t\tif (Value > Other.Value)"));
		AppendGeneratedAsLine(Source, TEXT("\t\t{"));
		AppendGeneratedAsLine(Source, TEXT("\t\t\treturn 1;"));
		AppendGeneratedAsLine(Source, TEXT("\t\t}"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn 0;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source, TEXT("\tbool opEquals(const FOverloadedComparisonValue& Other)"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source, TEXT("\t\tRecordComparisonOverload(201, Value, Other.Value);"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn Value == Other.Value;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(
			Source, TEXT("\tbool opEquals(const FOverloadedComparisonValue& Other) const"));
		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(
			Source, TEXT("\t\tRecordComparisonOverload(202, Value, Other.Value);"));
		AppendGeneratedAsLine(Source, TEXT("\t\treturn Value == Other.Value;"));
		AppendGeneratedAsLine(Source, TEXT("\t}"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static FString BuildOverloadedComparisonSource(const FOperatorCase& OperatorCase,
		const FOverloadRelationCase& RelationCase,
		const FOrderCase& OrderCase,
		const FReceiverCase& ReceiverCase)
	{
		using namespace AngelscriptNativeTestSupport;
		FString Source;
		AppendOverloadedComparisonType(Source);
		AppendGeneratedAsLine(Source, TEXT("bool RunOverloadedComparison()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		const bool bLeftConst = ReceiverCase.Constness == EReceiverConstness::Const &&
								OrderCase.Order == EComparisonOrder::LeftRight;
		const bool bRightConst = ReceiverCase.Constness == EReceiverConstness::Const &&
								 OrderCase.Order == EComparisonOrder::RightLeft;
		AppendGeneratedAsLine(Source,
			FString::Printf(TEXT("\t%sFOverloadedComparisonValue Left(%d);"),
				bLeftConst ? TEXT("const ") : TEXT(""),
				RelationCase.Left));
		AppendGeneratedAsLine(Source,
			FString::Printf(TEXT("\t%sFOverloadedComparisonValue Right(%d);"),
				bRightConst ? TEXT("const ") : TEXT(""),
				RelationCase.Right));
		if (OrderCase.Order == EComparisonOrder::LeftRight)
		{
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("\treturn Left %s Right;"), OperatorCase.Token));
		}
		else
		{
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("\treturn Right %s Left;"), OperatorCase.Token));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static bool UsesEqualsOverload(const FOperatorCase& OperatorCase)
	{
		return OperatorCase.Operator == EComparisonOperator::Equal ||
			   OperatorCase.Operator == EComparisonOperator::NotEqual;
	}

	static int32 ExpectedOverloadMarker(
		const FOperatorCase& OperatorCase, const FReceiverCase& ReceiverCase)
	{
		const int32 Base = UsesEqualsOverload(OperatorCase) ? 200 : 100;
		return Base + (ReceiverCase.Constness == EReceiverConstness::Const ? 2 : 1);
	}

	static bool HasNoErrors(const AngelscriptNativeTestSupport::FNativeMessageCollector& Messages)
	{
		for (const AngelscriptNativeTestSupport::FNativeMessageEntry& Entry : Messages.Entries)
		{
			if (Entry.Type == asMSGTYPE_ERROR)
			{
				return false;
			}
		}
		return true;
	}

	static asIScriptModule* CompileReportedSource(FNativeTestEngine& Engine,
		FAutomationTestBase& Test,
		const FString& SourceId,
		const FString& ModuleName,
		const FString& Source,
		int32& OutBuildResult)
	{
		using namespace AngelscriptNativeTestSupport;
		PrintGeneratedAsSource(Test, SourceId, ModuleName, Source);
		Engine.Reset(Test);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		asIScriptModule* Module = nullptr;
		OutBuildResult =
			CompileNativeModule(Engine.Get(), ModuleNameUtf8.Get(), SourceUtf8.Get(), Module);
		return Module;
	}

	static asIScriptModule* CompileAndReport(FNativeTestEngine& Engine,
		FAutomationTestBase& Test,
		const FString& SourceId,
		const FString& ModuleName,
		const FString& Source)
	{
		int32 BuildResult = asERROR;
		asIScriptModule* const Module =
			CompileReportedSource(Engine, Test, SourceId, ModuleName, Source, BuildResult);
		FNoDiscardAsserter Assert(Test);
		const FString Description =
			FString::Printf(TEXT("[%s] comparison source should compile. Build=%d Messages={%s}"),
				*SourceId,
				BuildResult,
				*Engine.GetMessagesText());
		if (!Assert.IsTrue(BuildResult >= 0, *Description) ||
			!Assert.IsNotNull(Module, *Description) ||
			!Assert.IsTrue(HasNoErrors(Engine.GetMessages()), *Description))
		{
			return nullptr;
		}
		return Module;
	}

	static asIScriptFunction* FindComparisonFunction(
		asIScriptModule& Module, const FOrderCase& OrderCase, const int ExpectedTypeId)
	{
		for (asUINT FunctionIndex = 0; FunctionIndex < Module.GetFunctionCount(); ++FunctionIndex)
		{
			asIScriptFunction* const Function = Module.GetFunctionByIndex(FunctionIndex);
			if (Function == nullptr ||
				FCStringAnsi::Strcmp(Function->GetName(), OrderCase.FunctionName) != 0 ||
				Function->GetParamCount() != 2)
			{
				continue;
			}
			int LeftTypeId = asINVALID_TYPE;
			int RightTypeId = asINVALID_TYPE;
			if (Function->GetParam(0, &LeftTypeId) >= 0 &&
				Function->GetParam(1, &RightTypeId) >= 0 && LeftTypeId == ExpectedTypeId &&
				RightTypeId == ExpectedTypeId)
			{
				return Function;
			}
		}
		return nullptr;
	}

	static bool VerifyComparisonMetadata(FAutomationTestBase& Test,
		asIScriptEngine& Engine,
		asIScriptFunction& Function,
		const int ExpectedTypeId,
		const FNativeCaseContext& Case)
	{
		FNoDiscardAsserter Assert(Test);
		int LeftTypeId = asINVALID_TYPE;
		int RightTypeId = asINVALID_TYPE;
		asDWORD LeftFlags = asTM_NONE;
		asDWORD RightFlags = asTM_NONE;
		const char* LeftName = nullptr;
		const char* RightName = nullptr;
		if (!Assert.AreEqual(asSUCCESS,
				Function.GetParam(0, &LeftTypeId, &LeftFlags, &LeftName),
				*Case.Describe(TEXT("comparison left parameter metadata should be readable"))) ||
			!Assert.AreEqual(asSUCCESS,
				Function.GetParam(1, &RightTypeId, &RightFlags, &RightName),
				*Case.Describe(TEXT("comparison right parameter metadata should be readable"))))
		{
			return false;
		}
		return Assert.AreEqual(ExpectedTypeId,
				   LeftTypeId,
				   *Case.Describe(TEXT("comparison left parameter should retain exact type"))) &&
			   Assert.AreEqual(ExpectedTypeId,
				   RightTypeId,
				   *Case.Describe(TEXT("comparison right parameter should retain exact type"))) &&
			   Assert.AreEqual(static_cast<asDWORD>(asTM_CONST),
				   LeftFlags,
				   *Case.Describe(TEXT("comparison left value parameter should retain the fork read-only flag"))) &&
			   Assert.AreEqual(static_cast<asDWORD>(asTM_CONST),
				   RightFlags,
				   *Case.Describe(TEXT("comparison right value parameter should retain the fork read-only flag"))) &&
			   Assert.AreEqual(FString(TEXT("Left")),
				   FString(UTF8_TO_TCHAR(LeftName != nullptr ? LeftName : "")),
				   *Case.Describe(TEXT("comparison left parameter should retain its name"))) &&
			   Assert.AreEqual(FString(TEXT("Right")),
				   FString(UTF8_TO_TCHAR(RightName != nullptr ? RightName : "")),
				   *Case.Describe(TEXT("comparison right parameter should retain its name"))) &&
			   Assert.AreEqual(Engine.GetTypeIdByDecl("bool"),
				   Function.GetReturnTypeId(),
				   *Case.Describe(TEXT("comparison result should retain bool type")));
	}

	template <typename ValueType>
	static bool ExpectedComparison(
		const ValueType Left, const ValueType Right, const FOperatorCase& OperatorCase)
	{
		if constexpr (std::is_floating_point_v<ValueType>)
		{
			if (std::isnan(Left) || std::isnan(Right))
			{
				// The fork's CMPf/CMPd convention maps an unordered comparison to
				// the positive result (equal=false, less=false, greater=true).
				switch (OperatorCase.Operator)
				{
				case EComparisonOperator::Greater:
				case EComparisonOperator::GreaterEqual:
				case EComparisonOperator::NotEqual:
					return true;
				default:
					return false;
				}
			}
		}
		switch (OperatorCase.Operator)
		{
		case EComparisonOperator::Less:
			return Left < Right;
		case EComparisonOperator::LessEqual:
			return Left <= Right;
		case EComparisonOperator::Greater:
			return Left > Right;
		case EComparisonOperator::GreaterEqual:
			return Left >= Right;
		case EComparisonOperator::Equal:
			return Left == Right;
		case EComparisonOperator::NotEqual:
			return Left != Right;
		default:
			return false;
		}
	}

	template <typename FloatType>
	static TValuePair<FloatType> FloatValues(const FFloatValueCase& ValueCase)
	{
		switch (ValueCase.Value)
		{
		case EFloatValue::NegativeZero:
			return {-static_cast<FloatType>(0.0), static_cast<FloatType>(0.0)};
		case EFloatValue::PositiveZero:
			return {static_cast<FloatType>(0.0), -static_cast<FloatType>(0.0)};
		case EFloatValue::NaN:
			return {std::numeric_limits<FloatType>::quiet_NaN(), static_cast<FloatType>(1.0)};
		case EFloatValue::PositiveInfinity:
			return {
				std::numeric_limits<FloatType>::infinity(), std::numeric_limits<FloatType>::max()};
		case EFloatValue::NegativeInfinity:
			return {-std::numeric_limits<FloatType>::infinity(),
				std::numeric_limits<FloatType>::lowest()};
		case EFloatValue::Minimum:
			return {std::numeric_limits<FloatType>::lowest(), static_cast<FloatType>(-1.0)};
		case EFloatValue::Maximum:
			return {std::numeric_limits<FloatType>::max(), static_cast<FloatType>(1.0)};
		case EFloatValue::EqualPair:
			return {static_cast<FloatType>(13.25), static_cast<FloatType>(13.25)};
		default:
			return {};
		}
	}

	static TValuePair<int32> IntegralValues(const FIntegralPairCase& PairCase)
	{
		switch (PairCase.Pair)
		{
		case EIntegralPair::EqualZero:
			return {0, 0};
		case EIntegralPair::EqualNamed:
			return {1, 1};
		case EIntegralPair::BelowAdjacent:
			return {-1, 0};
		case EIntegralPair::AboveAdjacent:
			return {1, 0};
		case EIntegralPair::MinimumBoundary:
			return {-128, -1};
		case EIntegralPair::MaximumBoundary:
			return {127, 1};
		default:
			return {};
		}
	}

	static bool VerifyEnumValues(FAutomationTestBase& Test, asITypeInfo& EnumType)
	{
		FNoDiscardAsserter Assert(Test);
		const struct
		{
			const ANSICHAR* Name;
			int32 Value;
		} ExpectedValues[] = {
			{"Minimum", -128},
			{"MinusOne", -1},
			{"Zero", 0},
			{"One", 1},
			{"Maximum", 127},
		};
		bool bPassed = Assert.AreEqual(UE_ARRAY_COUNT(ExpectedValues),
			static_cast<int32>(EnumType.GetEnumValueCount()),
			TEXT("enum comparison should retain every boundary enumerator"));
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(ExpectedValues); ++Index)
		{
			int Value = 0;
			const char* const Name =
				EnumType.GetEnumValueByIndex(static_cast<asUINT>(Index), &Value);
			bPassed &= Assert.AreEqual(FString(ANSI_TO_TCHAR(ExpectedValues[Index].Name)),
				FString(UTF8_TO_TCHAR(Name != nullptr ? Name : "")),
				TEXT("enum comparison should retain exact enumerator order and name"));
			bPassed &= Assert.AreEqual(ExpectedValues[Index].Value,
				static_cast<int32>(Value),
				TEXT("enum comparison should retain exact enumerator boundary value"));
		}
		return bPassed;
	}

	static bool ArraysEqual(const TArray<int32>& Left, const TArray<int32>& Right)
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

	static bool ArraysEqual(const TArray<uint64>& Left, const TArray<uint64>& Right)
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

	template <typename FloatType>
	static int SetFloatArguments(asIScriptContext& Context, const TValuePair<FloatType>& Values)
	{
		if constexpr (std::is_same_v<FloatType, float>)
		{
			const int LeftResult = Context.SetArgFloat(0, Values.Left);
			const int RightResult = Context.SetArgFloat(1, Values.Right);
			return LeftResult >= 0 && RightResult >= 0 ? asSUCCESS : asERROR;
		}
		else
		{
			const int LeftResult = Context.SetArgDouble(0, Values.Left);
			const int RightResult = Context.SetArgDouble(1, Values.Right);
			return LeftResult >= 0 && RightResult >= 0 ? asSUCCESS : asERROR;
		}
	}

	template <typename FloatType>
	static bool ExecuteFloatCase(FAutomationTestBase& Test,
		asIScriptEngine& Engine,
		asIScriptModule& Module,
		asIScriptContext& Context,
		const FNativeCaseContext& Case,
		const FOperatorCase& OperatorCase,
		const FFloatValueCase& ValueCase,
		const FOrderCase& OrderCase,
		const int ExpectedTypeId,
		FComparisonTrace& Trace)
	{
		FNoDiscardAsserter Assert(Test);
		asIScriptFunction* const Function =
			FindComparisonFunction(Module, OrderCase, ExpectedTypeId);
		if (!Assert.IsNotNull(
				Function, *Case.Describe(TEXT("floating comparison entry should resolve exactly"))))
		{
			return false;
		}
		bool bPassed = VerifyComparisonMetadata(Test, Engine, *Function, ExpectedTypeId, Case);
		const TValuePair<FloatType> Values = FloatValues<FloatType>(ValueCase);
		const FloatType ExpressionLeft =
			OrderCase.Order == EComparisonOrder::LeftRight ? Values.Left : Values.Right;
		const FloatType ExpressionRight =
			OrderCase.Order == EComparisonOrder::LeftRight ? Values.Right : Values.Left;
		const TArray<int32> ExpectedMarkers = OrderCase.Order == EComparisonOrder::LeftRight
												  ? TArray<int32>{1, 2}
												  : TArray<int32>{2, 1};
		const TArray<uint64> ExpectedBits = {
			FloatingBits(ExpressionLeft),
			FloatingBits(ExpressionRight),
		};

		Trace.Reset();
		bPassed &= Assert.AreEqual(asSUCCESS,
			Context.Prepare(Function),
			*Case.Describe(TEXT("floating comparison context should prepare")));
		bPassed &= Assert.AreEqual(asSUCCESS,
			SetFloatArguments(Context, Values),
			*Case.Describe(TEXT("floating comparison arguments should bind at physical width")));
		const int ExecuteResult = Context.Execute();
		const bool ActualResult = Context.GetReturnByte() != 0;
		bPassed &= Assert.AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
			ExecuteResult,
			*Case.Describe(TEXT("floating comparison should execute")));
		bPassed &=
			Assert.AreEqual(ExpectedComparison(ExpressionLeft, ExpressionRight, OperatorCase),
				ActualResult,
				*Case.Describe(TEXT("floating comparison should match independent IEEE result")));
		bPassed &= Assert.IsTrue(ArraysEqual(ExpectedMarkers, Trace.Markers),
			*Case.Describe(TEXT("floating operands should evaluate in exact source order")));
		bPassed &= Assert.IsTrue(ArraysEqual(ExpectedBits, Trace.Bits),
			*Case.Describe(TEXT("floating operands should preserve exact physical bits")));
		bPassed &= Assert.AreEqual(asSUCCESS,
			Context.Unprepare(),
			*Case.Describe(TEXT("floating comparison context should unprepare")));
		return bPassed;
	}

	static bool ExecuteIntegralCase(FAutomationTestBase& Test,
		asIScriptEngine& Engine,
		asIScriptModule& Module,
		asIScriptContext& Context,
		const FNativeCaseContext& Case,
		const FOperatorCase& OperatorCase,
		const FIntegralPairCase& PairCase,
		const FOrderCase& OrderCase,
		const int ExpectedTypeId,
		FComparisonTrace& Trace)
	{
		FNoDiscardAsserter Assert(Test);
		asIScriptFunction* const Function =
			FindComparisonFunction(Module, OrderCase, ExpectedTypeId);
		if (!Assert.IsNotNull(
				Function, *Case.Describe(TEXT("integral comparison entry should resolve exactly"))))
		{
			return false;
		}
		bool bPassed = VerifyComparisonMetadata(Test, Engine, *Function, ExpectedTypeId, Case);
		const TValuePair<int32> Values = IntegralValues(PairCase);
		const int32 ExpressionLeft =
			OrderCase.Order == EComparisonOrder::LeftRight ? Values.Left : Values.Right;
		const int32 ExpressionRight =
			OrderCase.Order == EComparisonOrder::LeftRight ? Values.Right : Values.Left;
		const TArray<int32> ExpectedMarkers = OrderCase.Order == EComparisonOrder::LeftRight
												  ? TArray<int32>{1, 2}
												  : TArray<int32>{2, 1};
		const TArray<int32> ExpectedValues = {ExpressionLeft, ExpressionRight};

		Trace.Reset();
		bPassed &= Assert.AreEqual(asSUCCESS,
			Context.Prepare(Function),
			*Case.Describe(TEXT("enum or alias comparison context should prepare")));
		bPassed &= Assert.AreEqual(asSUCCESS,
			Context.SetArgDWord(0, static_cast<asDWORD>(Values.Left)),
			*Case.Describe(TEXT("left enum or alias value should bind")));
		bPassed &= Assert.AreEqual(asSUCCESS,
			Context.SetArgDWord(1, static_cast<asDWORD>(Values.Right)),
			*Case.Describe(TEXT("right enum or alias value should bind")));
		const int ExecuteResult = Context.Execute();
		const bool ActualResult = Context.GetReturnByte() != 0;
		bPassed &= Assert.AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
			ExecuteResult,
			*Case.Describe(TEXT("enum or alias comparison should execute")));
		bPassed &=
			Assert.AreEqual(ExpectedComparison(ExpressionLeft, ExpressionRight, OperatorCase),
				ActualResult,
				*Case.Describe(TEXT("enum or alias comparison should match underlying relation")));
		bPassed &= Assert.IsTrue(ArraysEqual(ExpectedMarkers, Trace.Markers),
			*Case.Describe(TEXT("enum or alias operands should evaluate in exact source order")));
		bPassed &= Assert.IsTrue(ArraysEqual(ExpectedValues, Trace.IntegerValues),
			*Case.Describe(TEXT("enum or alias operands should retain exact underlying values")));
		bPassed &= Assert.AreEqual(asSUCCESS,
			Context.Unprepare(),
			*Case.Describe(TEXT("enum or alias comparison context should unprepare")));
		return bPassed;
	}

	static int32 LastSourceLineContaining(const FString& Source, const FString& Token)
	{
		TArray<FString> Lines;
		Source.ParseIntoArrayLines(Lines, false);
		for (int32 LineIndex = Lines.Num() - 1; LineIndex >= 0; --LineIndex)
		{
			if (Lines[LineIndex].Contains(Token, ESearchCase::CaseSensitive))
			{
				return LineIndex + 1;
			}
		}
		return INDEX_NONE;
	}

	static TArray<AngelscriptNativeTestSupport::FNativeMessageEntry> ErrorMessages(
		const AngelscriptNativeTestSupport::FNativeMessageCollector& Messages)
	{
		TArray<AngelscriptNativeTestSupport::FNativeMessageEntry> Errors;
		for (const AngelscriptNativeTestSupport::FNativeMessageEntry& Entry : Messages.Entries)
		{
			if (Entry.Type == asMSGTYPE_ERROR)
			{
				Errors.Add(Entry);
			}
		}
		return Errors;
	}

	static bool VerifyReferenceMetadata(FAutomationTestBase& Test,
		asIScriptEngine& Engine,
		asIScriptModule& Module,
		asIScriptFunction& Entry,
		const FNativeCaseContext& Case)
	{
		FNoDiscardAsserter Assert(Test);
		asITypeInfo* const Root = Engine.GetTypeInfoByDecl("FComparisonReferenceRoot");
		asITypeInfo* const Derived = Engine.GetTypeInfoByDecl("FComparisonReferenceDerived");
		asITypeInfo* const Sibling = Engine.GetTypeInfoByDecl("FComparisonReferenceSibling");
		if (!Assert.IsNotNull(
				Root, *Case.Describe(TEXT("reference comparison should publish root type"))) ||
			!Assert.IsNotNull(Derived,
				*Case.Describe(TEXT("reference comparison should publish derived type"))) ||
			!Assert.IsNotNull(
				Sibling, *Case.Describe(TEXT("reference comparison should publish sibling type"))))
		{
			return false;
		}
		asIScriptFunction* const Trace =
			AngelscriptNativeTestSupport::GetNativeFunctionByExactDecl(&Module,
				"FComparisonReferenceRoot TraceReferenceOperand("
				"const int, FComparisonReferenceRoot)");
		return Assert.IsTrue((Root->GetFlags() & asOBJ_REF) != 0 &&
								 (Derived->GetFlags() & asOBJ_REF) != 0 &&
								 (Sibling->GetFlags() & asOBJ_REF) != 0,
				   *Case.Describe(
					   TEXT("comparison reference fixtures should retain reference flags"))) &&
			   Assert.IsNotNull(Derived->GetMethodByDecl("FComparisonReferenceRoot opImplCast()"),
				   *Case.Describe(TEXT("derived comparison type should publish root cast"))) &&
			   Assert.IsNotNull(Sibling->GetMethodByDecl("FComparisonReferenceRoot opImplCast()"),
				   *Case.Describe(TEXT("sibling comparison type should publish root cast"))) &&
			   Assert.AreEqual(static_cast<asUINT>(0),
				   Entry.GetParamCount(),
				   *Case.Describe(TEXT("reference comparison entry should take no arguments"))) &&
			   Assert.AreEqual(Engine.GetTypeIdByDecl("bool"),
				   Entry.GetReturnTypeId(),
				   *Case.Describe(TEXT("reference comparison entry should return bool"))) &&
			   Assert.IsNotNull(Trace,
				   *Case.Describe(TEXT("reference comparison should publish typed trace helper")));
	}

	static bool VerifyReferenceTrace(FAutomationTestBase& Test,
		const FReferenceRelationCase& RelationCase,
		const FOrderCase& OrderCase,
		const FReferenceState& State,
		const FNativeCaseContext& Case)
	{
		FNoDiscardAsserter Assert(Test);
		const TArray<int32> ExpectedMarkers = OrderCase.Order == EComparisonOrder::LeftRight
												  ? TArray<int32>{1, 2}
												  : TArray<int32>{2, 1};
		if (!Assert.IsTrue(ArraysEqual(ExpectedMarkers, State.Markers),
				*Case.Describe(
					TEXT("reference comparison operands should evaluate in source order"))) ||
			!Assert.AreEqual(2,
				State.Identities.Num(),
				*Case.Describe(TEXT("reference comparison should record both operand identities"))))
		{
			return false;
		}

		int32 LeftIdentity = INDEX_NONE;
		int32 RightIdentity = INDEX_NONE;
		for (int32 Index = 0; Index < State.Markers.Num(); ++Index)
		{
			if (State.Markers[Index] == 1)
			{
				LeftIdentity = State.Identities[Index];
			}
			else if (State.Markers[Index] == 2)
			{
				RightIdentity = State.Identities[Index];
			}
		}
		const bool bExpectedLeftNull = RelationCase.Relation == EReferenceRelation::LeftNull ||
									   RelationCase.Relation == EReferenceRelation::BothNull;
		const bool bExpectedRightNull = RelationCase.Relation == EReferenceRelation::RightNull ||
										RelationCase.Relation == EReferenceRelation::BothNull;
		return Assert.AreEqual(bExpectedLeftNull,
				   LeftIdentity == 0,
				   *Case.Describe(TEXT("left reference should retain its null state"))) &&
			   Assert.AreEqual(bExpectedRightNull,
				   RightIdentity == 0,
				   *Case.Describe(TEXT("right reference should retain its null state"))) &&
			   Assert.AreEqual(RelationCase.bEqual,
				   LeftIdentity == RightIdentity,
				   *Case.Describe(TEXT("recorded reference identities should match relation")));
	}

	static bool HaveSameIds(const TSet<int32>& Left, const TSet<int32>& Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}

		for (const int32 Id : Left)
		{
			if (!Right.Contains(Id))
			{
				return false;
			}
		}

		return true;
	}

	static bool VerifyReferenceLifecycle(FAutomationTestBase& Test,
		const FReferenceRelationCase& RelationCase,
		const FReferenceState& State,
		const FNativeCaseContext& Case)
	{
		FNoDiscardAsserter Assert(Test);
		return Assert.AreEqual(RelationCase.ExpectedObjects,
				   State.Created,
				   *Case.Describe(TEXT("reference relation should create exact object count"))) &&
			   Assert.AreEqual(State.Created,
				   State.Destroyed,
				   *Case.Describe(
					   TEXT("reference relation should destroy every created object"))) &&
			   Assert.AreEqual(0,
				   State.Live,
				   *Case.Describe(TEXT("reference relation should leave no live object"))) &&
			   Assert.IsTrue(HaveSameIds(State.CreatedIds, State.DestroyedIds),
				   *Case.Describe(TEXT("reference relation should destroy exact identities"))) &&
			   Assert.AreEqual(State.Created + State.AddRefCalls,
				   State.ReleaseCalls,
				   *Case.Describe(TEXT("reference relation should balance every retained owner")));
	}

	static bool ExecuteReferenceCase(FAutomationTestBase& Test,
		asIScriptEngine& Engine,
		asIScriptModule& Module,
		const FNativeCaseContext& Case,
		const FReferenceRelationCase& RelationCase,
		const FOperatorCase& OperatorCase,
		const FOrderCase& OrderCase,
		FReferenceState& State)
	{
		using namespace AngelscriptNativeTestSupport;
		FNoDiscardAsserter Assert(Test);
		asIScriptFunction* const Entry =
			GetNativeFunctionByExactDecl(&Module, "bool RunReferenceComparison()");
		if (!Assert.IsNotNull(
				Entry, *Case.Describe(TEXT("reference comparison entry should resolve exactly"))))
		{
			return false;
		}
		bool bPassed = VerifyReferenceMetadata(Test, Engine, Module, *Entry, Case);
		asIScriptContext* const Context = Engine.CreateContext();
		if (!Assert.IsNotNull(
				Context, *Case.Describe(TEXT("reference comparison should create a context"))))
		{
			return false;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		const int ExecuteResult = PrepareAndExecute(Context, Entry);
		const bool ActualResult = Context->GetReturnByte() != 0;
		const bool ExpectedResult = OperatorCase.Operator == EComparisonOperator::Equal
										? RelationCase.bEqual
										: !RelationCase.bEqual;
		bPassed &= Assert.AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
			ExecuteResult,
			*Case.Describe(TEXT("reference equality comparison should execute")));
		bPassed &= Assert.AreEqual(ExpectedResult,
			ActualResult,
			*Case.Describe(
				TEXT("reference equality should use exact automatic-reference identity")));
		bPassed &= VerifyReferenceTrace(Test, RelationCase, OrderCase, State, Case);
		bPassed &= Assert.AreEqual(asSUCCESS,
			Context->Unprepare(),
			*Case.Describe(TEXT("reference comparison context should unprepare")));
		bPassed &= VerifyReferenceLifecycle(Test, RelationCase, State, Case);
		return bPassed;
	}

	static bool ExecuteReferenceRecovery(FAutomationTestBase& Test,
		asIScriptEngine& Engine,
		asIScriptModule& Module,
		const FNativeCaseContext& Case,
		FReferenceState& State)
	{
		using namespace AngelscriptNativeTestSupport;
		FNoDiscardAsserter Assert(Test);
		asIScriptFunction* const Entry =
			GetNativeFunctionByExactDecl(&Module, "bool RunReferenceComparison()");
		if (!Assert.IsNotNull(
				Entry, *Case.Describe(TEXT("reference recovery entry should resolve exactly"))))
		{
			return false;
		}
		asIScriptContext* const Context = Engine.CreateContext();
		if (!Assert.IsNotNull(
				Context, *Case.Describe(TEXT("reference recovery should create a context"))))
		{
			return false;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};
		const int ExecuteResult = PrepareAndExecute(Context, Entry);
		const bool bResult = Context->GetReturnByte() != 0;
		bool bPassed = Assert.AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
			ExecuteResult,
			*Case.Describe(TEXT("reference recovery should execute")));
		bPassed &= Assert.IsTrue(
			bResult, *Case.Describe(TEXT("reference recovery should preserve self identity")));
		bPassed &= Assert.AreEqual(asSUCCESS,
			Context->Unprepare(),
			*Case.Describe(TEXT("reference recovery context should unprepare")));
		const FReferenceRelationCase RecoveryRelation = {
			"recovery",
			EReferenceRelation::SameNonNull,
			1,
			true,
		};
		bPassed &= VerifyReferenceLifecycle(Test, RecoveryRelation, State, Case);
		return bPassed;
	}

	static asIScriptFunction* FindOverloadedComparisonMethod(asITypeInfo& Type,
		const FOperatorCase& OperatorCase,
		const FReceiverCase& ReceiverCase,
		asIScriptEngine& Engine)
	{
		const ANSICHAR* ExpectedName = UsesEqualsOverload(OperatorCase) ? "opEquals" : "opCmp";
		const int ExpectedReturnTypeId =
			Engine.GetTypeIdByDecl(UsesEqualsOverload(OperatorCase) ? "bool" : "int");
		for (asUINT MethodIndex = 0; MethodIndex < Type.GetMethodCount(); ++MethodIndex)
		{
			asIScriptFunction* const Method = Type.GetMethodByIndex(MethodIndex);
			if (Method == nullptr || FCStringAnsi::Strcmp(Method->GetName(), ExpectedName) != 0 ||
				Method->GetParamCount() != 1 || Method->GetReturnTypeId() != ExpectedReturnTypeId ||
				Method->IsReadOnly() != (ReceiverCase.Constness == EReceiverConstness::Const))
			{
				continue;
			}
			int ParameterTypeId = asINVALID_TYPE;
			asDWORD ParameterFlags = asTM_NONE;
			if (Method->GetParam(0, &ParameterTypeId, &ParameterFlags) >= 0 &&
				ParameterTypeId == Type.GetTypeId() &&
				ParameterFlags == (asTM_INOUTREF | asTM_CONST))
			{
				return Method;
			}
		}
		return nullptr;
	}

	static bool BytecodeCallsFunction(asIScriptFunction& Probe, const int32 ExpectedFunctionId)
	{
		asUINT BytecodeLength = 0;
		asDWORD* const Bytecode = Probe.GetByteCode(&BytecodeLength);
		if (Bytecode == nullptr || BytecodeLength == 0)
		{
			return false;
		}
		asUINT DwordIndex = 0;
		while (DwordIndex < BytecodeLength)
		{
			const asEBCInstr Opcode =
				static_cast<asEBCInstr>(*reinterpret_cast<const asBYTE*>(&Bytecode[DwordIndex]));
			if (static_cast<int32>(Opcode) > static_cast<int32>(asBC_MAXBYTECODE))
			{
				return false;
			}
			if ((Opcode == asBC_CALL || Opcode == asBC_CALLINTF) &&
				asBC_INTARG(&Bytecode[DwordIndex]) == ExpectedFunctionId)
			{
				return true;
			}
			const int32 InstructionSize = asBCTypeSize[asBCInfo[Opcode].type];
			if (InstructionSize <= 0)
			{
				return false;
			}
			DwordIndex += static_cast<asUINT>(InstructionSize);
		}
		return false;
	}

	static bool VerifyOverloadLifecycle(FAutomationTestBase& Test,
		const FOverloadRelationCase& RelationCase,
		const FOverloadState& State,
		const FNativeCaseContext& Case)
	{
		FNoDiscardAsserter Assert(Test);
		const TArray<int32> ExpectedCreationOrder = {1, 2};
		const TArray<int32> ExpectedDestructionOrder = {2, 1};
		const TArray<int32> ExpectedCreatedValues = {
			RelationCase.Left,
			RelationCase.Right,
		};
		const TArray<int32> ExpectedDestroyedValues = {
			RelationCase.Right,
			RelationCase.Left,
		};
		return Assert.AreEqual(2,
				   State.CreatedIds.Num(),
				   *Case.Describe(TEXT("overloaded comparison should create two exact values"))) &&
			   Assert.IsTrue(HaveSameIds(State.CreatedIds, State.DestroyedIds),
				   *Case.Describe(
					   TEXT("overloaded comparison should destroy exact value identities"))) &&
			   Assert.IsTrue(ArraysEqual(ExpectedCreationOrder, State.CreationOrder),
				   *Case.Describe(
					   TEXT("overloaded comparison should preserve construction order"))) &&
			   Assert.IsTrue(ArraysEqual(ExpectedDestructionOrder, State.DestructionOrder),
				   *Case.Describe(
					   TEXT("overloaded comparison should destroy locals in reverse order"))) &&
			   Assert.IsTrue(ArraysEqual(ExpectedCreatedValues, State.CreatedValues),
				   *Case.Describe(
					   TEXT("overloaded comparison should construct exact relation values"))) &&
			   Assert.IsTrue(ArraysEqual(ExpectedDestroyedValues, State.DestroyedValues),
				   *Case.Describe(
					   TEXT("overloaded comparison should destroy exact relation values")));
	}

	static bool ExecuteOverloadedComparisonCase(FAutomationTestBase& Test,
		asIScriptEngine& Engine,
		asIScriptModule& Module,
		const FNativeCaseContext& Case,
		const FOperatorCase& OperatorCase,
		const FOverloadRelationCase& RelationCase,
		const FOrderCase& OrderCase,
		const FReceiverCase& ReceiverCase,
		FOverloadState& State)
	{
		using namespace AngelscriptNativeTestSupport;
		FNoDiscardAsserter Assert(Test);
		asIScriptFunction* const Entry =
			GetNativeFunctionByExactDecl(&Module, "bool RunOverloadedComparison()");
		asITypeInfo* const Type = Module.GetTypeInfoByName("FOverloadedComparisonValue");
		if (!Assert.IsNotNull(Entry,
				*Case.Describe(TEXT("overloaded comparison entry should resolve exactly"))) ||
			!Assert.IsNotNull(
				Type, *Case.Describe(TEXT("overloaded comparison value type should publish"))))
		{
			return false;
		}
		asIScriptFunction* const Selected =
			FindOverloadedComparisonMethod(*Type, OperatorCase, ReceiverCase, Engine);
		if (!Assert.IsNotNull(Selected,
				*Case.Describe(TEXT("exact const or mutable comparison overload should resolve"))))
		{
			return false;
		}

		bool bPassed = Assert.AreEqual(static_cast<asUINT>(0),
			Entry->GetParamCount(),
			*Case.Describe(TEXT("overloaded comparison entry should take no arguments")));
		bPassed &= Assert.AreEqual(Engine.GetTypeIdByDecl("bool"),
			Entry->GetReturnTypeId(),
			*Case.Describe(TEXT("overloaded comparison entry should return bool")));
		bPassed &= Assert.IsTrue(BytecodeCallsFunction(*Entry, Selected->GetId()),
			*Case.Describe(
				TEXT("overloaded comparison bytecode should call exact selected method")));

		asIScriptContext* const Context = Engine.CreateContext();
		if (!Assert.IsNotNull(
				Context, *Case.Describe(TEXT("overloaded comparison should create a context"))))
		{
			return false;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		const int ExecuteResult = PrepareAndExecute(Context, Entry);
		const bool ActualResult = Context->GetReturnByte() != 0;
		const int32 ExpressionLeft =
			OrderCase.Order == EComparisonOrder::LeftRight ? RelationCase.Left : RelationCase.Right;
		const int32 ExpressionRight =
			OrderCase.Order == EComparisonOrder::LeftRight ? RelationCase.Right : RelationCase.Left;
		bPassed &= Assert.AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
			ExecuteResult,
			*Case.Describe(TEXT("overloaded comparison should execute")));
		bPassed &=
			Assert.AreEqual(ExpectedComparison(ExpressionLeft, ExpressionRight, OperatorCase),
				ActualResult,
				*Case.Describe(
					TEXT("overloaded comparison should return exact independent relation result")));
		bPassed &= Assert.AreEqual(1,
			State.Markers.Num(),
			*Case.Describe(TEXT("overloaded comparison should invoke one operator method")));
		if (State.Markers.Num() == 1)
		{
			bPassed &= Assert.AreEqual(ExpectedOverloadMarker(OperatorCase, ReceiverCase),
				State.Markers[0],
				*Case.Describe(
					TEXT("overloaded comparison should invoke exact const or mutable family")));
		}
		bPassed &= Assert.AreEqual(1,
			State.LeftValues.Num(),
			*Case.Describe(TEXT("overloaded comparison should record one receiver value")));
		bPassed &= Assert.AreEqual(1,
			State.RightValues.Num(),
			*Case.Describe(TEXT("overloaded comparison should record one argument value")));
		if (State.LeftValues.Num() == 1 && State.RightValues.Num() == 1)
		{
			bPassed &= Assert.AreEqual(ExpressionLeft,
				State.LeftValues[0],
				*Case.Describe(TEXT("operator receiver should retain expression-left value")));
			bPassed &= Assert.AreEqual(ExpressionRight,
				State.RightValues[0],
				*Case.Describe(TEXT("operator argument should retain expression-right value")));
		}
		bPassed &= Assert.AreEqual(asSUCCESS,
			Context->Unprepare(),
			*Case.Describe(TEXT("overloaded comparison context should unprepare")));
		bPassed &= VerifyOverloadLifecycle(Test, RelationCase, State, Case);
		return bPassed;
	}

	static void DiscardModule(FAutomationTestBase& Test,
		asIScriptEngine& Engine,
		const FString& ModuleName,
		const FString& Description)
	{
		FNoDiscardAsserter Assert(Test);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		if (!Assert.AreEqual(asSUCCESS,
				Engine.DiscardModule(ModuleNameUtf8.Get()),
				*FString::Printf(TEXT("%s should discard"), *Description)) ||
			!Assert.IsNull(Engine.GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
				*FString::Printf(TEXT("%s should leave no stale module"), *Description)))
		{
			return;
		}
	}

public:

	TEST_METHOD(EnumAndAliasByOperatorPairAndOrder)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("LANG-OP-COMPARISON-ENUM-ALIAS",
			ENativeEvidence::Compile | ENativeEvidence::Runtime | ENativeEvidence::Metadata);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		ASSERT_THAT(IsNotNull(
			Engine.Get(), TEXT("enum and alias comparison product should create an engine")));
		if (Engine.Get() == nullptr)
		{
			return;
		}
		const int AliasResult = Engine.Get()->RegisterTypedef("ComparisonAlias", "int");
		ASSERT_THAT(IsTrue(AliasResult >= 0,
			FString::Printf(TEXT("enum and alias comparison product should register the alias through the raw SDK. Result=%d Messages={%s}"),
				AliasResult,
				*Engine.GetMessagesText())));

		FComparisonTrace Trace;
		ASSERT_THAT(IsTrue(RegisterTraceFunctions(*Engine.Get(), Trace),
			TEXT("enum and alias comparison product should register trace functions")));
		asIScriptContext* const Context = Engine.Get()->CreateContext();
		ASSERT_THAT(IsNotNull(
			Context, TEXT("enum and alias comparison product should create one reusable context")));
		if (Context == nullptr)
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		TArray<FString> ConstructedIds;
		TSet<FString> UniqueIds;
		bool bAllCasesPassed = true;
		for (const FIntegralFamilyCase& FamilyCase : IntegralFamilyCases)
		{
			for (const FOperatorCase& OperatorCase : OperatorCases)
			{
				const FString ModuleName =
					FString::Printf(TEXT("ASNativeComparisonIntegral_%hs_%hs"),
						FamilyCase.CatalogName,
						OperatorCase.CatalogName);
				const FString SourceId = MakeNativeCaseId("LANG-OP-COMPARISON-ENUM-ALIAS-SOURCE",
					{ANSI_TO_TCHAR(FamilyCase.CatalogName),
						ANSI_TO_TCHAR(OperatorCase.CatalogName)});
				const FString Source = BuildIntegralComparisonSource(FamilyCase, OperatorCase);
				asIScriptModule* const Module =
					CompileAndReport(Engine, *TestRunner, SourceId, ModuleName, Source);
				if (Module == nullptr)
				{
					return;
				}
				const FTCHARToUTF8 TypeUtf8(*IntegralTypeDeclaration(FamilyCase));
				const int ExpectedTypeId = Module->GetTypeIdByDecl(TypeUtf8.Get());
				ASSERT_THAT(IsTrue(ExpectedTypeId >= 0,
					TEXT("enum or alias comparison source type should resolve in its module")));
				asITypeInfo* const EnumType = FamilyCase.Family == EIntegralFamily::Enum
												  ? Module->GetTypeInfoByName("EComparisonEnum")
												  : nullptr;
				if (FamilyCase.Family == EIntegralFamily::Enum)
				{
					ASSERT_THAT(IsNotNull(
						EnumType, TEXT("enum comparison should publish its nominal enum type")));
					if (EnumType != nullptr)
					{
						ASSERT_THAT(IsTrue(VerifyEnumValues(*TestRunner, *EnumType),
							TEXT("enum comparison should retain exact named boundary values")));
					}
				}
				else
				{
					ASSERT_THAT(AreEqual(Engine.Get()->GetTypeIdByDecl("int"),
						ExpectedTypeId,
						TEXT("comparison alias should preserve its int representation")));
				}

				for (const FIntegralPairCase& PairCase : IntegralPairCases)
				{
					for (const FOrderCase& OrderCase : OrderCases)
					{
						const FNativeCaseContext Case(
							MakeNativeCaseId("LANG-OP-COMPARISON-ENUM-ALIAS",
								{ANSI_TO_TCHAR(FamilyCase.CatalogName),
									ANSI_TO_TCHAR(OperatorCase.CatalogName),
									ANSI_TO_TCHAR(PairCase.CatalogName),
									ANSI_TO_TCHAR(OrderCase.CatalogName)}));
						ConstructedIds.Add(Case.GetId());
						const bool bUniqueCaseId = !UniqueIds.Contains(Case.GetId());
						UniqueIds.Add(Case.GetId());
						ASSERT_THAT(IsTrue(bUniqueCaseId,
							*Case.Describe(
								TEXT("enum or alias comparison case ID should be unique"))));
						bAllCasesPassed &= ExecuteIntegralCase(*TestRunner,
							*Engine.Get(),
							*Module,
							*Context,
							Case,
							OperatorCase,
							PairCase,
							OrderCase,
							ExpectedTypeId,
							Trace);
					}
				}

				DiscardModule(*TestRunner,
					*Engine.Get(),
					ModuleName,
					TEXT("enum or alias comparison module"));
			}
		}

		ASSERT_THAT(AreEqual(144,
			ConstructedIds.Num(),
			TEXT("enum and alias comparison product should construct all 144 catalog IDs")));
		ASSERT_THAT(AreEqual(ConstructedIds.Num(),
			UniqueIds.Num(),
			TEXT("enum and alias comparison product should construct no duplicate IDs")));
		ASSERT_THAT(IsTrue(bAllCasesPassed,
			TEXT("every enum and alias comparison cell should satisfy nominal type, order, "
				 "underlying value, and runtime result")));
	}
};

#endif
