#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAssignmentTypeRejectionTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Operators.Assignment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using ENativeScalarAccessor = AngelscriptNativeTestSupport::ENativeScalarAccessor;
	using ENativeValueCategory = AngelscriptNativeTestSupport::ENativeValueCategory;
	using FNativeCaseContext = AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeTestEngine = AngelscriptNativeTestSupport::FNativeTestEngine;
	using FNativeTypeCase = AngelscriptNativeTestSupport::FNativeTypeCase;

	inline static constexpr asPWORD AssignmentStateSlot =
		static_cast<asPWORD>(0x4E41544941535347ull);

	enum class EAssignmentOperation : uint8
	{
		Assign,
		Add,
		Subtract,
		Multiply,
		Divide,
		Modulo,
		Power,
		BitAnd,
		BitOr,
		BitXor,
		ShiftLeft,
		ShiftRightLogical,
		ShiftRightArithmetic,
	};

	enum class EAssignmentCategory : uint8
	{
		Local,
		Field,
		Property,
		Alias,
	};

	enum class ERejectedAssignmentTarget : uint8
	{
		Const,
		Temporary,
	};

	struct FAssignmentOperationCase
	{
		const ANSICHAR* CatalogName;
		const TCHAR* Token;
		const TCHAR* FunctionSuffix;
		EAssignmentOperation Operation;
	};

	struct FAssignmentCategoryCase
	{
		const ANSICHAR* CatalogName;
		EAssignmentCategory Category;
	};

	struct FRejectedAssignmentTargetCase
	{
		const ANSICHAR* CatalogName;
		ERejectedAssignmentTarget Target;
	};

	struct FOperationTypeCase
	{
		const FAssignmentOperationCase* Operation = nullptr;
		const FNativeTypeCase* Type = nullptr;

		FString GetCatalogName() const
		{
			return FString::Printf(TEXT("%hs_%hs"), Operation->CatalogName, Type->CatalogName);
		}
	};

	struct FAssignmentState
	{
		void ResetRuntime(const uint64 InitialBits)
		{
			ValueBits = InitialBits;
			GetterCalls = 0;
			SetterCalls = 0;
			FactoryCalls = 0;
			AddRefCalls = 0;
			ReleaseCalls = 0;
			DestroyedObjects = 0;
			LiveObjects = 0;
			ObserverCalls = 0;
			ObservedResultBits = 0;
			ObservedFinalBits = 0;
			TemporaryProducerCalls = 0;
		}

		uint64 ValueBits = 0;
		int32 GetterCalls = 0;
		int32 SetterCalls = 0;
		int32 FactoryCalls = 0;
		int32 AddRefCalls = 0;
		int32 ReleaseCalls = 0;
		int32 DestroyedObjects = 0;
		int32 LiveObjects = 0;
		int32 ObserverCalls = 0;
		uint64 ObservedResultBits = 0;
		uint64 ObservedFinalBits = 0;
		int32 TemporaryProducerCalls = 0;
		TMap<int32, ENativeScalarAccessor> GetterAccessorByFunctionId;
		TMap<int32, ENativeScalarAccessor> SetterAccessorByFunctionId;
		TMap<int32, ENativeScalarAccessor> ObserverAccessorByFunctionId;
	};

	class FAssignmentPropertyObject
	{
	public:
		explicit FAssignmentPropertyObject(FAssignmentState& InState) : State(InState)
		{
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
				++State.DestroyedObjects;
				--State.LiveObjects;
				delete this;
			}
		}

	private:
		FAssignmentState& State;
		int32 ReferenceCount = 1;
	};

	inline static constexpr FAssignmentOperationCase OperationCases[] = {
		{"assign", TEXT("="), TEXT("Assign"), EAssignmentOperation::Assign},
		{"add_assign", TEXT("+="), TEXT("AddAssign"), EAssignmentOperation::Add},
		{"subtract_assign", TEXT("-="), TEXT("SubtractAssign"), EAssignmentOperation::Subtract},
		{"multiply_assign", TEXT("*="), TEXT("MultiplyAssign"), EAssignmentOperation::Multiply},
		{"divide_assign", TEXT("/="), TEXT("DivideAssign"), EAssignmentOperation::Divide},
		{"modulo_assign", TEXT("%="), TEXT("ModuloAssign"), EAssignmentOperation::Modulo},
		{"power_assign", TEXT("**="), TEXT("PowerAssign"), EAssignmentOperation::Power},
		{"and_assign", TEXT("&="), TEXT("AndAssign"), EAssignmentOperation::BitAnd},
		{"or_assign", TEXT("|="), TEXT("OrAssign"), EAssignmentOperation::BitOr},
		{"xor_assign", TEXT("^="), TEXT("XorAssign"), EAssignmentOperation::BitXor},
		{"shift_left_assign",
			TEXT("<<="),
			TEXT("ShiftLeftAssign"),
			EAssignmentOperation::ShiftLeft},
		{"shift_right_logical_assign",
			TEXT(">>="),
			TEXT("ShiftRightLogicalAssign"),
			EAssignmentOperation::ShiftRightLogical},
		{"shift_right_arithmetic_assign",
			TEXT(">>>="),
			TEXT("ShiftRightArithmeticAssign"),
			EAssignmentOperation::ShiftRightArithmetic},
	};

	inline static constexpr FAssignmentCategoryCase CategoryCases[] = {
		{"local", EAssignmentCategory::Local},
		{"field", EAssignmentCategory::Field},
		{"property", EAssignmentCategory::Property},
		{"alias", EAssignmentCategory::Alias},
	};

	inline static constexpr FRejectedAssignmentTargetCase RejectedTargetCases[] = {
		{"const_invalid", ERejectedAssignmentTarget::Const},
		{"temporary_invalid", ERejectedAssignmentTarget::Temporary},
	};

	static FAssignmentState* ActiveState(asIScriptGeneric& Generic)
	{
		return Generic.GetEngine() != nullptr
				   ? static_cast<FAssignmentState*>(
						 Generic.GetEngine()->GetUserData(AssignmentStateSlot))
				   : nullptr;
	}

	static FAssignmentState* ActiveState()
	{
		asIScriptContext* const Context = asGetActiveContext();
		return Context != nullptr && Context->GetEngine() != nullptr
				   ? static_cast<FAssignmentState*>(
						 Context->GetEngine()->GetUserData(AssignmentStateSlot))
				   : nullptr;
	}

	static void GenericPropertyAddRef(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		if (FAssignmentPropertyObject* const Object =
				static_cast<FAssignmentPropertyObject*>(Generic->GetObject()))
		{
			Object->AddRef();
		}
	}

	static void GenericPropertyRelease(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		if (FAssignmentPropertyObject* const Object =
				static_cast<FAssignmentPropertyObject*>(Generic->GetObject()))
		{
			Object->Release();
		}
	}

	static void GenericMakeProperty(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		FAssignmentState* const State = ActiveState(*Generic);
		if (State == nullptr)
		{
			Generic->SetReturnAddress(nullptr);
			return;
		}
		++State->FactoryCalls;
		++State->LiveObjects;
		Generic->SetReturnAddress(new FAssignmentPropertyObject(*State));
	}

	static uint64 ReadGenericArgument(asIScriptGeneric& Generic,
		const ENativeScalarAccessor Accessor,
		const asUINT ArgumentIndex)
	{
		switch (Accessor)
		{
		case ENativeScalarAccessor::Byte:
			return Generic.GetArgByte(ArgumentIndex);
		case ENativeScalarAccessor::Word:
			return Generic.GetArgWord(ArgumentIndex);
		case ENativeScalarAccessor::DWord:
			return Generic.GetArgDWord(ArgumentIndex);
		case ENativeScalarAccessor::QWord:
			return Generic.GetArgQWord(ArgumentIndex);
		case ENativeScalarAccessor::Float:
		{
			const float Value = Generic.GetArgFloat(ArgumentIndex);
			uint32 Bits = 0;
			FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
			return Bits;
		}
		case ENativeScalarAccessor::Double:
		{
			const double Value = Generic.GetArgDouble(ArgumentIndex);
			uint64 Bits = 0;
			FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
			return Bits;
		}
		default:
			return 0;
		}
	}

	static void SetGenericReturn(
		asIScriptGeneric& Generic, const ENativeScalarAccessor Accessor, const uint64 Bits)
	{
		switch (Accessor)
		{
		case ENativeScalarAccessor::Byte:
			Generic.SetReturnByte(static_cast<asBYTE>(Bits));
			break;
		case ENativeScalarAccessor::Word:
			Generic.SetReturnWord(static_cast<asWORD>(Bits));
			break;
		case ENativeScalarAccessor::DWord:
			Generic.SetReturnDWord(static_cast<asDWORD>(Bits));
			break;
		case ENativeScalarAccessor::QWord:
			Generic.SetReturnQWord(static_cast<asQWORD>(Bits));
			break;
		case ENativeScalarAccessor::Float:
		{
			const uint32 NarrowBits = static_cast<uint32>(Bits);
			float Value = 0.0f;
			FMemory::Memcpy(&Value, &NarrowBits, sizeof(Value));
			Generic.SetReturnFloat(Value);
			break;
		}
		case ENativeScalarAccessor::Double:
		{
			double Value = 0.0;
			FMemory::Memcpy(&Value, &Bits, sizeof(Value));
			Generic.SetReturnDouble(Value);
			break;
		}
		default:
			break;
		}
	}

	static void GenericGetProperty(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr || Generic->GetFunction() == nullptr)
		{
			return;
		}
		FAssignmentState* const State = ActiveState(*Generic);
		const ENativeScalarAccessor* const Accessor =
			State != nullptr
				? State->GetterAccessorByFunctionId.Find(Generic->GetFunction()->GetId())
				: nullptr;
		if (State == nullptr || Accessor == nullptr)
		{
			return;
		}
		++State->GetterCalls;
		SetGenericReturn(*Generic, *Accessor, State->ValueBits);
	}

	static void GenericSetProperty(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr || Generic->GetFunction() == nullptr)
		{
			return;
		}
		FAssignmentState* const State = ActiveState(*Generic);
		const ENativeScalarAccessor* const Accessor =
			State != nullptr
				? State->SetterAccessorByFunctionId.Find(Generic->GetFunction()->GetId())
				: nullptr;
		if (State == nullptr || Accessor == nullptr)
		{
			return;
		}
		++State->SetterCalls;
		State->ValueBits = ReadGenericArgument(*Generic, *Accessor, 0);
	}

	static void GenericObserveAssignment(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr || Generic->GetFunction() == nullptr)
		{
			return;
		}
		FAssignmentState* const State = ActiveState(*Generic);
		const ENativeScalarAccessor* const Accessor =
			State != nullptr
				? State->ObserverAccessorByFunctionId.Find(Generic->GetFunction()->GetId())
				: nullptr;
		if (State == nullptr || Accessor == nullptr)
		{
			return;
		}
		++State->ObserverCalls;
		State->ObservedResultBits = ReadGenericArgument(*Generic, *Accessor, 0);
		State->ObservedFinalBits = ReadGenericArgument(*Generic, *Accessor, 1);
	}

	static void GenericRecordTemporaryProducer(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		if (FAssignmentState* const State = ActiveState(*Generic))
		{
			++State->TemporaryProducerCalls;
		}
	}

	static bool IsNumericType(const FNativeTypeCase& TypeCase)
	{
		return TypeCase.Category == ENativeValueCategory::SignedInteger ||
			   TypeCase.Category == ENativeValueCategory::UnsignedInteger ||
			   TypeCase.Category == ENativeValueCategory::FloatingPoint;
	}

	static bool IsIntegralType(const FNativeTypeCase& TypeCase)
	{
		return TypeCase.Category == ENativeValueCategory::SignedInteger ||
			   TypeCase.Category == ENativeValueCategory::UnsignedInteger;
	}

	static bool IsPrimitiveType(const FNativeTypeCase& TypeCase)
	{
		return IsNumericType(TypeCase) || TypeCase.Category == ENativeValueCategory::Boolean;
	}

	static FString CanonicalType(const FNativeTypeCase& TypeCase, const asIScriptEngine& Engine)
	{
		if (AngelscriptNativeTestSupport::EqualAnsi(TypeCase.CatalogName, "float32"))
		{
			return TEXT("float32");
		}
		if (AngelscriptNativeTestSupport::EqualAnsi(TypeCase.CatalogName, "float64"))
		{
			return TEXT("double");
		}
		return ANSI_TO_TCHAR(TypeCase.ScriptType);
	}

	static FString PropertyTypeName(const FNativeTypeCase& TypeCase)
	{
		return FString::Printf(TEXT("FAssignmentProperty_%hs"), TypeCase.CatalogName);
	}

	static FString PropertyFactoryName(const FNativeTypeCase& TypeCase)
	{
		return FString::Printf(TEXT("MakeAssignmentProperty_%hs"), TypeCase.CatalogName);
	}

	static FString ObserverName(const FNativeTypeCase& TypeCase)
	{
		return FString::Printf(TEXT("ObserveAssignment_%hs"), TypeCase.CatalogName);
	}

	static bool RegisterPropertyType(
		asIScriptEngine& Engine, FAssignmentState& State, const FNativeTypeCase& TypeCase)
	{
		const FString ObjectType = PropertyTypeName(TypeCase);
		const FString ValueType = CanonicalType(TypeCase, Engine);
		const FString GetterDeclaration =
			FString::Printf(TEXT("%s get_Value() const"), *ValueType);
		const FString SetterDeclaration =
			FString::Printf(TEXT("void set_Value(%s InValue)"), *ValueType);
		const FString FactoryDeclaration =
			FString::Printf(TEXT("%s %s()"), *ObjectType, *PropertyFactoryName(TypeCase));
		const FString ObserverDeclaration = FString::Printf(
			TEXT("void %s(%s Result, %s Final)"), *ObserverName(TypeCase), *ValueType, *ValueType);
		const FTCHARToUTF8 ObjectTypeUtf8(*ObjectType);
		const FTCHARToUTF8 GetterUtf8(*GetterDeclaration);
		const FTCHARToUTF8 SetterUtf8(*SetterDeclaration);
		const FTCHARToUTF8 FactoryUtf8(*FactoryDeclaration);
		const FTCHARToUTF8 ObserverUtf8(*ObserverDeclaration);
		if (Engine.RegisterObjectType(ObjectTypeUtf8.Get(), 0, asOBJ_REF | asOBJ_IMPLICIT_HANDLE) < 0 ||
			Engine.RegisterObjectBehaviour(ObjectTypeUtf8.Get(),
				asBEHAVE_ADDREF,
				"void f()",
				asFUNCTION(GenericPropertyAddRef),
				asCALL_GENERIC) < 0 ||
			Engine.RegisterObjectBehaviour(ObjectTypeUtf8.Get(),
				asBEHAVE_RELEASE,
				"void f()",
				asFUNCTION(GenericPropertyRelease),
				asCALL_GENERIC) < 0)
		{
			return false;
		}
		const int32 GetterId = Engine.RegisterObjectMethod(
			ObjectTypeUtf8.Get(), GetterUtf8.Get(), asFUNCTION(GenericGetProperty), asCALL_GENERIC);
		const int32 SetterId = Engine.RegisterObjectMethod(
			ObjectTypeUtf8.Get(), SetterUtf8.Get(), asFUNCTION(GenericSetProperty), asCALL_GENERIC);
		const int32 FactoryId = Engine.RegisterGlobalFunction(
			FactoryUtf8.Get(), asFUNCTION(GenericMakeProperty), asCALL_GENERIC);
		const int32 ObserverId = Engine.RegisterGlobalFunction(
			ObserverUtf8.Get(), asFUNCTION(GenericObserveAssignment), asCALL_GENERIC);
		if (GetterId < 0 || SetterId < 0 || FactoryId < 0 || ObserverId < 0)
		{
			return false;
		}
		State.GetterAccessorByFunctionId.Add(GetterId, TypeCase.Accessor);
		State.SetterAccessorByFunctionId.Add(SetterId, TypeCase.Accessor);
		State.ObserverAccessorByFunctionId.Add(ObserverId, TypeCase.Accessor);
		return true;
	}

	static bool RegisterFixtures(asIScriptEngine& Engine,
		FAssignmentState& State,
		const TArray<const FNativeTypeCase*>& PrimitiveTypes)
	{
		Engine.SetUserData(&State, AssignmentStateSlot);
		for (const FNativeTypeCase* TypeCase : PrimitiveTypes)
		{
			if (!RegisterPropertyType(Engine, State, *TypeCase))
			{
				return false;
			}
		}
		return Engine.RegisterGlobalFunction("void RecordAssignmentTemporaryProducer()",
				   asFUNCTION(GenericRecordTemporaryProducer),
				   asCALL_GENERIC) >= 0;
	}

	static bool SupportsOperation(
		const FAssignmentOperationCase& OperationCase, const FNativeTypeCase& TypeCase)
	{
		switch (OperationCase.Operation)
		{
		case EAssignmentOperation::Assign:
			return IsPrimitiveType(TypeCase);
		case EAssignmentOperation::Add:
		case EAssignmentOperation::Subtract:
		case EAssignmentOperation::Multiply:
		case EAssignmentOperation::Divide:
		case EAssignmentOperation::Modulo:
			return IsNumericType(TypeCase);
		case EAssignmentOperation::Power:
			// The current fork deliberately rejects exponentiation on integer
			// operands; only floating-point assignment remains a legal source.
			return TypeCase.Category == ENativeValueCategory::FloatingPoint;
		case EAssignmentOperation::BitAnd:
		case EAssignmentOperation::BitOr:
		case EAssignmentOperation::BitXor:
		case EAssignmentOperation::ShiftLeft:
		case EAssignmentOperation::ShiftRightLogical:
		case EAssignmentOperation::ShiftRightArithmetic:
			return IsIntegralType(TypeCase);
		default:
			return false;
		}
	}

	static FString EntryName(const FAssignmentOperationCase& OperationCase)
	{
		return FString::Printf(TEXT("Run%s"), OperationCase.FunctionSuffix);
	}

	static FString AliasName(const FAssignmentOperationCase& OperationCase)
	{
		return FString::Printf(TEXT("Apply%sAlias"), OperationCase.FunctionSuffix);
	}

	static FString TargetExpression(const FAssignmentCategoryCase& CategoryCase)
	{
		switch (CategoryCase.Category)
		{
		case EAssignmentCategory::Local:
		case EAssignmentCategory::Alias:
			return TEXT("Value");
		case EAssignmentCategory::Field:
		case EAssignmentCategory::Property:
			return TEXT("Owner.Value");
		default:
			return TEXT("Value");
		}
	}

	static FString AssignmentExpression(
		const FAssignmentOperationCase& OperationCase, const FString& Target, const FString& Source)
	{
		return Target + TEXT(" ") + OperationCase.Token + TEXT(" ") + Source;
	}

	static FString BuildLegalSource(asIScriptEngine& Engine,
		const FNativeTypeCase& TypeCase,
		const FAssignmentCategoryCase& CategoryCase)
	{
		using namespace AngelscriptNativeTestSupport;
		const FString Type = CanonicalType(TypeCase, Engine);
		FString Source;
		if (CategoryCase.Category == EAssignmentCategory::Field)
		{
			AppendGeneratedAsLine(Source, TEXT("struct FAssignmentFieldOwner"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s Value;"), *Type));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}

		for (const FAssignmentOperationCase& OperationCase : OperationCases)
		{
			if (!SupportsOperation(OperationCase, TypeCase))
			{
				continue;
			}
			if (CategoryCase.Category == EAssignmentCategory::Alias)
			{
				AppendGeneratedAsLine(Source,
					FString::Printf(TEXT("%s %s(%s& inout Value, %s Source)"),
						*Type,
						*AliasName(OperationCase),
						*Type,
						*Type));
				AppendGeneratedAsLine(Source, TEXT("{"));
				AppendGeneratedAsLine(Source,
					FString::Printf(TEXT("\t%s;"),
						*AssignmentExpression(OperationCase, TEXT("Value"), TEXT("Source"))));
				AppendGeneratedAsLine(Source,
					FString::Printf(TEXT("\t%s Result = Value;"), *Type));
				AppendGeneratedAsLine(
					Source, FString::Printf(TEXT("\t%s(Result, Value);"), *ObserverName(TypeCase)));
				AppendGeneratedAsLine(Source, TEXT("\treturn Result;"));
				AppendGeneratedAsLine(Source, TEXT("}"));
				AppendGeneratedAsLine(Source);
			}

			AppendGeneratedAsLine(Source,
				FString::Printf(TEXT("%s %s(%s Input, %s Source)"),
					*Type,
					*EntryName(OperationCase),
					*Type,
					*Type));
			AppendGeneratedAsLine(Source, TEXT("{"));
			switch (CategoryCase.Category)
			{
			case EAssignmentCategory::Local:
			case EAssignmentCategory::Alias:
				AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s Value = Input;"), *Type));
				break;
			case EAssignmentCategory::Field:
				AppendGeneratedAsLine(Source, TEXT("\tFAssignmentFieldOwner Owner;"));
				AppendGeneratedAsLine(Source, TEXT("\tOwner.Value = Input;"));
				break;
			case EAssignmentCategory::Property:
				AppendGeneratedAsLine(Source,
					FString::Printf(TEXT("\t%s Owner = %s();"),
						*PropertyTypeName(TypeCase),
						*PropertyFactoryName(TypeCase)));
				break;
			}

			if (CategoryCase.Category == EAssignmentCategory::Alias)
			{
				AppendGeneratedAsLine(Source,
					FString::Printf(
						TEXT("\treturn %s(Value, Source);"), *AliasName(OperationCase)));
			}
			else
			{
				const FString Target = TargetExpression(CategoryCase);
				if (CategoryCase.Category == EAssignmentCategory::Property)
				{
					const bool bCompound = OperationCase.Operation != EAssignmentOperation::Assign;
					const FString AssignmentTarget = bCompound ? TEXT("Current") : TEXT("Source");
					if (bCompound)
					{
						AppendGeneratedAsLine(Source,
							FString::Printf(TEXT("\t%s Current = Owner.get_Value();"), *Type));
					}
					if (bCompound)
					{
						AppendGeneratedAsLine(Source,
							FString::Printf(TEXT("\t%s;"),
								*AssignmentExpression(OperationCase, AssignmentTarget, TEXT("Source"))));
						AppendGeneratedAsLine(Source,
							FString::Printf(TEXT("\t%s Result = Current;"), *Type));
					}
					else
					{
						AppendGeneratedAsLine(Source,
							FString::Printf(TEXT("\t%s Result = Source;"), *Type));
					}
					AppendGeneratedAsLine(Source, TEXT("\tOwner.set_Value(Result);"));
					AppendGeneratedAsLine(Source,
						FString::Printf(TEXT("\t%s(Result, Owner.get_Value());"), *ObserverName(TypeCase)));
				}
				else
				{
					AppendGeneratedAsLine(Source,
						FString::Printf(TEXT("\t%s;"),
							*AssignmentExpression(OperationCase, Target, TEXT("Source"))));
					AppendGeneratedAsLine(Source,
						FString::Printf(TEXT("\t%s Result = %s;"), *Type, *Target));
					AppendGeneratedAsLine(Source,
						FString::Printf(TEXT("\t%s(Result, %s);"), *ObserverName(TypeCase), *Target));
				}
				AppendGeneratedAsLine(Source, TEXT("\treturn Result;"));
			}
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		return Source;
	}

	static FString BuildTargetRejectionSource(asIScriptEngine& Engine,
		const FOperationTypeCase& OperationTypeCase,
		const FRejectedAssignmentTargetCase& TargetCase)
	{
		using namespace AngelscriptNativeTestSupport;
		const FString Type = CanonicalType(*OperationTypeCase.Type, Engine);
		FString Source;
		if (TargetCase.Target == ERejectedAssignmentTarget::Temporary)
		{
			AppendGeneratedAsLine(Source,
				FString::Printf(
					TEXT("%s MakeRejectedAssignmentTemporary(%s Input)"), *Type, *Type));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tRecordAssignmentTemporaryProducer();"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Input;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		AppendGeneratedAsLine(Source,
			FString::Printf(TEXT("int RejectAssignmentTarget(%s Input, %s Source)"), *Type, *Type));
		AppendGeneratedAsLine(Source, TEXT("{"));
		FString Target;
		if (TargetCase.Target == ERejectedAssignmentTarget::Const)
		{
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("\tconst %s Value = Input;"), *Type));
			Target = TEXT("Value");
		}
		else
		{
			Target = TEXT("MakeRejectedAssignmentTemporary(Input)");
		}
		AppendGeneratedAsLine(Source,
			FString::Printf(TEXT("\t%s; // ASSIGNMENT_CAUSE"),
				*AssignmentExpression(*OperationTypeCase.Operation, Target, TEXT("Source"))));
		AppendGeneratedAsLine(Source, TEXT("\treturn 0;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static FString BuildTypeRejectionSource(asIScriptEngine& Engine,
		const FOperationTypeCase& OperationTypeCase,
		const FAssignmentCategoryCase& CategoryCase)
	{
		using namespace AngelscriptNativeTestSupport;
		const FString Type = CanonicalType(*OperationTypeCase.Type, Engine);
		FString Source;
		if (CategoryCase.Category == EAssignmentCategory::Field)
		{
			AppendGeneratedAsLine(Source, TEXT("struct FRejectedAssignmentField"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s Value;"), *Type));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}

		const bool bAlias = CategoryCase.Category == EAssignmentCategory::Alias;
		AppendGeneratedAsLine(Source,
			bAlias ? FString::Printf(
						 TEXT("int RejectAssignmentType(%s& inout Value, %s Source)"), *Type, *Type)
				   : FString::Printf(
						 TEXT("int RejectAssignmentType(%s Input, %s Source)"), *Type, *Type));
		AppendGeneratedAsLine(Source, TEXT("{"));
		switch (CategoryCase.Category)
		{
		case EAssignmentCategory::Local:
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s Value = Input;"), *Type));
			break;
		case EAssignmentCategory::Field:
			AppendGeneratedAsLine(Source, TEXT("\tFRejectedAssignmentField Owner;"));
			AppendGeneratedAsLine(Source, TEXT("\tOwner.Value = Input;"));
			break;
		case EAssignmentCategory::Property:
			AppendGeneratedAsLine(Source,
				FString::Printf(TEXT("\t%s Owner = %s();"),
					*PropertyTypeName(*OperationTypeCase.Type),
					*PropertyFactoryName(*OperationTypeCase.Type)));
			break;
		case EAssignmentCategory::Alias:
			break;
		}
		const FString Target = bAlias ? TEXT("Value") : TargetExpression(CategoryCase);
		AppendGeneratedAsLine(Source,
			FString::Printf(TEXT("\t%s; // ASSIGNMENT_CAUSE"),
				*AssignmentExpression(*OperationTypeCase.Operation, Target, TEXT("Source"))));
		AppendGeneratedAsLine(Source, TEXT("\treturn 0;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static FString BuildRecoverySource()
	{
		using namespace AngelscriptNativeTestSupport;
		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int RecoverAssignment()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 613;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
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

	static asIScriptModule* CompileLegalSource(FNativeTestEngine& Engine,
		FAutomationTestBase& Test,
		const FNativeTypeCase& TypeCase,
		const FAssignmentCategoryCase& CategoryCase,
		const FString& ModuleName)
	{
		using namespace AngelscriptNativeTestSupport;
		const FString SourceId = MakeNativeCaseId("LANG-OP-ASSIGNMENT-SOURCE",
			{ANSI_TO_TCHAR(CategoryCase.CatalogName), ANSI_TO_TCHAR(TypeCase.CatalogName)});
		const FString Source = BuildLegalSource(*Engine.Get(), TypeCase, CategoryCase);
		int32 BuildResult = asERROR;
		asIScriptModule* const Module =
			CompileReportedSource(Engine, Test, SourceId, ModuleName, Source, BuildResult);
		FNoDiscardAsserter Assert(Test);
		const FString Description = FString::Printf(
			TEXT("[%s] legal assignment source should compile. Build=%d Messages={%s}"),
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

	static uint64 FloatBits(const float Value)
	{
		uint32 Bits = 0;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		return Bits;
	}

	static uint64 FloatBits(const double Value)
	{
		uint64 Bits = 0;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		return Bits;
	}

	static uint64 InitialBits(const FNativeTypeCase& TypeCase)
	{
		if (TypeCase.Category == ENativeValueCategory::Boolean)
		{
			return 0;
		}
		if (TypeCase.Accessor == ENativeScalarAccessor::Float)
		{
			return FloatBits(4.0f);
		}
		if (TypeCase.Accessor == ENativeScalarAccessor::Double)
		{
			return FloatBits(4.0);
		}
		return 4;
	}

	static uint64 SourceBits(const FNativeTypeCase& TypeCase)
	{
		if (TypeCase.Category == ENativeValueCategory::Boolean)
		{
			return 1;
		}
		if (TypeCase.Accessor == ENativeScalarAccessor::Float)
		{
			return FloatBits(2.0f);
		}
		if (TypeCase.Accessor == ENativeScalarAccessor::Double)
		{
			return FloatBits(2.0);
		}
		return 2;
	}

	static uint64 ApplyIntegralOperation(
		const FAssignmentOperationCase& OperationCase, const uint64 Initial, const uint64 Source)
	{
		switch (OperationCase.Operation)
		{
		case EAssignmentOperation::Assign:
			return Source;
		case EAssignmentOperation::Add:
			return Initial + Source;
		case EAssignmentOperation::Subtract:
			return Initial - Source;
		case EAssignmentOperation::Multiply:
			return Initial * Source;
		case EAssignmentOperation::Divide:
			return Initial / Source;
		case EAssignmentOperation::Modulo:
			return Initial % Source;
		case EAssignmentOperation::Power:
			return 16;
		case EAssignmentOperation::BitAnd:
			return Initial & Source;
		case EAssignmentOperation::BitOr:
			return Initial | Source;
		case EAssignmentOperation::BitXor:
			return Initial ^ Source;
		case EAssignmentOperation::ShiftLeft:
			return Initial << Source;
		case EAssignmentOperation::ShiftRightLogical:
		case EAssignmentOperation::ShiftRightArithmetic:
			return Initial >> Source;
		default:
			return 0;
		}
	}

	static uint64 ExpectedBits(const FOperationTypeCase& OperationTypeCase)
	{
		const FNativeTypeCase& TypeCase = *OperationTypeCase.Type;
		const FAssignmentOperationCase& OperationCase = *OperationTypeCase.Operation;
		if (TypeCase.Category == ENativeValueCategory::Boolean)
		{
			return SourceBits(TypeCase);
		}
		if (TypeCase.Accessor == ENativeScalarAccessor::Float)
		{
			const float Initial = 4.0f;
			const float Source = 2.0f;
			switch (OperationCase.Operation)
			{
			case EAssignmentOperation::Assign:
				return FloatBits(Source);
			case EAssignmentOperation::Add:
				return FloatBits(Initial + Source);
			case EAssignmentOperation::Subtract:
				return FloatBits(Initial - Source);
			case EAssignmentOperation::Multiply:
				return FloatBits(Initial * Source);
			case EAssignmentOperation::Divide:
				return FloatBits(Initial / Source);
			case EAssignmentOperation::Modulo:
				return FloatBits(0.0f);
			case EAssignmentOperation::Power:
				return FloatBits(16.0f);
			default:
				return 0;
			}
		}
		if (TypeCase.Accessor == ENativeScalarAccessor::Double)
		{
			const double Initial = 4.0;
			const double Source = 2.0;
			switch (OperationCase.Operation)
			{
			case EAssignmentOperation::Assign:
				return FloatBits(Source);
			case EAssignmentOperation::Add:
				return FloatBits(Initial + Source);
			case EAssignmentOperation::Subtract:
				return FloatBits(Initial - Source);
			case EAssignmentOperation::Multiply:
				return FloatBits(Initial * Source);
			case EAssignmentOperation::Divide:
				return FloatBits(Initial / Source);
			case EAssignmentOperation::Modulo:
				return FloatBits(0.0);
			case EAssignmentOperation::Power:
				return FloatBits(16.0);
			default:
				return 0;
			}
		}
		return ApplyIntegralOperation(OperationCase, InitialBits(TypeCase), SourceBits(TypeCase));
	}

	static int SetArgument(asIScriptContext& Context,
		const asUINT ArgumentIndex,
		const FNativeTypeCase& TypeCase,
		const uint64 Bits)
	{
		switch (TypeCase.Accessor)
		{
		case ENativeScalarAccessor::Byte:
			return Context.SetArgByte(ArgumentIndex, static_cast<asBYTE>(Bits));
		case ENativeScalarAccessor::Word:
			return Context.SetArgWord(ArgumentIndex, static_cast<asWORD>(Bits));
		case ENativeScalarAccessor::DWord:
			return Context.SetArgDWord(ArgumentIndex, static_cast<asDWORD>(Bits));
		case ENativeScalarAccessor::QWord:
			return Context.SetArgQWord(ArgumentIndex, static_cast<asQWORD>(Bits));
		case ENativeScalarAccessor::Float:
		{
			const uint32 NarrowBits = static_cast<uint32>(Bits);
			float Value = 0.0f;
			FMemory::Memcpy(&Value, &NarrowBits, sizeof(Value));
			return Context.SetArgFloat(ArgumentIndex, Value);
		}
		case ENativeScalarAccessor::Double:
		{
			double Value = 0.0;
			FMemory::Memcpy(&Value, &Bits, sizeof(Value));
			return Context.SetArgDouble(ArgumentIndex, Value);
		}
		default:
			return asINVALID_TYPE;
		}
	}

	static uint64 ReadReturnBits(asIScriptContext& Context, const FNativeTypeCase& TypeCase)
	{
		switch (TypeCase.Accessor)
		{
		case ENativeScalarAccessor::Byte:
			return Context.GetReturnByte();
		case ENativeScalarAccessor::Word:
			return Context.GetReturnWord();
		case ENativeScalarAccessor::DWord:
			return Context.GetReturnDWord();
		case ENativeScalarAccessor::QWord:
			return Context.GetReturnQWord();
		case ENativeScalarAccessor::Float:
			return FloatBits(Context.GetReturnFloat());
		case ENativeScalarAccessor::Double:
			return FloatBits(Context.GetReturnDouble());
		default:
			return 0;
		}
	}

	static asIScriptFunction* FindFunction(asIScriptEngine& Engine,
		asIScriptModule& Module,
		const FString& Name,
		const FNativeTypeCase& TypeCase,
		const asUINT ParameterCount)
	{
		const int TypeId = Engine.GetTypeIdByDecl(TCHAR_TO_UTF8(*CanonicalType(TypeCase, Engine)));
		asIScriptFunction* Match = nullptr;
		for (asUINT FunctionIndex = 0; FunctionIndex < Module.GetFunctionCount(); ++FunctionIndex)
		{
			asIScriptFunction* const Candidate = Module.GetFunctionByIndex(FunctionIndex);
			if (Candidate == nullptr || FString(UTF8_TO_TCHAR(Candidate->GetName())) != Name ||
				Candidate->GetReturnTypeId() != TypeId ||
				Candidate->GetParamCount() != ParameterCount)
			{
				continue;
			}
			int FirstTypeId = asINVALID_TYPE;
			if (Candidate->GetParam(0, &FirstTypeId) < 0 || FirstTypeId != TypeId)
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

	static asIScriptFunction* FindEntry(asIScriptEngine& Engine,
		asIScriptModule& Module,
		const FOperationTypeCase& OperationTypeCase)
	{
		return FindFunction(
			Engine, Module, EntryName(*OperationTypeCase.Operation), *OperationTypeCase.Type, 2);
	}

	static bool VerifyPropertyMetadata(FAutomationTestBase& Test,
		asIScriptEngine& Engine,
		const FNativeCaseContext& Case,
		const FNativeTypeCase& TypeCase)
	{
		FNoDiscardAsserter Assert(Test);
		const FString Type = CanonicalType(TypeCase, Engine);
		const FTCHARToUTF8 ObjectTypeUtf8(*PropertyTypeName(TypeCase));
		asITypeInfo* const Owner = Engine.GetTypeInfoByDecl(ObjectTypeUtf8.Get());
		if (!Assert.IsNotNull(
				Owner, *Case.Describe(TEXT("assignment property owner should resolve exactly"))))
		{
			return false;
		}
		const FString GetterDeclaration = FString::Printf(TEXT("%s get_Value() const"), *Type);
		const FString SetterDeclaration = FString::Printf(TEXT("void set_Value(%s)"), *Type);
		const FTCHARToUTF8 GetterUtf8(*GetterDeclaration);
		const FTCHARToUTF8 SetterUtf8(*SetterDeclaration);
		asIScriptFunction* const Getter = Owner->GetMethodByDecl(GetterUtf8.Get());
		asIScriptFunction* const Setter = Owner->GetMethodByDecl(SetterUtf8.Get());
		asIScriptFunction* ResolvedGetter = Getter;
		asIScriptFunction* ResolvedSetter = Setter;
		if (ResolvedGetter == nullptr)
		{
			ResolvedGetter = Owner->GetMethodByName("get_Value");
		}
		if (ResolvedSetter == nullptr)
		{
			ResolvedSetter = Owner->GetMethodByName("set_Value");
		}
		const bool bPassed = Assert.IsNotNull(
				   ResolvedGetter, *Case.Describe(TEXT("assignment property getter should resolve"))) &&
			   Assert.IsNotNull(
				   ResolvedSetter, *Case.Describe(TEXT("assignment property setter should resolve")));
		if (bPassed && (!ResolvedGetter->IsProperty() || !ResolvedSetter->IsProperty()))
		{
			Test.AddInfo(FString::Printf(
				TEXT("[AS-FORK-LIMITATION] Id=%s native property decorator is unavailable; explicit accessors retained"),
				*Case.GetId()));
		}
		return bPassed;
	}

	static bool VerifyLegalMetadata(FAutomationTestBase& Test,
		asIScriptEngine& Engine,
		asIScriptModule& Module,
		asIScriptFunction& Entry,
		const FNativeCaseContext& Case,
		const FOperationTypeCase& OperationTypeCase,
		const FAssignmentCategoryCase& CategoryCase)
	{
		FNoDiscardAsserter Assert(Test);
		const FNativeTypeCase& TypeCase = *OperationTypeCase.Type;
		const int TypeId = Engine.GetTypeIdByDecl(TCHAR_TO_UTF8(*CanonicalType(TypeCase, Engine)));
		for (asUINT ParameterIndex = 0; ParameterIndex < 2; ++ParameterIndex)
		{
			int ParameterTypeId = asINVALID_TYPE;
			asDWORD ParameterFlags = asTM_NONE;
			const char* ParameterName = nullptr;
			const int ParamResult = Entry.GetParam(
				ParameterIndex, &ParameterTypeId, &ParameterFlags, &ParameterName);
			if (!Assert.AreEqual(asSUCCESS,
					ParamResult,
					*Case.Describe(
						TEXT("assignment entry parameter metadata should be readable"))) ||
				!Assert.AreEqual(TypeId,
					ParameterTypeId,
					*Case.Describe(TEXT("assignment entry should retain exact parameter type"))) ||
				!Assert.AreEqual(static_cast<asDWORD>(asTM_CONST),
					ParameterFlags,
					*Case.Describe(TEXT("assignment entry should retain fork const-normalized value metadata"))))
			{
				return false;
			}
			const FString ExpectedName = ParameterIndex == 0 ? TEXT("Input") : TEXT("Source");
			if (!Assert.AreEqual(ExpectedName,
					FString(UTF8_TO_TCHAR(ParameterName != nullptr ? ParameterName : "")),
					*Case.Describe(TEXT("assignment entry should retain parameter name"))))
			{
				return false;
			}
		}
		if (!Assert.AreEqual(TypeId,
				Entry.GetReturnTypeId(),
				*Case.Describe(TEXT("assignment expression should retain exact result type"))))
		{
			return false;
		}

		if (CategoryCase.Category == EAssignmentCategory::Field)
		{
			asITypeInfo* const Owner = Module.GetTypeInfoByName("FAssignmentFieldOwner");
			if (!Assert.IsNotNull(
					Owner, *Case.Describe(TEXT("assignment field owner should publish metadata"))))
			{
				return false;
			}
			const char* PropertyName = nullptr;
			int PropertyTypeId = asINVALID_TYPE;
			return Assert.AreEqual(asSUCCESS,
					   Owner->GetProperty(0, &PropertyName, &PropertyTypeId),
					   *Case.Describe(TEXT("assignment field metadata should be readable"))) &&
				   Assert.AreEqual(TypeId,
					   PropertyTypeId,
					   *Case.Describe(TEXT("assignment field should retain exact type"))) &&
				   Assert.AreEqual(FString(TEXT("Value")),
					   FString(UTF8_TO_TCHAR(PropertyName != nullptr ? PropertyName : "")),
					   *Case.Describe(TEXT("assignment field should retain exact name")));
		}
		if (CategoryCase.Category == EAssignmentCategory::Property)
		{
			return VerifyPropertyMetadata(Test, Engine, Case, TypeCase);
		}
		if (CategoryCase.Category == EAssignmentCategory::Alias)
		{
			asIScriptFunction* const Alias =
				FindFunction(Engine, Module, AliasName(*OperationTypeCase.Operation), TypeCase, 2);
			if (!Assert.IsNotNull(
					Alias, *Case.Describe(TEXT("assignment alias helper should resolve exactly"))))
			{
				return false;
			}
			int AliasTypeId = asINVALID_TYPE;
			asDWORD AliasFlags = asTM_NONE;
			return Assert.AreEqual(asSUCCESS,
					   Alias->GetParam(0, &AliasTypeId, &AliasFlags),
					   *Case.Describe(TEXT("assignment alias metadata should be readable"))) &&
				   Assert.AreEqual(TypeId,
					   AliasTypeId,
					   *Case.Describe(TEXT("assignment alias should retain exact type"))) &&
				   Assert.AreEqual(static_cast<asDWORD>(asTM_INOUTREF),
					   AliasFlags,
					   *Case.Describe(TEXT("assignment alias should retain inout modifier")));
		}

		for (asUINT VariableIndex = 0; VariableIndex < Entry.GetVarCount(); ++VariableIndex)
		{
			const char* Name = nullptr;
			int LocalTypeId = asINVALID_TYPE;
			if (Entry.GetVar(VariableIndex, &Name, &LocalTypeId) >= 0 && Name != nullptr &&
				FCStringAnsi::Strcmp(Name, "Value") == 0)
			{
				return Assert.AreEqual(TypeId,
					LocalTypeId,
					*Case.Describe(TEXT("assignment local should retain exact type")));
			}
		}
		return Assert.IsTrue(
			false, *Case.Describe(TEXT("assignment local should publish named lvalue")));
	}

	static bool ExecuteLegalCase(FAutomationTestBase& Test,
		asIScriptEngine& Engine,
		asIScriptModule& Module,
		asIScriptContext& Context,
		const FNativeCaseContext& Case,
		const FOperationTypeCase& OperationTypeCase,
		const FAssignmentCategoryCase& CategoryCase,
		FAssignmentState& State)
	{
		FNoDiscardAsserter Assert(Test);
		asIScriptFunction* const Entry = FindEntry(Engine, Module, OperationTypeCase);
		if (!Assert.IsNotNull(
				Entry, *Case.Describe(TEXT("assignment entry should resolve exactly"))) ||
			!VerifyLegalMetadata(
				Test, Engine, Module, *Entry, Case, OperationTypeCase, CategoryCase))
		{
			return false;
		}
		const FNativeTypeCase& TypeCase = *OperationTypeCase.Type;
		const uint64 Initial = InitialBits(TypeCase);
		const uint64 Source = SourceBits(TypeCase);
		const uint64 Expected = ExpectedBits(OperationTypeCase);
		State.ResetRuntime(Initial);
		bool bPassed = Assert.AreEqual(asSUCCESS,
			Context.Prepare(Entry),
			*Case.Describe(TEXT("assignment entry should prepare")));
		bPassed &= Assert.AreEqual(asSUCCESS,
			SetArgument(Context, 0, TypeCase, Initial),
			*Case.Describe(TEXT("assignment input should bind exact ABI")));
		bPassed &= Assert.AreEqual(asSUCCESS,
			SetArgument(Context, 1, TypeCase, Source),
			*Case.Describe(TEXT("assignment source should bind exact ABI")));
		const int ExecuteResult = Context.Execute();
		const uint64 ReturnBits = ReadReturnBits(Context, TypeCase);
		bPassed &= Assert.AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
			ExecuteResult,
			*Case.Describe(TEXT("assignment entry should execute")));
		bPassed &= Assert.AreEqual(Expected,
			ReturnBits,
			*Case.DescribeResult(Entry->GetDeclaration(),
				FString::Printf(TEXT("bits=0x%016llX"), Expected),
				FString::Printf(TEXT("bits=0x%016llX"), ReturnBits)));
		bPassed &= Assert.AreEqual(1,
			State.ObserverCalls,
			*Case.Describe(TEXT("assignment should report result and final target exactly once")));
		bPassed &= Assert.AreEqual(Expected,
			State.ObservedResultBits,
			*Case.Describe(TEXT("assignment observer should receive exact expression result")));
		bPassed &= Assert.AreEqual(Expected,
			State.ObservedFinalBits,
			*Case.Describe(TEXT("assignment observer should receive exact final target")));
		bPassed &= Assert.AreEqual(asSUCCESS,
			Context.Unprepare(),
			*Case.Describe(TEXT("assignment context should unprepare")));

		const bool bProperty = CategoryCase.Category == EAssignmentCategory::Property;
		const bool bCompound =
			OperationTypeCase.Operation->Operation != EAssignmentOperation::Assign;
		bPassed &= Assert.AreEqual(bProperty ? (bCompound ? 2 : 1) : 0,
			State.GetterCalls,
			*Case.Describe(TEXT("assignment property should perform exact getter count")));
		bPassed &= Assert.AreEqual(bProperty ? 1 : 0,
			State.SetterCalls,
			*Case.Describe(TEXT("assignment property should perform exactly one write")));
		bPassed &= Assert.AreEqual(bProperty ? 1 : 0,
			State.FactoryCalls,
			*Case.Describe(TEXT("assignment property should create one receiver")));
		bPassed &= Assert.AreEqual(bProperty ? 1 : 0,
			State.DestroyedObjects,
			*Case.Describe(TEXT("assignment property should destroy one receiver")));
		bPassed &= Assert.AreEqual(0,
			State.LiveObjects,
			*Case.Describe(TEXT("assignment should retain no property receiver")));
		bPassed &= Assert.AreEqual(State.FactoryCalls + State.AddRefCalls,
			State.ReleaseCalls,
			*Case.Describe(TEXT("assignment property references should balance")));
		if (bProperty)
		{
			bPassed &= Assert.AreEqual(Expected,
				State.ValueBits,
				*Case.Describe(TEXT("assignment property should retain exact final bits")));
		}
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

	static bool HasFunctionNamed(asIScriptModule& Module, const ANSICHAR* Name)
	{
		for (asUINT FunctionIndex = 0; FunctionIndex < Module.GetFunctionCount(); ++FunctionIndex)
		{
			asIScriptFunction* const Function = Module.GetFunctionByIndex(FunctionIndex);
			if (Function != nullptr && FCStringAnsi::Strcmp(Function->GetName(), Name) == 0)
			{
				return true;
			}
		}
		return false;
	}

	static bool ExecuteRecovery(FAutomationTestBase& Test,
		asIScriptEngine& Engine,
		asIScriptModule& Module,
		const FNativeCaseContext& Case)
	{
		using namespace AngelscriptNativeTestSupport;
		FNoDiscardAsserter Assert(Test);
		asIScriptFunction* const Recovery =
			GetNativeFunctionByExactDecl(&Module, "int RecoverAssignment()");
		if (!Assert.IsNotNull(
				Recovery, *Case.Describe(TEXT("assignment recovery should resolve exactly"))))
		{
			return false;
		}
		asIScriptContext* const Context = Engine.CreateContext();
		if (!Assert.IsNotNull(
				Context, *Case.Describe(TEXT("assignment recovery should create context"))))
		{
			return false;
		}
		ON_SCOPE_EXIT
		{
			Context->Release();
		};
		const int ExecuteResult = PrepareAndExecute(Context, Recovery);
		return Assert.AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
				   ExecuteResult,
				   *Case.Describe(TEXT("assignment recovery should execute"))) &&
			   Assert.AreEqual(613,
				   static_cast<int32>(Context->GetReturnDWord()),
				   *Case.Describe(TEXT("assignment recovery should return exact marker")));
	}

	static bool VerifyRejectedSource(FAutomationTestBase& Test,
		FNativeTestEngine& Engine,
		const FNativeCaseContext& Case,
		const FString& ModuleName,
		const FString& Source,
		const ANSICHAR* InvalidFunctionName,
		FAssignmentState& State)
	{
		FNoDiscardAsserter Assert(Test);
		State.ResetRuntime(0);
		int32 BuildResult = asERROR;
		asIScriptModule* Module =
			CompileReportedSource(Engine, Test, Case.GetId(), ModuleName, Source, BuildResult);
		bool bPassed = Assert.IsTrue(BuildResult < 0,
			*Case.DescribeResult("<assignment rejection build>",
				TEXT("negative build result"),
				FString::Printf(TEXT("%d Messages={%s}"), BuildResult, *Engine.GetMessagesText())));
		const TArray<AngelscriptNativeTestSupport::FNativeMessageEntry> Errors =
			ErrorMessages(Engine.GetMessages());
		bPassed &= Assert.IsTrue(Errors.Num() >= 1,
			*Case.DescribeResult("<assignment rejection diagnostic>",
				TEXT("at least one causal error"),
				Engine.GetMessagesText()));
		if (Errors.Num() > 1)
		{
			Test.AddInfo(FString::Printf(
				TEXT("[AS-FORK-LIMITATION] Id=%s unsupported assignment produced %d diagnostic entries"),
				*Case.GetId(), Errors.Num()));
		}
		const int32 ExpectedLine = LastSourceLineContaining(Source, TEXT("ASSIGNMENT_CAUSE"));
		bPassed &= Assert.IsTrue(ExpectedLine > 0,
			*Case.Describe(TEXT("assignment rejection source should retain causal marker")));
		if (Errors.Num() >= 1)
		{
			bPassed &= Assert.AreEqual(ExpectedLine,
				Errors[0].Row,
				*Case.Describe(TEXT("assignment rejection should own exact operator line")));
		}
		bPassed &= Assert.AreEqual(0,
			State.TemporaryProducerCalls,
			*Case.Describe(TEXT("rejected assignment should execute no temporary producer")));
		bPassed &= Assert.AreEqual(0,
			State.FactoryCalls,
			*Case.Describe(TEXT("rejected assignment should execute no property factory")));
		bPassed &= Assert.AreEqual(0,
			State.GetterCalls,
			*Case.Describe(TEXT("rejected assignment should execute no property getter")));
		bPassed &= Assert.AreEqual(0,
			State.SetterCalls,
			*Case.Describe(TEXT("rejected assignment should execute no property setter")));
		bPassed &= Assert.AreEqual(0,
			State.ObserverCalls,
			*Case.Describe(TEXT("rejected assignment should execute no result observer")));
		if (Module != nullptr)
		{
			bPassed &= Assert.IsTrue(!HasFunctionNamed(*Module, InvalidFunctionName),
				*Case.Describe(TEXT("rejected assignment should publish no callable entry")));
		}
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		if (Engine.Get()->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS) != nullptr)
		{
			Engine.Get()->DiscardModule(ModuleNameUtf8.Get());
		}

		const FString RecoverySource = BuildRecoverySource();
		State.ResetRuntime(0);
		int32 RecoveryBuildResult = asERROR;
		Module = CompileReportedSource(Engine,
			Test,
			Case.GetId() + TEXT(".recovery"),
			ModuleName,
			RecoverySource,
			RecoveryBuildResult);
		bPassed &= Assert.IsTrue(RecoveryBuildResult >= 0,
			*Case.DescribeResult("<assignment recovery build>",
				TEXT("successful same-name build"),
				Engine.GetMessagesText()));
		bPassed &= Assert.IsNotNull(
			Module, *Case.Describe(TEXT("assignment recovery should publish module")));
		if (RecoveryBuildResult >= 0 && Module != nullptr)
		{
			bPassed &= ExecuteRecovery(Test, *Engine.Get(), *Module, Case);
		}
		bPassed &= Assert.AreEqual(asSUCCESS,
			Engine.Get()->DiscardModule(ModuleNameUtf8.Get()),
			*Case.Describe(TEXT("assignment recovery module should discard")));
		bPassed &= Assert.IsNull(Engine.Get()->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
			*Case.Describe(TEXT("assignment recovery should leave no stale module")));
		return bPassed;
	}

	static TArray<const FNativeTypeCase*> CollectPrimitiveTypes()
	{
		TArray<const FNativeTypeCase*> Result;
		for (const FNativeTypeCase& TypeCase : AngelscriptNativeTestSupport::NativeTypeCases)
		{
			if (IsPrimitiveType(TypeCase))
			{
				Result.Add(&TypeCase);
			}
		}
		return Result;
	}

	static TArray<FOperationTypeCase> CollectOperationTypeCases(
		const TArray<const FNativeTypeCase*>& PrimitiveTypes, const bool bSupported)
	{
		TArray<FOperationTypeCase> Result;
		for (const FAssignmentOperationCase& OperationCase : OperationCases)
		{
			for (const FNativeTypeCase* TypeCase : PrimitiveTypes)
			{
				if (SupportsOperation(OperationCase, *TypeCase) == bSupported)
				{
					Result.Add({&OperationCase, TypeCase});
				}
			}
		}
		return Result;
	}

public:

	TEST_METHOD(TypeRejections)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("LANG-OP-ASSIGNMENT-TYPE-REJECTION",
			ENativeEvidence::Compile | ENativeEvidence::Diagnostic | ENativeEvidence::Metadata |
				ENativeEvidence::Isolation);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		ASSERT_THAT(
			IsNotNull(Engine.Get(), TEXT("assignment type rejection should create engine")));
		if (Engine.Get() == nullptr)
		{
			return;
		}
		const TArray<const FNativeTypeCase*> PrimitiveTypes = CollectPrimitiveTypes();
		FAssignmentState State;
		ASSERT_THAT(IsTrue(RegisterFixtures(*Engine.Get(), State, PrimitiveTypes),
			TEXT("assignment type rejection should register writable fixtures")));
		const TArray<FOperationTypeCase> UnsupportedOperationTypes =
			CollectOperationTypeCases(PrimitiveTypes, false);
		ASSERT_THAT(AreEqual(32,
			UnsupportedOperationTypes.Num(),
			TEXT("assignment type rejection should retain all 32 unsupported pairs")));

		TArray<FString> ConstructedIds;
		TSet<FString> UniqueIds;
		bool bAllCasesPassed = true;
		for (const FAssignmentCategoryCase& CategoryCase : CategoryCases)
		{
			for (const FOperationTypeCase& OperationTypeCase : UnsupportedOperationTypes)
			{
				const FString OperationTypeName = OperationTypeCase.GetCatalogName();
				const FNativeCaseContext Case(MakeNativeCaseId("LANG-OP-ASSIGNMENT-TYPE-REJECTION",
					{ANSI_TO_TCHAR(CategoryCase.CatalogName), *OperationTypeName}));
				ConstructedIds.Add(Case.GetId());
				const bool bUniqueCaseId = !UniqueIds.Contains(Case.GetId());
				UniqueIds.Add(Case.GetId());
				ASSERT_THAT(IsTrue(bUniqueCaseId,
					*Case.Describe(TEXT("assignment type rejection ID should be unique"))));
				if (CategoryCase.Category == EAssignmentCategory::Property)
				{
					bAllCasesPassed &= VerifyPropertyMetadata(
						*TestRunner, *Engine.Get(), Case, *OperationTypeCase.Type);
				}
				const FString ModuleName =
					FString::Printf(TEXT("ASNativeAssignmentTypeReject_%hs_%s"),
						CategoryCase.CatalogName,
						*OperationTypeName);
				const FString Source =
					BuildTypeRejectionSource(*Engine.Get(), OperationTypeCase, CategoryCase);
				bAllCasesPassed &= VerifyRejectedSource(
					*TestRunner, Engine, Case, ModuleName, Source, "RejectAssignmentType", State);
			}
		}
		ASSERT_THAT(AreEqual(128,
			ConstructedIds.Num(),
			TEXT("assignment type rejection should construct all 128 IDs")));
		ASSERT_THAT(AreEqual(ConstructedIds.Num(),
			UniqueIds.Num(),
			TEXT("assignment type rejection should construct no duplicate IDs")));
		ASSERT_THAT(IsTrue(bAllCasesPassed,
			TEXT("every unsupported writable assignment should own one diagnostic, property "
				 "metadata, no execution, same-name recovery, and cleanup")));
	}
};

#endif
