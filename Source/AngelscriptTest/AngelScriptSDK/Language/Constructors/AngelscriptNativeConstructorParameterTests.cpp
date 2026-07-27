#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FConstructorParameterTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Constructors.Parameters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using ENativeLifecycleEvent =
		AngelscriptNativeTestSupport::ENativeLifecycleEvent;
	using ENativeValueCategory =
		AngelscriptNativeTestSupport::ENativeValueCategory;
	using FNativeCaseContext =
		AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeLifecycleEntry =
		AngelscriptNativeTestSupport::FNativeLifecycleEntry;
	using FNativeLifecycleRecorder =
		AngelscriptNativeTestSupport::FNativeLifecycleRecorder;
	using FNativeMessageEntry =
		AngelscriptNativeTestSupport::FNativeMessageEntry;
	using FNativeTestEngine =
		AngelscriptNativeTestSupport::FNativeTestEngine;
	using FNativeTypeCase =
		AngelscriptNativeTestSupport::FNativeTypeCase;

	static constexpr asPWORD ConstructorParameterStateUserDataSlot =
		static_cast<asPWORD>(0x43544F525041524Dull);
	static constexpr asDWORD NormalizedByValueParameterFlags = asTM_CONST;

	struct FArityCase
	{
		const ANSICHAR* CatalogName;
		int32 Count;
	};

	struct FSelectionCase
	{
		const ANSICHAR* CatalogName;
		int32 Marker;
	};

	inline static constexpr FArityCase ArityCases[] =
	{
		{ "one", 1 },
		{ "two", 2 },
		{ "five", 5 },
		{ "sixteen", 16 },
	};

	inline static constexpr FSelectionCase SelectionCases[] =
	{
		{ "exact", 101 },
		{ "promotion", 201 },
		{ "explicit_conversion", 301 },
		{ "ambiguous", 401 },
		{ "missing", 501 },
	};

	struct FConstructorParameterState
	{
		TArray<int32> ArgumentOrder;
		TArray<int32> ArgumentValues;
		TArray<int32> ConsumedOrder;
		TArray<int32> ConsumedValues;
		TArray<int32> SelectedMarkers;

		void Reset()
		{
			ArgumentOrder.Reset();
			ArgumentValues.Reset();
			ConsumedOrder.Reset();
			ConsumedValues.Reset();
			SelectedMarkers.Reset();
		}
	};

	static FConstructorParameterState* GetActiveState()
	{
		asIScriptContext* const Context = asGetActiveContext();
		return Context != nullptr
			? static_cast<FConstructorParameterState*>(
				Context->GetEngine()->GetUserData(
					ConstructorParameterStateUserDataSlot))
			: nullptr;
	}

	static void RecordConstructorArgument(
		const int32 Index,
		const int32 Value)
	{
		if (FConstructorParameterState* const State = GetActiveState())
		{
			State->ArgumentOrder.Add(Index);
			State->ArgumentValues.Add(Value);
		}
	}

	static void RecordConstructorParameterConsumed(
		const int32 Index,
		const int32 Value)
	{
		if (FConstructorParameterState* const State = GetActiveState())
		{
			State->ConsumedOrder.Add(Index);
			State->ConsumedValues.Add(Value);
		}
	}

	static void RecordConstructorSelected(const int32 Marker)
	{
		if (FConstructorParameterState* const State = GetActiveState())
		{
			State->SelectedMarkers.Add(Marker);
		}
	}

	static bool RegisterConstructorParameterBridge(
		asIScriptEngine& ScriptEngine,
		FConstructorParameterState& State)
	{
		ScriptEngine.SetUserData(
			&State,
			ConstructorParameterStateUserDataSlot);
		const ASAutoCaller::FunctionCaller ArgumentCaller =
			ASAutoCaller::MakeFunctionCaller(RecordConstructorArgument);
		const ASAutoCaller::FunctionCaller ConsumedCaller =
			ASAutoCaller::MakeFunctionCaller(
				RecordConstructorParameterConsumed);
		const ASAutoCaller::FunctionCaller SelectionCaller =
			ASAutoCaller::MakeFunctionCaller(RecordConstructorSelected);
		return ScriptEngine.RegisterGlobalFunction(
			"void RecordConstructorArgument(int Index, int Value)",
			asFUNCTION(RecordConstructorArgument),
			asCALL_CDECL,
			*(asFunctionCaller*)&ArgumentCaller) >= 0
			&& ScriptEngine.RegisterGlobalFunction(
				"void RecordConstructorParameterConsumed(int Index, int Value)",
				asFUNCTION(RecordConstructorParameterConsumed),
				asCALL_CDECL,
				*(asFunctionCaller*)&ConsumedCaller) >= 0
			&& ScriptEngine.RegisterGlobalFunction(
				"void RecordConstructorSelected(int Marker)",
				asFUNCTION(RecordConstructorSelected),
				asCALL_CDECL,
				*(asFunctionCaller*)&SelectionCaller) >= 0;
	}

	static bool IsType(
		const FNativeTypeCase& TypeCase,
		const ANSICHAR* CatalogName)
	{
		return FCStringAnsi::Strcmp(
			TypeCase.CatalogName,
			CatalogName) == 0;
	}

	static bool IsSelection(
		const FSelectionCase& SelectionCase,
		const ANSICHAR* CatalogName)
	{
		return FCStringAnsi::Strcmp(
			SelectionCase.CatalogName,
			CatalogName) == 0;
	}

	static bool IsObjectValueType(const FNativeTypeCase& TypeCase)
	{
		return TypeCase.Category == ENativeValueCategory::ScriptValue
			|| TypeCase.Category == ENativeValueCategory::NativeValue;
	}

	static bool SupportsPromotion(const FNativeTypeCase& TypeCase)
	{
		return IsType(TypeCase, "int8")
			|| IsType(TypeCase, "int16")
			|| IsType(TypeCase, "int")
			|| IsType(TypeCase, "uint8")
			|| IsType(TypeCase, "uint16")
			|| IsType(TypeCase, "uint")
			|| IsType(TypeCase, "float32")
			|| IsType(TypeCase, "typedef");
	}

	static const ANSICHAR* PromotionType(
		const FNativeTypeCase& TypeCase)
	{
		if (IsType(TypeCase, "int8"))
		{
			return "int16";
		}
		if (IsType(TypeCase, "int16"))
		{
			return "int";
		}
		if (IsType(TypeCase, "int"))
		{
			return "int64";
		}
		if (IsType(TypeCase, "uint8"))
		{
			return "uint16";
		}
		if (IsType(TypeCase, "uint16"))
		{
			return "uint";
		}
		if (IsType(TypeCase, "uint"))
		{
			return "uint64";
		}
		if (IsType(TypeCase, "float32"))
		{
			return "double";
		}
		if (IsType(TypeCase, "typedef"))
		{
			return "int64";
		}
		return "FNoPromotion";
	}

	static const ANSICHAR* ExplicitTargetType(
		const FNativeTypeCase& TypeCase)
	{
		if (IsType(TypeCase, "int8"))
		{
			return "uint8";
		}
		if (IsType(TypeCase, "int16"))
		{
			return "uint16";
		}
		if (IsType(TypeCase, "int"))
		{
			return "uint";
		}
		if (IsType(TypeCase, "int64"))
		{
			return "uint64";
		}
		if (IsType(TypeCase, "uint8"))
		{
			return "int8";
		}
		if (IsType(TypeCase, "uint16"))
		{
			return "int16";
		}
		if (IsType(TypeCase, "uint"))
		{
			return "int";
		}
		if (IsType(TypeCase, "uint64"))
		{
			return "int64";
		}
		if (IsType(TypeCase, "float32")
			|| IsType(TypeCase, "bool")
			|| IsType(TypeCase, "enum")
			|| IsObjectValueType(TypeCase))
		{
			return "int";
		}
		if (IsType(TypeCase, "float64")
			|| IsType(TypeCase, "typedef"))
		{
			return "int64";
		}
		return "int";
	}

	static bool ExpectedBuild(
		const FNativeTypeCase& TypeCase,
		const FSelectionCase& SelectionCase)
	{
		if (IsSelection(SelectionCase, "exact")
			|| IsSelection(SelectionCase, "explicit_conversion"))
		{
			return true;
		}
		if (IsSelection(SelectionCase, "promotion"))
		{
			return SupportsPromotion(TypeCase);
		}
		return false;
	}

	static int32 ExpectedArgumentValue(
		const FNativeTypeCase& TypeCase,
		const int32 Index)
	{
		return TypeCase.Category == ENativeValueCategory::Boolean
			? (Index % 2 == 0 ? 1 : 0)
			: Index + 1;
	}

	static int32 ExpectedChecksum(
		const FNativeTypeCase& TypeCase,
		const FArityCase& ArityCase)
	{
		int32 Result = 0;
		for (int32 Index = 0; Index < ArityCase.Count; ++Index)
		{
			Result += ExpectedArgumentValue(TypeCase, Index);
		}
		return Result;
	}

	static asDWORD ExpectedConstructorParameterFlags(
		const FNativeTypeCase& TypeCase,
		const ANSICHAR* CandidateType)
	{
		const bool bCandidateIsOriginalObjectValue =
			IsObjectValueType(TypeCase)
			&& FCStringAnsi::Strcmp(CandidateType, TypeCase.ScriptType) == 0;
		return bCandidateIsOriginalObjectValue
			? asTM_INOUTREF | asTM_CONST
			: NormalizedByValueParameterFlags;
	}

	static void AppendTypeDeclarations(
		FString& Source,
		const FNativeTypeCase& TypeCase)
	{
		using namespace AngelscriptNativeTestSupport;

		if (TypeCase.Category == ENativeValueCategory::Enum)
		{
			AppendGeneratedAsLine(Source, TEXT("enum ENativeCaseEnum"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			for (int32 Index = 0; Index <= 16; ++Index)
			{
				if (Index < 16)
				{
					AppendGeneratedAsLine(Source, FString::Printf(
						TEXT("\tValue%d = %d,"),
						Index,
						Index));
				}
				else
				{
					AppendGeneratedAsLine(Source, FString::Printf(
						TEXT("\tValue%d = %d"),
						Index,
						Index));
				}
			}
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else if (TypeCase.Category == ENativeValueCategory::Typedef)
		{
			AppendGeneratedAsLine(
				Source,
				TEXT("// NativeCaseAlias is registered through the raw SDK type API."));
			AppendGeneratedAsLine(Source);
		}
		else if (TypeCase.Category == ENativeValueCategory::ScriptValue)
		{
			AppendGeneratedAsLine(Source, TEXT("struct FScriptCaseValue"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
			AppendGeneratedAsLine(Source, TEXT("\tint ObjectId = -1;"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tFScriptCaseValue()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tObjectId = BeginNativeScriptLifecycle(Value);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tFScriptCaseValue(int InValue)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tValue = InValue;"));
			AppendGeneratedAsLine(Source, TEXT("\t\tObjectId = BeginNativeScriptLifecycle(Value);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tFScriptCaseValue(const FScriptCaseValue& Other)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tValue = Other.Value;"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\t\tObjectId = CopyNativeScriptLifecycle(Other.ObjectId, Value);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tFScriptCaseValue& opAssign(const FScriptCaseValue& Other)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tValue = Other.Value;"));
			AppendGeneratedAsLine(
				Source,
				TEXT("\t\tAssignNativeScriptLifecycle(ObjectId, Other.ObjectId, Value);"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn this;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\t~FScriptCaseValue()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tEndNativeScriptLifecycle(ObjectId, Value);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
	}

	static void AppendNoConversionTypes(FString& Source)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("struct FNoPromotion"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("struct FNoMatch"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static FString ValueReadExpression(
		const FNativeTypeCase& TypeCase,
		const FString& Expression)
	{
		if (IsObjectValueType(TypeCase))
		{
			return FString::Printf(TEXT("%s.Value"), *Expression);
		}
		if (TypeCase.Category == ENativeValueCategory::Boolean)
		{
			return FString::Printf(
				TEXT("(%s ? 1 : 0)"),
				*Expression);
		}
		return FString::Printf(TEXT("int(%s)"), *Expression);
	}

	static FString CandidateReadExpression(
		const ANSICHAR* CandidateType,
		const FString& Expression)
	{
		if (FCStringAnsi::Strcmp(CandidateType, "FNoPromotion") == 0
			|| FCStringAnsi::Strcmp(CandidateType, "FNoMatch") == 0)
		{
			return Expression + TEXT(".Value");
		}
		if (FCStringAnsi::Strcmp(CandidateType, "FScriptCaseValue") == 0
			|| FCStringAnsi::Strcmp(CandidateType, "FNativeCaseValue") == 0)
		{
			return Expression + TEXT(".Value");
		}
		if (FCStringAnsi::Strcmp(CandidateType, "bool") == 0)
		{
			return FString::Printf(
				TEXT("(%s ? 1 : 0)"),
				*Expression);
		}
		return FString::Printf(TEXT("int(%s)"), *Expression);
	}

	static FString SourceLiteral(
		const FNativeTypeCase& TypeCase,
		const int32 Index)
	{
		const int32 Value = ExpectedArgumentValue(TypeCase, Index);
		if (TypeCase.Category == ENativeValueCategory::Boolean)
		{
			return Value != 0 ? TEXT("true") : TEXT("false");
		}
		if (TypeCase.Category == ENativeValueCategory::Enum)
		{
			return FString::Printf(
				TEXT("ENativeCaseEnum::Value%d"),
				Value);
		}
		return FString::Printf(
			TEXT("%hs(%d)"),
			TypeCase.ScriptType,
			Value);
	}

	static void AppendSourceObserver(
		FString& Source,
		const FNativeTypeCase& TypeCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("%hs ObserveConstructorSourceArgument(int Index, %hs Value)"),
			TypeCase.ScriptType,
			TypeCase.ScriptType));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\tRecordConstructorArgument(Index, %s);"),
			*ValueReadExpression(TypeCase, TEXT("Value"))));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static const ANSICHAR* CandidateParameterType(
		const FNativeTypeCase& TypeCase,
		const FSelectionCase& SelectionCase)
	{
		if (IsSelection(SelectionCase, "exact")
			|| IsSelection(SelectionCase, "ambiguous"))
		{
			return TypeCase.ScriptType;
		}
		if (IsSelection(SelectionCase, "promotion"))
		{
			return PromotionType(TypeCase);
		}
		if (IsSelection(SelectionCase, "explicit_conversion"))
		{
			return ExplicitTargetType(TypeCase);
		}
		return "FNoMatch";
	}

	static FString MakeParameterList(
		const ANSICHAR* CandidateType,
		const FArityCase& ArityCase,
		const TCHAR* ExtraParameter = nullptr)
	{
		TArray<FString> Parameters;
		for (int32 Index = 0; Index < ArityCase.Count; ++Index)
		{
			Parameters.Add(FString::Printf(
				TEXT("%hs P%d"),
				CandidateType,
				Index));
		}
		if (ExtraParameter != nullptr)
		{
			Parameters.Add(ExtraParameter);
		}
		return FString::Join(Parameters, TEXT(", "));
	}

	static FString MakeObservedSourceArgument(
		const FNativeTypeCase& TypeCase,
		const FSelectionCase& SelectionCase,
		const int32 Index)
	{
		const FString Observed = FString::Printf(
			TEXT("ObserveConstructorSourceArgument(%d, %s)"),
			Index,
			*SourceLiteral(TypeCase, Index));
		if (!IsSelection(SelectionCase, "explicit_conversion"))
		{
			return Observed;
		}

		const ANSICHAR* const TargetType = ExplicitTargetType(TypeCase);
		if (IsObjectValueType(TypeCase))
		{
			return FString::Printf(
				TEXT("%hs((%s).Value)"),
				TargetType,
				*Observed);
		}
		return FString::Printf(
			TEXT("%hs(%s)"),
			TargetType,
			*Observed);
	}

	static FString MakeArgumentList(
		const FNativeTypeCase& TypeCase,
		const FSelectionCase& SelectionCase,
		const FArityCase& ArityCase)
	{
		TArray<FString> Arguments;
		for (int32 Index = 0; Index < ArityCase.Count; ++Index)
		{
			Arguments.Add(MakeObservedSourceArgument(
				TypeCase,
				SelectionCase,
				Index));
		}
		return FString::Join(Arguments, TEXT(", "));
	}

	static void AppendConstructorBody(
		FString& Source,
		const ANSICHAR* CandidateType,
		const FArityCase& ArityCase,
		const int32 Marker,
		const int32 ExtraValue = 0)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("\t{"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\t\tMarker = %d;"),
			Marker));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\t\tRecordConstructorSelected(%d);"),
			Marker));
		for (int32 Index = 0; Index < ArityCase.Count; ++Index)
		{
			const FString ParameterName =
				FString::Printf(TEXT("P%d"), Index);
			const FString ReadExpression =
				CandidateReadExpression(CandidateType, ParameterName);
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t\tRecordConstructorParameterConsumed(%d, %s);"),
				Index,
				*ReadExpression));
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t\tChecksum += %s;"),
				*ReadExpression));
		}
		if (ExtraValue != 0)
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t\tChecksum += %d;"),
				ExtraValue));
		}
		AppendGeneratedAsLine(Source, TEXT("\t}"));
	}

	static void AppendProbeType(
		FString& Source,
		const FNativeTypeCase& TypeCase,
		const FSelectionCase& SelectionCase,
		const FArityCase& ArityCase)
	{
		using namespace AngelscriptNativeTestSupport;

		const ANSICHAR* const CandidateType =
			CandidateParameterType(TypeCase, SelectionCase);
		AppendGeneratedAsLine(Source, TEXT("struct FConstructorProbe"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\tint Marker = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tint Checksum = 0;"));
		AppendGeneratedAsLine(Source, TEXT("\tFNativeCaseValue Payload;"));
		AppendGeneratedAsLine(Source);
		if (IsSelection(SelectionCase, "ambiguous"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\tFConstructorProbe(%s)"),
				*MakeParameterList(
					CandidateType,
					ArityCase,
					TEXT("int Extra = 1"))));
			AppendConstructorBody(
				Source,
				CandidateType,
				ArityCase,
				401,
				1);
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\tFConstructorProbe(%s)"),
				*MakeParameterList(
					CandidateType,
					ArityCase,
					TEXT("float Extra = 1.0f"))));
			AppendConstructorBody(
				Source,
				CandidateType,
				ArityCase,
				402,
				2);
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\tFConstructorProbe(%s)"),
				*MakeParameterList(CandidateType, ArityCase)));
			AppendConstructorBody(
				Source,
				CandidateType,
				ArityCase,
				SelectionCase.Marker);
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendEntryFunctions(
		FString& Source,
		const FNativeTypeCase& TypeCase,
		const FSelectionCase& SelectionCase,
		const FArityCase& ArityCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("int RunConstructorParameter()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\tFConstructorProbe Value(%s);"),
			*MakeArgumentList(
				TypeCase,
				SelectionCase,
				ArityCase)));
		AppendGeneratedAsLine(Source, TEXT("\treturn Value.Checksum;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		AppendGeneratedAsLine(Source, TEXT("int RunConstructorParameterRecovery()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 97;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
	}

	static FString BuildConstructorParameterSource(
		const FNativeTypeCase& TypeCase,
		const FSelectionCase& SelectionCase,
		const FArityCase& ArityCase)
	{
		FString Source;
		AppendTypeDeclarations(Source, TypeCase);
		AppendNoConversionTypes(Source);
		AppendSourceObserver(Source, TypeCase);
		AppendProbeType(Source, TypeCase, SelectionCase, ArityCase);
		AppendEntryFunctions(
			Source,
			TypeCase,
			SelectionCase,
			ArityCase);
		return Source;
	}

	static int CompileAndReport(
		FAutomationTestBase& Test,
		asIScriptEngine& ScriptEngine,
		const FString& SourceId,
		const FString& ModuleName,
		const FString& Source,
		asIScriptModule*& OutModule)
	{
		using namespace AngelscriptNativeTestSupport;

		PrintGeneratedAsSource(Test, SourceId, ModuleName, Source);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 SourceUtf8(*Source);
		return CompileNativeModule(
			&ScriptEngine,
			ModuleNameUtf8.Get(),
			SourceUtf8.Get(),
			OutModule);
	}

	static bool HasLocatedDiagnostic(
		const FNativeTestEngine& Engine,
		const TCHAR* ExpectedText)
	{
		return Engine.GetMessages().Entries.ContainsByPredicate(
			[ExpectedText](const FNativeMessageEntry& Entry)
			{
				return Entry.Type == asMSGTYPE_ERROR
					&& Entry.Row > 0
					&& Entry.Column > 0
					&& Entry.Message.Contains(ExpectedText);
			});
	}

	static bool ConstructorMatches(
		asIScriptFunction& Function,
		asIScriptModule& Module,
		const FNativeTypeCase& TypeCase,
		const ANSICHAR* CandidateType,
		const FArityCase& ArityCase)
	{
		if (static_cast<int32>(Function.GetParamCount())
			!= ArityCase.Count)
		{
			return false;
		}
		const int32 ExpectedTypeId =
			Module.GetTypeIdByDecl(CandidateType);
		for (int32 Index = 0; Index < ArityCase.Count; ++Index)
		{
			int TypeId = asTYPEID_VOID;
			asDWORD Flags = asTM_NONE;
			const char* Name = nullptr;
			if (Function.GetParam(
				static_cast<asUINT>(Index),
				&TypeId,
				&Flags,
				&Name) < 0
				|| TypeId != ExpectedTypeId
				|| Flags != ExpectedConstructorParameterFlags(
					TypeCase,
					CandidateType))
			{
				return false;
			}
		}
		return true;
	}

	void LogConstructorMetadataMatchFailure(
		const FNativeCaseContext& Case,
		asITypeInfo& Type,
		asIScriptModule& Module,
		const ANSICHAR* CandidateType)
	{
		FString BehaviourDescriptions;
		for (asUINT BehaviourIndex = 0;
			BehaviourIndex < Type.GetBehaviourCount();
			++BehaviourIndex)
		{
			asEBehaviours Behaviour = asBEHAVE_CONSTRUCT;
			asIScriptFunction* const Function =
				Type.GetBehaviourByIndex(BehaviourIndex, &Behaviour);
			if (Function == nullptr)
			{
				continue;
			}

			FString Parameters;
			for (asUINT ParameterIndex = 0;
				ParameterIndex < Function->GetParamCount();
				++ParameterIndex)
			{
				int TypeId = asTYPEID_VOID;
				asDWORD Flags = asTM_NONE;
				const char* Name = nullptr;
				Function->GetParam(ParameterIndex, &TypeId, &Flags, &Name);
				if (!Parameters.IsEmpty())
				{
					Parameters += TEXT(", ");
				}
				Parameters += FString::Printf(
					TEXT("%u:TypeId=%d Flags=%u Name=%hs"),
					ParameterIndex,
					TypeId,
					Flags,
					Name != nullptr ? Name : "<null>");
			}
			if (!BehaviourDescriptions.IsEmpty())
			{
				BehaviourDescriptions += TEXT(" | ");
			}
			BehaviourDescriptions += FString::Printf(
				TEXT("Index=%u Behaviour=%d Declaration=%hs Parameters=[%s]"),
				BehaviourIndex,
				static_cast<int32>(Behaviour),
				Function->GetDeclaration() != nullptr
					? Function->GetDeclaration()
					: "<null>",
				*Parameters);
		}

		TestRunner->AddInfo(FString::Printf(
			TEXT("[%s][CONSTRUCTOR-METADATA-MATCH-FAILURE] Candidate=%hs CandidateTypeId=%d Behaviours=[%s]"),
			*Case.GetId(),
			CandidateType,
			Module.GetTypeIdByDecl(CandidateType),
			*BehaviourDescriptions));
	}

	asIScriptFunction* FindSelectedConstructor(
		const FNativeCaseContext& Case,
		const FNativeTypeCase& TypeCase,
		asITypeInfo& Type,
		asIScriptModule& Module,
		const ANSICHAR* CandidateType,
		const FArityCase& ArityCase)
	{
		asIScriptFunction* Result = nullptr;
		int32 MatchCount = 0;
		for (asUINT Index = 0; Index < Type.GetBehaviourCount(); ++Index)
		{
			asEBehaviours Behaviour = asBEHAVE_CONSTRUCT;
			asIScriptFunction* const Function =
				Type.GetBehaviourByIndex(Index, &Behaviour);
			if (Function != nullptr
				&& Behaviour == asBEHAVE_CONSTRUCT
				&& ConstructorMatches(
					*Function,
					Module,
					TypeCase,
					CandidateType,
					ArityCase))
			{
				Result = Function;
				++MatchCount;
			}
		}
		FNoDiscardAsserter Assertions(*TestRunner);
		if (!Assertions.AreEqual(1,
			MatchCount,
			*Case.Describe(TEXT("constructor selection should publish exactly one matching behavior"))))
		{
			LogConstructorMetadataMatchFailure(
				Case,
				Type,
				Module,
				CandidateType);
			return nullptr;
		}
		return Result;
	}

	void VerifyMetadata(
		const FNativeCaseContext& Case,
		const FNativeTypeCase& TypeCase,
		const FSelectionCase& SelectionCase,
		const FArityCase& ArityCase,
		asIScriptModule& Module)
	{
		asITypeInfo* const Type =
			Module.GetTypeInfoByName("FConstructorProbe");
		ASSERT_THAT(IsNotNull(Type,
			*Case.Describe(TEXT("constructor-parameter module should publish its probe type"))));
		if (Type == nullptr)
		{
			return;
		}
		const ANSICHAR* const CandidateType =
			CandidateParameterType(TypeCase, SelectionCase);
		asIScriptFunction* const Constructor =
			FindSelectedConstructor(
				Case,
				TypeCase,
				*Type,
				Module,
				CandidateType,
				ArityCase);
		ASSERT_THAT(IsNotNull(Constructor,
			*Case.Describe(TEXT("constructor-parameter metadata should resolve the selected constructor"))));
		if (Constructor == nullptr)
		{
			return;
		}

		for (int32 Index = 0; Index < ArityCase.Count; ++Index)
		{
			int TypeId = asTYPEID_VOID;
			asDWORD Flags = asTM_NONE;
			const char* Name = nullptr;
			ASSERT_THAT(IsTrue(Constructor->GetParam(
				static_cast<asUINT>(Index),
				&TypeId,
				&Flags,
				&Name) >= 0,
				*Case.Describe(TEXT("selected constructor should expose every parameter"))));
			const FString ExpectedName =
				FString::Printf(TEXT("P%d"), Index);
			ASSERT_THAT(IsTrue(Name != nullptr
				&& FCStringAnsi::Strcmp(
					Name,
					TCHAR_TO_ANSI(*ExpectedName)) == 0,
				*Case.Describe(TEXT("selected constructor should preserve parameter names in order"))));
			ASSERT_THAT(AreEqual(
				Module.GetTypeIdByDecl(CandidateType),
				TypeId,
				*Case.Describe(TEXT("selected constructor should preserve the candidate parameter type"))));
			ASSERT_THAT(AreEqual(
				ExpectedConstructorParameterFlags(TypeCase, CandidateType),
				Flags,
				*Case.Describe(TEXT("selected constructor parameters should preserve metadata for the resolved candidate type"))));
		}
		ASSERT_THAT(IsNotNull(
			Module.GetFunctionByDecl("int RunConstructorParameter()"),
			*Case.Describe(TEXT("constructor-parameter module should publish its exact entry"))));
		ASSERT_THAT(IsNotNull(
			Module.GetFunctionByDecl("int RunConstructorParameterRecovery()"),
			*Case.Describe(TEXT("constructor-parameter module should publish its exact recovery entry"))));
	}

	void VerifyTrace(
		const FNativeCaseContext& Case,
		const FNativeTypeCase& TypeCase,
		const FSelectionCase& SelectionCase,
		const FArityCase& ArityCase,
		const FConstructorParameterState& State)
	{
		ASSERT_THAT(AreEqual(
			ArityCase.Count,
			State.ArgumentOrder.Num(),
			*Case.Describe(TEXT("constructor call should evaluate every argument exactly once"))));
		ASSERT_THAT(AreEqual(
			ArityCase.Count,
			State.ConsumedOrder.Num(),
			*Case.Describe(TEXT("selected constructor should consume every parameter exactly once"))));
		ASSERT_THAT(AreEqual(
			ArityCase.Count,
			State.ArgumentValues.Num(),
			*Case.Describe(TEXT("constructor call should report every argument value"))));
		ASSERT_THAT(AreEqual(
			ArityCase.Count,
			State.ConsumedValues.Num(),
			*Case.Describe(TEXT("selected constructor should report every consumed value"))));
		for (int32 Index = 0;
			Index < ArityCase.Count
				&& Index < State.ArgumentOrder.Num()
				&& Index < State.ConsumedOrder.Num()
				&& Index < State.ArgumentValues.Num()
			&& Index < State.ConsumedValues.Num();
			++Index)
		{
			const int32 SourceArgumentIndex = ArityCase.Count - Index - 1;
			ASSERT_THAT(AreEqual(
				SourceArgumentIndex,
				State.ArgumentOrder[Index],
				*Case.Describe(TEXT("constructor arguments should evaluate from the final source argument to the first"))));
			ASSERT_THAT(AreEqual(Index, State.ConsumedOrder[Index],
				*Case.Describe(TEXT("constructor parameters should be consumed in declaration order"))));
			ASSERT_THAT(AreEqual(
				ExpectedArgumentValue(TypeCase, SourceArgumentIndex),
				State.ArgumentValues[Index],
				*Case.Describe(TEXT("constructor argument should preserve its source-family value in evaluation order"))));
			ASSERT_THAT(AreEqual(
				ExpectedArgumentValue(TypeCase, Index),
				State.ConsumedValues[Index],
				*Case.Describe(TEXT("selected constructor should receive each source-family value in declaration order"))));
			ASSERT_THAT(AreEqual(
				State.ArgumentValues[SourceArgumentIndex],
				State.ConsumedValues[Index],
				*Case.Describe(TEXT("selected conversion should preserve the bounded argument value across evaluation and declaration order"))));
		}
		ASSERT_THAT(AreEqual(1, State.SelectedMarkers.Num(),
			*Case.Describe(TEXT("constructor selection should execute exactly one candidate body"))));
		if (State.SelectedMarkers.Num() == 1)
		{
			ASSERT_THAT(AreEqual(
				SelectionCase.Marker,
				State.SelectedMarkers[0],
				*Case.Describe(TEXT("constructor selection marker should identify the exact candidate"))));
		}
	}

	void VerifyLifecycle(
		const FNativeCaseContext& Case,
		const FNativeTypeCase& TypeCase,
		const FArityCase& ArityCase,
		const FNativeLifecycleRecorder& Lifecycle)
	{
		using namespace AngelscriptNativeTestSupport;

		ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
			*Case.Describe(TEXT("constructor-parameter execution should leave no tracked object alive"))));
		const int32 ConstructionCount =
			Lifecycle.Num(ENativeLifecycleEvent::DefaultConstruct)
			+ Lifecycle.Num(ENativeLifecycleEvent::ValueConstruct)
			+ Lifecycle.Num(ENativeLifecycleEvent::CopyConstruct);
		ASSERT_THAT(IsTrue(ConstructionCount > 0,
			*Case.Describe(TEXT("constructor probe should construct its tracked payload"))));
		ASSERT_THAT(AreEqual(
			ConstructionCount,
			Lifecycle.Num(ENativeLifecycleEvent::Destruct),
			*Case.Describe(TEXT("constructor parameters and probe payload should balance destruction"))));
		if (IsObjectValueType(TypeCase))
		{
			ASSERT_THAT(IsTrue(ConstructionCount > 1,
				*Case.Describe(TEXT("object-value arguments should construct transferred source storage"))));
			const int32 CopyConstructionCount =
				Lifecycle.Num(ENativeLifecycleEvent::CopyConstruct);
			if (TypeCase.Category == ENativeValueCategory::ScriptValue)
			{
				ASSERT_THAT(AreEqual(
					ArityCase.Count * 2,
					Lifecycle.Num(ENativeLifecycleEvent::ValueConstruct),
					*Case.Describe(TEXT("script-value argument evaluation should value-construct source and working storage for every parameter"))));
				ASSERT_THAT(AreEqual(
					1,
					Lifecycle.Num(ENativeLifecycleEvent::DefaultConstruct),
					*Case.Describe(TEXT("script-value constructor probe should default-construct only its payload"))));
				ASSERT_THAT(AreEqual(
					0,
					CopyConstructionCount,
					*Case.Describe(TEXT("script-value parameters should use const inout normalization without transfer copies"))));
			}
			else
			{
				ASSERT_THAT(IsTrue(
					CopyConstructionCount > 0,
					*Case.Describe(TEXT("native-value arguments should exercise copy construction"))));
			}
		}

		TSet<int32> ConstructedIds;
		TSet<int32> DestructedIds;
		for (const FNativeLifecycleEntry& Entry : Lifecycle.GetEntries())
		{
			if (Entry.Event == ENativeLifecycleEvent::DefaultConstruct
				|| Entry.Event == ENativeLifecycleEvent::ValueConstruct
				|| Entry.Event == ENativeLifecycleEvent::CopyConstruct)
			{
				ASSERT_THAT(IsFalse(ConstructedIds.Contains(Entry.ObjectId),
					*Case.Describe(TEXT("constructor-parameter lifecycle should allocate unique identities"))));
				ConstructedIds.Add(Entry.ObjectId);
			}
			else if (Entry.Event == ENativeLifecycleEvent::Destruct)
			{
				ASSERT_THAT(IsTrue(ConstructedIds.Contains(Entry.ObjectId),
					*Case.Describe(TEXT("constructor-parameter destructor should identify constructed storage"))));
				ASSERT_THAT(IsFalse(DestructedIds.Contains(Entry.ObjectId),
					*Case.Describe(TEXT("constructor-parameter storage should not be destroyed twice"))));
				DestructedIds.Add(Entry.ObjectId);
			}
		}
		ASSERT_THAT(AreEqual(ConstructedIds.Num(), DestructedIds.Num(),
			*Case.Describe(TEXT("constructor-parameter lifecycle identities should balance"))));
	}

	void ExecuteSuccessfulModule(
		const FNativeCaseContext& Case,
		const FNativeTypeCase& TypeCase,
		const FSelectionCase& SelectionCase,
		const FArityCase& ArityCase,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module,
		FConstructorParameterState& State,
		FNativeLifecycleRecorder& Lifecycle)
	{
		using namespace AngelscriptNativeTestSupport;

		VerifyMetadata(
			Case,
			TypeCase,
			SelectionCase,
			ArityCase,
			Module);
		asIScriptFunction* const Entry =
			Module.GetFunctionByDecl("int RunConstructorParameter()");
		asIScriptFunction* const Recovery =
			Module.GetFunctionByDecl("int RunConstructorParameterRecovery()");
		if (Entry == nullptr || Recovery == nullptr)
		{
			return;
		}

		asIScriptContext* const Context = ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*Case.Describe(TEXT("constructor-parameter cell should create a reusable context"))));
		if (Context == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			PrepareAndExecute(Context, Entry),
			*Case.Describe(TEXT("constructor-parameter entry should finish"))));
		ASSERT_THAT(AreEqual(
			ExpectedChecksum(TypeCase, ArityCase),
			static_cast<int32>(Context->GetReturnDWord()),
			*Case.Describe(TEXT("selected constructor should return the complete parameter checksum"))));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->Unprepare(),
			*Case.Describe(TEXT("constructor-parameter context should release all argument storage"))));

		VerifyTrace(
			Case,
			TypeCase,
			SelectionCase,
			ArityCase,
			State);
		VerifyLifecycle(Case, TypeCase, ArityCase, Lifecycle);

		const int32 ArgumentCountBeforeRecovery =
			State.ArgumentOrder.Num();
		const int32 ConsumedCountBeforeRecovery =
			State.ConsumedOrder.Num();
		const int32 MarkerCountBeforeRecovery =
			State.SelectedMarkers.Num();
		ASSERT_THAT(IsTrue(Context->Prepare(Recovery) >= 0,
			*Case.Describe(TEXT("constructor-parameter context should prepare recovery"))));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_FINISHED),
			Context->Execute(),
			*Case.Describe(TEXT("constructor-parameter recovery should finish in the same context"))));
		ASSERT_THAT(AreEqual(
			97,
			static_cast<int32>(Context->GetReturnDWord()),
			*Case.Describe(TEXT("constructor-parameter recovery should return its sentinel"))));
		ASSERT_THAT(AreEqual(
			ArgumentCountBeforeRecovery,
			State.ArgumentOrder.Num(),
			*Case.Describe(TEXT("constructor-parameter recovery should evaluate no argument"))));
		ASSERT_THAT(AreEqual(
			ConsumedCountBeforeRecovery,
			State.ConsumedOrder.Num(),
			*Case.Describe(TEXT("constructor-parameter recovery should consume no parameter"))));
		ASSERT_THAT(AreEqual(
			MarkerCountBeforeRecovery,
			State.SelectedMarkers.Num(),
			*Case.Describe(TEXT("constructor-parameter recovery should select no constructor"))));
		ASSERT_THAT(AreEqual(
			asSUCCESS,
			Context->Unprepare(),
			*Case.Describe(TEXT("constructor-parameter recovery should unprepare cleanly"))));
		Context->Release();
		ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
			*Case.Describe(TEXT("constructor-parameter recovery should keep lifecycle balanced"))));
	}

	void VerifyRejectedBuild(
		const FNativeCaseContext& Case,
		const FNativeTypeCase& TypeCase,
		const FSelectionCase& SelectionCase,
		const int BuildResult,
		asIScriptModule* Module,
		const FNativeTestEngine& Engine,
		const FConstructorParameterState& State,
		const FNativeLifecycleRecorder& Lifecycle)
	{
		ASSERT_THAT(IsTrue(BuildResult < 0,
			*Case.Describe(TEXT("unsupported constructor selection should fail to compile"))));
		const TCHAR* const ExpectedText =
			IsSelection(SelectionCase, "ambiguous")
				? TEXT("Multiple matching signatures")
				: TEXT("No matching signatures");
		ASSERT_THAT(IsTrue(HasLocatedDiagnostic(Engine, ExpectedText),
			*Case.Describe(TEXT("unsupported constructor selection should own its located diagnostic"))));
		ASSERT_THAT(IsTrue(Module == nullptr
			|| Module->GetFunctionByDecl("int RunConstructorParameter()") == nullptr,
			*Case.Describe(TEXT("failed constructor selection should publish no callable entry"))));
		ASSERT_THAT(AreEqual(0, State.ArgumentOrder.Num(),
			*Case.Describe(TEXT("compile-time rejection should evaluate no argument"))));
		ASSERT_THAT(AreEqual(0, State.ConsumedOrder.Num(),
			*Case.Describe(TEXT("compile-time rejection should consume no parameter"))));
		ASSERT_THAT(AreEqual(0, State.SelectedMarkers.Num(),
			*Case.Describe(TEXT("compile-time rejection should execute no constructor body"))));
		ASSERT_THAT(AreEqual(0, Lifecycle.GetEntries().Num(),
			*Case.Describe(TEXT("compile-time rejection should construct no tracked storage"))));
		ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
			*Case.Describe(TEXT("compile-time rejection should leave no tracked object alive"))));

		if (IsSelection(SelectionCase, "promotion"))
		{
			ASSERT_THAT(IsFalse(SupportsPromotion(TypeCase),
				*Case.Describe(TEXT("rejected promotion cell should belong to an unsupported type family"))));
		}
	}

	void RunCell(
		const FNativeTypeCase& TypeCase,
		const FArityCase& ArityCase,
		const FSelectionCase& SelectionCase)
	{
		using namespace AngelscriptNativeTestSupport;

		const FNativeCaseContext Case(MakeNativeCaseId(
			"LANG-CTOR-PARAM-SELECT",
			{
				ANSI_TO_TCHAR(ArityCase.CatalogName),
				ANSI_TO_TCHAR(SelectionCase.CatalogName),
				ANSI_TO_TCHAR(TypeCase.CatalogName),
			}));
		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			*Case.Describe(TEXT("constructor-parameter cell should create a raw SDK engine"))));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FConstructorParameterState State;
		FNativeLifecycleRecorder Lifecycle;
		ASSERT_THAT(IsTrue(RegisterConstructorParameterBridge(*ScriptEngine, State),
			*Case.Describe(TEXT("constructor-parameter cell should register its trace bridge"))));
		ASSERT_THAT(IsTrue(RegisterNativeCaseValue(*ScriptEngine, Lifecycle),
			*Case.Describe(TEXT("constructor-parameter cell should register tracked native values"))));
		ASSERT_THAT(IsTrue(RegisterNativeScriptLifecycleBridge(*ScriptEngine, Lifecycle),
			*Case.Describe(TEXT("constructor-parameter cell should register script lifecycle callbacks"))));
		if (TypeCase.Category == ENativeValueCategory::Typedef)
		{
			ASSERT_THAT(IsTrue(ScriptEngine->RegisterTypedef(
				"NativeCaseAlias",
				"int") >= 0,
				*Case.Describe(TEXT("constructor-parameter typedef cell should register its raw SDK alias"))));
		}

		const FString ModuleName = FString::Printf(
			TEXT("ConstructorParameter_%hs_%hs_%hs"),
			TypeCase.CatalogName,
			ArityCase.CatalogName,
			SelectionCase.CatalogName);
		const FString Source = BuildConstructorParameterSource(
			TypeCase,
			SelectionCase,
			ArityCase);
		Engine.ResetMessages();
		State.Reset();
		Lifecycle.Reset();
		asIScriptModule* Module = nullptr;
		const int BuildResult = CompileAndReport(
			*TestRunner,
			*ScriptEngine,
			Case.GetId(),
			ModuleName,
			Source,
			Module);
		const bool bExpectedBuild =
			ExpectedBuild(TypeCase, SelectionCase);
		if (bExpectedBuild && BuildResult < 0)
		{
			TestRunner->AddInfo(FString::Printf(
				TEXT("[%s][UNEXPECTED-CONSTRUCTOR-BUILD-DIAGNOSTICS] %s"),
				*Case.GetId(),
				*Engine.GetMessagesText()));
		}
		ASSERT_THAT(AreEqual(
			bExpectedBuild,
			BuildResult >= 0,
			*Case.Describe(TEXT("constructor-parameter build result should match the constrained product"))));

		if (bExpectedBuild)
		{
			ASSERT_THAT(IsNotNull(Module,
				*Case.Describe(TEXT("successful constructor-parameter cell should publish its module"))));
			if (Module != nullptr)
			{
				ExecuteSuccessfulModule(
					Case,
					TypeCase,
					SelectionCase,
					ArityCase,
					*ScriptEngine,
					*Module,
					State,
					Lifecycle);
			}
		}
		else
		{
			VerifyRejectedBuild(
				Case,
				TypeCase,
				SelectionCase,
				BuildResult,
				Module,
				Engine,
				State,
				Lifecycle);

			const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
			ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
			ASSERT_THAT(IsNull(ScriptEngine->GetModule(
				ModuleNameUtf8.Get(),
				asGM_ONLY_IF_EXISTS),
				*Case.Describe(TEXT("failed constructor-parameter module should discard before recovery"))));

			const FSelectionCase& RecoverySelection =
				SelectionCases[0];
			const FString RecoverySource =
				BuildConstructorParameterSource(
					TypeCase,
					RecoverySelection,
					ArityCase);
			Engine.ResetMessages();
			State.Reset();
			Lifecycle.Reset();
			Module = nullptr;
			ASSERT_THAT(IsTrue(CompileAndReport(
				*TestRunner,
				*ScriptEngine,
				Case.GetId() + TEXT("-RECOVERY"),
				ModuleName,
				RecoverySource,
				Module) >= 0,
				*Case.Describe(TEXT("rejected constructor selection should allow exact same-name recovery"))));
			ASSERT_THAT(IsNotNull(Module,
				*Case.Describe(TEXT("constructor-parameter recovery should publish its module"))));
			if (Module != nullptr)
			{
				ExecuteSuccessfulModule(
					Case,
					TypeCase,
					RecoverySelection,
					ArityCase,
					*ScriptEngine,
					*Module,
					State,
					Lifecycle);
			}
		}

		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
		ASSERT_THAT(IsNull(ScriptEngine->GetModule(
			ModuleNameUtf8.Get(),
			asGM_ONLY_IF_EXISTS),
			*Case.Describe(TEXT("constructor-parameter module should discard cleanly"))));
		ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
			*Case.Describe(TEXT("constructor-parameter module discard should leave no live object"))));
	}

public:
	TEST_METHOD(ParameterTypesByArityAndSelection)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-CTOR-PARAM-SELECT",
			ENativeEvidence::Compile
				| ENativeEvidence::Diagnostic
				| ENativeEvidence::Runtime
				| ENativeEvidence::Metadata
				| ENativeEvidence::Lifecycle
				| ENativeEvidence::Cleanup);

		for (const FNativeTypeCase& TypeCase : NativeTypeCases)
		{
			if (!IsCoreValueTypeCase(TypeCase))
			{
				continue;
			}
			for (const FArityCase& ArityCase : ArityCases)
			{
				for (const FSelectionCase& SelectionCase : SelectionCases)
				{
					RunCell(TypeCase, ArityCase, SelectionCase);
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
