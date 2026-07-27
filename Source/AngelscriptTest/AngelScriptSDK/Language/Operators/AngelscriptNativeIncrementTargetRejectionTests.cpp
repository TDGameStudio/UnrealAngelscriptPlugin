#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FIncrementTargetRejectionTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Operators.Increment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using ENativeScalarAccessor = AngelscriptNativeTestSupport::ENativeScalarAccessor;
	using ENativeValueCategory = AngelscriptNativeTestSupport::ENativeValueCategory;
	using FNativeCaseContext = AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeTestEngine = AngelscriptNativeTestSupport::FNativeTestEngine;
	using FNativeTypeCase = AngelscriptNativeTestSupport::FNativeTypeCase;

	inline static constexpr asPWORD IncrementStateSlot =
		static_cast<asPWORD>(0x4E4154494E435253ull);

	enum class EIncrementOperator : uint8
	{
		PreIncrement,
		PostIncrement,
		PreDecrement,
		PostDecrement,
	};

	enum class EWritableCategory : uint8
	{
		Local,
		Field,
		Property,
		Alias,
	};

	enum class EObservation : uint8
	{
		Before,
		ExpressionResult,
		After,
	};

	enum class ERejectedTarget : uint8
	{
		Const,
		Temporary,
	};

	struct FOperatorCase
	{
		const ANSICHAR* CatalogName;
		const TCHAR* Token;
		const TCHAR* FunctionSuffix;
		EIncrementOperator Operator;
	};

	struct FCategoryCase
	{
		const ANSICHAR* CatalogName;
		EWritableCategory Category;
	};

	struct FObservationCase
	{
		const ANSICHAR* CatalogName;
		const TCHAR* FunctionSuffix;
		EObservation Observation;
	};

	struct FRejectedTargetCase
	{
		const ANSICHAR* CatalogName;
		ERejectedTarget Target;
	};

	struct FIncrementState
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
			RejectedProducerCalls = 0;
		}

		uint64 ValueBits = 0;
		int32 GetterCalls = 0;
		int32 SetterCalls = 0;
		int32 FactoryCalls = 0;
		int32 AddRefCalls = 0;
		int32 ReleaseCalls = 0;
		int32 DestroyedObjects = 0;
		int32 LiveObjects = 0;
		int32 RejectedProducerCalls = 0;
		TMap<int32, ENativeScalarAccessor> GetterAccessorByFunctionId;
		TMap<int32, ENativeScalarAccessor> SetterAccessorByFunctionId;
	};

	class FIncrementPropertyObject
	{
	public:
		explicit FIncrementPropertyObject(FIncrementState& InState) : State(InState)
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
		FIncrementState& State;
		int32 ReferenceCount = 1;
	};

	inline static constexpr FOperatorCase OperatorCases[] = {
		{"pre_increment", TEXT("++Value"), TEXT("PreIncrement"), EIncrementOperator::PreIncrement},
		{"post_increment",
			TEXT("Value++"),
			TEXT("PostIncrement"),
			EIncrementOperator::PostIncrement},
		{"pre_decrement", TEXT("--Value"), TEXT("PreDecrement"), EIncrementOperator::PreDecrement},
		{"post_decrement",
			TEXT("Value--"),
			TEXT("PostDecrement"),
			EIncrementOperator::PostDecrement},
	};

	inline static constexpr FCategoryCase CategoryCases[] = {
		{"local", EWritableCategory::Local},
		{"field", EWritableCategory::Field},
		{"property", EWritableCategory::Property},
		{"alias", EWritableCategory::Alias},
	};

	inline static constexpr FObservationCase ObservationCases[] = {
		{"before", TEXT("Before"), EObservation::Before},
		{"expression_result", TEXT("ExpressionResult"), EObservation::ExpressionResult},
		{"after", TEXT("After"), EObservation::After},
	};

	inline static constexpr FRejectedTargetCase RejectedTargetCases[] = {
		{"const_invalid", ERejectedTarget::Const},
		{"temporary_invalid", ERejectedTarget::Temporary},
	};

	static FIncrementState* ActiveState(asIScriptGeneric& Generic)
	{
		return Generic.GetEngine() != nullptr
				   ? static_cast<FIncrementState*>(
						 Generic.GetEngine()->GetUserData(IncrementStateSlot))
				   : nullptr;
	}

	static FIncrementState* ActiveState()
	{
		asIScriptContext* const Context = asGetActiveContext();
		return Context != nullptr && Context->GetEngine() != nullptr
				   ? static_cast<FIncrementState*>(
						 Context->GetEngine()->GetUserData(IncrementStateSlot))
				   : nullptr;
	}

	static void GenericPropertyAddRef(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			if (FIncrementPropertyObject* const Object =
					static_cast<FIncrementPropertyObject*>(Generic->GetObject()))
			{
				Object->AddRef();
			}
		}
	}

	static void GenericPropertyRelease(asIScriptGeneric* Generic)
	{
		if (Generic != nullptr)
		{
			if (FIncrementPropertyObject* const Object =
					static_cast<FIncrementPropertyObject*>(Generic->GetObject()))
			{
				Object->Release();
			}
		}
	}

	static void GenericMakeProperty(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr)
		{
			return;
		}
		FIncrementState* const State = ActiveState(*Generic);
		if (State == nullptr)
		{
			Generic->SetReturnAddress(nullptr);
			return;
		}
		++State->FactoryCalls;
		++State->LiveObjects;
		Generic->SetReturnAddress(new FIncrementPropertyObject(*State));
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

	static uint64 ReadGenericArgument(
		asIScriptGeneric& Generic, const ENativeScalarAccessor Accessor)
	{
		switch (Accessor)
		{
		case ENativeScalarAccessor::Byte:
			return Generic.GetArgByte(0);
		case ENativeScalarAccessor::Word:
			return Generic.GetArgWord(0);
		case ENativeScalarAccessor::DWord:
			return Generic.GetArgDWord(0);
		case ENativeScalarAccessor::QWord:
			return Generic.GetArgQWord(0);
		case ENativeScalarAccessor::Float:
		{
			const float Value = Generic.GetArgFloat(0);
			uint32 Bits = 0;
			FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
			return Bits;
		}
		case ENativeScalarAccessor::Double:
		{
			const double Value = Generic.GetArgDouble(0);
			uint64 Bits = 0;
			FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
			return Bits;
		}
		default:
			return 0;
		}
	}

	static void GenericGetProperty(asIScriptGeneric* Generic)
	{
		if (Generic == nullptr || Generic->GetFunction() == nullptr)
		{
			return;
		}
		FIncrementState* const State = ActiveState(*Generic);
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
		FIncrementState* const State = ActiveState(*Generic);
		const ENativeScalarAccessor* const Accessor =
			State != nullptr
				? State->SetterAccessorByFunctionId.Find(Generic->GetFunction()->GetId())
				: nullptr;
		if (State == nullptr || Accessor == nullptr)
		{
			return;
		}
		++State->SetterCalls;
		State->ValueBits = ReadGenericArgument(*Generic, *Accessor);
	}

	static void RecordIncrementRejectedProducer()
	{
		if (FIncrementState* const State = ActiveState())
		{
			++State->RejectedProducerCalls;
		}
	}

	static bool IsNumericType(const FNativeTypeCase& TypeCase)
	{
		return TypeCase.Category == ENativeValueCategory::SignedInteger ||
			   TypeCase.Category == ENativeValueCategory::UnsignedInteger ||
			   TypeCase.Category == ENativeValueCategory::FloatingPoint;
	}

	static bool IsBoolType(const FNativeTypeCase& TypeCase)
	{
		return TypeCase.Category == ENativeValueCategory::Boolean;
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
		return FString::Printf(TEXT("FIncrementProperty_%hs"), TypeCase.CatalogName);
	}

	static FString PropertyFactoryName(const FNativeTypeCase& TypeCase)
	{
		return FString::Printf(TEXT("MakeIncrementProperty_%hs"), TypeCase.CatalogName);
	}

	static bool RegisterPropertyType(
		asIScriptEngine& Engine, FIncrementState& State, const FNativeTypeCase& TypeCase)
	{
		const FString ObjectType = PropertyTypeName(TypeCase);
		const FString ValueType = CanonicalType(TypeCase, Engine);
		const FString GetterDeclaration =
			FString::Printf(TEXT("%s get_Value() const"), *ValueType);
		const FString SetterDeclaration =
			FString::Printf(TEXT("void set_Value(%s InValue)"), *ValueType);
		const FString FactoryDeclaration =
			FString::Printf(TEXT("%s %s()"), *ObjectType, *PropertyFactoryName(TypeCase));
		const FTCHARToUTF8 ObjectTypeUtf8(*ObjectType);
		const FTCHARToUTF8 GetterUtf8(*GetterDeclaration);
		const FTCHARToUTF8 SetterUtf8(*SetterDeclaration);
		const FTCHARToUTF8 FactoryUtf8(*FactoryDeclaration);
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
		if (GetterId < 0 || SetterId < 0 || FactoryId < 0)
		{
			return false;
		}
		State.GetterAccessorByFunctionId.Add(GetterId, TypeCase.Accessor);
		State.SetterAccessorByFunctionId.Add(SetterId, TypeCase.Accessor);
		return true;
	}

	static bool RegisterFixtures(asIScriptEngine& Engine,
		FIncrementState& State,
		const TArray<const FNativeTypeCase*>& NumericTypes,
		const FNativeTypeCase& BoolType)
	{
		Engine.SetUserData(&State, IncrementStateSlot);
		for (const FNativeTypeCase* TypeCase : NumericTypes)
		{
			if (!RegisterPropertyType(Engine, State, *TypeCase))
			{
				return false;
			}
		}
		if (!RegisterPropertyType(Engine, State, BoolType))
		{
			return false;
		}
		const ASAutoCaller::FunctionCaller RecordCaller =
			ASAutoCaller::MakeFunctionCaller(RecordIncrementRejectedProducer);
		return Engine.RegisterGlobalFunction("void RecordIncrementRejectedProducer()",
				   asFUNCTION(RecordIncrementRejectedProducer),
				   asCALL_CDECL,
				   *(asFunctionCaller*)&RecordCaller) >= 0;
	}

	static FString EntryName(
		const FOperatorCase& OperatorCase, const FObservationCase& ObservationCase)
	{
		return FString::Printf(
			TEXT("Observe%s%s"), OperatorCase.FunctionSuffix, ObservationCase.FunctionSuffix);
	}

	static FString ApplyAliasName(const FOperatorCase& OperatorCase)
	{
		return FString::Printf(TEXT("Apply%sAlias"), OperatorCase.FunctionSuffix);
	}

	static FString TargetExpression(const FCategoryCase& CategoryCase)
	{
		switch (CategoryCase.Category)
		{
		case EWritableCategory::Local:
			return TEXT("Value");
		case EWritableCategory::Field:
			return TEXT("Owner.Value");
		case EWritableCategory::Property:
			return TEXT("Owner.Value");
		case EWritableCategory::Alias:
			return TEXT("Value");
		default:
			return TEXT("Value");
		}
	}

	static FString OperatorExpression(const FOperatorCase& OperatorCase, const FString& Target)
	{
		switch (OperatorCase.Operator)
		{
		case EIncrementOperator::PreIncrement:
			return TEXT("++") + Target;
		case EIncrementOperator::PostIncrement:
			return Target + TEXT("++");
		case EIncrementOperator::PreDecrement:
			return TEXT("--") + Target;
		case EIncrementOperator::PostDecrement:
			return Target + TEXT("--");
		default:
			return Target;
		}
	}

	static FString BuildLegalSource(asIScriptEngine& Engine,
		const FNativeTypeCase& TypeCase,
		const FCategoryCase& CategoryCase)
	{
		using namespace AngelscriptNativeTestSupport;
		const FString Type = CanonicalType(TypeCase, Engine);
		FString Source;
		if (CategoryCase.Category == EWritableCategory::Field)
		{
			AppendGeneratedAsLine(Source, TEXT("struct FIncrementFieldOwner"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s Value;"), *Type));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}

		for (const FOperatorCase& OperatorCase : OperatorCases)
		{
			if (CategoryCase.Category == EWritableCategory::Alias)
			{
				AppendGeneratedAsLine(Source,
					FString::Printf(TEXT("%s %s(%s& inout Value)"),
						*Type,
						*ApplyAliasName(OperatorCase),
						*Type));
				AppendGeneratedAsLine(Source, TEXT("{"));
				AppendGeneratedAsLine(Source,
					FString::Printf(
						TEXT("\treturn %s;"), *OperatorExpression(OperatorCase, TEXT("Value"))));
				AppendGeneratedAsLine(Source, TEXT("}"));
				AppendGeneratedAsLine(Source);
			}

			for (const FObservationCase& ObservationCase : ObservationCases)
			{
				AppendGeneratedAsLine(Source,
					FString::Printf(TEXT("%s %s(%s Input)"),
						*Type,
						*EntryName(OperatorCase, ObservationCase),
						*Type));
				AppendGeneratedAsLine(Source, TEXT("{"));
				switch (CategoryCase.Category)
				{
				case EWritableCategory::Local:
					AppendGeneratedAsLine(
						Source, FString::Printf(TEXT("\t%s Value = Input;"), *Type));
					break;
				case EWritableCategory::Field:
					AppendGeneratedAsLine(Source, TEXT("\tFIncrementFieldOwner Owner;"));
					AppendGeneratedAsLine(Source, TEXT("\tOwner.Value = Input;"));
					break;
				case EWritableCategory::Property:
					AppendGeneratedAsLine(Source,
						FString::Printf(TEXT("\t%s Owner = %s();"),
							*PropertyTypeName(TypeCase),
							*PropertyFactoryName(TypeCase)));
					break;
				case EWritableCategory::Alias:
					AppendGeneratedAsLine(
						Source, FString::Printf(TEXT("\t%s Value = Input;"), *Type));
					break;
				}

				if (CategoryCase.Category == EWritableCategory::Property)
				{
					// The current fork does not expose the native property decorator;
					// exercise the same getter/setter ABI explicitly while preserving
					// pre/post result and before/after observations.
					AppendGeneratedAsLine(Source,
						FString::Printf(TEXT("\t%s Before = Owner.get_Value();"), *Type));
					AppendGeneratedAsLine(Source,
						FString::Printf(TEXT("\t%s Updated = Before;"), *Type));
					const FString UpdatedMutation = OperatorExpression(OperatorCase, TEXT("Updated"));
					AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s;"), *UpdatedMutation));
					AppendGeneratedAsLine(Source, TEXT("\tOwner.set_Value(Updated);"));
					switch (ObservationCase.Observation)
					{
					case EObservation::Before:
						AppendGeneratedAsLine(Source, TEXT("\treturn Before;"));
						break;
					case EObservation::ExpressionResult:
						AppendGeneratedAsLine(Source,
							FString::Printf(TEXT("\treturn %s;"),
								OperatorCase.Operator == EIncrementOperator::PreIncrement
									|| OperatorCase.Operator == EIncrementOperator::PreDecrement
								? TEXT("Updated")
								: TEXT("Before")));
						break;
					case EObservation::After:
						AppendGeneratedAsLine(Source, TEXT("\treturn Owner.get_Value();"));
						break;
					}
					AppendGeneratedAsLine(Source, TEXT("}"));
					AppendGeneratedAsLine(Source);
					continue;
				}

				const FString Target = TargetExpression(CategoryCase);
				const FString Mutation =
					CategoryCase.Category == EWritableCategory::Alias
						? FString::Printf(TEXT("%s(Value)"), *ApplyAliasName(OperatorCase))
						: OperatorExpression(OperatorCase, Target);
				switch (ObservationCase.Observation)
				{
				case EObservation::Before:
					AppendGeneratedAsLine(
						Source, FString::Printf(TEXT("\t%s Before = %s;"), *Type, *Target));
					AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s;"), *Mutation));
					AppendGeneratedAsLine(Source, TEXT("\treturn Before;"));
					break;
				case EObservation::ExpressionResult:
					AppendGeneratedAsLine(
						Source, FString::Printf(TEXT("\t%s Result = %s;"), *Type, *Mutation));
					AppendGeneratedAsLine(Source, TEXT("\treturn Result;"));
					break;
				case EObservation::After:
					AppendGeneratedAsLine(Source, FString::Printf(TEXT("\t%s;"), *Mutation));
					AppendGeneratedAsLine(Source, FString::Printf(TEXT("\treturn %s;"), *Target));
					break;
				}
				AppendGeneratedAsLine(Source, TEXT("}"));
				AppendGeneratedAsLine(Source);
			}
		}
		return Source;
	}

	static FString BuildTargetRejectionSource(asIScriptEngine& Engine,
		const FNativeTypeCase& TypeCase,
		const FOperatorCase& OperatorCase,
		const FRejectedTargetCase& TargetCase)
	{
		using namespace AngelscriptNativeTestSupport;
		const FString Type = CanonicalType(TypeCase, Engine);
		FString Source;
		if (TargetCase.Target == ERejectedTarget::Temporary)
		{
			AppendGeneratedAsLine(Source,
				FString::Printf(TEXT("%s MakeRejectedIncrementTemporary(%s Input)"), *Type, *Type));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tRecordIncrementRejectedProducer();"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Input;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		AppendGeneratedAsLine(
			Source, FString::Printf(TEXT("int RejectIncrementTarget(%s Input)"), *Type));
		AppendGeneratedAsLine(Source, TEXT("{"));
		FString Target;
		if (TargetCase.Target == ERejectedTarget::Const)
		{
			AppendGeneratedAsLine(
				Source, FString::Printf(TEXT("\tconst %s Value = Input;"), *Type));
			Target = TEXT("Value");
		}
		else
		{
			Target = TEXT("MakeRejectedIncrementTemporary(Input)");
		}
		AppendGeneratedAsLine(Source,
			FString::Printf(
				TEXT("\t%s; // INCREMENT_CAUSE"), *OperatorExpression(OperatorCase, Target)));
		AppendGeneratedAsLine(Source, TEXT("\treturn 0;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static FString BuildBoolRejectionSource(
		const FOperatorCase& OperatorCase, const FCategoryCase& CategoryCase)
	{
		using namespace AngelscriptNativeTestSupport;
		FString Source;
		if (CategoryCase.Category == EWritableCategory::Field)
		{
			AppendGeneratedAsLine(Source, TEXT("struct FRejectedBoolIncrementField"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tbool Value;"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		const bool bAlias = CategoryCase.Category == EWritableCategory::Alias;
		AppendGeneratedAsLine(Source,
			bAlias ? TEXT("int RejectIncrementBool(bool& inout Value)")
				   : TEXT("int RejectIncrementBool(bool Input)"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		switch (CategoryCase.Category)
		{
		case EWritableCategory::Local:
			AppendGeneratedAsLine(Source, TEXT("\tbool Value = Input;"));
			AppendGeneratedAsLine(Source,
				FString::Printf(TEXT("\t%s; // INCREMENT_CAUSE"),
					*OperatorExpression(OperatorCase, TEXT("Value"))));
			break;
		case EWritableCategory::Field:
			AppendGeneratedAsLine(Source, TEXT("\tFRejectedBoolIncrementField Owner;"));
			AppendGeneratedAsLine(Source, TEXT("\tOwner.Value = Input;"));
			AppendGeneratedAsLine(Source,
				FString::Printf(TEXT("\t%s; // INCREMENT_CAUSE"),
					*OperatorExpression(OperatorCase, TEXT("Owner.Value"))));
			break;
		case EWritableCategory::Property:
			AppendGeneratedAsLine(
				Source, TEXT("\tFIncrementProperty_bool Owner = MakeIncrementProperty_bool();"));
			AppendGeneratedAsLine(Source,
				FString::Printf(TEXT("\t%s; // INCREMENT_CAUSE"),
					*OperatorExpression(OperatorCase, TEXT("Owner.Value"))));
			break;
		case EWritableCategory::Alias:
			AppendGeneratedAsLine(Source,
				FString::Printf(TEXT("\t%s; // INCREMENT_CAUSE"),
					*OperatorExpression(OperatorCase, TEXT("Value"))));
			break;
		}
		AppendGeneratedAsLine(Source, TEXT("\treturn 0;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		return Source;
	}

	static FString BuildRecoverySource()
	{
		using namespace AngelscriptNativeTestSupport;
		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int RecoverIncrement()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 509;"));
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
		const FCategoryCase& CategoryCase,
		const FString& ModuleName)
	{
		using namespace AngelscriptNativeTestSupport;
		const FString SourceId = MakeNativeCaseId("LANG-OP-INCREMENT-SOURCE",
			{ANSI_TO_TCHAR(TypeCase.CatalogName), ANSI_TO_TCHAR(CategoryCase.CatalogName)});
		const FString Source = BuildLegalSource(*Engine.Get(), TypeCase, CategoryCase);
		int32 BuildResult = asERROR;
		asIScriptModule* const Module =
			CompileReportedSource(Engine, Test, SourceId, ModuleName, Source, BuildResult);
		FNoDiscardAsserter Assert(Test);
		const FString Description =
			FString::Printf(TEXT("[%s] increment source should compile. Build=%d Messages={%s}"),
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

	static asIScriptFunction* FindUnaryFunction(asIScriptEngine& Engine,
		asIScriptModule& Module,
		const FString& Name,
		const FNativeTypeCase& TypeCase,
		const int ReturnTypeId)
	{
		const FString Type = CanonicalType(TypeCase, Engine);
		const int TypeId = Engine.GetTypeIdByDecl(TCHAR_TO_UTF8(*Type));
		asIScriptFunction* Match = nullptr;
		for (asUINT FunctionIndex = 0; FunctionIndex < Module.GetFunctionCount(); ++FunctionIndex)
		{
			asIScriptFunction* const Candidate = Module.GetFunctionByIndex(FunctionIndex);
			if (Candidate == nullptr || FString(UTF8_TO_TCHAR(Candidate->GetName())) != Name ||
				Candidate->GetParamCount() != 1 || Candidate->GetReturnTypeId() != ReturnTypeId)
			{
				continue;
			}
			int ParameterTypeId = asINVALID_TYPE;
			if (Candidate->GetParam(0, &ParameterTypeId) >= 0 && ParameterTypeId == TypeId)
			{
				if (Match != nullptr)
				{
					return nullptr;
				}
				Match = Candidate;
			}
		}
		return Match;
	}

	static asIScriptFunction* FindEntry(asIScriptEngine& Engine,
		asIScriptModule& Module,
		const FNativeTypeCase& TypeCase,
		const FOperatorCase& OperatorCase,
		const FObservationCase& ObservationCase)
	{
		const int TypeId = Engine.GetTypeIdByDecl(TCHAR_TO_UTF8(*CanonicalType(TypeCase, Engine)));
		return FindUnaryFunction(
			Engine, Module, EntryName(OperatorCase, ObservationCase), TypeCase, TypeId);
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
		if (TypeCase.Accessor == ENativeScalarAccessor::Float)
		{
			return FloatBits(12.5f);
		}
		if (TypeCase.Accessor == ENativeScalarAccessor::Double)
		{
			return FloatBits(12.5);
		}
		return 12;
	}

	static uint64 FinalBits(const FNativeTypeCase& TypeCase, const FOperatorCase& OperatorCase)
	{
		const bool bIncrement = OperatorCase.Operator == EIncrementOperator::PreIncrement ||
								OperatorCase.Operator == EIncrementOperator::PostIncrement;
		if (TypeCase.Accessor == ENativeScalarAccessor::Float)
		{
			return FloatBits(bIncrement ? 13.5f : 11.5f);
		}
		if (TypeCase.Accessor == ENativeScalarAccessor::Double)
		{
			return FloatBits(bIncrement ? 13.5 : 11.5);
		}
		return bIncrement ? 13 : 11;
	}

	static uint64 ExpectedObservationBits(const FNativeTypeCase& TypeCase,
		const FOperatorCase& OperatorCase,
		const FObservationCase& ObservationCase)
	{
		if (ObservationCase.Observation == EObservation::Before)
		{
			return InitialBits(TypeCase);
		}
		const bool bPrefix = OperatorCase.Operator == EIncrementOperator::PreIncrement ||
							 OperatorCase.Operator == EIncrementOperator::PreDecrement;
		if (ObservationCase.Observation == EObservation::ExpressionResult)
		{
			return bPrefix ? FinalBits(TypeCase, OperatorCase) : InitialBits(TypeCase);
		}
		return FinalBits(TypeCase, OperatorCase);
	}

	static int SetArgument(asIScriptContext& Context, const FNativeTypeCase& TypeCase)
	{
		const uint64 Bits = InitialBits(TypeCase);
		switch (TypeCase.Accessor)
		{
		case ENativeScalarAccessor::Byte:
			return Context.SetArgByte(0, static_cast<asBYTE>(Bits));
		case ENativeScalarAccessor::Word:
			return Context.SetArgWord(0, static_cast<asWORD>(Bits));
		case ENativeScalarAccessor::DWord:
			return Context.SetArgDWord(0, static_cast<asDWORD>(Bits));
		case ENativeScalarAccessor::QWord:
			return Context.SetArgQWord(0, static_cast<asQWORD>(Bits));
		case ENativeScalarAccessor::Float:
		{
			const uint32 NarrowBits = static_cast<uint32>(Bits);
			float Value = 0.0f;
			FMemory::Memcpy(&Value, &NarrowBits, sizeof(Value));
			return Context.SetArgFloat(0, Value);
		}
		case ENativeScalarAccessor::Double:
		{
			double Value = 0.0;
			FMemory::Memcpy(&Value, &Bits, sizeof(Value));
			return Context.SetArgDouble(0, Value);
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

	static asIScriptFunction* FindAlias(asIScriptEngine& Engine,
		asIScriptModule& Module,
		const FNativeTypeCase& TypeCase,
		const FOperatorCase& OperatorCase)
	{
		const int TypeId = Engine.GetTypeIdByDecl(TCHAR_TO_UTF8(*CanonicalType(TypeCase, Engine)));
		return FindUnaryFunction(Engine, Module, ApplyAliasName(OperatorCase), TypeCase, TypeId);
	}

	static bool VerifyLegalMetadata(FAutomationTestBase& Test,
		asIScriptEngine& Engine,
		asIScriptModule& Module,
		asIScriptFunction& Entry,
		const FNativeCaseContext& Case,
		const FNativeTypeCase& TypeCase,
		const FOperatorCase& OperatorCase,
		const FCategoryCase& CategoryCase)
	{
		FNoDiscardAsserter Assert(Test);
		const FString Type = CanonicalType(TypeCase, Engine);
		const int TypeId = Engine.GetTypeIdByDecl(TCHAR_TO_UTF8(*Type));
		int ParameterTypeId = asINVALID_TYPE;
		asDWORD ParameterFlags = asTM_NONE;
		const char* ParameterName = nullptr;
		if (!Assert.AreEqual(asSUCCESS,
				Entry.GetParam(0, &ParameterTypeId, &ParameterFlags, &ParameterName),
				*Case.Describe(TEXT("increment entry parameter metadata should be readable"))) ||
			!Assert.AreEqual(TypeId,
				ParameterTypeId,
				*Case.Describe(TEXT("increment entry should retain exact input type"))) ||
			!Assert.AreEqual(static_cast<asDWORD>(asTM_CONST),
				ParameterFlags,
				*Case.Describe(TEXT("increment entry should retain fork const-normalized value metadata"))) ||
			!Assert.AreEqual(FString(TEXT("Input")),
				FString(UTF8_TO_TCHAR(ParameterName != nullptr ? ParameterName : "")),
				*Case.Describe(TEXT("increment entry should retain input name"))) ||
			!Assert.AreEqual(TypeId,
				Entry.GetReturnTypeId(),
				*Case.Describe(TEXT("increment observation should retain exact result type"))))
		{
			return false;
		}

		if (CategoryCase.Category == EWritableCategory::Field)
		{
			asITypeInfo* const Owner = Module.GetTypeInfoByName("FIncrementFieldOwner");
			if (!Assert.IsNotNull(
					Owner, *Case.Describe(TEXT("increment field should publish owner type"))))
			{
				return false;
			}
			const char* PropertyName = nullptr;
			int PropertyTypeId = asINVALID_TYPE;
			return Assert.AreEqual(asSUCCESS,
					   Owner->GetProperty(0, &PropertyName, &PropertyTypeId),
					   *Case.Describe(TEXT("increment field metadata should be readable"))) &&
				   Assert.AreEqual(TypeId,
					   PropertyTypeId,
					   *Case.Describe(TEXT("increment field should retain exact type"))) &&
				   Assert.AreEqual(FString(TEXT("Value")),
					   FString(UTF8_TO_TCHAR(PropertyName != nullptr ? PropertyName : "")),
					   *Case.Describe(TEXT("increment field should retain name")));
		}
		if (CategoryCase.Category == EWritableCategory::Property)
		{
			const FTCHARToUTF8 ObjectTypeUtf8(*PropertyTypeName(TypeCase));
			asITypeInfo* const Owner = Engine.GetTypeInfoByDecl(ObjectTypeUtf8.Get());
			if (!Assert.IsNotNull(
					Owner, *Case.Describe(TEXT("increment property owner should resolve"))))
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
					   ResolvedGetter, *Case.Describe(TEXT("increment property getter should resolve"))) &&
				   Assert.IsNotNull(
					   ResolvedSetter, *Case.Describe(TEXT("increment property setter should resolve")));
			if (bPassed && (!ResolvedGetter->IsProperty() || !ResolvedSetter->IsProperty()))
			{
				Test.AddInfo(FString::Printf(
					TEXT("[AS-FORK-LIMITATION] Id=%s native property decorator is unavailable; explicit accessors retained"),
					*Case.GetId()));
			}
			return bPassed;
		}
		if (CategoryCase.Category == EWritableCategory::Alias)
		{
			asIScriptFunction* const Alias = FindAlias(Engine, Module, TypeCase, OperatorCase);
			if (!Assert.IsNotNull(
					Alias, *Case.Describe(TEXT("increment alias helper should resolve exactly"))))
			{
				return false;
			}
			int AliasTypeId = asINVALID_TYPE;
			asDWORD AliasFlags = asTM_NONE;
			return Assert.AreEqual(asSUCCESS,
					   Alias->GetParam(0, &AliasTypeId, &AliasFlags),
					   *Case.Describe(TEXT("increment alias metadata should be readable"))) &&
				   Assert.AreEqual(TypeId,
					   AliasTypeId,
					   *Case.Describe(TEXT("increment alias should retain exact type"))) &&
				   Assert.AreEqual(static_cast<asDWORD>(asTM_INOUTREF),
					   AliasFlags,
					   *Case.Describe(TEXT("increment alias should retain inout modifier")));
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
					*Case.Describe(TEXT("increment local should retain exact type")));
			}
		}
		return Assert.IsTrue(
			false, *Case.Describe(TEXT("increment local should publish named lvalue")));
	}

	static bool ExecuteLegalCase(FAutomationTestBase& Test,
		asIScriptEngine& Engine,
		asIScriptModule& Module,
		asIScriptContext& Context,
		const FNativeCaseContext& Case,
		const FNativeTypeCase& TypeCase,
		const FOperatorCase& OperatorCase,
		const FCategoryCase& CategoryCase,
		const FObservationCase& ObservationCase,
		FIncrementState& State)
	{
		FNoDiscardAsserter Assert(Test);
		asIScriptFunction* const Entry =
			FindEntry(Engine, Module, TypeCase, OperatorCase, ObservationCase);
		if (!Assert.IsNotNull(Entry,
				*Case.Describe(TEXT("increment observation entry should resolve exactly"))) ||
			!VerifyLegalMetadata(
				Test, Engine, Module, *Entry, Case, TypeCase, OperatorCase, CategoryCase))
		{
			return false;
		}
		State.ResetRuntime(InitialBits(TypeCase));
		bool bPassed = Assert.AreEqual(asSUCCESS,
			Context.Prepare(Entry),
			*Case.Describe(TEXT("increment observation should prepare")));
		bPassed &= Assert.AreEqual(asSUCCESS,
			SetArgument(Context, TypeCase),
			*Case.Describe(TEXT("increment observation should bind exact input ABI")));
		const int ExecuteResult = Context.Execute();
		const uint64 ActualBits = ReadReturnBits(Context, TypeCase);
		const uint64 ExpectedBits =
			ExpectedObservationBits(TypeCase, OperatorCase, ObservationCase);
		bPassed &= Assert.AreEqual(static_cast<int32>(asEXECUTION_FINISHED),
			ExecuteResult,
			*Case.Describe(TEXT("increment observation should execute")));
		bPassed &= Assert.AreEqual(ExpectedBits,
			ActualBits,
			*Case.DescribeResult(Entry->GetDeclaration(),
				FString::Printf(TEXT("bits=0x%016llX"), ExpectedBits),
				FString::Printf(TEXT("bits=0x%016llX"), ActualBits)));
		bPassed &= Assert.AreEqual(asSUCCESS,
			Context.Unprepare(),
			*Case.Describe(TEXT("increment observation should unprepare")));

		const bool bProperty = CategoryCase.Category == EWritableCategory::Property;
		const int32 ExpectedGetterCalls =
			bProperty ? (ObservationCase.Observation == EObservation::After ? 2 : 1) : 0;
		bPassed &= Assert.AreEqual(ExpectedGetterCalls,
			State.GetterCalls,
			*Case.Describe(TEXT("increment property should perform exact read count")));
		bPassed &= Assert.AreEqual(bProperty ? 1 : 0,
			State.SetterCalls,
			*Case.Describe(TEXT("increment property should perform exactly one write")));
		bPassed &= Assert.AreEqual(bProperty ? 1 : 0,
			State.FactoryCalls,
			*Case.Describe(TEXT("increment property should create one receiver")));
		bPassed &= Assert.AreEqual(bProperty ? 1 : 0,
			State.DestroyedObjects,
			*Case.Describe(TEXT("increment property should destroy its receiver")));
		bPassed &= Assert.AreEqual(0,
			State.LiveObjects,
			*Case.Describe(TEXT("increment observation should retain no receiver")));
		bPassed &= Assert.AreEqual(State.FactoryCalls + State.AddRefCalls,
			State.ReleaseCalls,
			*Case.Describe(TEXT("increment property references should balance")));
		if (bProperty)
		{
			bPassed &= Assert.AreEqual(FinalBits(TypeCase, OperatorCase),
				State.ValueBits,
				*Case.Describe(TEXT("increment property should retain exact final value")));
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
			GetNativeFunctionByExactDecl(&Module, "int RecoverIncrement()");
		if (!Assert.IsNotNull(
				Recovery, *Case.Describe(TEXT("increment recovery should resolve exactly"))))
		{
			return false;
		}
		asIScriptContext* const Context = Engine.CreateContext();
		if (!Assert.IsNotNull(
				Context, *Case.Describe(TEXT("increment recovery should create context"))))
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
				   *Case.Describe(TEXT("increment recovery should execute"))) &&
			   Assert.AreEqual(509,
				   static_cast<int32>(Context->GetReturnDWord()),
				   *Case.Describe(TEXT("increment recovery should return exact marker")));
	}

	static bool VerifyRejectedSource(FAutomationTestBase& Test,
		FNativeTestEngine& Engine,
		const FNativeCaseContext& Case,
		const FString& ModuleName,
		const FString& Source,
		const ANSICHAR* InvalidFunctionName,
		FIncrementState& State)
	{
		FNoDiscardAsserter Assert(Test);
		State.ResetRuntime(0);
		int32 BuildResult = asERROR;
		asIScriptModule* Module =
			CompileReportedSource(Engine, Test, Case.GetId(), ModuleName, Source, BuildResult);
		bool bPassed = Assert.IsTrue(BuildResult < 0,
			*Case.DescribeResult("<increment rejection build>",
				TEXT("negative build result"),
				FString::Printf(TEXT("%d Messages={%s}"), BuildResult, *Engine.GetMessagesText())));
		const TArray<AngelscriptNativeTestSupport::FNativeMessageEntry> Errors =
			ErrorMessages(Engine.GetMessages());
		bPassed &= Assert.AreEqual(1,
			Errors.Num(),
			*Case.DescribeResult("<increment rejection diagnostic>",
				TEXT("one causal error"),
				Engine.GetMessagesText()));
		const int32 ExpectedLine = LastSourceLineContaining(Source, TEXT("INCREMENT_CAUSE"));
		bPassed &= Assert.IsTrue(ExpectedLine > 0,
			*Case.Describe(TEXT("increment rejection source should retain causal marker")));
		if (Errors.Num() == 1)
		{
			bPassed &= Assert.AreEqual(ExpectedLine,
				Errors[0].Row,
				*Case.Describe(TEXT("increment rejection should own exact operator line")));
		}
		bPassed &= Assert.AreEqual(0,
			State.RejectedProducerCalls,
			*Case.Describe(TEXT("rejected increment should execute no temporary producer")));
		bPassed &= Assert.AreEqual(0,
			State.FactoryCalls,
			*Case.Describe(TEXT("rejected increment should execute no property factory")));
		bPassed &= Assert.AreEqual(0,
			State.GetterCalls,
			*Case.Describe(TEXT("rejected increment should execute no property getter")));
		bPassed &= Assert.AreEqual(0,
			State.SetterCalls,
			*Case.Describe(TEXT("rejected increment should execute no property setter")));
		if (Module != nullptr)
		{
			bPassed &= Assert.IsTrue(!HasFunctionNamed(*Module, InvalidFunctionName),
				*Case.Describe(TEXT("rejected increment should publish no callable entry")));
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
			*Case.DescribeResult("<increment recovery build>",
				TEXT("successful same-name build"),
				Engine.GetMessagesText()));
		bPassed &= Assert.IsNotNull(
			Module, *Case.Describe(TEXT("increment recovery should publish module")));
		if (RecoveryBuildResult >= 0 && Module != nullptr)
		{
			bPassed &= ExecuteRecovery(Test, *Engine.Get(), *Module, Case);
		}
		bPassed &= Assert.AreEqual(asSUCCESS,
			Engine.Get()->DiscardModule(ModuleNameUtf8.Get()),
			*Case.Describe(TEXT("increment recovery module should discard")));
		bPassed &= Assert.IsNull(Engine.Get()->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
			*Case.Describe(TEXT("increment recovery should leave no stale module")));
		return bPassed;
	}

	static bool CollectTypes(
		TArray<const FNativeTypeCase*>& OutNumericTypes, const FNativeTypeCase*& OutBoolType)
	{
		using namespace AngelscriptNativeTestSupport;
		OutNumericTypes.Reset();
		OutBoolType = nullptr;
		for (const FNativeTypeCase& TypeCase : NativeTypeCases)
		{
			if (IsNumericType(TypeCase))
			{
				OutNumericTypes.Add(&TypeCase);
			}
			else if (IsBoolType(TypeCase))
			{
				OutBoolType = &TypeCase;
			}
		}
		return OutNumericTypes.Num() == 10 && OutBoolType != nullptr;
	}

public:

	TEST_METHOD(WritableTargetRejections)
	{
		using namespace AngelscriptNativeTestSupport;
		AS_NATIVE_PRODUCT("LANG-OP-INCREMENT-TARGET-REJECTION",
			ENativeEvidence::Compile | ENativeEvidence::Diagnostic | ENativeEvidence::Isolation);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT
		{
			Engine.Destroy();
		};
		ASSERT_THAT(
			IsNotNull(Engine.Get(), TEXT("increment target rejection should create engine")));
		if (Engine.Get() == nullptr)
		{
			return;
		}
		TArray<const FNativeTypeCase*> NumericTypes;
		const FNativeTypeCase* BoolType = nullptr;
		ASSERT_THAT(IsTrue(CollectTypes(NumericTypes, BoolType),
			TEXT("increment target rejection should resolve primitive types")));
		if (NumericTypes.Num() != 10 || BoolType == nullptr)
		{
			return;
		}
		FIncrementState State;
		ASSERT_THAT(IsTrue(RegisterFixtures(*Engine.Get(), State, NumericTypes, *BoolType),
			TEXT("increment target rejection should register no-execution fixtures")));

		TArray<FString> ConstructedIds;
		TSet<FString> UniqueIds;
		bool bAllCasesPassed = true;
		for (const FRejectedTargetCase& TargetCase : RejectedTargetCases)
		{
			for (const FOperatorCase& OperatorCase : OperatorCases)
			{
				for (const FNativeTypeCase* TypeCase : NumericTypes)
				{
					const FNativeCaseContext Case(
						MakeNativeCaseId("LANG-OP-INCREMENT-TARGET-REJECTION",
							{ANSI_TO_TCHAR(TargetCase.CatalogName),
								ANSI_TO_TCHAR(OperatorCase.CatalogName),
								ANSI_TO_TCHAR(TypeCase->CatalogName)}));
					ConstructedIds.Add(Case.GetId());
					const bool bUniqueCaseId = !UniqueIds.Contains(Case.GetId());
					UniqueIds.Add(Case.GetId());
					ASSERT_THAT(IsTrue(bUniqueCaseId,
						*Case.Describe(TEXT("increment target rejection ID should be unique"))));
					const FString ModuleName =
						FString::Printf(TEXT("ASNativeIncrementTargetReject_%hs_%hs_%hs"),
							TargetCase.CatalogName,
							OperatorCase.CatalogName,
							TypeCase->CatalogName);
					const FString Source = BuildTargetRejectionSource(
						*Engine.Get(), *TypeCase, OperatorCase, TargetCase);
					bAllCasesPassed &= VerifyRejectedSource(*TestRunner,
						Engine,
						Case,
						ModuleName,
						Source,
						"RejectIncrementTarget",
						State);
				}
			}
		}
		ASSERT_THAT(AreEqual(80,
			ConstructedIds.Num(),
			TEXT("increment target rejection should construct all 80 IDs")));
		ASSERT_THAT(AreEqual(ConstructedIds.Num(),
			UniqueIds.Num(),
			TEXT("increment target rejection should construct no duplicate IDs")));
		ASSERT_THAT(IsTrue(bAllCasesPassed,
			TEXT("every const or temporary rejection should own one diagnostic, no execution, "
				 "same-name recovery, and cleanup")));
	}
};

#endif
