#include "../../Support/AngelscriptNativeCaseTestSupport.h"
#include "../../Support/AngelscriptNativeCoreTestSupport.h"
#include "../../Support/AngelscriptNativeFixtureTestSupport.h"
#include "../../Support/AngelscriptNativeLanguageCaseTestSupport.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FVariableAssignmentTests,
	"Angelscript.TestModule.AngelScriptSDK.Language.Variables.Assignment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FNativeCaseContext = AngelscriptNativeTestSupport::FNativeCaseContext;
	using FNativeLifecycleRecorder = AngelscriptNativeTestSupport::FNativeLifecycleRecorder;
	using FNativeTypeCase = AngelscriptNativeTestSupport::FNativeTypeCase;
	using ENativeLifecycleEvent = AngelscriptNativeTestSupport::ENativeLifecycleEvent;
	using ENativeValueCategory = AngelscriptNativeTestSupport::ENativeValueCategory;

	struct FAssignmentCase
	{
		const ANSICHAR* CatalogName;
	};

	struct FTargetCase
	{
		const ANSICHAR* CatalogName;
	};

	inline static constexpr FAssignmentCase AssignmentCases[] =
	{
		{ "simple" },
		{ "copy_source" },
		{ "self_assignment" },
		{ "compound" },
		{ "reference_rebind" },
	};

	inline static constexpr FTargetCase TargetCases[] =
	{
		{ "mutable_local" },
		{ "const_local" },
		{ "mutable_field" },
		{ "const_field" },
		{ "reference_alias" },
		{ "temporary" },
		{ "expression_result" },
	};

	static bool IsAssignment(const FAssignmentCase& AssignmentCase, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(AssignmentCase.CatalogName, Name) == 0;
	}

	static bool IsTarget(const FTargetCase& TargetCase, const ANSICHAR* Name)
	{
		return FCStringAnsi::Strcmp(TargetCase.CatalogName, Name) == 0;
	}

	static bool IsReferenceType(const FNativeTypeCase& TypeCase)
	{
		return TypeCase.Category == ENativeValueCategory::ScriptReference
			|| TypeCase.Category == ENativeValueCategory::NativeReference;
	}

	static bool IsValueObjectType(const FNativeTypeCase& TypeCase)
	{
		return TypeCase.Category == ENativeValueCategory::ScriptValue
			|| TypeCase.Category == ENativeValueCategory::NativeValue;
	}

	static bool IsObjectType(const FNativeTypeCase& TypeCase)
	{
		return IsValueObjectType(TypeCase) || IsReferenceType(TypeCase);
	}

	static const ANSICHAR* GetGeneratedTypeName(const FNativeTypeCase& TypeCase)
	{
		// This fixture declares its own enum so every generated declaration must
		// use that local name rather than the shared language-case enum name.
		return TypeCase.Category == ENativeValueCategory::Enum
			? "ENativeAssignmentEnum"
			: TypeCase.ScriptType;
	}

	static bool SupportsCompoundAssignment(const FNativeTypeCase& TypeCase)
	{
		return TypeCase.Category == ENativeValueCategory::SignedInteger
			|| TypeCase.Category == ENativeValueCategory::UnsignedInteger
			|| TypeCase.Category == ENativeValueCategory::FloatingPoint
			|| TypeCase.Category == ENativeValueCategory::Typedef;
	}

	static bool IsWritableTarget(const FTargetCase& TargetCase)
	{
		return IsTarget(TargetCase, "mutable_local")
			|| IsTarget(TargetCase, "mutable_field")
			|| IsTarget(TargetCase, "reference_alias");
	}

	static bool IsCurrentForkAcceptedConstReferenceAssignment(
		const FNativeTypeCase& TypeCase,
		const FAssignmentCase& AssignmentCase,
		const FTargetCase& TargetCase)
	{
		// The current fork accepts this narrow const local reference assignment.
		// Keep it as an enabled characterization until the fork adopts the
		// stricter 2.38 const-target diagnostic.
		return IsReferenceType(TypeCase)
			&& (IsAssignment(AssignmentCase, "simple")
				|| IsAssignment(AssignmentCase, "copy_source")
				|| IsAssignment(AssignmentCase, "self_assignment")
				|| IsAssignment(AssignmentCase, "reference_rebind"))
			&& IsTarget(TargetCase, "const_local");
	}

	static bool IsCurrentForkAcceptedDiscardedValueAssignment(
		const FNativeTypeCase& TypeCase,
		const FAssignmentCase& AssignmentCase,
		const FTargetCase& TargetCase)
	{
		return IsValueObjectType(TypeCase)
			&& (IsAssignment(AssignmentCase, "simple")
				|| IsAssignment(AssignmentCase, "copy_source")
				|| IsAssignment(AssignmentCase, "self_assignment"))
			&& (IsTarget(TargetCase, "temporary") || IsTarget(TargetCase, "expression_result"));
	}

	static bool ShouldCompile(
		const FNativeTypeCase& TypeCase,
		const FAssignmentCase& AssignmentCase,
		const FTargetCase& TargetCase)
	{
		if (IsCurrentForkAcceptedConstReferenceAssignment(TypeCase, AssignmentCase, TargetCase))
		{
			return true;
		}
		if (IsCurrentForkAcceptedDiscardedValueAssignment(TypeCase, AssignmentCase, TargetCase))
		{
			return true;
		}
		if (!IsWritableTarget(TargetCase))
		{
			return false;
		}
		if (IsAssignment(AssignmentCase, "compound"))
		{
			return SupportsCompoundAssignment(TypeCase);
		}
		if (IsAssignment(AssignmentCase, "reference_rebind"))
		{
			return IsReferenceType(TypeCase);
		}
		return true;
	}

	static FString MakeSuffix(
		const FAssignmentCase& AssignmentCase,
		const FTargetCase& TargetCase,
		const FNativeTypeCase& TypeCase)
	{
		return FString::Printf(
			TEXT("%hs_%hs_%hs"),
			AssignmentCase.CatalogName,
			TargetCase.CatalogName,
			TypeCase.CatalogName);
	}

	static FString MakeTypedValue(const FNativeTypeCase& TypeCase, const int32 Value)
	{
		if (TypeCase.Category == ENativeValueCategory::Boolean)
		{
			return Value == 0 ? TEXT("false") : TEXT("true");
		}
		if (TypeCase.Category == ENativeValueCategory::Enum)
		{
			return Value == 0 ? TEXT("ENativeAssignmentEnum::Zero") : TEXT("ENativeAssignmentEnum::One");
		}
		if (TypeCase.Category == ENativeValueCategory::ScriptReference)
		{
			return FString::Printf(TEXT("FScriptCaseReference(%d)"), Value);
		}
		if (TypeCase.Category == ENativeValueCategory::NativeReference)
		{
			return FString::Printf(TEXT("CreateNativeCaseReference(%d)"), Value);
		}
		return FString::Printf(TEXT("%hs(%d)"), TypeCase.ScriptType, Value);
	}

	static int32 InitialObservedValue(const FNativeTypeCase& TypeCase)
	{
		return TypeCase.Category == ENativeValueCategory::Boolean
			|| TypeCase.Category == ENativeValueCategory::Enum
			? 0
			: 11;
	}

	static int32 SourceObservedValue(const FNativeTypeCase& TypeCase)
	{
		return TypeCase.Category == ENativeValueCategory::Boolean
			|| TypeCase.Category == ENativeValueCategory::Enum
			? 1
			: 29;
	}

	static int32 AlternateObservedValue(const FNativeTypeCase& TypeCase)
	{
		return TypeCase.Category == ENativeValueCategory::Boolean
			|| TypeCase.Category == ENativeValueCategory::Enum
			? 0
			: 37;
	}

	static int32 ExpectedResult(
		const FNativeTypeCase& TypeCase,
		const FAssignmentCase& AssignmentCase,
		const FTargetCase& TargetCase)
	{
		if (IsCurrentForkAcceptedDiscardedValueAssignment(TypeCase, AssignmentCase, TargetCase))
		{
			return 0;
		}
		if (IsAssignment(AssignmentCase, "self_assignment"))
		{
			return InitialObservedValue(TypeCase);
		}
		if (IsAssignment(AssignmentCase, "compound"))
		{
			return InitialObservedValue(TypeCase) + SourceObservedValue(TypeCase);
		}
		if (IsAssignment(AssignmentCase, "copy_source"))
		{
			const int32 SourceAfter = IsTarget(TargetCase, "reference_alias")
				? SourceObservedValue(TypeCase)
				: (IsObjectType(TypeCase)
				? AlternateObservedValue(TypeCase)
				: SourceObservedValue(TypeCase));
			const int32 TargetAfter = IsReferenceType(TypeCase)
				? SourceAfter
				: SourceObservedValue(TypeCase);
			return TargetAfter * 100 + SourceAfter;
		}
		if (IsAssignment(AssignmentCase, "reference_rebind"))
		{
			const int32 SharedAfter = AlternateObservedValue(TypeCase);
			return SharedAfter * 100 + SharedAfter;
		}
		return SourceObservedValue(TypeCase);
	}

	static int32 BeginScriptAssignmentValue(const int32 Value)
	{
		FNativeLifecycleRecorder* const Recorder = AngelscriptNativeTestSupport::GetActiveNativeLifecycleRecorder();
		if (Recorder == nullptr)
		{
			return INDEX_NONE;
		}
		const int32 ObjectId = Recorder->AllocateObjectId();
		Recorder->Record(ENativeLifecycleEvent::ValueConstruct, ObjectId, INDEX_NONE, Value);
		return ObjectId;
	}

	static int32 CopyScriptAssignmentValue(const int32 SourceObjectId, const int32 Value)
	{
		FNativeLifecycleRecorder* const Recorder = AngelscriptNativeTestSupport::GetActiveNativeLifecycleRecorder();
		if (Recorder == nullptr)
		{
			return INDEX_NONE;
		}
		const int32 ObjectId = Recorder->AllocateObjectId();
		Recorder->Record(ENativeLifecycleEvent::CopyConstruct, ObjectId, SourceObjectId, Value);
		return ObjectId;
	}

	static void RecordScriptAssignment(
		const int32 ObjectId,
		const int32 SourceObjectId,
		const int32 Value)
	{
		if (FNativeLifecycleRecorder* const Recorder = AngelscriptNativeTestSupport::GetActiveNativeLifecycleRecorder())
		{
			Recorder->Record(ENativeLifecycleEvent::Assign, ObjectId, SourceObjectId, Value);
		}
	}

	static void EndScriptAssignmentValue(const int32 ObjectId, const int32 Value)
	{
		if (FNativeLifecycleRecorder* const Recorder = AngelscriptNativeTestSupport::GetActiveNativeLifecycleRecorder())
		{
			Recorder->Record(ENativeLifecycleEvent::Destruct, ObjectId, INDEX_NONE, Value);
		}
	}

	static bool RegisterScriptAssignmentLifecycle(asIScriptEngine& Engine)
	{
		const ASAutoCaller::FunctionCaller BeginCaller = ASAutoCaller::MakeFunctionCaller(BeginScriptAssignmentValue);
		const ASAutoCaller::FunctionCaller CopyCaller = ASAutoCaller::MakeFunctionCaller(CopyScriptAssignmentValue);
		const ASAutoCaller::FunctionCaller AssignCaller = ASAutoCaller::MakeFunctionCaller(RecordScriptAssignment);
		const ASAutoCaller::FunctionCaller EndCaller = ASAutoCaller::MakeFunctionCaller(EndScriptAssignmentValue);
		return Engine.RegisterGlobalFunction(
			"int BeginScriptAssignmentValue(int Value)",
			asFUNCTION(BeginScriptAssignmentValue),
			asCALL_CDECL,
			*(asFunctionCaller*)&BeginCaller) >= 0
			&& Engine.RegisterGlobalFunction(
				"int CopyScriptAssignmentValue(int SourceObjectId, int Value)",
				asFUNCTION(CopyScriptAssignmentValue),
				asCALL_CDECL,
				*(asFunctionCaller*)&CopyCaller) >= 0
			&& Engine.RegisterGlobalFunction(
				"void RecordScriptAssignment(int ObjectId, int SourceObjectId, int Value)",
				asFUNCTION(RecordScriptAssignment),
				asCALL_CDECL,
				*(asFunctionCaller*)&AssignCaller) >= 0
			&& Engine.RegisterGlobalFunction(
				"void EndScriptAssignmentValue(int ObjectId, int Value)",
				asFUNCTION(EndScriptAssignmentValue),
				asCALL_CDECL,
				*(asFunctionCaller*)&EndCaller) >= 0;
	}

	static void AppendTypeDeclarations(FString& Source, const FNativeTypeCase& TypeCase)
	{
		using namespace AngelscriptNativeTestSupport;

		if (TypeCase.Category == ENativeValueCategory::Enum)
		{
			AppendGeneratedAsLine(Source, TEXT("enum ENativeAssignmentEnum"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tZero = 0,"));
			AppendGeneratedAsLine(Source, TEXT("\tOne = 1"));
			AppendGeneratedAsLine(Source, TEXT("}"));
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
			AppendGeneratedAsLine(Source, TEXT("\t\tObjectId = BeginScriptAssignmentValue(Value);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tFScriptCaseValue(int InValue)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tValue = InValue;"));
			AppendGeneratedAsLine(Source, TEXT("\t\tObjectId = BeginScriptAssignmentValue(Value);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tFScriptCaseValue(const FScriptCaseValue& Other)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tValue = Other.Value;"));
			AppendGeneratedAsLine(Source, TEXT("\t\tObjectId = CopyScriptAssignmentValue(Other.ObjectId, Value);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tFScriptCaseValue& opAssign(const FScriptCaseValue& Other)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tValue = Other.Value;"));
			AppendGeneratedAsLine(Source, TEXT("\t\tRecordScriptAssignment(ObjectId, Other.ObjectId, Value);"));
			AppendGeneratedAsLine(Source, TEXT("\t\treturn this;"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\t~FScriptCaseValue()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tEndScriptAssignmentValue(ObjectId, Value);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
		else if (TypeCase.Category == ENativeValueCategory::ScriptReference)
		{
			AppendGeneratedAsLine(Source, TEXT("class FScriptCaseReference"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\tint Value = 0;"));
			AppendGeneratedAsLine(Source, TEXT("\tint ObjectId = -1;"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tFScriptCaseReference()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tObjectId = BeginScriptAssignmentValue(Value);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\tFScriptCaseReference(int InValue)"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tValue = InValue;"));
			AppendGeneratedAsLine(Source, TEXT("\t\tObjectId = BeginScriptAssignmentValue(Value);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source);
			AppendGeneratedAsLine(Source, TEXT("\t~FScriptCaseReference()"));
			AppendGeneratedAsLine(Source, TEXT("\t{"));
			AppendGeneratedAsLine(Source, TEXT("\t\tEndScriptAssignmentValue(ObjectId, Value);"));
			AppendGeneratedAsLine(Source, TEXT("\t}"));
			AppendGeneratedAsLine(Source, TEXT("}"));
			AppendGeneratedAsLine(Source);
		}
	}

	static void AppendObservationFunction(FString& Source, const FNativeTypeCase& TypeCase)
	{
		using namespace AngelscriptNativeTestSupport;

		if (IsReferenceType(TypeCase))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("int ObserveAssignmentValue(const %hs Value)"),
				GetGeneratedTypeName(TypeCase)));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value == nullptr ? -1 : Value.Value;"));
		}
		else if (IsValueObjectType(TypeCase))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("int ObserveAssignmentValue(const %hs& in Value)"),
				GetGeneratedTypeName(TypeCase)));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value.Value;"));
		}
		else if (TypeCase.Category == ENativeValueCategory::Boolean)
		{
			AppendGeneratedAsLine(Source, TEXT("int ObserveAssignmentValue(bool Value)"));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TEXT("\treturn Value ? 1 : 0;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("int ObserveAssignmentValue(%hs Value)"),
				GetGeneratedTypeName(TypeCase)));
			AppendGeneratedAsLine(Source, TEXT("{"));
			AppendGeneratedAsLine(Source, TypeCase.Category == ENativeValueCategory::Typedef
				? TEXT("\treturn Value;")
				: TEXT("\treturn int(Value);"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendHolderType(FString& Source, const FNativeTypeCase& TypeCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("class FAssignmentHolder"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("\t%hs Target = %s;"),
			GetGeneratedTypeName(TypeCase),
			*MakeTypedValue(TypeCase, InitialObservedValue(TypeCase))));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static FString MakeTargetExpression(
		const FNativeTypeCase& TypeCase,
		const FTargetCase& TargetCase)
	{
		if (IsTarget(TargetCase, "mutable_local") || IsTarget(TargetCase, "const_local"))
		{
			return TEXT("Target");
		}
		if (IsTarget(TargetCase, "mutable_field") || IsTarget(TargetCase, "const_field"))
		{
			return TEXT("Holder.Target");
		}
		if (IsTarget(TargetCase, "reference_alias"))
		{
			return TEXT("Alias");
		}
		const FString InitialValue = MakeTypedValue(TypeCase, InitialObservedValue(TypeCase));
		if (IsTarget(TargetCase, "temporary"))
		{
			return TypeCase.Category == ENativeValueCategory::Enum
				? TEXT("ENativeAssignmentEnum(0)")
				: InitialValue;
		}
		return FString::Printf(
			TEXT("(true ? %s : %s)"),
			*InitialValue,
			*MakeTypedValue(TypeCase, SourceObservedValue(TypeCase)));
	}

	static void AppendSourceMutation(
		FString& Source,
		const FNativeTypeCase& TypeCase,
		const FString& SourceName,
		const FString& Indent)
	{
		using namespace AngelscriptNativeTestSupport;

		if (IsObjectType(TypeCase))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%s%s.Value = %d;"),
				*Indent,
				*SourceName,
				AlternateObservedValue(TypeCase)));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%s%s = %s;"),
				*Indent,
				*SourceName,
				*MakeTypedValue(TypeCase, AlternateObservedValue(TypeCase))));
		}
	}

	static void AppendAssignmentOperation(
		FString& Source,
		const FNativeTypeCase& TypeCase,
		const FAssignmentCase& AssignmentCase,
		const FString& TargetExpression,
		const FString& Indent)
	{
		using namespace AngelscriptNativeTestSupport;

		if (IsAssignment(AssignmentCase, "self_assignment"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%s%s = %s;"),
				*Indent,
				*TargetExpression,
				*TargetExpression));
		}
		else if (IsAssignment(AssignmentCase, "compound"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%s%s += %s;"),
				*Indent,
				*TargetExpression,
				*MakeTypedValue(TypeCase, SourceObservedValue(TypeCase))));
		}
		else if (IsAssignment(AssignmentCase, "reference_rebind") && !IsReferenceType(TypeCase))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%s%s = nullptr;"),
				*Indent,
				*TargetExpression));
		}
		else
		{
			const FString SourceExpression = IsAssignment(AssignmentCase, "copy_source")
				|| IsAssignment(AssignmentCase, "reference_rebind")
				? TEXT("Source")
				: MakeTypedValue(TypeCase, SourceObservedValue(TypeCase));
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%s%s = %s;"),
				*Indent,
				*TargetExpression,
				*SourceExpression));
		}
	}

	static void AppendResultReturn(
		FString& Source,
		const FNativeTypeCase& TypeCase,
		const FAssignmentCase& AssignmentCase,
		const FString& TargetExpression,
		const FString& Indent)
	{
		using namespace AngelscriptNativeTestSupport;

		const bool bMutateSource = IsObjectType(TypeCase)
			&& (IsAssignment(AssignmentCase, "copy_source")
				|| IsAssignment(AssignmentCase, "reference_rebind"));
		if (bMutateSource)
		{
			AppendSourceMutation(Source, TypeCase, TEXT("Source"), Indent);
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%sreturn ObserveAssignmentValue(%s) * 100 + ObserveAssignmentValue(Source);"),
				*Indent,
				*TargetExpression));
		}
		else if (IsAssignment(AssignmentCase, "copy_source")
			|| IsAssignment(AssignmentCase, "reference_rebind"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%sreturn ObserveAssignmentValue(%s) * 100 + ObserveAssignmentValue(Source);"),
				*Indent,
				*TargetExpression));
		}
		else
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("%sreturn ObserveAssignmentValue(%s);"),
				*Indent,
				*TargetExpression));
		}
	}

	static void AppendAliasFunction(
		FString& Source,
		const FNativeTypeCase& TypeCase,
		const FAssignmentCase& AssignmentCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, FString::Printf(
			TEXT("int ApplyAssignmentAlias(%hs& inout Alias, %hs Source)"),
			GetGeneratedTypeName(TypeCase),
			GetGeneratedTypeName(TypeCase)));
		AppendGeneratedAsLine(Source, TEXT("{"));
		if (IsAssignment(AssignmentCase, "self_assignment"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tAlias = Alias;"));
		}
		else if (IsAssignment(AssignmentCase, "compound"))
		{
			AppendGeneratedAsLine(Source, TEXT("\tAlias += Source;"));
		}
		else if (IsAssignment(AssignmentCase, "reference_rebind") && !IsReferenceType(TypeCase))
		{
			AppendGeneratedAsLine(Source, TEXT("\tAlias = nullptr;"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\tAlias = Source;"));
		}
		if (IsAssignment(AssignmentCase, "copy_source"))
		{
			AppendGeneratedAsLine(Source,
				TEXT("\treturn ObserveAssignmentValue(Alias) * 100 + ObserveAssignmentValue(Source);"));
		}
		else if (IsAssignment(AssignmentCase, "reference_rebind"))
		{
			AppendResultReturn(Source, TypeCase, AssignmentCase, TEXT("Alias"), TEXT("\t"));
		}
		else
		{
			AppendGeneratedAsLine(Source, TEXT("\tint SourceTrace = ObserveAssignmentValue(Source);"));
			AppendGeneratedAsLine(Source, TEXT("\treturn ObserveAssignmentValue(Alias) + SourceTrace - SourceTrace;"));
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static void AppendEntryTargetDeclaration(
		FString& Source,
		const FNativeTypeCase& TypeCase,
		const FTargetCase& TargetCase)
	{
		using namespace AngelscriptNativeTestSupport;

		if (IsTarget(TargetCase, "mutable_local")
			|| IsTarget(TargetCase, "const_local")
			|| IsTarget(TargetCase, "reference_alias"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t%s%hs Target = %s;"),
				IsTarget(TargetCase, "const_local") ? TEXT("const ") : TEXT(""),
				GetGeneratedTypeName(TypeCase),
				*MakeTypedValue(TypeCase, InitialObservedValue(TypeCase))));
		}
		else if (IsTarget(TargetCase, "mutable_field") || IsTarget(TargetCase, "const_field"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t%sFAssignmentHolder Holder = FAssignmentHolder();"),
				IsTarget(TargetCase, "const_field") ? TEXT("const ") : TEXT("")));
		}
	}

	static void AppendEntryFunction(
		FString& Source,
		const FNativeTypeCase& TypeCase,
		const FAssignmentCase& AssignmentCase,
		const FTargetCase& TargetCase)
	{
		using namespace AngelscriptNativeTestSupport;

		AppendGeneratedAsLine(Source, TEXT("int RunVariableAssignment()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendEntryTargetDeclaration(Source, TypeCase, TargetCase);
		if (IsAssignment(AssignmentCase, "copy_source")
			|| IsAssignment(AssignmentCase, "reference_rebind"))
		{
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\t%hs Source = %s;"),
				GetGeneratedTypeName(TypeCase),
				*MakeTypedValue(TypeCase, SourceObservedValue(TypeCase))));
		}

		if (IsTarget(TargetCase, "reference_alias"))
		{
			const FString AliasSource = IsAssignment(AssignmentCase, "copy_source")
				|| IsAssignment(AssignmentCase, "reference_rebind")
				? TEXT("Source")
				: MakeTypedValue(TypeCase, SourceObservedValue(TypeCase));
			AppendGeneratedAsLine(Source, FString::Printf(
				TEXT("\treturn ApplyAssignmentAlias(Target, %s);"),
				*AliasSource));
		}
		else
		{
			const FString TargetExpression = MakeTargetExpression(TypeCase, TargetCase);
			AppendAssignmentOperation(Source, TypeCase, AssignmentCase, TargetExpression, TEXT("\t"));
			if (IsWritableTarget(TargetCase)
				|| IsCurrentForkAcceptedConstReferenceAssignment(TypeCase, AssignmentCase, TargetCase))
			{
				AppendResultReturn(Source, TypeCase, AssignmentCase, TargetExpression, TEXT("\t"));
			}
			else if (IsCurrentForkAcceptedDiscardedValueAssignment(TypeCase, AssignmentCase, TargetCase))
			{
				AppendGeneratedAsLine(Source, TEXT("\treturn 0;"));
			}
			else
			{
				AppendGeneratedAsLine(Source, TEXT("\treturn 0;"));
			}
		}
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
	}

	static FString BuildAssignmentSource(
		const FNativeTypeCase& TypeCase,
		const FAssignmentCase& AssignmentCase,
		const FTargetCase& TargetCase)
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendTypeDeclarations(Source, TypeCase);
		AppendObservationFunction(Source, TypeCase);
		if (IsTarget(TargetCase, "mutable_field") || IsTarget(TargetCase, "const_field"))
		{
			AppendHolderType(Source, TypeCase);
		}
		if (IsTarget(TargetCase, "reference_alias"))
		{
			AppendAliasFunction(Source, TypeCase, AssignmentCase);
		}
		AppendEntryFunction(Source, TypeCase, AssignmentCase, TargetCase);
		return Source;
	}

	static FString BuildRecoverySource()
	{
		using namespace AngelscriptNativeTestSupport;

		FString Source;
		AppendGeneratedAsLine(Source, TEXT("int RunVariableAssignmentRecovery()"));
		AppendGeneratedAsLine(Source, TEXT("{"));
		AppendGeneratedAsLine(Source, TEXT("\treturn 97;"));
		AppendGeneratedAsLine(Source, TEXT("}"));
		AppendGeneratedAsLine(Source);
		return Source;
	}

	static bool HasLocatedError(const AngelscriptNativeTestSupport::FNativeMessageCollector& Messages)
	{
		return Messages.Entries.ContainsByPredicate([](const AngelscriptNativeTestSupport::FNativeMessageEntry& Entry)
		{
			return Entry.Type == asMSGTYPE_ERROR
				&& Entry.Row > 0
				&& Entry.Column > 0
				&& !Entry.Section.IsEmpty()
				&& !Entry.Message.IsEmpty();
		});
	}

	void VerifyMetadata(
		const FNativeCaseContext& Case,
		const FNativeTypeCase& TypeCase,
		const FAssignmentCase& AssignmentCase,
		const FTargetCase& TargetCase,
		asIScriptModule& Module)
	{
		asIScriptFunction* const Entry = Module.GetFunctionByDecl("int RunVariableAssignment()");
		ASSERT_THAT(IsNotNull(Entry,
			*Case.Describe(TEXT("legal assignment cell should expose its exact entry metadata"))));
		const int32 ExpectedTypeId = Module.GetTypeIdByDecl(GetGeneratedTypeName(TypeCase));
		ASSERT_THAT(IsTrue(ExpectedTypeId >= 0,
			*Case.Describe(TEXT("legal assignment cell should resolve its catalog type declaration"))));
		if (IsTarget(TargetCase, "mutable_field"))
		{
			asITypeInfo* const HolderType = Module.GetTypeInfoByName("FAssignmentHolder");
			ASSERT_THAT(IsNotNull(HolderType,
				*Case.Describe(TEXT("field assignment should publish its holder type"))));
			if (HolderType != nullptr)
			{
				const char* Name = nullptr;
				int TypeId = asTYPEID_VOID;
				ASSERT_THAT(IsTrue(HolderType->GetProperty(0, &Name, &TypeId) >= 0
					&& Name != nullptr
					&& FCStringAnsi::Strcmp(Name, "Target") == 0,
					*Case.Describe(TEXT("field assignment should expose the exact Target property"))));
				ASSERT_THAT(AreEqual(ExpectedTypeId, TypeId,
					*Case.Describe(TEXT("field assignment metadata should preserve the catalog type"))));
			}
		}
		else if (IsTarget(TargetCase, "reference_alias"))
		{
			const TArray<asIScriptFunction*> AliasCandidates = FindNativeFunctionsByName(&Module, "ApplyAssignmentAlias");
			asIScriptFunction* const AliasFunction = AliasCandidates.Num() == 1 ? AliasCandidates[0] : nullptr;
			ASSERT_THAT(IsNotNull(AliasFunction,
				*Case.Describe(TEXT("alias assignment should expose its exact inout helper"))));
			if (AliasFunction != nullptr)
			{
				int TypeId = asTYPEID_VOID;
				asDWORD TypeModifier = asTM_NONE;
				const char* Name = nullptr;
				ASSERT_THAT(IsTrue(AliasFunction->GetParam(0, &TypeId, &TypeModifier, &Name) >= 0
					&& Name != nullptr
					&& FCStringAnsi::Strcmp(Name, "Alias") == 0,
					*Case.Describe(TEXT("alias helper should preserve the Alias parameter name"))));
				ASSERT_THAT(AreEqual(ExpectedTypeId, TypeId,
					*Case.Describe(TEXT("alias helper should preserve the catalog parameter type"))));
				ASSERT_THAT(AreEqual(static_cast<uint32>(asTM_INOUTREF), static_cast<uint32>(TypeModifier),
					*Case.Describe(TEXT("alias helper should preserve inout direction metadata"))));
			}
		}
		else if (Entry != nullptr)
		{
			bool bFoundTarget = false;
			for (asUINT Index = 0; Index < Entry->GetVarCount(); ++Index)
			{
				const char* Name = nullptr;
				int TypeId = asTYPEID_VOID;
				if (Entry->GetVar(Index, &Name, &TypeId) >= 0
					&& Name != nullptr
					&& FCStringAnsi::Strcmp(Name, "Target") == 0)
				{
					bFoundTarget = true;
					if (IsCurrentForkAcceptedConstReferenceAssignment(TypeCase, AssignmentCase, TargetCase))
					{
						const bool bReferenceHandle = (TypeId & asTYPEID_OBJHANDLE) != 0;
						ASSERT_THAT(IsTrue(bReferenceHandle,
							*Case.Describe(TEXT("current fork const-reference assignment should retain an object-handle debug type"))));
					}
					else
					{
						ASSERT_THAT(AreEqual(ExpectedTypeId, TypeId,
							*Case.Describe(TEXT("local assignment target should preserve its debug type"))));
					}
				}
			}
			if (IsCurrentForkAcceptedConstReferenceAssignment(TypeCase, AssignmentCase, TargetCase))
			{
				ASSERT_THAT(IsTrue(bFoundTarget,
					*Case.Describe(TEXT("current fork const-reference assignment should expose its local debug variable"))));
			}
			else if (IsCurrentForkAcceptedDiscardedValueAssignment(TypeCase, AssignmentCase, TargetCase))
			{
				ASSERT_THAT(IsFalse(bFoundTarget,
					*Case.Describe(TEXT("current fork discarded-value assignment should preserve its omission from local debug-variable metadata"))));
			}
			else
			{
				ASSERT_THAT(IsTrue(bFoundTarget,
					*Case.Describe(TEXT("local assignment should expose the Target debug variable"))));
			}
		}
	}

	void VerifyLifecycle(
		const FNativeCaseContext& Case,
		const FNativeTypeCase& TypeCase,
		const FAssignmentCase& AssignmentCase,
		const FNativeLifecycleRecorder& Lifecycle)
	{
		ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
			*Case.Describe(TEXT("assignment execution should leave no tracked object alive"))));
		if (!IsObjectType(TypeCase))
		{
			ASSERT_THAT(AreEqual(0, Lifecycle.GetEntries().Num(),
				*Case.Describe(TEXT("primitive assignment should not produce object lifecycle events"))));
			return;
		}

		TSet<int32> ConstructedIds;
		TSet<int32> DestructedIds;
		for (const AngelscriptNativeTestSupport::FNativeLifecycleEntry& Entry : Lifecycle.GetEntries())
		{
			if (Entry.Event == ENativeLifecycleEvent::DefaultConstruct
				|| Entry.Event == ENativeLifecycleEvent::ValueConstruct
				|| Entry.Event == ENativeLifecycleEvent::CopyConstruct)
			{
				ASSERT_THAT(IsFalse(ConstructedIds.Contains(Entry.ObjectId),
					*Case.Describe(TEXT("each assignment object construction should allocate a unique identity"))));
				if (Entry.Event == ENativeLifecycleEvent::CopyConstruct)
				{
					ASSERT_THAT(IsTrue(ConstructedIds.Contains(Entry.RelatedObjectId),
						*Case.Describe(TEXT("each assignment copy should identify an existing source"))));
				}
				ConstructedIds.Add(Entry.ObjectId);
			}
			else if (Entry.Event == ENativeLifecycleEvent::Destruct)
			{
				ASSERT_THAT(IsTrue(ConstructedIds.Contains(Entry.ObjectId),
					*Case.Describe(TEXT("each assignment destructor should identify a constructed object"))));
				ASSERT_THAT(IsFalse(DestructedIds.Contains(Entry.ObjectId),
					*Case.Describe(TEXT("each assignment object should be destroyed no more than once"))));
				DestructedIds.Add(Entry.ObjectId);
			}
		}
		ASSERT_THAT(IsTrue(ConstructedIds.Num() > 0,
			*Case.Describe(TEXT("object assignment should exercise at least one real object"))));
		ASSERT_THAT(AreEqual(ConstructedIds.Num(), DestructedIds.Num(),
			*Case.Describe(TEXT("every assignment object should have one matching destructor"))));
		if (IsAssignment(AssignmentCase, "self_assignment") && IsValueObjectType(TypeCase))
		{
			ASSERT_THAT(IsTrue(Lifecycle.Num(ENativeLifecycleEvent::Assign) > 0,
				*Case.Describe(TEXT("value self-assignment should invoke its assignment behavior"))));
		}
		if (TypeCase.Category == ENativeValueCategory::NativeReference)
		{
			ASSERT_THAT(IsTrue(Lifecycle.Num(ENativeLifecycleEvent::AddRef) > 0,
				*Case.Describe(TEXT("native-reference assignment should retain at least one alias"))));
			ASSERT_THAT(IsTrue(Lifecycle.Num(ENativeLifecycleEvent::Release) > 0,
				*Case.Describe(TEXT("native-reference assignment should release every retained alias"))));
		}
	}

	void ExecuteLegalCell(
		const FNativeCaseContext& Case,
		const FNativeTypeCase& TypeCase,
		const FAssignmentCase& AssignmentCase,
		const FTargetCase& TargetCase,
		asIScriptEngine& ScriptEngine,
		asIScriptModule& Module,
		FNativeLifecycleRecorder& Lifecycle)
	{
		using namespace AngelscriptNativeTestSupport;

		VerifyMetadata(Case, TypeCase, AssignmentCase, TargetCase, Module);
		asIScriptFunction* const Entry = Module.GetFunctionByDecl("int RunVariableAssignment()");
		if (Entry == nullptr)
		{
			return;
		}
		asIScriptContext* const Context = ScriptEngine.CreateContext();
		ASSERT_THAT(IsNotNull(Context,
			*Case.Describe(TEXT("legal assignment cell should create an execution context"))));
		if (Context == nullptr)
		{
			return;
		}
		ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, Entry),
			*Case.Describe(TEXT("legal assignment cell should finish"))));
		ASSERT_THAT(AreEqual(ExpectedResult(TypeCase, AssignmentCase, TargetCase), static_cast<int32>(Context->GetReturnDWord()),
			*Case.Describe(TEXT("assignment result should preserve value-copy or reference-alias semantics"))));
		ASSERT_THAT(AreEqual(asSUCCESS, Context->Unprepare(),
			*Case.Describe(TEXT("assignment context should release all locals"))));
		Context->Release();
		VerifyLifecycle(Case, TypeCase, AssignmentCase, Lifecycle);
	}

	void CompileRecovery(
		const FNativeCaseContext& Case,
		asIScriptEngine& ScriptEngine,
		AngelscriptNativeTestSupport::FNativeTestEngine& Engine,
		const FString& ModuleName)
	{
		using namespace AngelscriptNativeTestSupport;

		const FString RecoverySource = BuildRecoverySource();
		PrintGeneratedAsSource(
			*TestRunner,
			Case.GetId() + TEXT("-RECOVERY"),
			ModuleName,
			RecoverySource);
		const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
		const FTCHARToUTF8 RecoverySourceUtf8(*RecoverySource);
		Engine.ResetMessages();
		asIScriptModule* RecoveryModule = nullptr;
		ASSERT_THAT(IsTrue(CompileNativeModule(
			&ScriptEngine,
			ModuleNameUtf8.Get(),
			RecoverySourceUtf8.Get(),
			RecoveryModule) >= 0,
			*Case.Describe(TEXT("rejected assignment should permit a same-name recovery build"))));
		ASSERT_THAT(IsNotNull(RecoveryModule,
			*Case.Describe(TEXT("assignment recovery should publish a clean module"))));
		if (RecoveryModule != nullptr)
		{
			asIScriptFunction* const Recovery = RecoveryModule->GetFunctionByDecl("int RunVariableAssignmentRecovery()");
			ASSERT_THAT(IsNotNull(Recovery,
				*Case.Describe(TEXT("assignment recovery should expose its exact entry"))));
			if (Recovery != nullptr)
			{
				asIScriptContext* const Context = ScriptEngine.CreateContext();
				ASSERT_THAT(IsNotNull(Context,
					*Case.Describe(TEXT("assignment recovery should create an execution context"))));
				if (Context != nullptr)
				{
					ASSERT_THAT(AreEqual(static_cast<int32>(asEXECUTION_FINISHED), PrepareAndExecute(Context, Recovery),
						*Case.Describe(TEXT("assignment recovery should execute cleanly"))));
					ASSERT_THAT(AreEqual(97, static_cast<int32>(Context->GetReturnDWord()),
						*Case.Describe(TEXT("assignment recovery should not retain failed-build state"))));
					Context->Release();
				}
			}
		}
		ScriptEngine.DiscardModule(ModuleNameUtf8.Get());
	}

public:
	TEST_METHOD(TypesByAssignmentAndTarget)
	{
		using namespace AngelscriptNativeTestSupport;

		AS_NATIVE_PRODUCT("LANG-VAR-ASSIGN-TARGET",
			ENativeEvidence::Compile
			| ENativeEvidence::Diagnostic
			| ENativeEvidence::Runtime
			| ENativeEvidence::Metadata
			| ENativeEvidence::Lifecycle
			| ENativeEvidence::Cleanup);

		FNativeTestEngine Engine;
		Engine.Create(*TestRunner);
		ON_SCOPE_EXIT { Engine.Destroy(); };
		asIScriptEngine* const ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine,
			TEXT("Variable-assignment product should create a raw SDK engine")));
		if (ScriptEngine == nullptr)
		{
			return;
		}

		FNativeLifecycleRecorder Lifecycle;
		Lifecycle.Reset();
		ASSERT_THAT(IsTrue(RegisterNativeCaseValue(*ScriptEngine, Lifecycle),
			TEXT("Variable-assignment product should register its tracked native value")));
		ASSERT_THAT(IsTrue(RegisterNativeCaseReference(*ScriptEngine, &Lifecycle),
			TEXT("Variable-assignment product should register its tracked native reference")));
		ASSERT_THAT(IsTrue(RegisterScriptAssignmentLifecycle(*ScriptEngine),
			TEXT("Variable-assignment product should register script value/reference lifecycle callbacks")));
		ASSERT_THAT(IsTrue(RegisterCoreLanguageTypedef(*ScriptEngine),
			TEXT("Variable-assignment product should register its core typedef through the raw SDK API")));

		for (const FAssignmentCase& AssignmentCase : AssignmentCases)
		{
			for (const FTargetCase& TargetCase : TargetCases)
			{
				for (const FNativeTypeCase& TypeCase : NativeTypeCases)
				{
					if (TypeCase.Category == ENativeValueCategory::Null)
					{
						continue;
					}

					Lifecycle.Reset();
					const FNativeCaseContext Case(MakeNativeCaseId(
						"LANG-VAR-ASSIGN-TARGET",
						{
							ANSI_TO_TCHAR(AssignmentCase.CatalogName),
							ANSI_TO_TCHAR(TargetCase.CatalogName),
							ANSI_TO_TCHAR(TypeCase.CatalogName),
						}));
					const FString Suffix = MakeSuffix(AssignmentCase, TargetCase, TypeCase);
					const FString ModuleName = TEXT("VariableAssignment_") + Suffix;
					const FString Source = BuildAssignmentSource(TypeCase, AssignmentCase, TargetCase);
					PrintGeneratedAsSource(*TestRunner, Case.GetId(), ModuleName, Source);
					const FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
					const FTCHARToUTF8 SourceUtf8(*Source);
					Engine.ResetMessages();
					asIScriptModule* Module = nullptr;
					const int BuildResult = CompileNativeModule(
						ScriptEngine,
						ModuleNameUtf8.Get(),
						SourceUtf8.Get(),
						Module);
					const bool bShouldCompile = ShouldCompile(TypeCase, AssignmentCase, TargetCase);
					if (bShouldCompile)
					{
						if (IsCurrentForkAcceptedConstReferenceAssignment(TypeCase, AssignmentCase, TargetCase))
						{
							TestRunner->AddInfo(FString::Printf(
								TEXT("[%s] current fork accepts simple or copy-source assignment to a const local script/native reference; runtime value and cleanup remain enabled until the stricter 2.38 diagnostic is adopted"),
								*Case.GetId()));
						}
						else if (IsCurrentForkAcceptedDiscardedValueAssignment(TypeCase, AssignmentCase, TargetCase))
						{
							TestRunner->AddInfo(FString::Printf(
								TEXT("[%s] current fork accepts assignment to a temporary script/native value; the product records compile, execution completion, and cleanup without treating the discarded temporary as a writable result"),
								*Case.GetId()));
						}
						ASSERT_THAT(IsTrue(BuildResult >= 0,
							*Case.Describe(TEXT("legal assignment cell should compile"))));
						ASSERT_THAT(IsNotNull(Module,
							*Case.Describe(TEXT("legal assignment cell should publish a module"))));
						if (BuildResult >= 0 && Module != nullptr)
						{
							ExecuteLegalCell(
								Case,
								TypeCase,
								AssignmentCase,
								TargetCase,
								*ScriptEngine,
								*Module,
								Lifecycle);
						}
					}
					else
					{
						ASSERT_THAT(IsTrue(BuildResult < 0,
							*Case.Describe(TEXT("non-writable or unsupported assignment cell should be rejected"))));
						ASSERT_THAT(IsTrue(HasLocatedError(Engine.GetMessages()),
							*Case.Describe(TEXT("rejected assignment should report a located diagnostic"))));
						ASSERT_THAT(AreEqual(0, Lifecycle.GetLiveObjectCount(),
							*Case.Describe(TEXT("failed assignment build should create no runtime object"))));
					}

					ScriptEngine->DiscardModule(ModuleNameUtf8.Get());
					ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
						*Case.Describe(TEXT("assignment cell should discard its isolated module"))));
					if (!bShouldCompile)
					{
						CompileRecovery(Case, *ScriptEngine, Engine, ModuleName);
						ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS),
							*Case.Describe(TEXT("assignment recovery should leave no module behind"))));
					}
				}
			}
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
